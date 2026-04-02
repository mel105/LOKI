#include <loki/stationarity/kpssTest.hpp>
#include <loki/stationarity/stationarityUtils.hpp>
#include <loki/stats/distributions.hpp>
#include <loki/core/exceptions.hpp>

#include <Eigen/Dense>

#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

using namespace loki;

namespace loki::stationarity {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

KpssTest::KpssTest(Config cfg)
    : m_cfg(std::move(cfg))
{}

// ---------------------------------------------------------------------------
// test
// ---------------------------------------------------------------------------

TestResult KpssTest::test(const std::vector<double>& y) const
{
    if (y.size() < 5) {
        throw DataException(
            "KpssTest: series must have at least 5 observations, got " +
            std::to_string(y.size()) + ".");
    }

    const std::size_t n = y.size();
    const double      dn = static_cast<double>(n);

    // Step 1: regress y on deterministic component, obtain OLS residuals.
    // "level" -> constant only:  y_t = mu + e_t
    // "trend" -> constant+trend: y_t = mu + beta*t + e_t
    const bool hasTrend = (m_cfg.trendType == "trend");
    const int  k = hasTrend ? 2 : 1;

    Eigen::MatrixXd X(static_cast<Eigen::Index>(n), k);
    Eigen::VectorXd Y(static_cast<Eigen::Index>(n));

    for (std::size_t t = 0; t < n; ++t) {
        Y(static_cast<Eigen::Index>(t)) = y[t];
        X(static_cast<Eigen::Index>(t), 0) = 1.0;
        if (hasTrend) {
            X(static_cast<Eigen::Index>(t), 1) = static_cast<double>(t + 1);
        }
    }

    const Eigen::LDLT<Eigen::MatrixXd> ldlt(X.transpose() * X);
    if (ldlt.info() != Eigen::Success) {
        throw AlgorithmException("KpssTest: degenerate design matrix.");
    }

    const Eigen::VectorXd beta   = ldlt.solve(X.transpose() * Y);
    const Eigen::VectorXd resVec = Y - X * beta;

    std::vector<double> residuals(n);
    for (std::size_t t = 0; t < n; ++t) {
        residuals[t] = resVec(static_cast<Eigen::Index>(t));
    }

    // Step 2: Newey-West long-run variance s^2.
    const double s2 = detail::neweyWestVariance(residuals, m_cfg.lags);

    if (s2 < std::numeric_limits<double>::epsilon()) {
        throw AlgorithmException(
            "KpssTest: long-run variance is zero -- series may be constant.");
    }

    // Step 3: KPSS statistic eta = (1/n^2) * sum S_t^2 / s^2
    // where S_t = sum_{i=0}^{t} e_i (partial sums of residuals).
    double sumS2 = 0.0;
    double S     = 0.0;
    for (std::size_t t = 0; t < n; ++t) {
        S     += residuals[t];
        sumS2 += S * S;
    }
    const double eta = sumS2 / (dn * dn * s2);

    // Step 4: Critical values from Kwiatkowski et al. (1992), Table 1.
    const std::string& tt = m_cfg.trendType;
    const double cv01  = loki::stats::kpssCriticalValue(0.01,  tt);
    const double cv05  = loki::stats::kpssCriticalValue(0.05,  tt);
    const double cv10  = loki::stats::kpssCriticalValue(0.10,  tt);

    // Determine significance level bucket for the rejected flag.
    const double alpha = m_cfg.significanceLevel;
    double cvAlpha = cv05;
    if      (alpha <= 0.015) cvAlpha = cv01;
    else if (alpha <= 0.075) cvAlpha = cv05;
    else                     cvAlpha = cv10;

    // Reject H0 (stationarity) if eta > critical value (right-tailed).
    const bool rejected = (eta > cvAlpha);

    // Determine actual lags used (for reporting).
    const int lagsUsed = (m_cfg.lags < 0)
        ? static_cast<int>(std::floor(
              4.0 * std::pow(dn / 100.0, 2.0 / 9.0)))
        : m_cfg.lags;

    TestResult result;
    result.statistic    = eta;
    result.pValue       = std::numeric_limits<double>::quiet_NaN();
    result.critVal1pct  = cv01;
    result.critVal5pct  = cv05;
    result.critVal10pct = cv10;
    result.rejected     = rejected;
    result.testName     = "kpss";
    result.trendType    = m_cfg.trendType;
    result.lags         = std::max(1, lagsUsed);

    return result;
}

} // namespace loki::stationarity
