#include <loki/homogeneity/peltDetector.hpp>
#include <loki/core/exceptions.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

using namespace loki;

namespace loki::homogeneity {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

PeltDetector::PeltDetector(Config cfg)
    : m_cfg{std::move(cfg)}
{}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

std::vector<ChangePoint> PeltDetector::detectAll(const std::vector<double>& z,
                                                  const std::vector<double>& times) const
{
    const std::size_t n = z.size();

    if (n < MIN_SERIES) {
        throw DataException(
            "PeltDetector::detectAll: series too short (n=" +
            std::to_string(n) + ", minimum=" + std::to_string(MIN_SERIES) + ").");
    }
    if (!times.empty() && times.size() != n) {
        throw DataException(
            "PeltDetector::detectAll: times.size() != z.size().");
    }

    const int minSeg = std::max(2, m_cfg.minSegmentLength);

    // --- Precompute prefix sums and prefix sum-of-squares (O(n)) ------------
    std::vector<double> prefix(n + 1, 0.0);
    std::vector<double> prefix2(n + 1, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        prefix[i + 1]  = prefix[i]  + z[i];
        prefix2[i + 1] = prefix2[i] + z[i] * z[i];
    }

    // --- Estimate sigma^2 and compute penalty -------------------------------
    const double sigma2 = estimateSigma2(z);
    const double beta   = computePenalty(n, sigma2, m_cfg.penaltyType, m_cfg.fixedPenalty);

    // --- PELT dynamic programming -------------------------------------------
    // F[t] = minimum penalised cost for segmenting z[0..t).
    // cp[t] = the last change point position that achieves F[t].
    //         cp[t] == 0 means the segment starts at the beginning.
    //
    // Recurrence:
    //   F[0]  = -beta   (sentinel: cost before any data, no CP counted yet)
    //   F[t]  = min_{s in R_t} { F[s] + cost(s, t) + beta }
    //
    // where R_t is the set of candidate last-CP positions maintained by PELT.
    // Pruning rule: remove s from R_t if F[s] + cost(s,t) + beta >= F[t] + beta,
    // i.e. if F[s] + cost(s,t) >= F[t].  This is the PELT inequality.

    const double INF = std::numeric_limits<double>::infinity();

    std::vector<double>      F(n + 1, INF);
    std::vector<std::size_t> cpLast(n + 1, 0);

    F[0] = -beta;  // sentinel

    // Candidate set R: indices s such that a CP at s is still viable.
    std::vector<std::size_t> R;
    R.reserve(n);
    R.push_back(0);  // start with s=0 (beginning of series)

    for (std::size_t t = 1; t <= n; ++t) {

        // Find the best s in R such that segment [s, t) is long enough.
        for (std::size_t s : R) {
            if (static_cast<int>(t - s) < minSeg) continue;

            const double cost = F[s] + segmentCost(prefix, prefix2, s, t) + beta;
            if (cost < F[t]) {
                F[t]      = cost;
                cpLast[t] = s;
            }
        }

        // If F[t] is still INF, no valid segmentation found -- extend the
        // last candidate's segment (happens only at the very beginning when
        // minSeg forces all early t to be unreachable).
        if (F[t] == INF) {
            // Fallback: treat everything as one segment so far.
            F[t]      = segmentCost(prefix, prefix2, 0, t);
            cpLast[t] = 0;
        }

        // Add t to R as a new candidate change point position.
        R.push_back(t);

        // PELT pruning: remove s from R if F[s] + cost(s,t) >= F[t].
        // Elements that satisfy the pruning inequality at time t will also
        // satisfy it for all future t' > t, so they can be permanently removed.
        std::vector<std::size_t> R_new;
        R_new.reserve(R.size());
        for (std::size_t s : R) {
            // Keep s if it might still be optimal for some future t.
            // Pruning condition: F[s] + cost(s,t) < F[t] + beta
            // (we keep s if the inequality is NOT yet violated).
            const double costSt = (static_cast<int>(t - s) >= minSeg)
                                  ? segmentCost(prefix, prefix2, s, t)
                                  : 0.0;  // too short -- can't prune yet
            if (F[s] + costSt < F[t] + beta) {
                R_new.push_back(s);
            }
        }
        R = std::move(R_new);
    }

    // --- Backtrack to recover change point positions ------------------------
    // Trace from cpLast[n] back to 0 to get all CPs.
    std::vector<std::size_t> cpPositions;
    std::size_t t = n;
    while (t > 0) {
        const std::size_t s = cpLast[t];
        if (s > 0) {
            cpPositions.push_back(s);
        }
        t = s;
    }

    // cpPositions is in reverse order; sort ascending.
    std::sort(cpPositions.begin(), cpPositions.end());

    // --- Build ChangePoint output -------------------------------------------
    std::vector<ChangePoint> result;
    result.reserve(cpPositions.size());

    for (std::size_t cpIdx : cpPositions) {
        // Compute shift: mean of segment after CP minus mean of segment before CP.
        // For simplicity use the two adjacent segments defined by consecutive CPs.
        // We find the segment boundaries for this CP.

        // Segment before: from previous CP (or 0) to cpIdx.
        // Segment after:  from cpIdx to next CP (or n).
        // Find prev and next in cpPositions.
        const auto it = std::lower_bound(cpPositions.begin(), cpPositions.end(), cpIdx);

        const std::size_t prevBound = (it == cpPositions.begin()) ? 0 : *(std::prev(it));
        const std::size_t nextBound = (std::next(it) == cpPositions.end())
                                      ? n
                                      : *std::next(it);

        const std::size_t nBefore = cpIdx - prevBound;
        const std::size_t nAfter  = nextBound - cpIdx;

        const double meanBefore = (nBefore > 0)
            ? (prefix[cpIdx] - prefix[prevBound]) / static_cast<double>(nBefore)
            : 0.0;
        const double meanAfter  = (nAfter > 0)
            ? (prefix[nextBound] - prefix[cpIdx]) / static_cast<double>(nAfter)
            : 0.0;

        const double shift = meanAfter - meanBefore;
        const double mjd   = times.empty() ? 0.0 : times[cpIdx];

        result.push_back(ChangePoint{cpIdx, mjd, shift, 0.0});
    }

    return result;
}

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

double PeltDetector::computePenalty(std::size_t n,
                                    double sigma2,
                                    const std::string& type,
                                    double fixedVal)
{
    // Guard against degenerate sigma2.
    const double s2 = (sigma2 > std::numeric_limits<double>::epsilon())
                      ? sigma2
                      : 1.0;

    const double dn = static_cast<double>(n);

    if (type == "aic")   return 2.0 * s2;
    if (type == "mbic")  return 3.0 * std::log(dn) * s2;
    if (type == "fixed") return fixedVal;
    // Default: "bic"
    return std::log(dn) * s2;
}

// ---------------------------------------------------------------------------

double PeltDetector::segmentCost(const std::vector<double>& prefix,
                                  const std::vector<double>& prefix2,
                                  std::size_t begin,
                                  std::size_t end)
{
    // cost(begin, end) = sum_{i=begin}^{end-1} (z[i] - mean)^2
    //                  = sum(z^2) - n * mean^2
    //                  = prefix2[end] - prefix2[begin]
    //                    - (prefix[end] - prefix[begin])^2 / (end - begin)
    const double dn  = static_cast<double>(end - begin);
    const double sum = prefix[end]  - prefix[begin];
    const double ss  = prefix2[end] - prefix2[begin];
    return ss - (sum * sum) / dn;
}

// ---------------------------------------------------------------------------

double PeltDetector::estimateSigma2(const std::vector<double>& z)
{
    const std::size_t n = z.size();

    if (n < 3) {
        // Fallback: sample variance.
        double sum = 0.0;
        for (double v : z) sum += v;
        const double mean = sum / static_cast<double>(n);
        double ss = 0.0;
        for (double v : z) { const double d = v - mean; ss += d * d; }
        return (n > 1) ? ss / static_cast<double>(n - 1) : 1.0;
    }

    // First differences: removes level shifts due to change points.
    std::vector<double> diffs;
    diffs.reserve(n - 1);
    for (std::size_t i = 1; i < n; ++i) {
        diffs.push_back(z[i] - z[i - 1]);
    }

    // MAD of differences.
    std::vector<double> absDiffs;
    absDiffs.reserve(diffs.size());
    for (double d : diffs) absDiffs.push_back(std::abs(d));

    const std::size_t m = absDiffs.size();
    std::nth_element(absDiffs.begin(),
                     absDiffs.begin() + static_cast<std::ptrdiff_t>(m / 2),
                     absDiffs.end());
    const double madDiff = absDiffs[m / 2];

    // For first differences of iid N(0, sigma^2):
    // diff ~ N(0, 2*sigma^2), so MAD(diff) = sqrt(2) * 0.6745 * sigma.
    // Therefore sigma = MAD(diff) / (sqrt(2) * 0.6745).
    static constexpr double SQRT2     = 1.41421356237;
    static constexpr double NORM_CONST = 0.6745;
    const double sigma = madDiff / (SQRT2 * NORM_CONST);

    if (sigma < std::numeric_limits<double>::epsilon()) {
        // Series is nearly constant -- fallback to tiny positive value.
        return 1e-10;
    }

    return sigma * sigma;
}

} // namespace loki::homogeneity
