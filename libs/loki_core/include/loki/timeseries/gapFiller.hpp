#pragma once

#include "loki/timeseries/timeSeries.hpp"

#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

namespace loki {

// -----------------------------------------------------------------------------
//  GapInfo
// -----------------------------------------------------------------------------

/**
 * @brief Describes a single gap (run of missing epochs) found in a TimeSeries.
 *
 * A gap is either a NaN observation that is still present in the series, or
 * a stretch of entirely absent epochs detected via a jump in the time axis that
 * exceeds the expected sampling step.
 *
 * startMjd / endMjd are nullopt when the series carries no calendar timestamp
 * (index-only series).
 */
struct GapInfo {
    std::size_t             startIndex; ///< Index of first missing epoch (0-based).
    std::size_t             endIndex;   ///< Index of last missing epoch (inclusive).
    std::size_t             count;      ///< Number of missing epochs in this gap.
    std::optional<double>   startMjd;   ///< MJD of first missing epoch, or nullopt.
    std::optional<double>   endMjd;     ///< MJD of last missing epoch, or nullopt.
};

// -----------------------------------------------------------------------------
//  GapFiller
// -----------------------------------------------------------------------------

/**
 * @brief Detects and fills gaps in a TimeSeries.
 *
 * A gap is defined as either:
 *   - an observation whose value is NaN, or
 *   - a jump in the time axis greater than expectedStep * gapThresholdFactor,
 *     indicating that entire epochs are absent from the data.
 *
 * The expected sampling step is estimated as the median of consecutive
 * time differences, making it robust to a small number of outlier jumps.
 *
 * Supported fill strategies:
 *   - LINEAR        Linearly interpolates between the two known neighbours.
 *                   Leading / trailing gaps are filled by bfill / ffill.
 *   - FORWARD_FILL  Repeats the last known value forward. Leading gaps use bfill.
 *   - MEAN          Replaces every missing value with the global series mean.
 *   - MEDIAN_YEAR   Looks up the median annual profile value for each missing epoch.
 *                   Requires a ProfileLookup function supplied to fill(). If called
 *                   via the single-argument fill() without a lookup, throws ConfigException.
 *   - NONE          Detection only; the series is returned unchanged.
 *
 * GapFiller does not depend on MedianYearSeries directly. The caller constructs
 * the profile and passes it as a std::function<double(const TimeStamp&)>. This
 * keeps loki_core free of any dependency on loki_homogeneity.
 *
 * If maxFillLength > 0, gaps longer than that limit are left as NaN and a
 * warning is logged. This limit does not apply to leading / trailing gaps.
 */
class GapFiller {
public:

    // -------------------------------------------------------------------------
    //  Types
    // -------------------------------------------------------------------------

    /**
     * @brief Lookup function that maps a timestamp to a median profile value.
     *
     * Used with Strategy::MEDIAN_YEAR. Typically a lambda wrapping
     * MedianYearSeries::valueAt(). Returns NaN if the slot is under-populated.
     */
    using ProfileLookup = std::function<double(const TimeStamp&)>;

    // -------------------------------------------------------------------------
    //  Strategy
    // -------------------------------------------------------------------------

    /**
     * @brief Available gap-filling strategies.
     */
    enum class Strategy {
        LINEAR,       ///< Linear interpolation (bfill/ffill at edges).
        FORWARD_FILL, ///< Propagate last known value forward (bfill at leading edge).
        MEAN,         ///< Replace with global series mean.
        MEDIAN_YEAR,  ///< Median annual profile lookup (ProfileLookup required).
        SPLINE,       ///< Replace with cubic spline model.
        NONE          ///< Detection only; do not modify values.
    };

    // -------------------------------------------------------------------------
    //  Config
    // -------------------------------------------------------------------------

    /**
     * @brief Configuration for GapFiller.
     */
    struct Config {
        Strategy    strategy;
        double      gapThresholdFactor; ///< Multiplier on expectedStep for jump detection.
        std::size_t maxFillLength;      ///< Max gap length to fill; 0 = unlimited.
        std::size_t minSeriesYears;     ///< Minimum series span required for MEDIAN_YEAR.

