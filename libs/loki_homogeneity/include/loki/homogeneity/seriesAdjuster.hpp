#pragma once

#include <loki/homogeneity/changePointResult.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <vector>

namespace loki::homogeneity {

/**
 * @brief Adjusts a time series by removing mean shifts at detected change points.
 *
 * Applies cumulative mean-shift corrections to all segments left of the
 * rightmost (reference) segment. The reference segment is always the last one
 * and is left unchanged.
 *
 * Given change points at global indices [i1, i2, ..., ik] with shifts
 * [s1, s2, ..., sk], the correction applied to a point at index p is:
 *
 *   correction(p) = sum of sj for all j where ij > p
 *
 * i.e. each point receives the cumulative shift of all change points to its right.
 * The adjusted value is: adjusted[p] = original[p] - correction(p).
 *
 * The returned TimeSeries has the same timestamps and flags as the input.
 * The metadata name receives the suffix "_adj".
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
     * @return Adjusted TimeSeries with metadata name suffix "_adj".
     * @throws AlgorithmException if any globalIndex >= original.size().
     */
    [[nodiscard]]
    loki::TimeSeries adjust(const loki::TimeSeries&         original,
                            const std::vector<ChangePoint>& changePoints) const;
};

} // namespace loki::homogeneity
