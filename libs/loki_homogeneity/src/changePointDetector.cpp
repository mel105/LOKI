#include <loki/homogeneity/changePointDetector.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/stats/descriptive.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numbers>
#include <stdexcept>

using namespace loki;

namespace loki::homogeneity {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

ChangePointDetector::ChangePointDetector(Config cfg)
    : m_cfg{cfg}
{}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

ChangePointResult ChangePointDetector::detect(const std::vector<double>& z,
                                              std::size_t begin,
                                              std::size_t end) const
{
    if (end <= begin || (end - begin) < MIN_SEGMENT) {
        throw DataException(
            "ChangePointDetector::detect: segment [" +
            std::to_string(begin) + ", " + std::to_string(end) +
            ") is too short (minimum " + std::to_string(MIN_SEGMENT) + " points).");
    }

    if (end > z.size()) {
        throw DataException(
            "ChangePointDetector::detect: end index " + std::to_string(end) +
            " exceeds series length " + std::to_string(z.size()) + ".");
    }

    const std::size_t n = end - begin;

    // --- Step 1: prefix sums (O(n)) -----------------------------------------
    const auto prefix = buildPrefixSums(z, begin, end);

    // --- Step 2: T_k statistics (O(n)) --------------------------------------
    const TStatResult tstat = computeTStatistics(z, begin, end, prefix);

    // --- Step 3: initial critical value -------------------------------------
    double critVal = computeCriticalValue(n, m_cfg.significanceLevel);

    // --- Step 4: segment means ----------------------------------------------
    auto [meanBefore, meanAfter] = computeMeans(z, begin, tstat.splitIndex, end);

    // --- Step 5: sigmaStar and ACF lag-1 ------------------------------------
    auto [sigmaStar, acfLag1] = computeSigmaStar(
        z, begin, tstat.splitIndex, end, meanBefore, meanAfter);

    // Scale critical value unconditionally (design decision: always apply).
    critVal *= sigmaStar;

    // --- Step 6: p-value ----------------------------------------------------
    const double pValue = computePValue(tstat.maxTk, sigmaStar, n);

    // --- Step 7: hypothesis test --------------------------------------------
    ChangePointResult result;
    result.maxTk         = tstat.maxTk;
    result.criticalValue = critVal;
    result.pValue        = pValue;
    result.acfLag1       = acfLag1;
    result.sigmaStar     = sigmaStar;
    result.meanBefore    = meanBefore;
    result.meanAfter     = meanAfter;
    result.shift         = meanAfter - meanBefore;

    result.detected = (tstat.maxTk > critVal);

    if (result.detected) {
        // splitIndex is absolute; store relative-to-begin for the caller.
        result.index = static_cast<int>(tstat.splitIndex - begin);

        auto [ciLow, ciHigh] = computeConfidenceInterval(
            tstat.splitIndex, begin, result.shift, tstat.sk, m_cfg.significanceLevel);
        result.confIntervalLow  = ciLow;
        result.confIntervalHigh = ciHigh;
    }

    return result;
}

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

std::vector<double> ChangePointDetector::buildPrefixSums(const std::vector<double>& z,
                                                          std::size_t begin,
                                                          std::size_t end)
{
    const std::size_t n = end - begin;
    std::vector<double> prefix(n + 1, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        prefix[i + 1] = prefix[i] + z[begin + i];
    }
    return prefix;
}

// ---------------------------------------------------------------------------

ChangePointDetector::TStatResult
ChangePointDetector::computeTStatistics(const std::vector<double>& z,
                                        std::size_t begin,
                                        std::size_t end,
                                        const std::vector<double>& prefix)
{
    const std::size_t n  = end - begin;
    const double      dn = static_cast<double>(n);
    const double totalSum = prefix[n];

    // We need pooled std-dev sk at each k.
    // sk^2 = (SS_before(k) + SS_after(k)) / (n - 2)
    // where SS_before(k) = sum_{i<k} (z[i] - meanBefore)^2
    //       SS_after(k)  = sum_{i>=k} (z[i] - meanAfter)^2
    //
    // Computed with a two-pass approach: first find the split with max Tk
    // using prefix sums alone, then recompute sk for every k in one more pass.

    double      maxTk      = 0.0;
    std::size_t bestK      = begin + 1;  // absolute index of best split

    // First pass: find the split index with maximum |Tk| using prefix sums.
    // k runs from 1 to n-1 (at least one point on each side).
    for (std::size_t k = 1; k < n; ++k) {
        const double dk         = static_cast<double>(k);
        const double sumBefore  = prefix[k];
        const double sumAfter   = totalSum - sumBefore;
        const double meanBefore = sumBefore / dk;
        const double meanAfter  = sumAfter  / (dn - dk);

        // SS via the computational formula: SS = sum(x^2) - n*mean^2
        // We need sum-of-squares; build them in a third vector to avoid
        // a second nested loop. For correctness with arbitrary n, we compute
        // them directly here -- still O(n) per candidate, giving O(n^2) total
        // for sk. To keep the first pass O(n) we defer sk computation below.

        // Approximate Tk without sk for the first pass using the shift magnitude.
        // This is used only to identify the best split; sk is computed afterwards.
        const double shift = std::abs(meanAfter - meanBefore);
        const double tkApprox = std::sqrt((dn - dk) * dk / dn) * shift;

        if (tkApprox > maxTk) {
            maxTk = tkApprox;
            bestK = begin + k;
        }
    }

    // Second pass: compute sk for every k and the true Tk at the best split,
    // plus populate the sk vector used by computeConfidenceInterval.
    // We accept O(n^2) for sk only because this second pass is separated
    // and the first pass already located the candidate.  For very long series
    // (n > 10 000) consider an incremental SS update -- documented as a
    // future optimisation.
    std::vector<double> sk(n, 0.0);
    double trueTkAtBest = 0.0;

    for (std::size_t k = 1; k < n; ++k) {
        const double dk         = static_cast<double>(k);
        const double sumBefore  = prefix[k];
        const double sumAfter   = totalSum - sumBefore;
        const double meanBefore = sumBefore / dk;
        const double meanAfter  = sumAfter  / (dn - dk);

        double ssBefore = 0.0;
        for (std::size_t i = 0; i < k; ++i) {
            const double d = z[begin + i] - meanBefore;
            ssBefore += d * d;
        }
        double ssAfter = 0.0;
        for (std::size_t i = k; i < n; ++i) {
            const double d = z[begin + i] - meanAfter;
            ssAfter += d * d;
        }

        // Guard against zero pooled std-dev (e.g. noise-free synthetic series).
        // When both segments are perfectly flat the shift is real; fallback to 1.0
        // so Tk equals the raw shift magnitude scaled by sqrt(k*(n-k)/n).
        const double rawSkk = (n > 2)
            ? std::sqrt((ssBefore + ssAfter) / static_cast<double>(n - 2))
            : 1.0;
        const double skk = (rawSkk > std::numeric_limits<double>::epsilon())
                           ? rawSkk
                           : 1.0;
        sk[k] = skk;

        if (begin + k == bestK) {
            trueTkAtBest = std::sqrt((dn - dk) * dk / dn) *
                           std::abs(meanAfter - meanBefore) / skk;
        }
    }

    // Use the true (sk-normalised) value at the best split.
    maxTk = trueTkAtBest;

    return TStatResult{maxTk, bestK, sk};
}

// ---------------------------------------------------------------------------

double ChangePointDetector::computeCriticalValue(std::size_t n, double alpha)
{
    // Asymptotic distribution: Yao & Davis (1986).
    // cv = (c * a_n) + b_n
    // where c = -log(-(sqrt(pi)/2) * log(1 - alpha))
    //       a_n = 1 / sqrt(2 * log(log(n)))
    //       b_n = 1/a_n + (a_n/2) * log(log(log(n)))
    const double dn   = static_cast<double>(n);
    const double lln  = std::log(std::log(dn));
    const double a_n  = 1.0 / std::sqrt(2.0 * lln);
    const double b_n  = 1.0 / a_n + (a_n / 2.0) * std::log(lln);
    const double sqrtPiOver2 = std::sqrt(std::numbers::pi) / 2.0;
    const double cval = -std::log(-sqrtPiOver2 * std::log(1.0 - alpha));

    return (cval * a_n) + b_n;
}

// ---------------------------------------------------------------------------

std::pair<double, double>
ChangePointDetector::computeMeans(const std::vector<double>& z,
                                  std::size_t begin,
                                  std::size_t splitIndex,
                                  std::size_t end)
{
    double sumBefore = 0.0;
    for (std::size_t i = begin; i < splitIndex; ++i) { sumBefore += z[i]; }

    double sumAfter = 0.0;
    for (std::size_t i = splitIndex; i < end; ++i) { sumAfter += z[i]; }

    const double nBefore = static_cast<double>(splitIndex - begin);
    const double nAfter  = static_cast<double>(end - splitIndex);

    const double meanBefore = (nBefore > 0.0) ? sumBefore / nBefore : 0.0;
    const double meanAfter  = (nAfter  > 0.0) ? sumAfter  / nAfter  : 0.0;

    return {meanBefore, meanAfter};
}

// ---------------------------------------------------------------------------

std::pair<double, double>
ChangePointDetector::computeSigmaStar(const std::vector<double>& z,
                                      std::size_t begin,
                                      std::size_t splitIndex,
                                      std::size_t end,
                                      double meanBefore,
                                      double meanAfter)
{
    const std::size_t n = end - begin;

    // Build centred residuals: subtract segment mean from each observation.
    std::vector<double> residuals;
    residuals.reserve(n);

    for (std::size_t i = begin; i < splitIndex; ++i) {
        residuals.push_back(z[i] - meanBefore);
    }
    for (std::size_t i = splitIndex; i < end; ++i) {
        residuals.push_back(z[i] - meanAfter);
    }

    // Bartlett window width.
    const std::size_t L = static_cast<std::size_t>(
        std::ceil(std::pow(static_cast<double>(n), 1.0 / 3.0)));

    // ACF of residuals up to lag L (need at least lag 1).
    const int maxLag = static_cast<int>(L) + 1;
    const std::vector<double> acfVec = loki::stats::acf(residuals, maxLag);

    const double acfLag1 = (acfVec.size() > 1) ? acfVec[1] : 0.0;

    // Estimate spectral density at frequency 0 using Bartlett window.
    // f0 = acf[0] + 2 * sum_{i=1}^{L} w_i * acf[i]
    // where w_i = 1 - i/(L+1)  (Bartlett)
    double f0 = acfVec[0];
    for (std::size_t i = 1; i <= L && i < acfVec.size(); ++i) {
        const double weight = 1.0 - static_cast<double>(i) / static_cast<double>(L);
        f0 += 2.0 * weight * acfVec[i];
    }

    const double sigmaStar = std::sqrt(std::abs(f0));

    return {sigmaStar, acfLag1};
}

// ---------------------------------------------------------------------------

double ChangePointDetector::computePValue(double maxTk,
                                          double sigmaStar,
                                          std::size_t n)
{
    const double dn = static_cast<double>(n);
    // Normalise by sigmaStar before applying the Gumbel approximation.
    const double TkNorm = (sigmaStar > std::numeric_limits<double>::epsilon())
                          ? maxTk / sigmaStar
                          : maxTk;

    const double an = std::sqrt(2.0 * std::log(std::log(dn)));
    const double bn = 2.0 * std::log(std::log(dn))
                    + 0.5 * std::log(std::log(std::log(dn)))
                    - 0.5 * std::log(std::numbers::pi);

    const double y = (an * TkNorm) - bn;

    return 1.0 - std::exp(-2.0 * std::exp(-y));
}

// ---------------------------------------------------------------------------

std::pair<int, int>
ChangePointDetector::computeConfidenceInterval(std::size_t splitIndex,
                                               std::size_t begin,
                                               double shift,
                                               const std::vector<double>& sk,
                                               double alpha)
{
    // Hardcoded alpha-quantile table (Antoch & Huskova style).
    // Keys are 1 - alpha (i.e. confidence level), values are quantile constants.
    static const std::map<double, double> ALPHA_TABLE{
        {0.90,  4.696},
        {0.95,  7.687},
        {0.975, 11.033},
        {0.99,  15.868},
        {0.995, 19.767},
    };

    const double confLevel = 1.0 - alpha;
    const auto   it        = ALPHA_TABLE.find(confLevel);

    if (it == ALPHA_TABLE.end()) {
        // Alpha not in table; CI not available.
        return {-1, -1};
    }

    const double alphaCrit = it->second;
    const std::size_t localK = splitIndex - begin;  // relative split index

    if (localK >= sk.size() || std::abs(shift) < std::numeric_limits<double>::epsilon()) {
        return {-1, -1};
    }

    const double skAtK  = sk[localK];
    const double margin = std::floor((alphaCrit * skAtK * skAtK) /
                                     (shift * shift));

    const int low  = static_cast<int>(localK) - static_cast<int>(margin);
    const int high = static_cast<int>(localK) + static_cast<int>(margin);

    return {low, high};
}

} // namespace loki::homogeneity