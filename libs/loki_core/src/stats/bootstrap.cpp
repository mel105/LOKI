#include <loki/stats/bootstrap.hpp>

#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/stats/distributions.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <string>

using namespace loki;
using namespace loki::stats;

// ---------------------------------------------------------------------------
//  Internal helpers
// ---------------------------------------------------------------------------

namespace {

/// Validates common preconditions shared by all three CI functions.
void validateConfig(std::size_t n, const BootstrapConfig& cfg, std::size_t minN = 2)
{
    if (n < minN) {
        throw SeriesTooShortException(
            "bootstrap: data must have at least " + std::to_string(minN)
            + " observations, got " + std::to_string(n) + ".");
    }
    if (cfg.nResamples < 10) {
        throw ConfigException(
            "bootstrap: nResamples must be >= 10, got "
            + std::to_string(cfg.nResamples) + ".");
    }
    if (cfg.alpha <= 0.0 || cfg.alpha >= 0.5) {
        throw ConfigException(
            "bootstrap: alpha must be in (0, 0.5), got "
            + std::to_string(cfg.alpha) + ".");
    }
}

/// Computes mean and standard deviation of a vector of bootstrap estimates.
void bootStats(const std::vector<double>& boot, double& meanOut, double& seOut)
{
    const double n = static_cast<double>(boot.size());
    meanOut = std::accumulate(boot.begin(), boot.end(), 0.0) / n;
    double sumSq = 0.0;
    for (double v : boot) {
        const double d = v - meanOut;
        sumSq += d * d;
    }
    seOut = (boot.size() > 1) ? std::sqrt(sumSq / (n - 1.0)) : 0.0;
}

/// Returns the p-th quantile of a sorted vector (linear interpolation).
double quantileSorted(const std::vector<double>& sorted, double p)
{
    const double idx = p * static_cast<double>(sorted.size() - 1);
    const std::size_t lo = static_cast<std::size_t>(idx);
    const std::size_t hi = lo + 1;
    if (hi >= sorted.size()) return sorted.back();
    const double frac = idx - static_cast<double>(lo);
    return sorted[lo] + frac * (sorted[hi] - sorted[lo]);
}

/// Reindexes data according to an index vector produced by Sampler.
std::vector<double> applyIndices(
    const std::vector<double>& data,
    const std::vector<std::size_t>& indices)
{
    std::vector<double> resampled;
    resampled.reserve(indices.size());
    for (std::size_t idx : indices) {
        resampled.push_back(data[idx]);
    }
    return resampled;
}

/// Estimates the ACF-based block length: first lag where |ACF| < 1.96/sqrt(n).
/// Clamped to [1, n/4].
std::size_t autoBlockLength(const std::vector<double>& data)
{
    const std::size_t n = data.size();
    const double threshold = 1.96 / std::sqrt(static_cast<double>(n));

    // Compute mean.
    const double mean = std::accumulate(data.begin(), data.end(), 0.0)
                        / static_cast<double>(n);

    // Variance (lag-0 autocovariance).
    double var = 0.0;
    for (double v : data) {
        const double d = v - mean;
        var += d * d;
    }
    var /= static_cast<double>(n);

    if (var < 1e-15) {
        // Constant series -- block length 1 is fine.
        return 1;
    }

    // Search for first lag where |ACF| < threshold.
    const std::size_t maxLag = n / 2;
    for (std::size_t lag = 1; lag <= maxLag; ++lag) {
        double cov = 0.0;
        for (std::size_t i = 0; i + lag < n; ++i) {
            cov += (data[i] - mean) * (data[i + lag] - mean);
        }
        cov /= static_cast<double>(n);
        const double acf = cov / var;
        if (std::abs(acf) < threshold) {
            // Clamp to [1, n/4].
            const std::size_t maxBL = std::max(std::size_t{1}, n / 4);
            return std::min(lag, maxBL);
        }
    }

    // All lags significant -- use n/4.
    return std::max(std::size_t{1}, n / 4);
}

} // anonymous namespace

// ---------------------------------------------------------------------------
//  percentileCI
// ---------------------------------------------------------------------------

BootstrapResult percentileCI(
    const std::vector<double>& data,
    const StatFn&              statistic,
    Sampler&                   sampler,
    const BootstrapConfig&     cfg)
{
    validateConfig(data.size(), cfg, 2);

    const double estimate = statistic(data);
    const std::size_t n   = data.size();

    std::vector<double> boot;
    boot.reserve(static_cast<std::size_t>(cfg.nResamples));

    for (int r = 0; r < cfg.nResamples; ++r) {
        const auto indices   = sampler.bootstrapIndices(n);
        const auto resampled = applyIndices(data, indices);
        boot.push_back(statistic(resampled));
    }

    std::sort(boot.begin(), boot.end());

    double bootMean = 0.0, bootSe = 0.0;
    bootStats(boot, bootMean, bootSe);

    const double lower = quantileSorted(boot, cfg.alpha / 2.0);
    const double upper = quantileSorted(boot, 1.0 - cfg.alpha / 2.0);

    return BootstrapResult{
        estimate,
        lower,
        upper,
        bootMean - estimate,
        bootSe,
        cfg.nResamples
    };
}

// ---------------------------------------------------------------------------
//  bcaCI
// ---------------------------------------------------------------------------

