#pragma once

#include <loki/homogeneity/changePointResult.hpp>

#include <cstddef>
#include <utility>
#include <vector>

namespace loki::homogeneity {

/**
 * @brief Configuration for ChangePointDetector.
 *
 * Defined outside the class to avoid a GCC bug with nested structs
 * that have member initializers used as default constructor arguments.
 */
struct ChangePointDetectorConfig {
    /// Significance level alpha for the hypothesis test (e.g. 0.05).
    double significanceLevel = 0.05;

    /// Lag-1 ACF threshold kept for diagnostics; sigmaStar is always applied.
    double acfDependenceLimit = 0.2;
};

/**
 * @brief Detects a single change point in one segment of a time series.
 *
 * Implements the T-statistic approach based on Yao & Davis (1986) with the
 * noise-dependence correction factor sigmaStar from Antoch et al. (1995).
 *
 * The algorithm:
 *   1. Build prefix sums over [begin, end) -- O(n).
 *   2. Compute T_k for every candidate split k in O(n) using prefix sums.
 *   3. Derive the asymptotic critical value from the Gumbel distribution.
 *   4. Estimate sigmaStar from the Bartlett-windowed spectral density of the
 *      centred residuals and scale the critical value accordingly.
 *   5. Compute an asymptotic p-value and a confidence interval for the
 *      change point position.
 *
 * The caller is expected to pass a pre-processed (deseasonalised, gap-free)
 * series. Indices are 0-based positions in the z vector; [begin, end) is a
 * half-open interval.
 */
class ChangePointDetector {
public:
    /// Configuration type alias.
    using Config = ChangePointDetectorConfig;

    /**
     * @brief Constructs a detector with the given configuration.
     * @param cfg Detection parameters. Defaults are appropriate for most use cases.
     */
    explicit ChangePointDetector(Config cfg = Config{});

    /**
     * @brief Runs the change point test on the sub-series z[begin .. end).
     *
     * @param z     Full deseasonalised series (only indices [begin, end) are used).
     * @param begin First index of the segment to test (inclusive).
     * @param end   One-past-last index of the segment to test (exclusive).
     * @return      Detection result. result.detected == false means the segment
     *              is homogeneous at the configured significance level.
     * @throws loki::DataException if the segment has fewer than MIN_SEGMENT points.
     */
    [[nodiscard]]
    ChangePointResult detect(const std::vector<double>& z,
                             std::size_t begin,
                             std::size_t end) const;

    /// Minimum number of points required in a segment.
    static constexpr std::size_t MIN_SEGMENT = 5;

private:
    // -----------------------------------------------------------------------
    // Internal result type
    // -----------------------------------------------------------------------

    struct TStatResult {
        double              maxTk{0.0};
        std::size_t         splitIndex{0};  // absolute index into z
        std::vector<double> sk;             // pooled std-dev at each k
    };

    // -----------------------------------------------------------------------
    // Static helpers -- no shared state, straightforward to unit-test
    // -----------------------------------------------------------------------

    /**
     * @brief Builds a prefix-sum array over z[begin .. end).
     *
     * prefix[0] = 0, prefix[i] = z[begin] + ... + z[begin+i-1].
     * Returned vector length: (end - begin) + 1.
     */
    static std::vector<double> buildPrefixSums(const std::vector<double>& z,
                                               std::size_t begin,
                                               std::size_t end);

    /**
     * @brief Computes T_k statistics for all valid splits.
     *
     * First pass uses prefix sums (O(n)) to locate the candidate split.
     * Second pass computes the pooled std-dev sk needed for the true Tk
     * and for the confidence interval.
     */
    static TStatResult computeTStatistics(const std::vector<double>& z,
                                          std::size_t begin,
                                          std::size_t end,
                                          const std::vector<double>& prefix);

    /**
     * @brief Asymptotic critical value from Yao & Davis (1986).
     * @param n     Segment length.
     * @param alpha Significance level (e.g. 0.05).
     */
    static double computeCriticalValue(std::size_t n, double alpha);

    /**
     * @brief Computes segment means on either side of splitIndex.
     * @return {meanBefore, meanAfter}
     */
    static std::pair<double, double> computeMeans(const std::vector<double>& z,
                                                  std::size_t begin,
                                                  std::size_t splitIndex,
                                                  std::size_t end);

    /**
     * @brief Estimates the sigmaStar noise-correction factor (Antoch et al., 1995).
     *
     * Centres each segment around its own mean, then estimates the spectral
     * density at frequency 0 using a Bartlett window of width ceil(n^(1/3)).
     *
     * @return {sigmaStar, acfLag1}
     */
    static std::pair<double, double> computeSigmaStar(const std::vector<double>& z,
                                                      std::size_t begin,
                                                      std::size_t splitIndex,
                                                      std::size_t end,
                                                      double meanBefore,
                                                      double meanAfter);

    /**
     * @brief Asymptotic p-value from the Gumbel extreme-value distribution.
     */
    static double computePValue(double maxTk, double sigmaStar, std::size_t n);

    /**
     * @brief Confidence interval for the change point position.
     *
     * Uses a hardcoded alpha-quantile table. Returns indices relative to begin.
     * Both values are -1 when alpha is not in the lookup table.
     *
     * @return {low, high}
     */
    static std::pair<int, int> computeConfidenceInterval(std::size_t splitIndex,
                                                         std::size_t begin,
                                                         double shift,
                                                         const std::vector<double>& sk,
                                                         double alpha);

    Config m_cfg;
};

} // namespace loki::homogeneity