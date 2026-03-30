#include <loki/regression/harmonicRegressor.hpp>
#include <loki/regression/regressorUtils.hpp>
#include <loki/math/lsq.hpp>
#include <loki/math/designMatrix.hpp>
#include <loki/core/exceptions.hpp>

#include <cmath>
#include <limits>

using namespace loki;
using namespace loki::regression;

// -----------------------------------------------------------------------------
//  Construction
// -----------------------------------------------------------------------------

HarmonicRegressor::HarmonicRegressor(const RegressionConfig& cfg)
    : m_cfg(cfg)
{}

// -----------------------------------------------------------------------------
//  fit()
// -----------------------------------------------------------------------------

RegressionResult HarmonicRegressor::fit(const TimeSeries& ts)
{
    const int K      = m_cfg.harmonicTerms;
    const int nParam = 1 + 2 * K;  // a0 + K*(sin+cos)

    // Collect valid observations.
    std::vector<double>      tVec;
    std::vector<double>      yVec;
    std::vector<::TimeStamp> times;

    tVec.reserve(ts.size());
    yVec.reserve(ts.size());
    times.reserve(ts.size());

    double tRef = std::numeric_limits<double>::quiet_NaN();

    for (std::size_t i = 0; i < ts.size(); ++i) {
        if (!isValid(ts[i])) continue;
        if (std::isnan(tRef)) tRef = ts[i].time.mjd();
        tVec.push_back(ts[i].time.mjd() - tRef);
        yVec.push_back(ts[i].value);
        times.push_back(ts[i].time);
    }

    const int n = static_cast<int>(tVec.size());

    if (n < nParam + 1) {
        throw DataException(
            "HarmonicRegressor::fit(): K=" + std::to_string(K) +
            " requires at least " + std::to_string(nParam + 1) +
            " valid observations, got " + std::to_string(n) + ".");
    }

    Eigen::VectorXd t = Eigen::Map<const Eigen::VectorXd>(tVec.data(), n);
    Eigen::VectorXd l = Eigen::Map<const Eigen::VectorXd>(yVec.data(), n);
    const Eigen::MatrixXd A = buildDesignMatrix(t);

    LsqSolver::Config solverCfg;
    solverCfg.robust        = m_cfg.robust;
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

    result.designMatrix = A;

    m_lastResult = result;
    m_fitted     = true;

    return result;
}

// -----------------------------------------------------------------------------
//  name()
// -----------------------------------------------------------------------------

std::string HarmonicRegressor::name() const
{
    return "HarmonicRegressor(K=" + std::to_string(m_cfg.harmonicTerms) +
           ", T=" + std::to_string(static_cast<int>(m_cfg.period)) + "d)";
}

// -----------------------------------------------------------------------------
//  predict()
// -----------------------------------------------------------------------------

std::vector<PredictionPoint>
HarmonicRegressor::predict(const std::vector<double>& xNew) const
{
    if (!m_fitted) {
        throw AlgorithmException(
            "HarmonicRegressor::predict(): must call fit() before predict().");
    }
 
    const int k = static_cast<int>(xNew.size());
    Eigen::VectorXd tVec = Eigen::Map<const Eigen::VectorXd>(xNew.data(), k);
    const Eigen::MatrixXd aNew = buildDesignMatrix(tVec);
 
    return detail::computeIntervals(m_lastResult, aNew, xNew, m_cfg.confidenceLevel);
}

// -----------------------------------------------------------------------------
//  amplitude() and phase()
// -----------------------------------------------------------------------------

double HarmonicRegressor::amplitude(int k) const
{
    if (!m_fitted) {
        throw AlgorithmException(
            "HarmonicRegressor::amplitude(): must call fit() before.");
    }
    const int K = m_cfg.harmonicTerms;
    if (k < 1 || k > K) {
        throw AlgorithmException(
            "HarmonicRegressor::amplitude(): k=" + std::to_string(k) +
            " out of range [1, " + std::to_string(K) + "].");
    }
    // Coefficients: [a0, s1, c1, s2, c2, ...]
    const double sk = m_lastResult.coefficients[2 * k - 1];
    const double ck = m_lastResult.coefficients[2 * k];
    return std::sqrt(sk * sk + ck * ck);
}

double HarmonicRegressor::phase(int k) const
{
    if (!m_fitted) {
        throw AlgorithmException(
            "HarmonicRegressor::phase(): must call fit() before.");
    }
    const int K = m_cfg.harmonicTerms;
    if (k < 1 || k > K) {
        throw AlgorithmException(
            "HarmonicRegressor::phase(): k=" + std::to_string(k) +
            " out of range [1, " + std::to_string(K) + "].");
    }
    const double sk = m_lastResult.coefficients[2 * k - 1];
    const double ck = m_lastResult.coefficients[2 * k];
    return std::atan2(sk, ck);
}

// -----------------------------------------------------------------------------
//  Private helpers
// -----------------------------------------------------------------------------

std::vector<double> HarmonicRegressor::periods() const
{
    const int K = m_cfg.harmonicTerms;
    std::vector<double> ps;
    ps.reserve(static_cast<std::size_t>(K));
    for (int k = 1; k <= K; ++k)
        ps.push_back(m_cfg.period / static_cast<double>(k));
    return ps;
}

Eigen::MatrixXd HarmonicRegressor::buildDesignMatrix(const Eigen::VectorXd& t) const
{
    return DesignMatrix::harmonic(t, periods());
}
