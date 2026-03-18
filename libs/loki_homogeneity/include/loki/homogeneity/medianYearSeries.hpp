#pragma once

#include "loki/timeseries/timeSeries.hpp"
#include "loki/timeseries/timeStamp.hpp"

#include <cstddef>
#include <vector>

namespace loki::homogeneity {

/**
 * @brief Computes and stores a median annual profile from a TimeSeries.
 *
 * The profile is a flat vector of length 366 * slotsPerDay, where each slot
 * holds the median of all valid values observed at that position (day-of-year
 * + time-of-day bucket) across all years in the series.
 *
 * Slots with fewer than Config::minYears valid observations are left as NaN
 * and logged as warnings. This allows the profile to be used even when data
 * coverage is uneven across years.
 *
 * Resolution is derived automatically from the median sampling step of the
 * input series. Resolutions finer than Config::maxResolutionDays (default
 * 1 hour) are rejected with ConfigException, as the median year concept loses
 * statistical meaning at sub-hourly scales.
 *
 * Intended use: construct once from the full series, then call valueAt() for
 * each missing epoch during gap filling.
 *
 * @note This class belongs to loki_homogeneity, not loki_core, because the
 *       median year model is specific to the homogeneity analysis pipeline.
 */
class MedianYearSeries {
public:

    // -------------------------------------------------------------------------
    //  Config
    // -------------------------------------------------------------------------

    /**
     * @brief Configuration for MedianYearSeries.
     */
    struct Config {
        int    minYears;          ///< Minimum valid values per slot to compute median.
        double maxResolutionDays; ///< Finest allowed step (default = 1/24 day = 1 hour).

        Config()
            : minYears(5)
            , maxResolutionDays(1.0 / 24.0)
        {}
    };

    // -------------------------------------------------------------------------
    //  Construction
    // -------------------------------------------------------------------------

    /**
     * @brief Constructs and computes the median annual profile from a series.
     *
     * The sampling step is estimated as the median of consecutive time
     * differences. If the series spans fewer than Config::minYears years,
     * a warning is logged but computation continues with available data.
     *
     * @param series Input series. Must be sorted and have at least 2 observations.
     * @param cfg    Configuration (defaults suitable for daily/hourly climate data).
     * @throws ConfigException  if the estimated resolution is finer than maxResolutionDays.
     * @throws ConfigException  if the series has no calendar-meaningful timestamp
     *                          (i.e. all MJD values are identical or non-monotonic).
     * @throws DataException    if the series has fewer than 2 observations.
     * @throws AlgorithmException if the series is not sorted.
     */
    explicit MedianYearSeries(const TimeSeries& series, Config cfg = Config{});

    // -------------------------------------------------------------------------
    //  Query interface
    // -------------------------------------------------------------------------

    /**
     * @brief Returns the median profile value for the slot matching a timestamp.
     *
     * Maps the timestamp to a (DOY, slot-of-day) index and returns the
     * precomputed median. If the slot has fewer than minYears valid values,
     * NaN is returned.
     *
     * @param ts Timestamp of the missing observation.
     * @return Median value for the corresponding annual slot, or NaN.
     */
    [[nodiscard]]
    double valueAt(const TimeStamp& ts) const noexcept;

    /**
     * @brief Returns the total number of slots in the profile.
     *
     * Equal to 366 * slotsPerDay.
     */
    [[nodiscard]]
    std::size_t profileSize() const noexcept;

    /**
     * @brief Returns the estimated sampling step in days.
     */
    [[nodiscard]]
    double stepDays() const noexcept;

    /**
     * @brief Returns the number of time slots per day.
     *
     * For daily data: 1. For 6-hourly: 4. For hourly: 24.
     */
    [[nodiscard]]
    int slotsPerDay() const noexcept;

private:

    std::vector<double> m_profile;    ///< Precomputed medians, length = 366 * m_slotsPerDay.
    int                 m_slotsPerDay;
    double              m_stepDays;
    Config              m_cfg;

    // -------------------------------------------------------------------------
    //  Private helpers
    // -------------------------------------------------------------------------

    /**
     * @brief Estimates the sampling step as the median of consecutive MJD diffs.
     * @param series Sorted series with at least 2 observations.
     * @return Step in days.
     * @throws ConfigException if all consecutive differences are zero.
     */
    [[nodiscard]]
    static double estimateStep(const TimeSeries& series);

    /**
     * @brief Fills m_profile from the series data.
     *
     * For each observation, maps it to a slot index and accumulates the value.
     * After all observations are processed, computes the median per slot.
     * Slots with count < minYears are set to NaN.
     */
    void computeProfile(const TimeSeries& series);

    /**
     * @brief Maps a timestamp to its slot index in the profile vector.
     *
     * Slot = (doy - 1) * m_slotsPerDay + slotOfDay
     * where doy in [1, 366] and slotOfDay in [0, m_slotsPerDay).
     *
     * @param ts Timestamp to map.
     * @return Slot index in [0, 366 * m_slotsPerDay).
     */
    [[nodiscard]]
    std::size_t slotIndex(const TimeStamp& ts) const noexcept;
};

} // namespace loki::homogeneity
