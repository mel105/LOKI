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

    // Sort change points by globalIndex ascending (copy, do not mutate caller's vector).
    std::vector<ChangePoint> sorted = changePoints;
    std::sort(sorted.begin(), sorted.end(),
              [](const ChangePoint& a, const ChangePoint& b) {
                  return a.globalIndex < b.globalIndex;
              });

    // Build the output series with adjusted metadata.
    SeriesMetadata meta        = original.metadata();
    meta.componentName        += "_adj";

    TimeSeries result;
    result.setMetadata(std::move(meta));
    result.reserve(n);

    // For each observation, compute the cumulative correction:
    // correction = sum of shifts for all change points whose globalIndex > i.
    // Since sorted is ascending, all change points with index > i are a suffix
    // of the sorted vector. We iterate from right to left once to build a
    // prefix-sum-from-right, then apply it in a single forward pass.

    // suffixShift[j] = sum of sorted[j].shift + sorted[j+1].shift + ... + sorted[k-1].shift
    // where k = sorted.size(). This is the correction for points in segment j
    // (i.e. points in [sorted[j-1].globalIndex, sorted[j].globalIndex)).
    // Points in segment 0 (before the first change point) get the full sum.

    const std::size_t numCp = sorted.size();

    // totalShift[j] = sum of shifts from change point j onwards (rightward).
    // totalShift[numCp] = 0 (reference segment, no correction).
    std::vector<double> totalShift(numCp + 1, 0.0);
    for (std::size_t j = numCp; j > 0; --j) {
        totalShift[j - 1] = totalShift[j] + sorted[j - 1].shift;
    }

    // Forward pass: assign correction for each observation.
    // segmentIdx is the index into sorted[] of the change point immediately
    // to the right of the current observation — i.e. the correction comes
    // from totalShift[segmentIdx].
    std::size_t segmentIdx = 0;

    for (std::size_t i = 0; i < n; ++i) {
        // Advance segmentIdx past all change points whose globalIndex <= i.
        // After this, segmentIdx points to the first change point with index > i,
        // which is exactly the right boundary of the current segment.
        while (segmentIdx < numCp && sorted[segmentIdx].globalIndex <= i) {
            ++segmentIdx;
        }

        const double correction  = totalShift[segmentIdx];
        const double adjustedVal = original[i].value - correction;

        result.append(original[i].time, adjustedVal, original[i].flag);
    }

    if (!sorted.empty()) {
        LOKI_INFO("SeriesAdjuster: applied " + std::to_string(numCp) +
                  " shift(s) to series '" + original.metadata().componentName + "'.");
    }

    return result;
}
