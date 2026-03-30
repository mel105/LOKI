#include <loki/regression/robustRegressor.hpp>
#include <loki/regression/regressorUtils.hpp>
#include <loki/math/lsq.hpp>
#include <loki/math/designMatrix.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <cmath>
#include <limits>

using namespace loki;
using namespace loki::regression;

// -----------------------------------------------------------------------------
//  Constants -- Huber and Bisquare tuning constants
// -----------------------------------------------------------------------------

static constexpr double HUBER_K    = 1.345;  // 95% efficiency for Gaussian data
static constexpr double BISQUARE_C = 4.685;  // 95% efficiency for Gaussian data

// -----------------------------------------------------------------------------
//  Construction
// -----------------------------------------------------------------------------

RobustRegressor::RobustRegressor(const RegressionConfig& cfg)
    : m_cfg(cfg)
{
    if (!m_cfg.robust) {
        LOKI_WARNING("RobustRegressor: config.robust is false -- forcing robust=true.");
        m_cfg.robust = true;
    }
}

// -----------------------------------------------------------------------------
//  fit()
// -----------------------------------------------------------------------------

RegressionResult RobustRegressor::fit(const TimeSeries& ts)
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
            "RobustRegressor::fit(): degree=" + std::to_string(degree) +
            " requires at least " + std::to_string(nParam + 1) +
            " valid observations, got " + std::to_string(n) + ".");
    }

    Eigen::VectorXd x = Eigen::Map<const Eigen::VectorXd>(xVec.data(), n);
    Eigen::VectorXd l = Eigen::Map<const Eigen::VectorXd>(yVec.data(), n);
    const Eigen::MatrixXd A = buildDesignMatrix(x);

    // Delegate to LsqSolver with robust mode.
    LsqSolver::Config solverCfg;
    solverCfg.robust        = true;
    solverCfg.maxIterations = m_cfg.robustIterations;
    solverCfg.weightFn      = (m_cfg.robustWeightFn == "huber")
                              ? LsqWeightFunction::HUBER
                              : LsqWeightFunction::BISQUARE;

    const LsqResult lsq = LsqSolver::solve(A, l, solverCfg);

    RegressionResult result;
    result.modelName    = name();
    result.tRef         = tRef;
    result.coefficients = lsq.coefficients;
    result.residuals    = lsq.residuals;
    result.cofactorX    = lsq.cofactorX;
    result.sigma0       = lsq.sigma0;
    result.dof          = lsq.dof;
    result.converged    = lsq.converged;

    detail::computeGoodnessOfFit(result, l, nParam);

    const Eigen::VectorXd fitted = A * lsq.coefficients;
    result.fitted = TimeSeries(ts.metadata());
    for (int i = 0; i < n; ++i)
        result.fitted.append(times[static_cast<std::size_t>(i)], fitted[i]);

    // Compute final IRLS weights for diagnostics.
    // Recompute from final residuals using the chosen weight function.
    const LsqWeightFunction fn = solverCfg.weightFn;
    const Eigen::VectorXd w = irlsWeights(lsq.residuals, lsq.sigma0, fn);

    m_weights.clear();
    m_weights.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        WeightedObservation wo;
        wo.index    = static_cast<std::size_t>(i);
        wo.t        = xVec[static_cast<std::size_t>(i)];
        wo.y        = yVec[static_cast<std::size_t>(i)];
        wo.fitted   = fitted[i];
        wo.residual = lsq.residuals[i];
        wo.weight   = w[i];
        m_weights.push_back(wo);
    }

    result.designMatrix = A;

    m_lastResult = result;
    m_fitted     = true;

    return result;
}

// -----------------------------------------------------------------------------
//  name()
// -----------------------------------------------------------------------------

std::string RobustRegressor::name() const
{
    return "RobustRegressor(degree=" + std::to_string(m_cfg.polynomialDegree) +
           ", fn=" + m_cfg.robustWeightFn + ")";
}

// -----------------------------------------------------------------------------
//  predict()
// -----------------------------------------------------------------------------

std::vector<PredictionPoint>
RobustRegressor::predict(const std::vector<double>& xNew) const
{
    if (!m_fitted) {
        throw AlgorithmException(
            "RobustRegressor::predict(): must call fit() before predict().");
    }
 
    const int k = static_cast<int>(xNew.size());
    Eigen::VectorXd xVec = Eigen::Map<const Eigen::VectorXd>(xNew.data(), k);
    const Eigen::MatrixXd aNew = buildDesignMatrix(xVec);
 
    return detail::computeIntervals(m_lastResult, aNew, xNew, m_cfg.confidenceLevel);
}

// -----------------------------------------------------------------------------
//  weightedObservations()
// -----------------------------------------------------------------------------

const std::vector<RobustRegressor::WeightedObservation>&
RobustRegressor::weightedObservations() const
{
    if (!m_fitted) {
        throw AlgorithmException(
            "RobustRegressor::weightedObservations(): must call fit() before.");
    }
    return m_weights;
}

// -----------------------------------------------------------------------------
//  Private helpers
// -----------------------------------------------------------------------------

Eigen::MatrixXd RobustRegressor::buildDesignMatrix(const Eigen::VectorXd& x) const
{
    return DesignMatrix::polynomial(x, m_cfg.polynomialDegree);
}

Eigen::VectorXd RobustRegressor::irlsWeights(const Eigen::VectorXd& residuals,
                                               double                  sigma,
                                               LsqWeightFunction       fn)
{
    const int n = static_cast<int>(residuals.size());
    Eigen::VectorXd w(n);

    if (sigma < std::numeric_limits<double>::epsilon()) {
        w.setOnes();
        return w;
    }

    for (int i = 0; i < n; ++i) {
        const double u = std::fabs(residuals[i]) / sigma;

        if (fn == LsqWeightFunction::HUBER) {
            w[i] = (u <= HUBER_K) ? 1.0 : HUBER_K / u;
        } else {
            // Bisquare (Tukey)
            if (u >= BISQUARE_C) {
                w[i] = 0.0;
            } else {
                const double t = 1.0 - (u / BISQUARE_C) * (u / BISQUARE_C);
                w[i] = t * t;
            }
        }
    }
    return w;
}
