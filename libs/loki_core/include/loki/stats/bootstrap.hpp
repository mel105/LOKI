#pragma once

#include <loki/stats/sampling.hpp>

#include <cstddef>
#include <functional>
#include <vector>

namespace loki::stats {

// ---------------------------------------------------------------------------
//  StatFn
// ---------------------------------------------------------------------------

/**
 * @brief Type alias for a scalar statistic computed from a data vector.
 *
 * The function receives a resampled (or original) data vector and returns
 * a single double. Common examples:
 *   - sample mean:    [](const auto& v){ return mean(v); }
 *   - sample median:  [](const auto& v){ return median(v); }
 *   - Hurst exponent: [](const auto& v){ return hurstExponent(v); }
 *   - SNHT T-statistic at a fixed candidate point
 *
 * The function must be pure with respect to the input vector (no hidden state
 * that changes between calls). It may throw -- exceptions propagate to the caller.
 */
using StatFn = std::function<double(const std::vector<double>&)>;

// ---------------------------------------------------------------------------
//  BootstrapConfig (defined outside functions -- GCC 13 aggregate-init)
// ---------------------------------------------------------------------------

/**
 * @brief Configuration for bootstrap confidence interval estimation.
 */
struct BootstrapConfig {
    int         nResamples;   ///< Number of bootstrap resamples. Default 1000.
    double      alpha;        ///< Significance level. Default 0.05 -> 95% CI.
    std::size_t blockLength;  ///< Block length for blockCI. 0 = automatic from ACF.

    BootstrapConfig()
        : nResamples(1000)
        , alpha(0.05)
        , blockLength(0)
    {}

    BootstrapConfig(int nr, double a, std::size_t bl)
        : nResamples(nr)
        , alpha(a)
        , blockLength(bl)
    {}
};

// ---------------------------------------------------------------------------
//  BootstrapResult
// ---------------------------------------------------------------------------

/**
 * @brief Result of a bootstrap confidence interval estimation.
 *
 * All fields are always populated. For BCa, bias and se reflect the
 * bootstrap distribution of the statistic. For percentile and block
 * bootstrap, bias and se are computed identically from the resample distribution.
 *
 * Interpretation:
 *   - [lower, upper] is the (1 - alpha) confidence interval.
 *   - bias > 0 means the statistic tends to overestimate the true value.
 *   - se is the standard deviation of the bootstrap distribution.
 */
struct BootstrapResult {
    double estimate;    ///< Statistic computed on the original data.
    double lower;       ///< Lower bound of the CI.
    double upper;       ///< Upper bound of the CI.
    double bias;        ///< Bootstrap bias: mean(boot_estimates) - estimate.
    double se;          ///< Bootstrap standard error: std(boot_estimates).
    int    nResamples;  ///< Number of resamples actually used.
};

// ---------------------------------------------------------------------------
//  Bootstrap functions
// ---------------------------------------------------------------------------

/**
 * @brief Percentile bootstrap confidence interval (iid resampling).
 *
 * Algorithm:
 *   1. Compute estimate = statistic(data).
 *   2. Draw nResamples bootstrap samples (with replacement, iid).
 *   3. Compute the statistic on each resample -> boot_estimates[].
 *   4. CI = [quantile(alpha/2), quantile(1 - alpha/2)] of boot_estimates.
 *
 * Suitable for: large samples, statistics without strong bias, iid data.
 * NOT suitable for: autocorrelated time series -- use blockCI() instead.
 *
 * @param data      Original data vector (NaN-free).
 * @param statistic Scalar statistic to estimate.
 * @param sampler   Random sampler (caller owns; seed controls reproducibility).
 * @param cfg       Configuration (nResamples, alpha; blockLength ignored).
 * @return          BootstrapResult with CI and diagnostics.
 * @throws SeriesTooShortException if data.size() < 2.
 * @throws ConfigException         if cfg.nResamples < 10 or alpha not in (0, 0.5).
 */
BootstrapResult percentileCI(
    const std::vector<double>& data,
    const StatFn&              statistic,
    Sampler&                   sampler,
    const BootstrapConfig&     cfg = BootstrapConfig{});

/**
 * @brief Bias-corrected and accelerated (BCa) bootstrap confidence interval.
 *
 * Improves on the percentile bootstrap by correcting for:
 *   - bias: systematic over/underestimation of the statistic.
 *   - acceleration: non-constant variance of the statistic across the parameter space.
 *
 * Algorithm (Efron & Tibshirani, 1993):
 *   1. Compute estimate = statistic(data) and boot_estimates[].
 *   2. Bias correction z0: proportion of boot_estimates < estimate -> normalQuantile(prop).
 *   3. Acceleration a: jackknife estimate of the rate of change of SE with the parameter.
 *   4. Adjusted quantiles: alpha1, alpha2 based on z0 and a.
 *   5. CI = [quantile(alpha1), quantile(alpha2)] of boot_estimates.
 *
 * Suitable for: small to medium samples, statistics with bias or skewed distributions.
 * NOT suitable for: autocorrelated time series -- use blockCI() instead.
 *
 * @param data      Original data vector (NaN-free).
 * @param statistic Scalar statistic to estimate.
 * @param sampler   Random sampler.
 * @param cfg       Configuration (nResamples, alpha; blockLength ignored).
 * @return          BootstrapResult with BCa-corrected CI.
 * @throws SeriesTooShortException if data.size() < 2.
 * @throws ConfigException         if cfg.nResamples < 10 or alpha not in (0, 0.5).
 * @throws AlgorithmException      if BCa acceleration is undefined (degenerate jackknife).
 */
BootstrapResult bcaCI(
    const std::vector<double>& data,
    const StatFn&              statistic,
    Sampler&                   sampler,
    const BootstrapConfig&     cfg = BootstrapConfig{});

/**
 * @brief Block bootstrap confidence interval for autocorrelated time series.
 *
 * Resamples whole contiguous blocks to preserve the short-range dependence
 * structure of the series. Uses the moving block bootstrap (MBB): blocks
 * of length blockLength are drawn with replacement from all possible starting
 * positions in [0, n - blockLength].
 *
 * Block length selection (cfg.blockLength):
 *   - 0 (default): automatic estimation from the ACF. The block length is set
 *     to the first lag at which |ACF(lag)| < 1.96 / sqrt(n), clamped to [1, n/4].
 *     A LOKI_WARNING is logged with the selected length.
 *   - > 0: use the specified value directly.
 *
 * Suitable for: climatological data, GNSS time series, sensor data with
 * significant autocorrelation (ACF lag-1 > ~0.2).
 * NOT suitable for: iid data (unnecessary overhead; use percentileCI instead).
 *
 * @param data      Original data vector (NaN-free, ordered in time).
 * @param statistic Scalar statistic to estimate.
 * @param sampler   Random sampler.
 * @param cfg       Configuration. cfg.blockLength = 0 triggers auto-selection.
 * @return          BootstrapResult with CI and diagnostics.
 * @throws SeriesTooShortException if data.size() < 4.
 * @throws ConfigException         if cfg.nResamples < 10, alpha not in (0, 0.5),
 *                                 or blockLength > data.size().
 */
BootstrapResult blockCI(
    const std::vector<double>& data,
    const StatFn&              statistic,
    Sampler&                   sampler,
    const BootstrapConfig&     cfg = BootstrapConfig{});

} // namespace loki::stats
