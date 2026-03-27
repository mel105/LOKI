#include <loki/regression/trendEstimator.hpp>
#include <loki/regression/regressorUtils.hpp>
#include <loki/math/lsq.hpp>
#include <loki/math/designMatrix.hpp>
#include <loki/core/exceptions.hpp>

#include <cmath>
#include <limits>
#include <numbers>

using namespace loki;
using namespace loki::regression;

// -----------------------------------------------------------------------------
//  Construction
// -----------------------------------------------------------------------------

TrendEstimator::TrendEstimator(const RegressionConfig& cfg)
    : m_cfg(cfg)
{}

// -----------------------------------------------------------------------------
//  name()
// -----------------------------------------------------------------------------

std::string TrendEstimator::name() const
{
    return "TrendEstimator(K=" + std::to_string(m_cfg.harmonicTerms) +
           ", T=" + std::to_string(static_cast<int>(m_cfg.period)) + "d)";
}

// -----------------------------------------------------------------------------
//  fit()
// -----------------------------------------------------------------------------

TrendEstimator::DecompositionResult TrendEstimator::fit(const TimeSeries& ts)
{
    const int K      = m_cfg.harmonicTerms;
    const int nParam = 2 + 2 * K;  // a0, a1, K*(sin+cos)

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
            "TrendEstimator::fit(): K=" + std::to_string(K) +
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

    // Populate regression result.
    RegressionResult regResult;
    regResult.modelName    = name();
    regResult.tRef         = tRef;
    regResult.coefficients = lsq.coefficients;
    regResult.residuals    = lsq.residuals;
    regResult.cofactorX    = lsq.cofactorX;
    regResult.sigma0       = lsq.sigma0;
    regResult.dof          = lsq.dof;
    regResult.converged    = lsq.converged;

    detail::computeGoodnessOfFit(regResult, l, nParam);

    // Extract coefficients.
    const double a0 = lsq.coefficients[0];  // intercept
    const double a1 = lsq.coefficients[1];  // slope

    const auto& meta = ts.metadata();

    // Build component TimeSeries.
    DecompositionResult result;
    result.regression      = regResult;
    result.trendSlope      = a1;
    result.trendIntercept  = a0;

    result.trend     = TimeSeries(meta);
    result.seasonal  = TimeSeries(meta);
    result.residuals = TimeSeries(meta);

    // Fitted = trend + seasonal column by column.
    for (int i = 0; i < n; ++i) {
        const double ti = t[i];

        // Trend component: a0 + a1*t
        const double trendVal = a0 + a1 * ti;

        // Seasonal component: sum of harmonic terms
        double seasonalVal = 0.0;
        const auto ps = periods();
        for (int k = 0; k < K; ++k) {
            const double omega = 2.0 * std::numbers::pi / ps[static_cast<std::size_t>(k)];
            const double sk = lsq.coefficients[2 + 2 * k];
            const double ck = lsq.coefficients[2 + 2 * k + 1];
            seasonalVal += sk * std::sin(omega * ti) + ck * std::cos(omega * ti);
        }

        const ::TimeStamp& stamp = times[static_cast<std::size_t>(i)];
        result.trend.append(stamp,     trendVal);
        result.seasonal.append(stamp,  seasonalVal);
        result.residuals.append(stamp, l[i] - trendVal - seasonalVal);
    }

    // Fitted TimeSeries (trend + seasonal).
    regResult.fitted = TimeSeries(meta);
    for (int i = 0; i < n; ++i)
        regResult.fitted.append(
            times[static_cast<std::size_t>(i)],
            result.trend[static_cast<std::size_t>(i)].value +
            result.seasonal[static_cast<std::size_t>(i)].value);

    result.regression = regResult;
    return result;
}

// -----------------------------------------------------------------------------
//  Private helpers
// -----------------------------------------------------------------------------

std::vector<double> TrendEstimator::periods() const
{
    const int K = m_cfg.harmonicTerms;
    std::vector<double> ps;
    ps.reserve(static_cast<std::size_t>(K));
    for (int k = 1; k <= K; ++k)
        ps.push_back(m_cfg.period / static_cast<double>(k));
    return ps;
}

Eigen::MatrixXd TrendEstimator::buildDesignMatrix(const Eigen::VectorXd& t) const
{
    const int n = static_cast<int>(t.size());
    const int K = m_cfg.harmonicTerms;

    // Columns: [1, t, sin(2pi*t/T1), cos(2pi*t/T1), ..., sin(2pi*t/TK), cos(2pi*t/TK)]
    Eigen::MatrixXd A(n, 2 + 2 * K);

    // Trend columns.
    A.col(0).setOnes();
    A.col(1) = t;

    // Harmonic columns.
    const auto ps = periods();
    for (int k = 0; k < K; ++k) {
        const double omega = 2.0 * std::numbers::pi / ps[static_cast<std::size_t>(k)];
        A.col(2 + 2 * k)     = (omega * t.array()).sin().matrix();
        A.col(2 + 2 * k + 1) = (omega * t.array()).cos().matrix();
    }

    return A;
}
