#include <loki/stationarity/adfTest.hpp>
#include <loki/stats/distributions.hpp>
#include <loki/core/exceptions.hpp>

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

using namespace loki;

namespace loki::stationarity {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

AdfTest::AdfTest(Config cfg)
    : m_cfg(std::move(cfg))
{}

// ---------------------------------------------------------------------------
// autoMaxLag -- Schwert (1989) rule of thumb
// ---------------------------------------------------------------------------

int AdfTest::autoMaxLag(std::size_t n)
{
    return static_cast<int>(
        std::floor(12.0 * std::pow(static_cast<double>(n) / 100.0, 0.25)));
}

// ---------------------------------------------------------------------------
// fitRegression
//
// Fits the ADF OLS regression for a given lag count and returns the
// t-statistic on the lagged level (tau), AIC, and BIC.
//
// Design matrix columns depend on trendType:
//   "none"     : [y_{t-1}, dy_{t-1}, ..., dy_{t-lag}]
//   "constant" : [1, y_{t-1}, dy_{t-1}, ..., dy_{t-lag}]
//   "trend"    : [1, t, y_{t-1}, dy_{t-1}, ..., dy_{t-lag}]
//
// dy  = first-differenced series (length n-1)
// y   = original series (length n)
// After removing the first lag observations, the regression uses T = n-1-lag rows.
// ---------------------------------------------------------------------------

AdfTest::FitResult AdfTest::fitRegression(const std::vector<double>& dy,
                                           const std::vector<double>& y,
                                           int lag) const
{
    // Number of usable observations after losing lag+1 initial values.
    // dy[lag], dy[lag+1], ..., dy[n-2] are the dependent variable.
    // Corresponding lagged level: y[lag], y[lag+1], ..., y[n-2].
    const int T    = static_cast<int>(dy.size()) - lag; // n-1-lag

    if (T < 5) {
        throw DataException(
            "AdfTest: too few observations after lag removal (T=" +
            std::to_string(T) + "). Reduce lag or provide more data.");
    }

    // Determine number of regressors.
    // col 0: constant (if constant or trend)
    // col 1: trend index (if trend)
    // col k: y_{t-1} (the level lag -- gamma coefficient)
    // remaining cols: lagged differences dy_{t-1} .. dy_{t-lag}
    const bool hasConst = (m_cfg.trendType == "constant" || m_cfg.trendType == "trend");
    const bool hasTrend = (m_cfg.trendType == "trend");

    int levelCol = 0;
    if (hasConst) ++levelCol;
    if (hasTrend) ++levelCol;
    // levelCol is now the column index of y_{t-1}.

    const int k = levelCol + 1 + lag; // total regressors

    Eigen::MatrixXd X(T, k);
    Eigen::VectorXd Y_dep(T);

    for (int t = 0; t < T; ++t) {
        // Observation index in dy: t + lag
        const int dyIdx = t + lag;
        // Corresponding level index in y: same as dyIdx (dy[i] = y[i+1]-y[i], lagged level is y[dyIdx])
        const int yIdx  = dyIdx;

        Y_dep(t) = dy[static_cast<std::size_t>(dyIdx)];

        int col = 0;
        if (hasConst) { X(t, col) = 1.0; ++col; }
        if (hasTrend) { X(t, col) = static_cast<double>(t + 1); ++col; }

        // Lagged level y_{t-1}
        X(t, col) = y[static_cast<std::size_t>(yIdx)];
        ++col;

        // Lagged differences dy_{t-1} .. dy_{t-lag}
        for (int j = 1; j <= lag; ++j) {
            X(t, col) = dy[static_cast<std::size_t>(dyIdx - j)];
            ++col;
        }
    }

    // OLS via normal equations with LDLT.
    const Eigen::MatrixXd XtX = X.transpose() * X;
    const Eigen::VectorXd XtY = X.transpose() * Y_dep;

    const Eigen::LDLT<Eigen::MatrixXd> ldlt(XtX);
    if (ldlt.info() != Eigen::Success) {
        throw AlgorithmException(
            "AdfTest: OLS system is numerically singular at lag=" +
            std::to_string(lag) + ".");
    }

    const Eigen::VectorXd beta = ldlt.solve(XtY);
    const Eigen::VectorXd resid = Y_dep - X * beta;
    const double sse = resid.squaredNorm();
    const double sigma2 = sse / static_cast<double>(T - k);

    if (sigma2 <= 0.0) {
        throw AlgorithmException(
            "AdfTest: zero residual variance at lag=" + std::to_string(lag) + ".");
    }

    // Standard error of the gamma coefficient (column levelCol).
    const Eigen::MatrixXd XtXinv = ldlt.solve(Eigen::MatrixXd::Identity(k, k));
    const double seGamma = std::sqrt(sigma2 * XtXinv(levelCol, levelCol));

    const double tau = (seGamma > 0.0)
        ? beta(levelCol) / seGamma
        : std::numeric_limits<double>::quiet_NaN();

    // Information criteria: AIC = T*ln(sse/T) + 2*k, BIC = T*ln(sse/T) + k*ln(T)
    const double logLik = static_cast<double>(T) * std::log(sse / static_cast<double>(T));
    const double aic = logLik + 2.0 * static_cast<double>(k);
    const double bic = logLik + static_cast<double>(k) * std::log(static_cast<double>(T));

    return {tau, aic, bic};
}

// ---------------------------------------------------------------------------
// test
// ---------------------------------------------------------------------------

TestResult AdfTest::test(const std::vector<double>& y) const
{
    if (y.size() < 10) {
        throw DataException(
            "AdfTest: series must have at least 10 observations, got " +
            std::to_string(y.size()) + ".");
    }

    // First-difference the series.
    const std::size_t n = y.size();
    std::vector<double> dy;
    dy.reserve(n - 1);
    for (std::size_t i = 1; i < n; ++i) {
        dy.push_back(y[i] - y[i - 1]);
    }

    // Determine lag range.
    const int maxLag = (m_cfg.maxLags < 0)
        ? autoMaxLag(n)
        : m_cfg.maxLags;

    int bestLag = 0;
    double bestCrit = std::numeric_limits<double>::max();

    if (m_cfg.lagSelection == "fixed") {
        bestLag = std::max(0, m_cfg.maxLags);
    } else {
        // Search lags 0..maxLag and select by AIC or BIC.
        for (int lag = 0; lag <= maxLag; ++lag) {
            // Guard: need at least T=5 observations after lag removal.
            if (static_cast<int>(dy.size()) - lag < 5) break;
            try {
                const FitResult fr = fitRegression(dy, y, lag);
                const double crit = (m_cfg.lagSelection == "bic") ? fr.bic : fr.aic;
                if (crit < bestCrit) {
                    bestCrit = crit;
                    bestLag  = lag;
                }
            } catch (const AlgorithmException&) {
                // Skip singular configurations.
                break;
            }
        }
    }

    // Final fit with selected lag.
    const FitResult best = fitRegression(dy, y, bestLag);

    // Critical values from MacKinnon (1994) response surface.
    const double cv01  = loki::stats::adfCriticalValue(0.01, n, m_cfg.trendType);
    const double cv05  = loki::stats::adfCriticalValue(0.05, n, m_cfg.trendType);
    const double cv10  = loki::stats::adfCriticalValue(0.10, n, m_cfg.trendType);

    // Reject H0 (unit root) if tau < critical value at the configured significance level.
    TestResult result;
    result.statistic    = best.tau;
    result.pValue       = std::numeric_limits<double>::quiet_NaN(); // not available from response surface
    result.critVal1pct  = cv01;
    result.critVal5pct  = cv05;
    result.critVal10pct = cv10;
    result.rejected     = best.tau < loki::stats::adfCriticalValue(
                              m_cfg.significanceLevel <= 0.015 ? 0.01
                            : m_cfg.significanceLevel <= 0.075 ? 0.05
                            : 0.10, n, m_cfg.trendType);
    result.testName     = "adf";
    result.trendType    = m_cfg.trendType;
    result.lags         = bestLag;

    return result;
}

} // namespace loki::stationarity