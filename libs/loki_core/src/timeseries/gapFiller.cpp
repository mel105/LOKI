#include "loki/timeseries/gapFiller.hpp"

#include "loki/core/exceptions.hpp"
#include "loki/core/logger.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>

using namespace loki;

// =============================================================================
//  Construction
// =============================================================================

GapFiller::GapFiller(Config cfg)
    : m_cfg(cfg)
{
}

// =============================================================================
//  Public interface
// =============================================================================

std::vector<GapInfo> GapFiller::detectGaps(const TimeSeries& series) const
{
    if (!series.isSorted()) {
        throw AlgorithmException(
            "GapFiller::detectGaps: series is not sorted. Call sortByTime() first.");
    }
    if (series.size() < 2) {
        throw DataException(
            "GapFiller::detectGaps: series must have at least 2 observations.");
    }

    const double step = estimateExpectedStep(series);
    auto gaps = detectGapsInternal(series, step);

    // Log summary.
    if (gaps.empty()) {
        LOKI_INFO("GapFiller: no gaps detected in series (n=" +
                 std::to_string(series.size()) + ").");
    } else {
        std::size_t totalMissing = 0;
        for (const auto& g : gaps) {
            totalMissing += g.count;
        }
        const double pct = 100.0 * static_cast<double>(totalMissing) /
                           static_cast<double>(series.size() + totalMissing);
        std::ostringstream oss;
        oss << "GapFiller: detected " << gaps.size() << " gap(s), "
            << totalMissing << " missing epoch(s) ("
            << std::fixed;
        oss.precision(2);
        oss << pct << "%) in series (n=" << series.size() << ").";
        LOKI_INFO(oss.str());
    }

    return gaps;
}

// -----------------------------------------------------------------------------

TimeSeries GapFiller::fill(const TimeSeries& series) const
{
    if (m_cfg.strategy == Strategy::MEDIAN_YEAR) {
        throw ConfigException(
            "GapFiller::fill: MEDIAN_YEAR requires a ProfileLookup. "
            "Use fill(series, profileLookup) instead.");
    }
    if (!series.isSorted()) {
        throw AlgorithmException(
            "GapFiller::fill: series is not sorted. Call sortByTime() first.");
    }
    if (series.size() < 2) {
        throw DataException(
            "GapFiller::fill: series must have at least 2 observations.");
    }

    if (m_cfg.strategy == Strategy::NONE) {
        LOKI_INFO("GapFiller: strategy is NONE, returning series unchanged.");
        return series;
    }

    const double step = estimateExpectedStep(series);
    auto gaps = detectGapsInternal(series, step);

    if (gaps.empty()) {
        LOKI_INFO("GapFiller: no gaps found, returning series unchanged.");
        return series;
    }

    // Verify that at least one valid observation exists.
    bool hasValid = false;
    for (std::size_t i = 0; i < series.size(); ++i) {
        if (isValid(series[i])) { hasValid = true; break; }
    }
    if (!hasValid) {
        throw DataException(
            "GapFiller::fill: all values are NaN -- cannot fill without a known value.");
    }

    switch (m_cfg.strategy) {
        case Strategy::LINEAR:       return fillLinear(series, gaps);
        case Strategy::FORWARD_FILL: return fillForwardFill(series, gaps);
        case Strategy::MEAN:         return fillMean(series, gaps);
        default:
            throw AlgorithmException("GapFiller::fill: unhandled strategy.");
    }
}

// -----------------------------------------------------------------------------

