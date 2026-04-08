#pragma once

#include <cstdint>
#include <loki/homogeneity/changePointResult.hpp>

#include <cstddef>
#include <vector>

namespace loki::homogeneity {

/**
 * @brief Configuration for SnhtDetector.
 *
 * Defined outside the class to avoid a GCC 13 bug with nested structs
 * that have member initializers used as default constructor arguments.
 */
struct SnhtDetectorConfig {
    /// Significance level alpha for the hypothesis test (e.g. 0.05).
    double significance{0.05};

    /// Minimum segment length to attempt detection.
    int minSegmentLength{30};

    /// Number of Monte Carlo permutations used to estimate the p-value.
    /// 999 gives good precision at reasonable runtime; use 4999 for publication.
    int nPermutations{999};

    /// Random seed for the permutation sampler. 0 = non-deterministic (random_device).
    uint64_t seed{0};
};

/**
 * @brief Detects a single change point using the Standard Normal Homogeneity Test
 *        (SNHT, Alexandersson 1986).
 *
 * The T-statistic at split position k is:
 *
 *   T(k) = k * (z1_bar - z_bar)^2 / s^2  +  (n-k) * (z2_bar - z_bar)^2 / s^2
 *
 * where:
 *   z1_bar = mean of z[begin .. begin+k)
 *   z2_bar = mean of z[begin+k .. end)
 *   z_bar  = overall mean of the segment
 *   s^2    = pooled sample variance of the segment
 *
 * The candidate split k* maximises T(k) over all valid k in [1, n-1].
 *
 * Because the asymptotic distribution of max T(k) has no closed-form critical
 * value table for large n, the p-value is estimated via Monte Carlo permutation:
 * the segment is randomly shuffled nPermutations times and T_max is recomputed
 * on each shuffle. The p-value is the fraction of shuffled T_max values that
 * exceed or equal the observed T_max.
 *
 * A change point is declared when pValue < cfg.significance.
 *
 * Reference: Alexandersson, H. (1986). A homogeneity test applied to
 * precipitation data. Journal of Climatology, 6(6), 661-675.
 */
class SnhtDetector {
public:

    /// Configuration type alias.
    using Config = SnhtDetectorConfig;

    /**
     * @brief Constructs an SnhtDetector with the given configuration.
     * @param cfg Detection parameters.
     */
    explicit SnhtDetector(Config cfg = Config{});

    /**
     * @brief Runs the SNHT on the sub-series z[begin .. end).
     *
     * @param z     Full deseasonalised series (only [begin, end) is used).
     * @param begin First index of the segment (inclusive).
     * @param end   One-past-last index of the segment (exclusive).
     * @return      Detection result. result.detected == false means the segment
     *              is homogeneous at the configured significance level.
     * @throws loki::DataException if the segment has fewer than MIN_SEGMENT points.
     */
    [[nodiscard]]
    ChangePointResult detect(const std::vector<double>& z,
                             std::size_t begin,
                             std::size_t end) const;

    /// Minimum number of points required in a segment.
    static constexpr std::size_t MIN_SEGMENT = 10;

private:

    Config m_cfg;

    // -----------------------------------------------------------------------
    //  Static helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Computes T(k) for all valid k and returns T_max and k*.
     *
     * Uses prefix sums for O(n) mean computation. Variance is computed once
     * over the full segment.
     *
     * @param z     Full series.
     * @param begin Inclusive start index.
     * @param end   Exclusive end index.
     * @return      {T_max, k_star_absolute} where k_star is an absolute index into z.
     */
    static std::pair<double, std::size_t> computeTMax(
        const std::vector<double>& z,
        std::size_t begin,
        std::size_t end);

    /**
     * @brief Estimates the p-value via Monte Carlo permutation.
     *
     * Shuffles z[begin..end) nPermutations times, recomputes T_max on each
     * shuffle, and returns the fraction of shuffled T_max values >= observedTMax.
     *
     * @param z            Full series (segment will be copied internally).
     * @param begin        Inclusive start.
     * @param end          Exclusive end.
     * @param observedTMax T_max on the original (unshuffled) segment.
     * @param nPerm        Number of permutations.
     * @param seed         RNG seed (0 = random_device).
     * @return             Estimated p-value in (0, 1].
     */
    static double estimatePValue(const std::vector<double>& z,
                                 std::size_t begin,
                                 std::size_t end,
                                 double observedTMax,
                                 int nPerm,
                                 uint64_t seed);
};

} // namespace loki::homogeneity
