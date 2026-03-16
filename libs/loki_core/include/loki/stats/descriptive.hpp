#pragma once

#include "loki/core/exceptions.hpp"
#include "loki/core/nanPolicy.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace loki::stats {

// =============================================================================
// Summary structure
// =============================================================================

/**
 * @brief Aggregated descriptive statistics for a single data series.
 *
 * Returned by summarize(). Fields that cannot be computed (e.g. skewness
 * for n < 3, or Hurst when disabled) are set to NaN.
 */
struct SummaryStats {
    std::size_t n;           ///< Number of valid (non-NaN) observations.
    std::size_t nMissing;    ///< Number of NaN values in the original series.
    double      min;         ///< Minimum value.
    double      max;         ///< Maximum value.
    double      range;       ///< max - min.
    double      mean;        ///< Arithmetic mean.
    double      median;      ///< Median (50th percentile).
    double      q1;          ///< First quartile (25th percentile).
    double      q3;          ///< Third quartile (75th percentile).
    double      iqr;         ///< Interquartile range (Q3 - Q1).
    double      variance;    ///< Sample variance (denominator n-1).
    double      stddev;      ///< Sample standard deviation.
    double      mad;         ///< Median absolute deviation.
    double      cv;          ///< Coefficient of variation (stddev / |mean|).
    double      skewness;    ///< Pearson's moment coefficient of skewness.
    double      kurtosis;    ///< Excess kurtosis (Fisher definition, normal = 0).
    double      hurstExp;    ///< Hurst exponent (R/S analysis). NaN if n < 20 or disabled.
};

// =============================================================================
// Central tendency
// =============================================================================

/**
 * @brief Computes the arithmetic mean.
 * @param x      Input data.
 * @param policy NaN handling policy.
 * @return Arithmetic mean.
 * @throws DataException if x is empty or (policy == THROW and NaN present).
 */
double mean(const std::vector<double>& x,
            loki::NanPolicy policy = loki::NanPolicy::THROW);

/**
 * @brief Computes the median (50th percentile).
 * @param x      Input data (need not be sorted).
 * @param policy NaN handling policy.
 * @return Median value.
 * @throws DataException if x is empty or (policy == THROW and NaN present).
 */
double median(const std::vector<double>& x,
              loki::NanPolicy policy = loki::NanPolicy::THROW);

/**
 * @brief Returns the mode(s) -- value(s) with the highest frequency.
 *
 * For continuous floating-point data the mode is rarely meaningful.
 * If all values are distinct, returns an empty vector.
 *
 * @param x      Input data.
 * @param policy NaN handling policy.
 * @return Vector of modal values (may contain more than one element).
 * @throws DataException if x is empty or (policy == THROW and NaN present).
 */
std::vector<double> mode(const std::vector<double>& x,
                         loki::NanPolicy policy = loki::NanPolicy::THROW);

/**
 * @brief Computes a symmetrically trimmed mean.
 *
 * Removes @p fraction of observations from each tail before averaging.
 * For example, fraction = 0.1 discards the lowest 10% and highest 10%.
 *
 * @param x        Input data.
 * @param fraction Proportion to trim from each tail, in [0.0, 0.5).
 * @param policy   NaN handling policy.
 * @return Trimmed mean.
 * @throws DataException if x is empty, fraction is out of range,
 *         or (policy == THROW and NaN present).
 */
double trimmedMean(const std::vector<double>& x,
                   double fraction,
                   loki::NanPolicy policy = loki::NanPolicy::THROW);

// =============================================================================
// Dispersion
// =============================================================================

/**
 * @brief Computes the sample or population variance.
 * @param x          Input data.
 * @param population If true, divides by n (population); otherwise by n-1 (sample).
 * @param policy     NaN handling policy.
 * @return Variance.
 * @throws DataException if x has fewer than 2 elements (sample) or 1 (population).
 * @throws DataException if (policy == THROW and NaN present).
 */
double variance(const std::vector<double>& x,
                bool population = false,
                loki::NanPolicy policy = loki::NanPolicy::THROW);

/**
 * @brief Computes the sample or population standard deviation.
 * @param x          Input data.
 * @param population If true, divides by n; otherwise by n-1.
 * @param policy     NaN handling policy.
 * @return Standard deviation.
 * @throws DataException if fewer than 2 valid elements available.
 * @throws DataException if (policy == THROW and NaN present).
 */
double stddev(const std::vector<double>& x,
              bool population = false,
              loki::NanPolicy policy = loki::NanPolicy::THROW);