TimeSeries GapFiller::fill(const TimeSeries& series,
                            const ProfileLookup& profileLookup) const
{
    if (!series.isSorted()) {
        throw AlgorithmException(
            "GapFiller::fill: series is not sorted. Call sortByTime() first.");
    }
    if (series.size() < 2) {
        throw DataException(
            "GapFiller::fill: series must have at least 2 observations.");
    }

    if (m_cfg.strategy == Strategy::NONE) {
        LOKI_INFO("GapFiller: strategy is NONE, returning series unchanged.");
        return series;
    }

    const double step = estimateExpectedStep(series);
    auto gaps = detectGapsInternal(series, step);

    if (gaps.empty()) {
        LOKI_INFO("GapFiller: no gaps found, returning series unchanged.");
        return series;
    }

    // For non-MEDIAN_YEAR strategies, delegate to the single-arg helpers
    // (they don't use the lookup). All-NaN check only needed for those.
    if (m_cfg.strategy != Strategy::MEDIAN_YEAR) {
        bool hasValid = false;
        for (std::size_t i = 0; i < series.size(); ++i) {
            if (isValid(series[i])) { hasValid = true; break; }
        }
        if (!hasValid) {
            throw DataException(
                "GapFiller::fill: all values are NaN -- cannot fill without a known value.");
        }
        switch (m_cfg.strategy) {
            case Strategy::LINEAR:       return fillLinear(series, gaps);
            case Strategy::FORWARD_FILL: return fillForwardFill(series, gaps);
            case Strategy::MEAN:         return fillMean(series, gaps);
            default:
                throw AlgorithmException("GapFiller::fill: unhandled strategy.");
        }
    }

    return fillMedianYear(series, gaps, profileLookup);
}

// =============================================================================
//  Private helpers
// =============================================================================

double GapFiller::estimateExpectedStep(const TimeSeries& series) const
{
    // Collect all consecutive MJD differences.
    std::vector<double> diffs;
    diffs.reserve(series.size() - 1);
    for (std::size_t i = 1; i < series.size(); ++i) {
        const double d = series[i].time.mjd() - series[i - 1].time.mjd();
        if (d > 0.0) {
            diffs.push_back(d);
        }
    }
    if (diffs.empty()) {
        throw DataException(
            "GapFiller::estimateExpectedStep: all timestamps are identical.");
    }

    // Median of differences — robust against a few large jumps.
    const std::size_t mid = diffs.size() / 2;
    std::nth_element(diffs.begin(), diffs.begin() + static_cast<std::ptrdiff_t>(mid), diffs.end());
    double median = diffs[mid];
    if (diffs.size() % 2 == 0 && mid > 0) {
        // Average the two middle elements for even-length vectors.
        const auto it = std::max_element(diffs.begin(), diffs.begin() + static_cast<std::ptrdiff_t>(mid));
        median = (median + *it) / 2.0;
    }
    return median;
}

// -----------------------------------------------------------------------------

std::vector<GapInfo> GapFiller::detectGapsInternal(const TimeSeries& series,
                                                    double            expectedStep) const
{
    std::vector<GapInfo> gaps;
    const double threshold = expectedStep * m_cfg.gapThresholdFactor;

    // Helper: does this observation count as NaN?
    auto isMissing = [](const Observation& obs) {
        return !isValid(obs);
    };

    std::size_t i = 0;
    while (i < series.size()) {
        // Case 1: NaN value in the series.
        if (isMissing(series[i])) {
            std::size_t j = i;
            while (j < series.size() && isMissing(series[j])) { ++j; }
            // Gap from index i to j-1 (inclusive).
            GapInfo g;
            g.startIndex = i;
            g.endIndex   = j - 1;
            g.count      = j - i;
            g.startMjd   = series[i].time.mjd();
            g.endMjd     = series[j - 1].time.mjd();
            gaps.push_back(g);
            i = j;
            continue;
        }

        // Case 2: time jump between obs[i] and obs[i+1].
        if (i + 1 < series.size()) {
            const double jump = series[i + 1].time.mjd() - series[i].time.mjd();
            if (jump > threshold) {
                // Estimate how many epochs are missing.
                const auto missing = static_cast<std::size_t>(
                    std::round(jump / expectedStep) - 1);
                if (missing > 0) {
                    // The gap sits between index i and i+1 in the stored series.
                    // We record it as starting at i+1 (would-be first missing slot)
                    // with count = missing. startIndex == endIndex + 1 signals
                    // that these are absent rows (not stored NaNs).
                    GapInfo g;
                    g.startIndex = i + 1;
                    g.endIndex   = i;           // sentinel: endIndex < startIndex -> absent rows
                    g.count      = missing;
                    g.startMjd   = series[i].time.mjd() + expectedStep;
                    g.endMjd     = series[i + 1].time.mjd() - expectedStep;
                    gaps.push_back(g);
                }
            }
        }
        ++i;
    }

    return gaps;
}

