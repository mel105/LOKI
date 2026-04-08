#include <loki/homogeneity/snhtDetector.hpp>
#include <loki/core/exceptions.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <random>

using namespace loki;

namespace loki::homogeneity {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

SnhtDetector::SnhtDetector(Config cfg)
    : m_cfg{cfg}
{}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

ChangePointResult SnhtDetector::detect(const std::vector<double>& z,
                                       std::size_t begin,
                                       std::size_t end) const
{
    if (end <= begin || (end - begin) < MIN_SEGMENT) {
        throw DataException(
            "SnhtDetector::detect: segment [" +
            std::to_string(begin) + ", " + std::to_string(end) +
            ") is too short (minimum " + std::to_string(MIN_SEGMENT) + " points).");
    }

    if (end > z.size()) {
        throw DataException(
            "SnhtDetector::detect: end index " + std::to_string(end) +
            " exceeds series length " + std::to_string(z.size()) + ".");
    }

    // --- Step 1: compute observed T_max and optimal split -------------------
    auto [tMax, kStar] = computeTMax(z, begin, end);

    // --- Step 2: compute segment means at k* --------------------------------
    const std::size_t nBefore = kStar - begin;
    const std::size_t nAfter  = end - kStar;

    double sumBefore = 0.0;
    for (std::size_t i = begin; i < kStar; ++i) sumBefore += z[i];
    double sumAfter = 0.0;
    for (std::size_t i = kStar; i < end; ++i) sumAfter += z[i];

    const double meanBefore = sumBefore / static_cast<double>(nBefore);
    const double meanAfter  = sumAfter  / static_cast<double>(nAfter);

    // --- Step 3: Monte Carlo p-value ----------------------------------------
    const double pValue = estimatePValue(z, begin, end, tMax,
                                         m_cfg.nPermutations, m_cfg.seed);

    // --- Step 4: fill result -------------------------------------------------
    ChangePointResult result;
    result.maxTk         = tMax;
    result.criticalValue = 0.0;   // not used -- decision is p-value based
    result.pValue        = pValue;
    result.acfLag1       = 0.0;   // SNHT does not use ACF correction
    result.sigmaStar     = 1.0;   // SNHT does not use sigmaStar
    result.meanBefore    = meanBefore;
    result.meanAfter     = meanAfter;
    result.shift         = meanAfter - meanBefore;
    result.detected      = (pValue < m_cfg.significance);

    if (result.detected) {
        // Store position relative to begin (consistent with ChangePointDetector).
        result.index = static_cast<int>(kStar - begin);
        // Confidence interval not implemented for SNHT (permutation-based CI
        // would require additional computation -- left as future extension).
        result.confIntervalLow  = -1;
        result.confIntervalHigh = -1;
    }

    return result;
}

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

std::pair<double, std::size_t>
SnhtDetector::computeTMax(const std::vector<double>& z,
                          std::size_t begin,
                          std::size_t end)
{
    const std::size_t n  = end - begin;
    const double      dn = static_cast<double>(n);

    // Overall mean and variance of the segment.
    double sumAll = 0.0;
    for (std::size_t i = begin; i < end; ++i) sumAll += z[i];
    const double zBar = sumAll / dn;

    double ssAll = 0.0;
    for (std::size_t i = begin; i < end; ++i) {
        const double d = z[i] - zBar;
        ssAll += d * d;
    }

    // Guard: if segment is perfectly flat, T(k) == 0 for all k.
    // s2 = 0 would cause division by zero; return T_max = 0 -> not detected.
    const double s2 = ssAll / dn;
    if (s2 < std::numeric_limits<double>::epsilon()) {
        return {0.0, begin + n / 2};
    }

    // Prefix sums for O(n) mean computation.
    // prefix[i] = sum of z[begin .. begin+i).
    std::vector<double> prefix(n + 1, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        prefix[i + 1] = prefix[i] + z[begin + i];
    }

    double      tMax  = 0.0;
    std::size_t kStar = begin + 1;   // absolute index

    // k runs from 1 to n-1: at least one point on each side.
    for (std::size_t k = 1; k < n; ++k) {
        const double dk     = static_cast<double>(k);
        const double z1Bar  = prefix[k] / dk;
        const double z2Bar  = (sumAll - prefix[k]) / (dn - dk);

        // T(k) = k*(z1_bar - z_bar)^2/s^2 + (n-k)*(z2_bar - z_bar)^2/s^2
        const double d1 = z1Bar - zBar;
        const double d2 = z2Bar - zBar;
        const double tk = (dk * d1 * d1 + (dn - dk) * d2 * d2) / s2;

        if (tk > tMax) {
            tMax  = tk;
            kStar = begin + k;
        }
    }

    return {tMax, kStar};
}

// ---------------------------------------------------------------------------

double SnhtDetector::estimatePValue(const std::vector<double>& z,
                                    std::size_t begin,
                                    std::size_t end,
                                    double observedTMax,
                                    int nPerm,
                                    uint64_t seed)
{
    const std::size_t n = end - begin;

    // Copy the segment so we can shuffle in-place.
    std::vector<double> seg(z.begin() + static_cast<std::ptrdiff_t>(begin),
                            z.begin() + static_cast<std::ptrdiff_t>(end));

    // Initialise RNG.
    std::mt19937_64 rng;
    if (seed == 0) {
        std::random_device rd;
        rng.seed(rd());
    } else {
        rng.seed(seed);
    }

    // Pre-compute overall sum and variance (invariant under permutation).
    const double dn = static_cast<double>(n);
    double sumAll = 0.0;
    for (double v : seg) sumAll += v;
    const double zBar = sumAll / dn;

    double ssAll = 0.0;
    for (double v : seg) {
        const double d = v - zBar;
        ssAll += d * d;
    }
    const double s2 = ssAll / dn;

    // If s2 is effectively zero, all permutations also give T_max = 0.
    // p-value is 1 (no detection) -- but this case is already handled in detect().
    if (s2 < std::numeric_limits<double>::epsilon()) {
        return 1.0;
    }

    // Prefix sum buffer reused across permutations.
    std::vector<double> prefix(n + 1, 0.0);

    int countExceed = 0;

    for (int p = 0; p < nPerm; ++p) {
        std::shuffle(seg.begin(), seg.end(), rng);

        // Rebuild prefix sums.
        prefix[0] = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            prefix[i + 1] = prefix[i] + seg[i];
        }

        // Find T_max for this permutation.
        double tMaxPerm = 0.0;
        for (std::size_t k = 1; k < n; ++k) {
            const double dk    = static_cast<double>(k);
            const double z1Bar = prefix[k] / dk;
            const double z2Bar = (sumAll - prefix[k]) / (dn - dk);
            const double d1    = z1Bar - zBar;
            const double d2    = z2Bar - zBar;
            const double tk    = (dk * d1 * d1 + (dn - dk) * d2 * d2) / s2;
            if (tk > tMaxPerm) tMaxPerm = tk;
        }

        if (tMaxPerm >= observedTMax) {
            ++countExceed;
        }
    }

    // Add 1 to numerator and denominator (Phipson & Smyth 2010 correction)
    // to avoid exact p-value = 0 and ensure valid interpretation.
    return static_cast<double>(countExceed + 1) /
           static_cast<double>(nPerm + 1);
}

} // namespace loki::homogeneity
