#include <loki/homogeneity/multiChangePointDetector.hpp>
#include <loki/homogeneity/changePointResult.hpp>
#include <loki/core/exceptions.hpp>

#include <algorithm>
#include <stdexcept>

using namespace loki;
using namespace loki::homogeneity;

// ----------------------------------------------------------------------------
// Construction
// ----------------------------------------------------------------------------

MultiChangePointDetector::MultiChangePointDetector(Config cfg)
    : m_cfg{std::move(cfg)}
{
}

// ----------------------------------------------------------------------------
// Public interface
// ----------------------------------------------------------------------------

std::vector<ChangePoint> MultiChangePointDetector::detect(
        const std::vector<double>& z,
        const std::vector<double>& times) const
{
    if (!times.empty() && times.size() != z.size()) {
        throw DataException(
            "MultiChangePointDetector::detect: times.size() != z.size()");
    }

    // PELT and BOCPD find all change points in a single pass -- no recursion.
    if (m_cfg.method == "pelt") {
        PeltDetector det(m_cfg.peltConfig);
        return det.detectAll(z, times);
    }
    if (m_cfg.method == "bocpd") {
        BocpdDetector det(m_cfg.bocpdConfig);
        return det.detectAll(z, times);
    }

    // yao_davis and snht use recursive binary splitting via split().
    std::vector<ChangePoint> result;
    result.reserve(16);

    split(z, times, 0, z.size(), result);

    std::sort(result.begin(), result.end(),
              [](const ChangePoint& a, const ChangePoint& b) {
                  return a.globalIndex < b.globalIndex;
              });

    return result;
}

// ----------------------------------------------------------------------------
// Private recursive implementation
// ----------------------------------------------------------------------------

void MultiChangePointDetector::split(
        const std::vector<double>& z,
        const std::vector<double>& times,
        std::size_t begin,
        std::size_t end,
        std::vector<ChangePoint>& result) const
{
    const std::size_t segLen = end - begin;

    // --- Guard 1: segment too short (number of points) ---
    if (segLen < m_cfg.minSegmentPoints) {
        return;
    }

    // --- Guard 2: segment too short (time span) ---
    if (!times.empty() && m_cfg.minSegmentSeconds > 0.0) {
        // times are MJD (days); convert threshold from seconds to days
        static constexpr double SECONDS_PER_DAY = 86400.0;
        const double spanDays =
            (times[end - 1] - times[begin]) * SECONDS_PER_DAY;
        if (spanDays < m_cfg.minSegmentSeconds) {
            return;
        }
    }

    // --- Run single-segment detector on [begin, end) ---
    // ChangePointDetector detector(m_cfg.detectorConfig);
    // const ChangePointResult cp = detector.detect(z, begin, end);
    ChangePointResult cp;
    if (m_cfg.method == "snht") {
        SnhtDetector detector(m_cfg.snhtConfig);
        cp = detector.detect(z, begin, end);
    } else {
        ChangePointDetector detector(m_cfg.detectorConfig);
        cp = detector.detect(z, begin, end);
    }

    if (!cp.detected) {
        return; // segment is stationary -- stop recursion here
    }

    // cp.index is relative to the sub-series [begin, end); convert to global
    const std::size_t globalIdx = begin + static_cast<std::size_t>(cp.index);

    const double mjd = times.empty() ? 0.0 : times[globalIdx];

    result.push_back(ChangePoint{
        globalIdx,
        mjd,
        cp.shift,
        cp.pValue
    });

    // --- Recurse on both halves ---
    split(z, times, begin,     globalIdx, result); // left  [begin, globalIdx)
    split(z, times, globalIdx, end,       result); // right [globalIdx, end)
}