// =============================================================================
//  Fill strategies
// =============================================================================

// Helper: build a working copy as vector<Observation>, apply fill, pack into TimeSeries.
// For LINEAR and FORWARD_FILL we need to insert synthetic epochs for absent-row gaps.

namespace {

/**
 * @brief Expands the series into a dense vector, inserting NaN placeholders
 *        for absent-row gaps (those where endIndex < startIndex).
 */
std::vector<Observation> expandSeries(const TimeSeries&          series,
                                      const std::vector<GapInfo>& gaps,
                                      double                      expectedStep)
{
    std::vector<Observation> out;
    out.reserve(series.size() + gaps.size() * 4); // rough estimate

    // Map from original index -> whether a gap (absent rows) starts after it.
    // We process the series left-to-right and insert synthetic NaN epochs.
    std::size_t gapIdx = 0;
    for (std::size_t i = 0; i < series.size(); ++i) {
        out.push_back(series[i]);

        // After storing obs[i], check if there is an absent-row gap that
        // falls between index i and i+1 (sentinel: endIndex < startIndex).
        while (gapIdx < gaps.size() &&
               gaps[gapIdx].startIndex == i + 1 &&
               gaps[gapIdx].endIndex < gaps[gapIdx].startIndex) {
            const GapInfo& g = gaps[gapIdx];
            const double   mjdBase = series[i].time.mjd();
            for (std::size_t k = 1; k <= g.count; ++k) {
                const double mjd = mjdBase + static_cast<double>(k) * expectedStep;
                Observation syn;
                syn.time  = TimeStamp::fromMjd(mjd);
                syn.value = std::numeric_limits<double>::quiet_NaN();
                syn.flag  = 0;
                out.push_back(syn);
            }
            ++gapIdx;
        }
    }
    return out;
}

/**
 * @brief Packs a flat observation vector back into a TimeSeries,
 *        copying metadata from the original.
 */
TimeSeries packTimeSeries(std::vector<Observation> obs,
                          const TimeSeries&        original)
{
    TimeSeries result(original.metadata());
    result.reserve(obs.size());
    for (auto& o : obs) {
        result.append(o.time, o.value, o.flag);
    }
    return result;
}

/**
 * @brief Applies bfill to leading NaNs and ffill to trailing NaNs in place.
 *
 * Leading  NaN(s): replaced by the first valid value (backward fill).
 * Trailing NaN(s): replaced by the last valid value (forward fill).
 */
void fillEdges(std::vector<Observation>& obs)
{
    // Find first valid.
    std::size_t firstValid = obs.size();
    for (std::size_t i = 0; i < obs.size(); ++i) {
        if (isValid(obs[i])) { firstValid = i; break; }
    }
    if (firstValid == obs.size()) return; // all NaN — handled upstream

    // Bfill leading NaNs.
    for (std::size_t i = 0; i < firstValid; ++i) {
        obs[i].value = obs[firstValid].value;
    }

    // Ffill trailing NaNs.
    std::size_t lastValid = 0;
    for (std::size_t i = obs.size(); i-- > 0; ) {
        if (isValid(obs[i])) { lastValid = i; break; }
    }
    for (std::size_t i = lastValid + 1; i < obs.size(); ++i) {
        obs[i].value = obs[lastValid].value;
    }
}

} // anonymous namespace

// -----------------------------------------------------------------------------

