#include <loki/regression/polynomialRegressor.hpp>
#include <loki/regression/regressorUtils.hpp>
#include <loki/math/lsq.hpp>
#include <loki/math/designMatrix.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <random>

using namespace loki;
using namespace loki::regression;

// -----------------------------------------------------------------------------
//  Constants
// -----------------------------------------------------------------------------

static constexpr int MAX_CV_FOLDS  = 100;
static constexpr int MIN_CV_FOLDS  = 2;
static constexpr int DEFAULT_FOLDS = 10;

// -----------------------------------------------------------------------------
//  Construction
// -----------------------------------------------------------------------------

PolynomialRegressor::PolynomialRegressor(const RegressionConfig& cfg)
    : m_cfg(cfg)
{}

// -----------------------------------------------------------------------------
//  fit()
// -----------------------------------------------------------------------------

RegressionResult PolynomialRegressor::fit(const TimeSeries& ts)
{
    const int degree = m_cfg.polynomialDegree;
    const int nParam = degree + 1;

    // Collect valid observations.
    std::vector<double>      xVec;
    std::vector<double>      yVec;
    std::vector<::TimeStamp> times;

    xVec.reserve(ts.size());
    yVec.reserve(ts.size());
    times.reserve(ts.size());

    double tRef = std::numeric_limits<double>::quiet_NaN();

    for (std::size_t i = 0; i < ts.size(); ++i) {
        if (!isValid(ts[i])) continue;
        if (std::isnan(tRef)) tRef = ts[i].time.mjd();
        xVec.push_back(ts[i].time.mjd() - tRef);
        yVec.push_back(ts[i].value);
        times.push_back(ts[i].time);
    }

    const int n = static_cast<int>(xVec.size());

    if (n < nParam + 1) {
        throw DataException(
            "PolynomialRegressor::fit(): degree=" + std::to_string(degree) +
            " requires at least " + std::to_string(nParam + 1) +
            " valid observations, got " + std::to_string(n) + ".");
    }

    m_x = Eigen::Map<const Eigen::VectorXd>(xVec.data(), n);
    m_l = Eigen::Map<const Eigen::VectorXd>(yVec.data(), n);
    const Eigen::MatrixXd A = buildDesignMatrix(m_x);

    LsqSolver::Config solverCfg;
    solverCfg.robust        = m_cfg.robust;
    solverCfg.maxIterations = m_cfg.robustIterations;
    solverCfg.weightFn      = (m_cfg.robustWeightFn == "huber")
                              ? LsqWeightFunction::HUBER
                              : LsqWeightFunction::BISQUARE;

    const LsqResult lsq = LsqSolver::solve(A, m_l, solverCfg);

    RegressionResult result;
    result.modelName    = name();
    result.tRef         = tRef;
    result.coefficients = lsq.coefficients;
    result.residuals    = lsq.residuals;
    result.cofactorX    = lsq.cofactorX;
    result.sigma0       = lsq.sigma0;
    result.dof          = lsq.dof;
    result.converged    = lsq.converged;

    detail::computeGoodnessOfFit(result, m_l, nParam);

    const Eigen::VectorXd fitted = A * lsq.coefficients;
    result.fitted = TimeSeries(ts.metadata());
    for (int i = 0; i < n; ++i)
        result.fitted.append(times[static_cast<std::size_t>(i)], fitted[i]);

    m_lastResult = result;
    m_fitted     = true;

    return result;
}

// -----------------------------------------------------------------------------
//  name()
// -----------------------------------------------------------------------------

std::string PolynomialRegressor::name() const
{
    return "PolynomialRegressor(degree=" + std::to_string(m_cfg.polynomialDegree) + ")";
}

// -----------------------------------------------------------------------------
//  predict()
// -----------------------------------------------------------------------------

std::vector<PredictionPoint>
PolynomialRegressor::predict(const std::vector<double>& xNew) const
{
    if (!m_fitted) {
        throw AlgorithmException(
            "PolynomialRegressor::predict(): must call fit() before predict().");
    }

    const int k = static_cast<int>(xNew.size());
    Eigen::VectorXd xVec = Eigen::Map<const Eigen::VectorXd>(xNew.data(), k);
    const Eigen::MatrixXd aNew = buildDesignMatrix(xVec);

    // Fix x field in PredictionPoint -- detail::computeIntervals uses col 1
    // which is correct for polynomial (x is in column 1).
    return detail::computeIntervals(m_lastResult, aNew, m_cfg.confidenceLevel);
}

// -----------------------------------------------------------------------------
//  leaveOneOutCV()
// -----------------------------------------------------------------------------

PolynomialRegressor::CVResult PolynomialRegressor::leaveOneOutCV() const
{
    if (!m_fitted) {
        throw AlgorithmException(
            "PolynomialRegressor::leaveOneOutCV(): must call fit() before CV.");
    }

    // For robust fits, fall back to k-fold CV.
    if (m_cfg.robust) {
        LOKI_WARNING("PolynomialRegressor::leaveOneOutCV(): robust mode active -- "
                     "falling back to k-fold CV.");
        return kFoldCV();
    }

    const int n = static_cast<int>(m_x.size());
    const Eigen::MatrixXd A = buildDesignMatrix(m_x);

    // Hat matrix diagonal h_ii = (A * (A^T A)^{-1} * A^T)_ii
    // = row_i(A) * cofactorX * row_i(A)^T
    // This avoids forming the full n x n hat matrix.
    const Eigen::VectorXd& r = m_lastResult.residuals;

    double sumSq  = 0.0;
    double sumAbs = 0.0;
    double sumErr = 0.0;

    for (int i = 0; i < n; ++i) {
        const Eigen::VectorXd ai = A.row(i);
        const double hii = (ai.transpose() * m_lastResult.cofactorX * ai).value();

        if (std::fabs(1.0 - hii) < 1e-10) {
            // Leverage point with h_ii ~ 1 -- skip (would cause division by zero).
            continue;
        }

        const double cvErr = r[i] / (1.0 - hii);
        sumSq  += cvErr * cvErr;
        sumAbs += std::fabs(cvErr);
        sumErr += cvErr;
    }

    const double nd = static_cast<double>(n);
    CVResult cv;
    cv.rmse  = std::sqrt(sumSq  / nd);
    cv.mae   = sumAbs / nd;
    cv.bias  = sumErr / nd;
    cv.folds = n;
    return cv;
}

