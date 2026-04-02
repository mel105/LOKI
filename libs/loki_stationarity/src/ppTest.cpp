#include <loki/stationarity/ppTest.hpp>
#include <loki/stationarity/stationarityUtils.hpp>
#include <loki/stats/distributions.hpp>
#include <loki/core/exceptions.hpp>

#include <Eigen/Dense>

#include <cmath>
#include <limits>
#include <vector>

using namespace loki;

namespace loki::stationarity {

PpTest::PpTest(Config cfg)
    : m_cfg(std::move(cfg))
{}

TestResult PpTest::test(const std::vector<double>& y) const
{
    if (y.size() < 10) {
        throw DataException(
            "PpTest: series must have at least 10 observations, got " +
            std::to_string(y.size()) + ".");
    }

    const std::size_t n  = y.size();
    const double      dn = static_cast<double>(n);

    const bool hasConst = (m_cfg.trendType == "constant" || m_cfg.trendType == "trend");
    const bool hasTrend = (m_cfg.trendType == "trend");

    const int    T  = static_cast<int>(n) - 1;
    const double dT = static_cast<double>(T);

    int levelCol = 0;
    if (hasConst) ++levelCol;
    if (hasTrend) ++levelCol;
    const int k = levelCol + 1;

    Eigen::MatrixXd X(T, k);
    Eigen::VectorXd Y_dep(T);

    for (int t = 0; t < T; ++t) {
        Y_dep(t) = y[static_cast<std::size_t>(t + 1)] -
                   y[static_cast<std::size_t>(t)];
        int col = 0;
        if (hasConst) { X(t, col) = 1.0;                        ++col; }
        if (hasTrend) { X(t, col) = static_cast<double>(t + 1); ++col; }
        X(t, col) = y[static_cast<std::size_t>(t)];
    }

    const Eigen::MatrixXd XtX = X.transpose() * X;
    const Eigen::LDLT<Eigen::MatrixXd> ldlt(XtX);
    if (ldlt.info() != Eigen::Success) {
        throw AlgorithmException("PpTest: OLS system is numerically singular.");
    }

    const Eigen::VectorXd beta   = ldlt.solve(X.transpose() * Y_dep);
    const Eigen::VectorXd resVec = Y_dep - X * beta;

    // Biased residual variance (PP convention: divide by T not T-k).
    const double sigma2 = resVec.squaredNorm() / dT;
    const double s      = std::sqrt(sigma2);

    // Newey-West long-run variance.
    std::vector<double> residuals(static_cast<std::size_t>(T));
    for (int t = 0; t < T; ++t)
        residuals[static_cast<std::size_t>(t)] = resVec(t);

    const double lambda2 = detail::neweyWestVariance(residuals, m_cfg.lags);
    if (lambda2 < std::numeric_limits<double>::epsilon())
        throw AlgorithmException("PpTest: Newey-West long-run variance is zero.");
    const double lambda = std::sqrt(lambda2);

    // OLS t-statistic on gamma using X'X and its inverse.
    const Eigen::MatrixXd XtXinv     = ldlt.solve(Eigen::MatrixXd::Identity(k, k));
    const double          XtXinvGamma = XtXinv(levelCol, levelCol);
    const double          XtXGamma    = XtX(levelCol, levelCol);

    if (XtXinvGamma <= 0.0 || XtXGamma <= 0.0)
        throw AlgorithmException("PpTest: degenerate gamma variance.");

    const double seGamma = s * std::sqrt(XtXinvGamma);
    if (seGamma < std::numeric_limits<double>::epsilon())
        throw AlgorithmException("PpTest: standard error of gamma is zero.");

    const double tAlpha = beta(levelCol) / seGamma;

    // PP Z(t) statistic (Phillips & Perron 1988, eq. 11;
    // Davidson & MacKinnon 2004, p. 612):
    //
    //   Z(t) = t_alpha * (s / lambda)
    //          - 0.5 * (lambda^2 - s^2) / lambda
    //            * sqrt(T) / (s * sqrt(XtX_{gamma,gamma}))
    //
    // The second term uses XtX (normal equations), not XtX inverse.
    const double correction = 0.5 * (lambda2 - sigma2) / lambda
                              * std::sqrt(dT) / (s * std::sqrt(XtXGamma));
    const double Zt = tAlpha * (s / lambda) - correction;

    const double cv01 = loki::stats::adfCriticalValue(0.01, n, m_cfg.trendType);
    const double cv05 = loki::stats::adfCriticalValue(0.05, n, m_cfg.trendType);
    const double cv10 = loki::stats::adfCriticalValue(0.10, n, m_cfg.trendType);

    const double alpha = m_cfg.significanceLevel;
    double cvAlpha = cv05;
    if      (alpha <= 0.015) cvAlpha = cv01;
    else if (alpha <= 0.075) cvAlpha = cv05;
    else                     cvAlpha = cv10;

    const int lagsUsed = (m_cfg.lags < 0)
        ? std::max(1, static_cast<int>(std::floor(
              4.0 * std::pow(dn / 100.0, 2.0 / 9.0))))
        : m_cfg.lags;

    TestResult result;
    result.statistic    = Zt;
    result.pValue       = std::numeric_limits<double>::quiet_NaN();
    result.critVal1pct  = cv01;
    result.critVal5pct  = cv05;
    result.critVal10pct = cv10;
    result.rejected     = (Zt < cvAlpha);
    result.testName     = "pp";
    result.trendType    = m_cfg.trendType;
    result.lags         = lagsUsed;

    return result;
}

} // namespace loki::stationarity