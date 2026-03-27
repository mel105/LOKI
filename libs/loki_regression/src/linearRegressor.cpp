#include <loki/regression/linearRegressor.hpp>
#include <loki/regression/regressorUtils.hpp>
#include <loki/math/lsq.hpp>
#include <loki/core/exceptions.hpp>

#include <cmath>
#include <limits>

using namespace loki;
using namespace loki::regression;

LinearRegressor::LinearRegressor(const RegressionConfig& cfg)
    : m_cfg(cfg)
{}

RegressionResult LinearRegressor::fit(const TimeSeries& ts)
{
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

    const int n      = static_cast<int>(xVec.size());
    const int nParam = 2;

    if (n < nParam + 1) {
        throw DataException(
            "LinearRegressor::fit(): need at least " + std::to_string(nParam + 1) +
            " valid observations, got " + std::to_string(n) + ".");
    }

    Eigen::VectorXd x = Eigen::Map<const Eigen::VectorXd>(xVec.data(), n);
    Eigen::VectorXd l = Eigen::Map<const Eigen::VectorXd>(yVec.data(), n);
    const Eigen::MatrixXd A = buildDesignMatrix(x);

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

    m_lastResult = result;
    m_fitted     = true;

    return result;
}

std::string LinearRegressor::name() const
{
    return "LinearRegressor";
}

std::vector<PredictionPoint>
LinearRegressor::predict(const std::vector<double>& xNew) const
{
    if (!m_fitted) {
        throw AlgorithmException(
            "LinearRegressor::predict(): must call fit() before predict().");
    }

    const int k = static_cast<int>(xNew.size());
    Eigen::MatrixXd aNew(k, 2);
    for (int i = 0; i < k; ++i) {
        aNew(i, 0) = 1.0;
        aNew(i, 1) = xNew[static_cast<std::size_t>(i)];
    }

    return detail::computeIntervals(m_lastResult, aNew, m_cfg.confidenceLevel);
}

Eigen::MatrixXd LinearRegressor::buildDesignMatrix(const Eigen::VectorXd& x)
{
    const int m = static_cast<int>(x.size());
    Eigen::MatrixXd A(m, 2);
    A.col(0).setOnes();
    A.col(1) = x;
    return A;
}