BootstrapResult bcaCI(
    const std::vector<double>& data,
    const StatFn&              statistic,
    Sampler&                   sampler,
    const BootstrapConfig&     cfg)
{
    validateConfig(data.size(), cfg, 2);

    const double estimate = statistic(data);
    const std::size_t n   = data.size();

    // Step 1: generate bootstrap distribution.
    std::vector<double> boot;
    boot.reserve(static_cast<std::size_t>(cfg.nResamples));
    for (int r = 0; r < cfg.nResamples; ++r) {
        const auto indices   = sampler.bootstrapIndices(n);
        const auto resampled = applyIndices(data, indices);
        boot.push_back(statistic(resampled));
    }

    // Step 2: bias-correction z0.
    // Proportion of boot estimates strictly less than the original estimate.
    std::size_t countBelow = 0;
    for (double v : boot) {
        if (v < estimate) ++countBelow;
    }
    // Avoid degenerate p = 0 or 1 for normalQuantile.
    const double propBelow = (static_cast<double>(countBelow) + 0.5)
                             / (static_cast<double>(cfg.nResamples) + 1.0);
    const double z0 = normalQuantile(propBelow);

    // Step 3: acceleration a via jackknife.
    // Jackknife estimates: theta_i = statistic computed leaving out observation i.
    std::vector<double> jackknife(n);
    for (std::size_t i = 0; i < n; ++i) {
        std::vector<double> leaveOne;
        leaveOne.reserve(n - 1);
        for (std::size_t j = 0; j < n; ++j) {
            if (j != i) leaveOne.push_back(data[j]);
        }
        jackknife[i] = statistic(leaveOne);
    }

    // Jackknife mean.
    const double jackMean = std::accumulate(jackknife.begin(), jackknife.end(), 0.0)
                            / static_cast<double>(n);

    // Acceleration: a = sum((mean - theta_i)^3) / (6 * (sum((mean - theta_i)^2))^1.5)
    double num = 0.0, den = 0.0;
    for (double tj : jackknife) {
        const double d = jackMean - tj;
        num += d * d * d;
        den += d * d;
    }
    den = std::pow(den, 1.5);

    if (std::abs(den) < 1e-15) {
        throw AlgorithmException(
            "bcaCI: degenerate jackknife distribution -- acceleration undefined. "
            "The statistic may be constant across leave-one-out subsets.");
    }

    const double a = num / (6.0 * den);

    // Step 4: adjusted quantile levels.
    const double za2  = normalQuantile(cfg.alpha / 2.0);
    const double z1a2 = normalQuantile(1.0 - cfg.alpha / 2.0);

    auto adjustedAlpha = [&](double z) -> double {
        const double num2 = z0 + z;
        const double arg  = z0 + num2 / (1.0 - a * num2);
        // Clamp to avoid normalCdf domain issues.
        return normalCdf(arg);
    };

    const double alpha1 = adjustedAlpha(za2);
    const double alpha2 = adjustedAlpha(z1a2);

    // Step 5: CI from sorted bootstrap distribution.
    std::sort(boot.begin(), boot.end());

    double bootMean = 0.0, bootSe = 0.0;
    bootStats(boot, bootMean, bootSe);

    // Clamp adjusted alphas to (0, 1) to avoid edge quantiles.
    const double safeAlpha1 = std::max(0.001, std::min(0.999, alpha1));
    const double safeAlpha2 = std::max(0.001, std::min(0.999, alpha2));

    const double lower = quantileSorted(boot, safeAlpha1);
    const double upper = quantileSorted(boot, safeAlpha2);

    return BootstrapResult{
        estimate,
        lower,
        upper,
        bootMean - estimate,
        bootSe,
        cfg.nResamples
    };
}

// ---------------------------------------------------------------------------
//  blockCI
// ---------------------------------------------------------------------------

BootstrapResult blockCI(
    const std::vector<double>& data,
    const StatFn&              statistic,
    Sampler&                   sampler,
    const BootstrapConfig&     cfg)
{
    validateConfig(data.size(), cfg, 4);

    const std::size_t n = data.size();

    // Resolve block length.
    std::size_t blockLen = cfg.blockLength;
    if (blockLen == 0) {
        blockLen = autoBlockLength(data);
        LOKI_WARNING("blockCI: auto block length selected = "
                     + std::to_string(blockLen) + " (n=" + std::to_string(n) + ").");
    }
    if (blockLen > n) {
        throw ConfigException(
            "blockCI: blockLength (" + std::to_string(blockLen)
            + ") must not exceed data size (" + std::to_string(n) + ").");
    }

    const double estimate = statistic(data);

    std::vector<double> boot;
    boot.reserve(static_cast<std::size_t>(cfg.nResamples));

    for (int r = 0; r < cfg.nResamples; ++r) {
        const auto indices   = sampler.blockBootstrapIndices(n, blockLen);
        const auto resampled = applyIndices(data, indices);
        boot.push_back(statistic(resampled));
    }

    std::sort(boot.begin(), boot.end());

    double bootMean = 0.0, bootSe = 0.0;
    bootStats(boot, bootMean, bootSe);

    const double lower = quantileSorted(boot, cfg.alpha / 2.0);
    const double upper = quantileSorted(boot, 1.0 - cfg.alpha / 2.0);

    return BootstrapResult{
        estimate,
        lower,
        upper,
        bootMean - estimate,
        bootSe,
        cfg.nResamples
    };
}
