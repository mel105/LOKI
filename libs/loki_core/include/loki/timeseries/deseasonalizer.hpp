#pragma once

#include <loki/core/exceptions.hpp>
#include <loki/timeseries/timeSeries.hpp>
#include <loki/timeseries/timeStamp.hpp>

#include <functional>
#include <string>
#include <vector>

namespace loki {

/**
 * @brief Removes the seasonal component from a time series.
 *
 * Supports three strategies:
 *  - MOVING_AVERAGE : subtracts a centered simple moving average.
 *  - MEDIAN_YEAR    : subtracts the median annual profile obtained from
 *                     a MedianYearSeries lookup. Requires a time axis with
 *                     calendar meaning (UTC / GPST / MJD). Throws
 *                     ConfigException if the series step is < 1 hour.
 *  - NONE           : no-op; residuals equal the original values.
 *
 * The caller supplies the profile lookup for MEDIAN_YEAR via a
 * std::function<double(const TimeStamp&)> (typically a lambda wrapping
 * MedianYearSeries::valueAt).
 */
class Deseasonalizer {
public:

    // ------------------------------------------------------------------
    // Types
    // ------------------------------------------------------------------

    /// Deseasonalization strategy.
    enum class Strategy {
        MOVING_AVERAGE,
        MEDIAN_YEAR,
        NONE
    };

    /// Configuration for Deseasonalizer.
    struct Config {
        Strategy strategy;

        /// Window size for MOVING_AVERAGE (must be odd >= 3; even values
        /// are rounded up automatically). Default 365 (daily resolution).
        int maWindowSize;

        /// @brief Constructs Config with default values.
        Config() : strategy{Strategy::MOVING_AVERAGE}, maWindowSize{365} {}

        /// @brief Constructs Config with explicit values.
        Config(Strategy s, int win) : strategy{s}, maWindowSize{win} {}
    };

    /// Result returned by deseasonalize().
    struct Result {
        /// Residuals: original values minus the estimated seasonal component.
        std::vector<double> residuals;

        /// Estimated seasonal component (same length as residuals).
        /// All zeros for Strategy::NONE.
        std::vector<double> seasonal;

        /// Residuals as a TimeSeries with the same time axis as the input.
        /// SeriesMetadata::componentName receives the suffix "_deseas".
        TimeSeries series;
    };

    // ------------------------------------------------------------------
    // Construction
    // ------------------------------------------------------------------

    /**
     * @brief Constructs a Deseasonalizer with the given configuration.
     * @param cfg Configuration. Defaults to MOVING_AVERAGE, window 365.
     */
    explicit Deseasonalizer(Config cfg = Config{});

    // ------------------------------------------------------------------
    // Interface
    // ------------------------------------------------------------------

    /**
     * @brief Removes seasonal component from the series.
     *
     * For Strategy::MOVING_AVERAGE and Strategy::NONE the profileLookup
     * argument is ignored and may be left as nullptr (default).
     *
     * For Strategy::MEDIAN_YEAR profileLookup must be provided.
     * The series step must be >= 1 hour; finer resolution throws
     * ConfigException.
     *
     * @param series        Input time series.
     * @param profileLookup Callable (TimeStamp -> double) providing the
     *                      seasonal value for a given timestamp. Required
     *                      for MEDIAN_YEAR; ignored otherwise.
     * @return Result containing residuals, seasonal component, and a
     *         TimeSeries with name suffix "_deseas".
     * @throws ConfigException if the strategy/series combination is invalid.
     * @throws DataException   if the series contains NaN values or is empty.
     */
    [[nodiscard]]
    Result deseasonalize(
        const TimeSeries& series,
        std::function<double(const ::TimeStamp&)> profileLookup = nullptr) const;

private:

    Config m_cfg;

    [[nodiscard]]
    Result applyMovingAverage(const TimeSeries& series) const;

    [[nodiscard]]
    Result applyMedianYear(
        const TimeSeries& series,
        const std::function<double(const ::TimeStamp&)>& profileLookup) const;

    [[nodiscard]]
    Result applyNone(const TimeSeries& series) const;

    /// Validates that the series step is >= 1 hour (for MEDIAN_YEAR).
    /// @throws ConfigException on violation.
    static void checkMinResolution(const TimeSeries& series);
};

} // namespace loki