/**
 * @brief Computes the Median Absolute Deviation (MAD).
 *
 * MAD = median( |x_i - median(x)| )
 * Robust alternative to standard deviation; unaffected by outliers.
 *
 * @param x      Input data.
 * @param policy NaN handling policy.
 * @return MAD value.
 * @throws DataException if x is empty or (policy == THROW and NaN present).
 */
double mad(const std::vector<double>& x,
           loki::NanPolicy policy = loki::NanPolicy::THROW);

/**
 * @brief Computes the Interquartile Range (Q3 - Q1).
 * @param x      Input data.
 * @param policy NaN handling policy.
 * @return IQR value.
 * @throws DataException if x is empty or (policy == THROW and NaN present).
 */
double iqr(const std::vector<double>& x,
           loki::NanPolicy policy = loki::NanPolicy::THROW);

/**
 * @brief Returns the range (max - min).
 * @param x      Input data.
 * @param policy NaN handling policy.
 * @return Range value.
 * @throws DataException if x is empty or (policy == THROW and NaN present).
 */
double range(const std::vector<double>& x,
             loki::NanPolicy policy = loki::NanPolicy::THROW);

/**
 * @brief Computes the Coefficient of Variation (stddev / |mean|).
 *
 * Expressed as a dimensionless ratio (not percentage).
 * Undefined when mean is zero; throws in that case.
 *
 * @param x      Input data.
 * @param policy NaN handling policy.
 * @return CV value.
 * @throws DataException if mean is zero or x is empty.
 * @throws DataException if (policy == THROW and NaN present).
 */
double cv(const std::vector<double>& x,
          loki::NanPolicy policy = loki::NanPolicy::THROW);

// =============================================================================
// Distribution shape
// =============================================================================

/**
 * @brief Computes Pearson's moment coefficient of skewness.
 *
 * Positive skewness: right tail is longer.
 * Negative skewness: left tail is longer.
 * Uses the bias-corrected formula (n / ((n-1)(n-2))).
 *
 * @param x      Input data.
 * @param policy NaN handling policy.
 * @return Skewness coefficient.
 * @throws DataException if fewer than 3 valid elements.
 * @throws DataException if (policy == THROW and NaN present).
 */
double skewness(const std::vector<double>& x,
                loki::NanPolicy policy = loki::NanPolicy::THROW);

/**
 * @brief Computes the excess kurtosis (Fisher definition).
 *
 * Normal distribution has excess kurtosis = 0.
 * Positive: heavier tails than normal (leptokurtic).
 * Negative: lighter tails (platykurtic).
 * Uses the bias-corrected formula.
 *
 * @param x      Input data.
 * @param policy NaN handling policy.
 * @return Excess kurtosis.
 * @throws DataException if fewer than 4 valid elements.
 * @throws DataException if (policy == THROW and NaN present).
 */
double kurtosis(const std::vector<double>& x,
                loki::NanPolicy policy = loki::NanPolicy::THROW);

// =============================================================================
// Quantiles / order statistics
// =============================================================================

/**
 * @brief Computes an arbitrary quantile using linear interpolation (type 7).
 *
 * This is the default method used by R and NumPy.
 *
 * @param x      Input data (need not be sorted).
 * @param p      Probability in [0.0, 1.0].
 * @param policy NaN handling policy.
 * @return Quantile value at probability p.
 * @throws DataException if p is outside [0, 1] or x is empty.
 * @throws DataException if (policy == THROW and NaN present).
 */
double quantile(const std::vector<double>& x,
                double p,
                loki::NanPolicy policy = loki::NanPolicy::THROW);

/**
 * @brief Returns the five-number summary: {min, Q1, median, Q3, max}.
 * @param x      Input data.
 * @param policy NaN handling policy.
 * @return Array of five values in ascending order.
 * @throws DataException if x is empty or (policy == THROW and NaN present).
 */
std::array<double, 5> fiveNumberSummary(const std::vector<double>& x,
                                        loki::NanPolicy policy = loki::NanPolicy::THROW);

// =============================================================================
// Bivariate statistics
// =============================================================================

/**
 * @brief Computes the sample covariance between two series.
 *
 * NaN pairs are handled according to policy: both elements of a pair
 * are dropped if either is NaN (pairwise deletion).
 *
 * @param x      First series.
 * @param y      Second series (must have the same length as x).
 * @param policy NaN handling policy.
 * @return Sample covariance.
 * @throws DataException if sizes differ or fewer than 2 valid pairs.
 * @throws DataException if (policy == THROW and NaN present).
 */
double covariance(const std::vector<double>& x,
                  const std::vector<double>& y,
                  loki::NanPolicy policy = loki::NanPolicy::THROW);

