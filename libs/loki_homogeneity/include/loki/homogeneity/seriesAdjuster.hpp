#pragma once

#include <loki/homogeneity/changePointResult.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <vector>

namespace loki::homogeneity {

/**
 * @brief Adjusts a time series by removing mean shifts at detected change points.
 *
 * Applies cumulative mean-shift corrections to all segments right of the
 * first (reference) segment. The reference segment is always the first one
 * and is left unchanged. All subsequent segments are pulled toward the mean
 * level of the first segment.
 *
 * Given change points sorted by index [cp_0, cp_1, ..., cp_{k-1}] with shifts
 * [s_0, s_1, ..., s_{k-1}], the correction for segment j (j >= 1) is:
 *
 *   correction(j) = s_0 + s_1 + ... + s_{j-1}
 *
 * The adjusted value is: adjusted[i] = original[i] - correction(segment(i)).
 * Points in segment 0 receive correction 0 (unchanged).
 *
 * The returned TimeSeries has the same timestamps and flags as the input.
 * The metadata componentName receives the suffix "_adj".
 *
 * @note SeriesAdjuster is stateless; it holds no configuration.
 *       Construct once and reuse across multiple series.
 */
class SeriesAdjuster {
public:

    SeriesAdjuster() = default;

    /**
     * @brief Adjusts the time series using the provided change points.
     *
     * Change points are sorted internally by globalIndex, so the order
     * of the input vector does not affect the result.
     *
     * If changePoints is empty, a copy of the original series is returned
     * with the "_adj" suffix appended to the metadata name.
     *
     * @param original     Input time series. Must not be empty.
     * @param changePoints Change points produced by MultiChangePointDetector.
     * @return Adjusted TimeSeries with metadata componentName suffix "_adj".
     * @throws AlgorithmException if any globalIndex >= original.size().
     */
    [[nodiscard]]
    loki::TimeSeries adjust(const loki::TimeSeries&         original,
                            const std::vector<ChangePoint>& changePoints) const;
};

} // namespace loki::homogeneity
