#pragma once

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

} // namespace loki::stats
