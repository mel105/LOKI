#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace loki::stats {

/**
 * @brief Cumulative distribution function of the standard normal distribution.
 * @param x Quantile value.
 * @return P(X <= x) for X ~ N(0, 1).
 */
double normalCdf(double x);

/**
 * @brief Quantile function (inverse CDF) of the standard normal distribution.
 * @param p Probability in (0, 1).
 * @return x such that P(X <= x) = p.
 * @throws AlgorithmException if p is not in (0, 1).
 */
double normalQuantile(double p);

/**
 * @brief Cumulative distribution function of the Student's t-distribution.
 * @param t Quantile value.
 * @param df Degrees of freedom (must be > 0).
 * @return P(X <= t) for X ~ t(df).
 * @throws AlgorithmException if df <= 0.
 */
double tCdf(double t, double df);

/**
 * @brief Quantile function (inverse CDF) of the Student's t-distribution.
 * @param p Probability in (0, 1).
 * @param df Degrees of freedom (must be > 0).
 * @return x such that P(X <= x) = p for X ~ t(df).
 * @throws AlgorithmException if p is not in (0, 1) or df <= 0.
 */
double tQuantile(double p, double df);

/**
 * @brief Cumulative distribution function of the chi-squared distribution.
 * @param x Quantile value (must be >= 0).
 * @param df Degrees of freedom (must be > 0).
 * @return P(X <= x) for X ~ chi2(df).
 * @throws AlgorithmException if x < 0 or df <= 0.
 */
double chi2Cdf(double x, double df);

/**
 * @brief Quantile function (inverse CDF) of the chi-squared distribution.
 * @param p Probability in (0, 1).
 * @param df Degrees of freedom (must be > 0).
 * @return x such that P(X <= x) = p for X ~ chi2(df).
 * @throws AlgorithmException if p is not in (0, 1) or df <= 0.
 */
double chi2Quantile(double p, double df);

/**
 * @brief Cumulative distribution function of the F-distribution.
 * @param x Quantile value (must be >= 0).
 * @param df1 Numerator degrees of freedom (must be > 0).
 * @param df2 Denominator degrees of freedom (must be > 0).
 * @return P(X <= x) for X ~ F(df1, df2).
 * @throws AlgorithmException if x < 0, df1 <= 0, or df2 <= 0.
 */
double fCdf(double x, double df1, double df2);

/**
 * @brief ADF critical values via MacKinnon (1994) response surface.
 *
 * Returns the critical value tau* such that P(ADF statistic <= tau*) = alpha
 * under the unit-root null hypothesis. Based on the response surface
 * regression coefficients from MacKinnon (1994, Table 1).
 *
 * The response surface model is:
 *   cv(alpha, n) = beta_inf + beta_1 / n + beta_2 / n^2
 *
 * @param alpha     Significance level. Supported values: 0.01, 0.05, 0.10.
 * @param n         Sample size used in the ADF regression (>= 20 recommended).
 * @param trendType Deterministic component specification:
 *                  "none"     -- no constant, no trend
 *                  "constant" -- constant only (most common)
 *                  "trend"    -- constant and linear trend
 * @return Critical value (negative number; reject H0 if ADF stat < cv).
 * @throws ConfigException if alpha or trendType are not among the supported values.
 */
double adfCriticalValue(double alpha,
                        std::size_t n,
                        const std::string& trendType);

/**
 * @brief KPSS critical values from Kwiatkowski et al. (1992), Table 1.
 *
 * Returns the critical value eta* such that P(KPSS statistic > eta*) = alpha
 * under the stationarity null hypothesis. Values are taken directly from the
 * original paper (asymptotic, independent of n).
 *
 * @param alpha     Significance level. Supported values: 0.01, 0.025, 0.05, 0.10.
 * @param trendType Specification of the deterministic component:
 *                  "level" -- test stationarity around a constant (eta_mu)
 *                  "trend" -- test stationarity around a linear trend (eta_tau)
 * @return Critical value (positive number; reject H0 if KPSS stat > cv).
 * @throws ConfigException if alpha or trendType are not among the supported values.
 */
double kpssCriticalValue(double alpha,
                         const std::string& trendType);

} // namespace loki::stats