// -----------------------------------------------------------------------------
//  kFoldCV()
// -----------------------------------------------------------------------------

PolynomialRegressor::CVResult PolynomialRegressor::kFoldCV() const
{
    if (!m_fitted) {
        throw AlgorithmException(
            "PolynomialRegressor::kFoldCV(): must call fit() before CV.");
    }

    const int n = static_cast<int>(m_x.size());
    const int degree  = m_cfg.polynomialDegree;
    const int nParam  = degree + 1;

    // Determine number of folds with clamping and warning.
    int k = (m_cfg.cvFolds < MIN_CV_FOLDS) ? DEFAULT_FOLDS : m_cfg.cvFolds;
    const int maxFolds = std::min(MAX_CV_FOLDS, n / 2);

    if (k > maxFolds) {
        LOKI_WARNING("PolynomialRegressor::kFoldCV(): requested cv_folds=" +
                     std::to_string(k) + " exceeds maximum (" +
                     std::to_string(maxFolds) + ") -- clamping.");
        k = maxFolds;
    }
    if (k < MIN_CV_FOLDS) {
        throw DataException(
            "PolynomialRegressor::kFoldCV(): not enough data for " +
            std::to_string(MIN_CV_FOLDS) + " folds (n=" + std::to_string(n) + ").");
    }

    // Create shuffled index vector for fold assignment.
    std::vector<int> indices(static_cast<std::size_t>(n));
    std::iota(indices.begin(), indices.end(), 0);
    // Use fixed seed for reproducibility.
    std::mt19937 rng(42);
    std::shuffle(indices.begin(), indices.end(), rng);

    double sumSq  = 0.0;
    double sumAbs = 0.0;
    double sumErr = 0.0;
    int    nPred  = 0;

    for (int fold = 0; fold < k; ++fold) {
        // Partition indices into train / test.
        std::vector<int> testIdx, trainIdx;
        for (int i = 0; i < n; ++i) {
            if (i % k == fold) testIdx.push_back(indices[static_cast<std::size_t>(i)]);
            else               trainIdx.push_back(indices[static_cast<std::size_t>(i)]);
        }

        if (static_cast<int>(trainIdx.size()) <= nParam) continue;

        // Build train matrices.
        const int nTrain = static_cast<int>(trainIdx.size());
        Eigen::VectorXd xTrain(nTrain), lTrain(nTrain);
        for (int i = 0; i < nTrain; ++i) {
            xTrain[i] = m_x[trainIdx[static_cast<std::size_t>(i)]];
            lTrain[i] = m_l[trainIdx[static_cast<std::size_t>(i)]];
        }
        const Eigen::MatrixXd ATrain = buildDesignMatrix(xTrain);

        // Fit on training fold.
        LsqSolver::Config solverCfg;
        solverCfg.robust        = m_cfg.robust;
        solverCfg.maxIterations = m_cfg.robustIterations;
        solverCfg.weightFn      = (m_cfg.robustWeightFn == "huber")
                                  ? LsqWeightFunction::HUBER
                                  : LsqWeightFunction::BISQUARE;

        const LsqResult lsq = LsqSolver::solve(ATrain, lTrain, solverCfg);

        // Predict on test fold and accumulate errors.
        const int nTest = static_cast<int>(testIdx.size());
        Eigen::VectorXd xTest(nTest);
        for (int i = 0; i < nTest; ++i)
            xTest[i] = m_x[testIdx[static_cast<std::size_t>(i)]];

        const Eigen::MatrixXd ATest = buildDesignMatrix(xTest);
        const Eigen::VectorXd yPred = ATest * lsq.coefficients;

        for (int i = 0; i < nTest; ++i) {
            const double obs = m_l[testIdx[static_cast<std::size_t>(i)]];
            const double err = yPred[i] - obs;
            sumSq  += err * err;
            sumAbs += std::fabs(err);
            sumErr += err;
            ++nPred;
        }
    }

    if (nPred == 0) {
        throw AlgorithmException(
            "PolynomialRegressor::kFoldCV(): no valid predictions produced.");
    }

    const double nd = static_cast<double>(nPred);
    CVResult cv;
    cv.rmse  = std::sqrt(sumSq  / nd);
    cv.mae   = sumAbs / nd;
    cv.bias  = sumErr / nd;
    cv.folds = k;
    return cv;
}

// -----------------------------------------------------------------------------
//  buildDesignMatrix()
// -----------------------------------------------------------------------------

Eigen::MatrixXd PolynomialRegressor::buildDesignMatrix(const Eigen::VectorXd& x) const
{
    return DesignMatrix::polynomial(x, m_cfg.polynomialDegree);
}
