#pragma once

#include <loki/timeseries/timeSeries.hpp>
#include <loki/timeseries/timeStamp.hpp>

#include <cstddef>
#include <vector>

namespace loki {

/**
 * @brief Computes and stores a median annual profile from a TimeSeries.
 *
 * The profile is a flat vector of length 366 * slotsPerDay, where each slot
 * holds the median of all valid values observed at that position (day-of-year
 * + time-of-day bucket) across all years in the series.
 *
 * Slots with fewer than Config::minYears valid observations are left as NaN.
 *
 * Resolution is derived automatically from the median sampling step of the
 * input series. Resolutions finer than Config::maxResolutionDays (default
 * 1 hour) are rejected with ConfigException.
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
     * @param series Input series. Must be sorted and have at least 2 observations.
     * @param cfg    Configuration.
     * @throws ConfigException  if the estimated resolution is finer than maxResolutionDays.
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
     * @param ts Timestamp of the observation.
     * @return Median value for the corresponding annual slot, or NaN if under-populated.
     */
    [[nodiscard]]
    double valueAt(const TimeStamp& ts) const noexcept;

    /**
     * @brief Returns the total number of slots in the profile (366 * slotsPerDay).
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
     */
    [[nodiscard]]
    int slotsPerDay() const noexcept;

private:

    std::vector<double> m_profile;
    int                 m_slotsPerDay;
    double              m_stepDays;
    Config              m_cfg;

    [[nodiscard]]
    static double estimateStep(const TimeSeries& series);

    void computeProfile(const TimeSeries& series);

    [[nodiscard]]
    std::size_t slotIndex(const TimeStamp& ts) const noexcept;
};

} // namespace loki
