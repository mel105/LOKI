#include <loki/stats/permutation.hpp>

#include <loki/core/exceptions.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <string>

using namespace loki;
// using namespace loki::stats;
namespace loki::stats{
// ---------------------------------------------------------------------------
//  Internal helpers
// ---------------------------------------------------------------------------

namespace {

void validateConfig(std::size_t n, const PermutationConfig& cfg, std::size_t minN = 2)
{
    if (n < minN) {
        throw SeriesTooShortException(
            "permutation test: data must have at least " + std::to_string(minN)
            + " observations, got " + std::to_string(n) + ".");
    }
    if (cfg.nPermutations < 99) {
        throw ConfigException(
            "permutation test: nPermutations must be >= 99, got "
            + std::to_string(cfg.nPermutations) + ".");
    }
    if (cfg.alpha <= 0.0 || cfg.alpha >= 0.5) {
        throw ConfigException(
            "permutation test: alpha must be in (0, 0.5), got "
            + std::to_string(cfg.alpha) + ".");
    }
}

/// Computes p-value from a permutation distribution and an observed statistic.
/// nPermutations is the total number of random permutations (excludes original).
double computePValue(
    const std::vector<double>& permDist,
    double                     observed,
    Alternative                alternative)
{
    const double np = static_cast<double>(permDist.size());

    double count = 0.0;
    switch (alternative) {
        case Alternative::TWO_SIDED:
            for (double v : permDist) {
                if (std::abs(v) >= std::abs(observed)) count += 1.0;
            }
            break;
        case Alternative::GREATER:
            for (double v : permDist) {
                if (v >= observed) count += 1.0;
            }
            break;
        case Alternative::LESS:
            for (double v : permDist) {
                if (v <= observed) count += 1.0;
            }
            break;
    }

    // Add 1 to numerator and denominator (include original as a permutation).
    return (count + 1.0) / (np + 1.0);
}

/// Pearson correlation coefficient between two equal-length vectors.
double pearsonR(const std::vector<double>& x, const std::vector<double>& y)
{
    const std::size_t n = x.size();
    const double nd     = static_cast<double>(n);

    double sumX = 0.0, sumY = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        sumX += x[i];
        sumY += y[i];
    }
    const double meanX = sumX / nd;
    const double meanY = sumY / nd;

    double num = 0.0, varX = 0.0, varY = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double dx = x[i] - meanX;
        const double dy = y[i] - meanY;
        num  += dx * dy;
        varX += dx * dx;
        varY += dy * dy;
    }

    const double denom = std::sqrt(varX * varY);
    if (denom < 1e-15) return 0.0;
    return num / denom;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
//  oneSampleTest
// ---------------------------------------------------------------------------

PermutationResult oneSampleTest(
    const std::vector<double>& data,
    const StatFn&              statistic,
    double                     hypothesizedValue,
    Sampler&                   sampler,
    const PermutationConfig&   cfg)
{
    validateConfig(data.size(), cfg, 2);

    const double observed = statistic(data);
    const std::size_t n   = data.size();

    // Centre the data so that H0 holds exactly in the shifted sample.
    // Under H0 the statistic equals hypothesizedValue, so we shift by
    // (hypothesizedValue - observed) to make statistic(shifted) = hypothesizedValue.
    std::vector<double> shifted(n);
    const double shift = hypothesizedValue - observed;
    for (std::size_t i = 0; i < n; ++i) {
        shifted[i] = data[i] + shift;
    }

    // Permutation: randomly flip signs of (shifted[i] - hypothesizedValue).
    // This simulates symmetric sampling under H0.
    std::uniform_int_distribution<int> coinFlip(0, 1);

    std::vector<double> permDist;
    permDist.reserve(static_cast<std::size_t>(cfg.nPermutations));

    std::vector<double> flipped(n);
    for (int r = 0; r < cfg.nPermutations; ++r) {
        for (std::size_t i = 0; i < n; ++i) {
            const double centred = shifted[i] - hypothesizedValue;
            const double sign = (coinFlip(sampler.rng()) == 1) ? 1.0 : -1.0;
            flipped[i]           = hypothesizedValue + sign * centred;
        }
        permDist.push_back(statistic(flipped));
    }

    const double pVal     = computePValue(permDist, observed, cfg.alternative);
    const bool   rejected = (pVal < cfg.alpha);

    return PermutationResult{
        observed,
        pVal,
        rejected,
        cfg.nPermutations,
        "one-sample-permutation"
    };
}

// ---------------------------------------------------------------------------
//  twoSampleTest
// ---------------------------------------------------------------------------

PermutationResult twoSampleTest(
    const std::vector<double>& a,
    const std::vector<double>& b,
    const StatFn2&             statistic,
    Sampler&                   sampler,
    const PermutationConfig&   cfg)
{
    validateConfig(a.size(), cfg, 2);
    validateConfig(b.size(), cfg, 2);

    const double observed = statistic(a, b);
    const std::size_t na  = a.size();
    const std::size_t nb  = b.size();
    const std::size_t ntot = na + nb;

    // Pool both groups.
    std::vector<double> pooled;
    pooled.reserve(ntot);
    for (double v : a) pooled.push_back(v);
    for (double v : b) pooled.push_back(v);

    // Index vector for shuffling.
    std::vector<std::size_t> idx(ntot);
    std::iota(idx.begin(), idx.end(), 0);

    std::vector<double> permDist;
    permDist.reserve(static_cast<std::size_t>(cfg.nPermutations));

    std::vector<double> groupA(na), groupB(nb);
    for (int r = 0; r < cfg.nPermutations; ++r) {
        std::shuffle(idx.begin(), idx.end(), sampler.rng());
        for (std::size_t i = 0; i < na; ++i)  groupA[i] = pooled[idx[i]];
        for (std::size_t i = 0; i < nb; ++i)  groupB[i] = pooled[idx[na + i]];
        permDist.push_back(statistic(groupA, groupB));
    }

    const double pVal     = computePValue(permDist, observed, cfg.alternative);
    const bool   rejected = (pVal < cfg.alpha);

    return PermutationResult{
        observed,
        pVal,
        rejected,
        cfg.nPermutations,
        "two-sample-permutation"
    };
}

// ---------------------------------------------------------------------------
//  correlationTest
// ---------------------------------------------------------------------------

PermutationResult correlationTest(
    const std::vector<double>& x,
    const std::vector<double>& y,
    Sampler&                   sampler,
    const PermutationConfig&   cfg)
{
    validateConfig(x.size(), cfg, 4);
    if (x.size() != y.size()) {
        throw DataException(
            "permutation correlationTest: x and y must have the same size ("
            + std::to_string(x.size()) + " vs " + std::to_string(y.size()) + ").");
    }

    const double observed = pearsonR(x, y);
    // const std::size_t n   = y.size();

    // Permute y, keep x fixed.     
    std::vector<double> yPerm(y.begin(), y.end());

    std::vector<double> permDist;
    permDist.reserve(static_cast<std::size_t>(cfg.nPermutations));

    for (int r = 0; r < cfg.nPermutations; ++r) {
        std::shuffle(yPerm.begin(), yPerm.end(), sampler.rng());
        permDist.push_back(pearsonR(x, yPerm));
    }

    const double pVal     = computePValue(permDist, observed, cfg.alternative);
    const bool   rejected = (pVal < cfg.alpha);

    return PermutationResult{
        observed,
        pVal,
        rejected,
        cfg.nPermutations,
        "permutation-correlation"
    };
}
} // namespace loki::stats