TimeSeries GapFiller::fillLinear(const TimeSeries&          series,
                                  const std::vector<GapInfo>& gaps) const
{
    const double step = estimateExpectedStep(series);
    auto obs = expandSeries(series, gaps, step);

    // Fill edges first (bfill/ffill — cannot interpolate without two anchors).
    fillEdges(obs);

    std::size_t skipped = 0;

    // Walk through and interpolate interior NaN runs.
    std::size_t i = 0;
    while (i < obs.size()) {
        if (!isValid(obs[i])) {
            // Find the run [i, j).
            std::size_t j = i;
            while (j < obs.size() && !isValid(obs[j])) { ++j; }

            const std::size_t runLen = j - i;

            // Check maxFillLength (0 = unlimited; edges already handled).
            if (m_cfg.maxFillLength > 0 && runLen > m_cfg.maxFillLength) {
                LOKI_WARNING("GapFiller: gap of " + std::to_string(runLen) +
                         " epoch(s) at index " + std::to_string(i) +
                         " exceeds maxFillLength=" +
                         std::to_string(m_cfg.maxFillLength) + "; leaving as NaN.");
                ++skipped;
                i = j;
                continue;
            }

            // We need a valid value before (left) and after (right) the run.
            // fillEdges guarantees obs[0] and obs[n-1] are valid, so
            // i > 0 and j < obs.size() are guaranteed for interior runs.
            const double vLeft  = obs[i - 1].value;
            const double vRight = obs[j].value;
            const double mLeft  = obs[i - 1].time.mjd();
            const double mRight = obs[j].time.mjd();
            const double span   = mRight - mLeft;

            for (std::size_t k = i; k < j; ++k) {
                const double t = (obs[k].time.mjd() - mLeft) / span;
                obs[k].value = vLeft + t * (vRight - vLeft);
            }
            i = j;
        } else {
            ++i;
        }
    }

    if (skipped > 0) {
        LOKI_WARNING("GapFiller: " + std::to_string(skipped) +
                 " gap(s) left unfilled (too long).");
    }

    LOKI_INFO("GapFiller: LINEAR fill complete (" +
             std::to_string(obs.size() - series.size()) + " epoch(s) inserted).");

    return packTimeSeries(std::move(obs), series);
}

// -----------------------------------------------------------------------------

TimeSeries GapFiller::fillForwardFill(const TimeSeries&          series,
                                       const std::vector<GapInfo>& gaps) const
{
    const double step = estimateExpectedStep(series);
    auto obs = expandSeries(series, gaps, step);

    fillEdges(obs); // handles leading NaNs via bfill

    std::size_t skipped = 0;

    for (std::size_t i = 1; i < obs.size(); ++i) {
        if (!isValid(obs[i])) {
            // Measure the run length.
            std::size_t j = i;
            while (j < obs.size() && !isValid(obs[j])) { ++j; }
            const std::size_t runLen = j - i;

            if (m_cfg.maxFillLength > 0 && runLen > m_cfg.maxFillLength) {
                LOKI_WARNING("GapFiller: gap of " + std::to_string(runLen) +
                         " epoch(s) at index " + std::to_string(i) +
                         " exceeds maxFillLength=" +
                         std::to_string(m_cfg.maxFillLength) + "; leaving as NaN.");
                ++skipped;
                i = j - 1; // -1 because the outer loop increments
                continue;
            }

            // Forward-fill from the last valid observation (obs[i-1]).
            const double lastKnown = obs[i - 1].value;
            for (std::size_t k = i; k < j; ++k) {
                obs[k].value = lastKnown;
            }
            i = j - 1;
        }
    }

    if (skipped > 0) {
        LOKI_WARNING("GapFiller: " + std::to_string(skipped) +
                 " gap(s) left unfilled (too long).");
    }

    LOKI_INFO("GapFiller: FORWARD_FILL complete (" +
             std::to_string(obs.size() - series.size()) + " epoch(s) inserted).");

    return packTimeSeries(std::move(obs), series);
}

// -----------------------------------------------------------------------------

