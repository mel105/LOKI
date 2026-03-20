#include <loki/homogeneity/seriesAdjuster.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <algorithm>
#include <string>

using namespace loki;
using namespace loki::homogeneity;

// ---------------------------------------------------------------------------

TimeSeries SeriesAdjuster::adjust(const TimeSeries&               original,
                                  const std::vector<ChangePoint>& changePoints) const
{
    const std::size_t n = original.size();

    // Validate all indices before touching any data.
    for (const auto& cp : changePoints) {
        if (cp.globalIndex >= n) {
            throw AlgorithmException(
                "SeriesAdjuster::adjust: globalIndex " +
                std::to_string(cp.globalIndex) +
                " is out of range (series size = " +
                std::to_string(n) + ").");
        }
    }

    // Sort change points by globalIndex ascending.
    std::vector<ChangePoint> sorted = changePoints;
    std::sort(sorted.begin(), sorted.end(),
              [](const ChangePoint& a, const ChangePoint& b) {
                  return a.globalIndex < b.globalIndex;
              });

    // Build output series with updated metadata.
    SeriesMetadata meta   = original.metadata();
    meta.componentName   += "_adj";

    TimeSeries result;
    result.setMetadata(std::move(meta));
    result.reserve(n);

    const std::size_t numCp = sorted.size();

    // Prefix shift: correction for segment j = cumulative sum of shifts at CP[0..j-1].
    // Reference segment is the FIRST segment -- no correction applied to it.
    //
    //   prefixShift[0] = 0                            (segment 0, reference)
    //   prefixShift[1] = sorted[0].shift              (segment 1)
    //   prefixShift[j] = sum of sorted[0..j-1].shift  (segment j)
    //
    // Subtracting prefixShift[j] from each value in segment j pulls it toward
    // the mean level of segment 0.
    std::vector<double> prefixShift(numCp + 1, 0.0);
    for (std::size_t j = 0; j < numCp; ++j) {
        prefixShift[j + 1] = prefixShift[j] + sorted[j].shift;
    }

    // Forward pass: assign correction per observation.
    // segmentIdx = how many CPs have globalIndex <= i (i.e. current segment index).
    std::size_t segmentIdx = 0;

    for (std::size_t i = 0; i < n; ++i) {
        while (segmentIdx < numCp && sorted[segmentIdx].globalIndex <= i) {
            ++segmentIdx;
        }

        const double adjustedVal = original[i].value - prefixShift[segmentIdx];
        result.append(original[i].time, adjustedVal, original[i].flag);
    }

    LOKI_INFO("SeriesAdjuster: applied " + std::to_string(numCp) +
              " shift(s) to series '" + original.metadata().componentName +
              "'. Reference = first segment.");

    return result;
}
