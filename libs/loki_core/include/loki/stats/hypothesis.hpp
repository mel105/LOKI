#pragma once

#include <loki/core/nanPolicy.hpp>

#include <string>
#include <vector>

namespace loki::stats {

/**
 * @brief Result of a statistical hypothesis test.
 */
struct HypothesisResult {
    double      statistic{0.0};   ///< Test statistic value.
    double      pValue{1.0};      ///< p-value of the test.
    bool        rejected{false};  ///< True if null hypothesis rejected at given alpha.
    std::string testName;         ///< Name of the test (for logging and reports).
};

/**
 * @brief Jarque-Bera test for normality.
 *
 * Tests H0: data follow a normal distribution.
 * Based on sample skewness and excess kurtosis.
 * Asymptotically valid for large samples (n >= 30 recommended).
 *
 * @param data  Input data. NaN handling controlled by policy.
 * @param alpha Significance level (default 0.05).
 * @param policy NaN handling policy (default SKIP).
 * @return HypothesisResult with testName "jarque-bera".
 * @throws DataException if fewer than 8 valid observations remain.
 */
HypothesisResult jarqueBera(
    const std::vector<double>& data,
    double                     alpha  = 0.05,
    NanPolicy                  policy = NanPolicy::SKIP);

/**
 * @brief Shapiro-Wilk test for normality.
 *
 * Tests H0: data follow a normal distribution.
 * Valid for n in [3, 5000]. For n > 5000, logs a warning and falls back
 * to Jarque-Bera, returning result with testName "shapiro-wilk (fallback: jarque-bera)".
 *
 * @param data  Input data. NaN handling controlled by policy.
 * @param alpha Significance level (default 0.05).
 * @param policy NaN handling policy (default SKIP).
 * @return HypothesisResult with testName "shapiro-wilk" or fallback name.
 * @throws DataException if fewer than 3 valid observations remain.
 */
HypothesisResult shapiroWilk(
    const std::vector<double>& data,
    double                     alpha  = 0.05,
    NanPolicy                  policy = NanPolicy::SKIP);

/**
 * @brief One-sample Kolmogorov-Smirnov test against N(mu, sigma^2).
 *
 * Tests H0: data follow a normal distribution with parameters estimated from data.
 * p-value approximated via the Kolmogorov distribution.
 *
 * @param data  Input data. NaN handling controlled by policy.
 * @param alpha Significance level (default 0.05).
 * @param policy NaN handling policy (default SKIP).
 * @return HypothesisResult with testName "kolmogorov-smirnov".
 * @throws DataException if fewer than 5 valid observations remain.
 */
HypothesisResult kolmogorovSmirnov(
    const std::vector<double>& data,
    double                     alpha  = 0.05,
    NanPolicy                  policy = NanPolicy::SKIP);

/**
 * @brief Runs test for randomness (independence of residuals).
 *
 * Tests H0: sequence is random (no serial dependence).
 * Uses the number of runs above/below the median and the normal approximation.
 *
 * @param data  Input data. NaN handling controlled by policy.
 * @param alpha Significance level (default 0.05).
 * @param policy NaN handling policy (default SKIP).
 * @return HypothesisResult with testName "runs-test".
 * @throws DataException if fewer than 10 valid observations remain.
 */
HypothesisResult runsTest(
    const std::vector<double>& data,
    double                     alpha  = 0.05,
    NanPolicy                  policy = NanPolicy::SKIP);

/**
 * @brief Durbin-Watson statistic for first-order autocorrelation in residuals.
 *
 * Returns the DW statistic in [0, 4]:
 *   ~2.0  -> no autocorrelation
 *   < 1.5 -> positive autocorrelation
 *   > 2.5 -> negative autocorrelation
 *
 * Interpretation of rejection is context-dependent (depends on n and number
 * of regressors), so only the raw statistic is returned.
 *
 * @param residuals Regression residuals. NaN handling controlled by policy.
 * @param policy NaN handling policy (default SKIP).
 * @return Durbin-Watson statistic.
 * @throws DataException if fewer than 2 valid observations remain.
 */
double durbinWatson(
    const std::vector<double>& residuals,
    NanPolicy                  policy = NanPolicy::SKIP);

/**
 * @brief Ljung-Box test for autocorrelation up to a given lag.
 *
 * Tests H0: no autocorrelation up to lag maxLag (residuals are white noise).
 *
 * Test statistic:
 *   Q = n*(n+2) * sum_{k=1}^{maxLag} rho_k^2 / (n-k)
 *
 * Under H0, Q is asymptotically chi-squared with maxLag degrees of freedom.
 * Useful as a residual diagnostic after fitting AR/ARIMA models.
 *
 * Note: for very short series or large maxLag the chi-squared approximation
 * deteriorates. A commonly recommended choice is maxLag = min(10, n/5).
 *
 * @param data   Input data (typically model residuals). NaN handling by policy.
 * @param maxLag Maximum lag to include in the test statistic (>= 1).
 * @param alpha  Significance level (default 0.05).
 * @param policy NaN handling policy (default SKIP).
 * @return HypothesisResult with testName "ljung-box" and statistic = Q.
 *         The lags field is not used here; use maxLag directly from the call site.
 * @throws DataException if fewer than maxLag+2 valid observations remain, or maxLag < 1.
 */
HypothesisResult ljungBox(
    const std::vector<double>& data,
    int                        maxLag,
    double                     alpha  = 0.05,
    NanPolicy                  policy = NanPolicy::SKIP);

} // namespace loki::stats