#pragma once

#include <loki/homogeneity/changePointResult.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace loki::homogeneity {

/**
 * @brief Configuration for PeltDetector.
 *
 * Defined outside the class to avoid a GCC 13 bug with nested structs
 * that have member initializers used as default constructor arguments.
 */
struct PeltDetectorConfig {
    /// Penalty type: "bic" | "aic" | "mbic" | "fixed".
    /// "bic"  : beta = log(n) * sigma^2   -- good general default
    /// "aic"  : beta = 2 * sigma^2         -- more change points than bic
    /// "mbic" : beta = 3 * log(n) * sigma^2 -- conservative, fewer CPs
    /// "fixed": beta = fixedPenalty        -- manual control
    std::string penaltyType{"bic"};

    /// Penalty value used when penaltyType == "fixed". Ignored otherwise.
    double fixedPenalty{0.0};

    /// Minimum number of observations in any segment.
    /// Must be >= 2. Recommended: >= 1 year in samples for climatological data.
    int minSegmentLength{2};
};

/**
 * @brief Detects multiple change points using the PELT algorithm
 *        (Pruned Exact Linear Time, Killick et al. 2012).
 *
 * Minimises the penalised cost:
 *
 *   sum_{segments} cost(segment) + beta * number_of_changepoints
 *
 * where cost(segment) = sum of squared deviations from the segment mean
 * (i.e. the negative log-likelihood under a Gaussian model with unknown mean
 * and known variance, scaled by variance).
 *
 * The variance sigma^2 is estimated robustly from the full series using
 * the MAD estimator: sigma = median(|x - median(x)|) / 0.6745.
 *
 * PELT achieves O(n) amortised complexity via a pruning rule: candidate
 * change points whose cumulative cost exceeds the current best plus beta
 * are permanently discarded from future consideration.
 *
 * Unlike ChangePointDetector and SnhtDetector (which find one CP per call),
 * PeltDetector::detectAll() returns ALL change points in a single pass.
 * MultiChangePointDetector calls it once and bypasses the recursive split().
 *
 * Reference: Killick, R., Fearnhead, P., & Eckley, I. A. (2012).
 * Optimal detection of changepoints with a linear computational cost.
 * Journal of the American Statistical Association, 107(500), 1590-1598.
 */
class PeltDetector {
public:

    /// Configuration type alias.
    using Config = PeltDetectorConfig;

    /**
     * @brief Constructs a PeltDetector with the given configuration.
     * @param cfg Detection parameters.
     */
    explicit PeltDetector(Config cfg = Config{});

    /**
     * @brief Detects all change points in z using PELT.
     *
     * Returns all detected change points in a single pass -- no recursion.
     * The returned vector is sorted by globalIndex (ascending).
     *
     * @param z     Deseasonalised, gap-free time series values.
     * @param times Optional MJD timestamps (same length as z).
     *              Pass empty vector when no timestamps are available;
     *              ChangePoint::mjd will be 0.0 in that case.
     * @return      Sorted vector of detected change points.
     * @throws loki::DataException if z has fewer than MIN_SERIES points,
     *         or if times is non-empty and times.size() != z.size().
     */
    [[nodiscard]]
    std::vector<ChangePoint> detectAll(const std::vector<double>& z,
                                       const std::vector<double>& times = {}) const;

    /// Minimum series length required for detection.
    static constexpr std::size_t MIN_SERIES = 4;

private:

    Config m_cfg;

    /**
     * @brief Computes the penalty beta from the series length and penaltyType.
     *
     * sigma^2 is estimated via MAD for robustness against the change points
     * themselves (OLS variance would be inflated by inter-segment shifts).
     *
     * @param n        Series length.
     * @param sigma2   Estimated variance (MAD-based).
     * @param type     Penalty type string.
     * @param fixedVal Value used when type == "fixed".
     */
    static double computePenalty(std::size_t n,
                                 double sigma2,
                                 const std::string& type,
                                 double fixedVal);

    /**
     * @brief Cost of fitting a constant-mean model to z[begin .. end).
     *
     * cost = sum_{i=begin}^{end-1} (z[i] - mean)^2
     *      = SS_total(begin, end)
     *
     * Computed in O(1) using precomputed prefix sums and prefix sum-of-squares.
     *
     * @param prefix   Prefix sums: prefix[i] = sum z[0..i).
     * @param prefix2  Prefix sum-of-squares: prefix2[i] = sum z[i]^2 for i<i.
     * @param begin    Inclusive start.
     * @param end      Exclusive end.
     */
    static double segmentCost(const std::vector<double>& prefix,
                               const std::vector<double>& prefix2,
                               std::size_t begin,
                               std::size_t end);

    /**
     * @brief Estimates sigma^2 via the normalised MAD of the first-differences.
     *
     * Using first differences rather than raw values removes the level shifts
     * caused by change points and gives a consistent estimate of the within-
     * segment variance even in the presence of multiple structural breaks.
     *
     * sigma^2 = (median(|diff[i]|) / (sqrt(2) * 0.6745))^2
     *
     * Falls back to the sample variance of z if the series is too short
     * for differencing or if MAD is zero.
     *
     * @param z Full series.
     */
    static double estimateSigma2(const std::vector<double>& z);
};

} // namespace loki::homogeneity