TimeSeries GapFiller::fillMean(const TimeSeries&          series,
                                const std::vector<GapInfo>& gaps) const
{
    // Compute mean of all valid values.
    double sum   = 0.0;
    std::size_t n = 0;
    for (std::size_t i = 0; i < series.size(); ++i) {
        if (isValid(series[i])) {
            sum += series[i].value;
            ++n;
        }
    }
    const double mean = sum / static_cast<double>(n);

    const double step = estimateExpectedStep(series);
    auto obs = expandSeries(series, gaps, step);

    std::size_t skipped = 0;

    for (std::size_t i = 0; i < obs.size(); ++i) {
        if (!isValid(obs[i])) {
            // Check run length against maxFillLength.
            std::size_t j = i;
            while (j < obs.size() && !isValid(obs[j])) { ++j; }
            const std::size_t runLen = j - i;

            if (m_cfg.maxFillLength > 0 && runLen > m_cfg.maxFillLength) {
                LOKI_WARNING("GapFiller: gap of " + std::to_string(runLen) +
                         " epoch(s) at index " + std::to_string(i) +
                         " exceeds maxFillLength=" +
                         std::to_string(m_cfg.maxFillLength) + "; leaving as NaN.");
                ++skipped;
                i = j - 1;
                continue;
            }

            for (std::size_t k = i; k < j; ++k) {
                obs[k].value = mean;
            }
            i = j - 1;
        }
    }

    if (skipped > 0) {
        LOKI_WARNING("GapFiller: " + std::to_string(skipped) +
                 " gap(s) left unfilled (too long).");
    }

    LOKI_INFO("GapFiller: MEAN fill complete (mean=" +
             std::to_string(mean) + ", " +
             std::to_string(obs.size() - series.size()) + " epoch(s) inserted).");

    return packTimeSeries(std::move(obs), series);
}

// -----------------------------------------------------------------------------

TimeSeries GapFiller::fillMedianYear(const TimeSeries&          series,
                                      const std::vector<GapInfo>& gaps,
                                      const ProfileLookup&        lookup) const
{
    const double step = estimateExpectedStep(series);
    auto obs = expandSeries(series, gaps, step);

    // Edges: bfill leading NaNs, ffill trailing NaNs.
    // For MEDIAN_YEAR we still handle edges the same way -- the profile value
    // is the correct fill for interior gaps, but edge epochs may be outside
    // the profile's reliable range (e.g. the very first/last observation).
    fillEdges(obs);

    std::size_t skipped    = 0;
    std::size_t nanProfile = 0;

    std::size_t i = 0;
    while (i < obs.size()) {
        if (!isValid(obs[i])) {
            std::size_t j = i;
            while (j < obs.size() && !isValid(obs[j])) { ++j; }
            const std::size_t runLen = j - i;

            if (m_cfg.maxFillLength > 0 && runLen > m_cfg.maxFillLength) {
                LOKI_WARNING("GapFiller: gap of " + std::to_string(runLen) +
                             " epoch(s) at index " + std::to_string(i) +
                             " exceeds maxFillLength=" +
                             std::to_string(m_cfg.maxFillLength) + "; leaving as NaN.");
                ++skipped;
                i = j;
                continue;
            }

            for (std::size_t k = i; k < j; ++k) {
                const double profileVal = lookup(obs[k].time);
                if (profileVal != profileVal) { // NaN check
                    // Profile slot under-populated -- leave as NaN.
                    ++nanProfile;
                } else {
                    obs[k].value = profileVal;
                }
            }
            i = j;
        } else {
            ++i;
        }
    }

    if (skipped > 0) {
        LOKI_WARNING("GapFiller: " + std::to_string(skipped) +
                     " gap(s) left unfilled (too long).");
    }
    if (nanProfile > 0) {
        LOKI_WARNING("GapFiller: " + std::to_string(nanProfile) +
                     " epoch(s) left as NaN (profile slot under-populated).");
    }

    LOKI_INFO("GapFiller: MEDIAN_YEAR fill complete (" +
              std::to_string(obs.size() - series.size()) + " epoch(s) inserted).");

    return packTimeSeries(std::move(obs), series);
}