/**
 * @brief Computes Pearson's product-moment correlation coefficient.
 * @param x      First series.
 * @param y      Second series.
 * @param policy NaN handling policy.
 * @return Pearson r in [-1, 1].
 * @throws DataException if sizes differ or standard deviation is zero.
 * @throws DataException if (policy == THROW and NaN present).
 */
double pearsonR(const std::vector<double>& x,
                const std::vector<double>& y,
                loki::NanPolicy policy = loki::NanPolicy::THROW);

/**
 * @brief Computes Spearman's rank correlation coefficient.
 *
 * Non-parametric alternative to Pearson r; robust to outliers and
 * monotone non-linear relationships.
 *
 * @param x      First series.
 * @param y      Second series.
 * @param policy NaN handling policy.
 * @return Spearman rho in [-1, 1].
 * @throws DataException if sizes differ or fewer than 2 valid pairs.
 * @throws DataException if (policy == THROW and NaN present).
 */
double spearmanR(const std::vector<double>& x,
                 const std::vector<double>& y,
                 loki::NanPolicy policy = loki::NanPolicy::THROW);

// =============================================================================
// Time series specific
// =============================================================================

/**
 * @brief Computes the autocorrelation coefficient at a given lag.
 *
 * Uses the biased estimator (divides by n, not n-lag), consistent with
 * most statistical software and the Box-Jenkins convention.
 *
 * @param x      Input time series.
 * @param lag    Non-negative lag value. Lag 0 always returns 1.0.
 * @param policy NaN handling policy.
 * @return Autocorrelation in [-1, 1].
 * @throws DataException if x is empty, lag < 0, or lag >= x.size().
 * @throws DataException if (policy == THROW and NaN present).
 */
double autocorrelation(const std::vector<double>& x,
                       int lag,
                       loki::NanPolicy policy = loki::NanPolicy::THROW);

/**
 * @brief Computes the full autocorrelation function (ACF) up to maxLag.
 *
 * Returns a vector of length (maxLag + 1) where element k is the
 * autocorrelation at lag k. Element 0 is always 1.0.
 *
 * @param x       Input time series.
 * @param maxLag  Maximum lag to compute (inclusive). Must be < x.size().
 * @param policy  NaN handling policy.
 * @return Vector of autocorrelation values, indices 0 .. maxLag.
 * @throws DataException if x is empty or maxLag >= x.size().
 * @throws DataException if (policy == THROW and NaN present).
 */
std::vector<double> acf(const std::vector<double>& x,
                        int maxLag,
                        loki::NanPolicy policy = loki::NanPolicy::THROW);

/**
 * @brief Estimates the Hurst exponent via rescaled range (R/S) analysis.
 *
 * The Hurst exponent H characterises the long-range memory of a series:
 *   H ~  0.5  => random walk / no memory (white noise)
 *   H >  0.5  => persistent trend (typical for climate, GNSS bias)
 *   H <  0.5  => mean-reverting / anti-persistent
 *
 * The R/S method divides the series into subseries of varying lengths,
 * computes the rescaled range for each, then fits log(R/S) ~ H * log(n)
 * by ordinary least squares.
 *
 * @param x      Input time series.
 * @param policy NaN handling policy.
 * @return Estimated Hurst exponent, typically in (0, 1).
 * @throws DataException if fewer than 20 valid observations.
 * @throws DataException if (policy == THROW and NaN present).
 */
double hurstExponent(const std::vector<double>& x,
                     loki::NanPolicy policy = loki::NanPolicy::THROW);

// =============================================================================
// Summary
// =============================================================================

/**
 * @brief Computes all descriptive statistics in a single call.
 *
 * @param x            Input data series.
 * @param policy       NaN handling policy. Defaults to SKIP: NaN values are
 *                     counted in nMissing and removed before computation.
 * @param computeHurst If true, includes the Hurst exponent (R/S analysis).
 *                     Set to false to skip this computationally expensive step.
 *                     Field hurstExp is NaN when disabled or when n < 20.
 * @return Populated SummaryStats struct.
 * @throws DataException if x is empty or all values are NaN.
 */
SummaryStats summarize(const std::vector<double>& x,
                       loki::NanPolicy policy = loki::NanPolicy::SKIP,
                       bool computeHurst = true);

/**
 * @brief Formats a SummaryStats struct into a human-readable multi-line string.
 *
 * Intended for logging and console output, not for serialisation.
 *
 * @param s     SummaryStats to format.
 * @param label Optional label printed in the header line.
 * @return Formatted string.
 */
std::string formatSummary(const SummaryStats& s,
                          const std::string& label = "");

} // namespace loki::stats