        Config()
            : strategy(Strategy::LINEAR)
            , gapThresholdFactor(1.5)
            , maxFillLength(0)
            , minSeriesYears(10)
        {}
    };

    // -------------------------------------------------------------------------
    //  Construction
    // -------------------------------------------------------------------------

    /**
     * @brief Constructs a GapFiller with the given configuration.
     * @param cfg Configuration. Defaults are suitable for daily climatological data.
     */
    explicit GapFiller(Config cfg = Config{});

    // -------------------------------------------------------------------------
    //  Public interface
    // -------------------------------------------------------------------------

    /**
     * @brief Detects all gaps in the series and returns their descriptions.
     *
     * Does not modify the series. Logs the number of gaps found and the
     * total percentage of missing epochs.
     *
     * @param series Input series. Must be sorted; throws AlgorithmException otherwise.
     * @return Vector of GapInfo structs, one per contiguous gap, in index order.
     * @throws AlgorithmException if the series is not sorted.
     * @throws DataException if the series has fewer than 2 observations.
     */
    [[nodiscard]]
    std::vector<GapInfo> detectGaps(const TimeSeries& series) const;

    /**
     * @brief Fills gaps using LINEAR, FORWARD_FILL, MEAN, or NONE strategy.
     *
     * @throws ConfigException if strategy is MEDIAN_YEAR (use the overload with ProfileLookup).
     * @throws AlgorithmException if the series is not sorted.
     * @throws DataException if the series has fewer than 2 observations.
     * @throws DataException if all values are NaN.
     */
    [[nodiscard]]
    TimeSeries fill(const TimeSeries& series) const;

    /**
     * @brief Fills gaps using any strategy, including MEDIAN_YEAR.
     *
     * The ProfileLookup is called for each missing epoch when strategy is
     * MEDIAN_YEAR. For all other strategies the lookup is ignored. If the
     * lookup returns NaN for a slot (under-populated profile), that epoch
     * is left as NaN and a warning is logged.
     *
     * Typical usage:
     * @code
     *   MedianYearSeries mys(series);
     *   GapFiller gf(cfg);
     *   auto filled = gf.fill(series, [&mys](const TimeStamp& ts) {
     *       return mys.valueAt(ts);
     *   });
     * @endcode
     *
     * @param series        Input series. Must be sorted.
     * @param profileLookup Function mapping a timestamp to its profile value.
     * @throws AlgorithmException if the series is not sorted.
     * @throws DataException if the series has fewer than 2 observations.
     * @throws DataException if all values are NaN and strategy is not MEDIAN_YEAR.
     */
    [[nodiscard]]
    TimeSeries fill(const TimeSeries& series,
                    const ProfileLookup& profileLookup) const;

private:

    Config m_cfg;

    // -------------------------------------------------------------------------
    //  Private helpers
    // -------------------------------------------------------------------------

    /**
     * @brief Estimates the expected sampling step as the median of consecutive diffs.
     * @param series Sorted series with at least 2 observations.
     * @return Median MJD difference between consecutive observations.
     */
    [[nodiscard]]
    double estimateExpectedStep(const TimeSeries& series) const;

    /**
     * @brief Detects gaps in an already-validated, sorted series.
     *
     * Internal implementation called after precondition checks.
     */
    [[nodiscard]]
    std::vector<GapInfo> detectGapsInternal(const TimeSeries& series,
                                            double            expectedStep) const;

    [[nodiscard]]
    TimeSeries fillLinear(const TimeSeries&          series,
                          const std::vector<GapInfo>& gaps) const;

    [[nodiscard]]
    TimeSeries fillSpline(const TimeSeries& series,
                          const std::vector<GapInfo>& gaps) const;

    [[nodiscard]]
    TimeSeries fillForwardFill(const TimeSeries&          series,
                               const std::vector<GapInfo>& gaps) const;

    [[nodiscard]]
    TimeSeries fillMean(const TimeSeries&          series,
                        const std::vector<GapInfo>& gaps) const;

    [[nodiscard]]
    TimeSeries fillMedianYear(const TimeSeries&          series,
                              const std::vector<GapInfo>& gaps,
                              const ProfileLookup&        lookup) const;
};

} // namespace loki
