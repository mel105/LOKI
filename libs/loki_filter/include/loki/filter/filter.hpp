#pragma once

#include <loki/filter/filterResult.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <string>

namespace loki {

/**
 * @brief Abstract base class for all time series filters.
 *
 * Each concrete filter implements apply() and name().
 * Configuration is handled by a nested Config struct in each subclass.
 *
 * Usage example:
 * @code
 *   MovingAverageFilter f{MovingAverageFilter::Config{.window = 365}};
 *   FilterResult result = f.apply(series);
 * @endcode
 */
class Filter {
public:
    virtual ~Filter() = default;

    /**
     * @brief Apply the filter to a time series.
     *
     * @param series  Input time series. Must not be empty.
     * @return        FilterResult with filtered series, residuals, and filter name.
     * @throws DataException if the series is too short for the filter configuration.
     */
    virtual FilterResult apply(const TimeSeries& series) const = 0;

    /**
     * @brief Return the human-readable name of this filter.
     *
     * Used in log messages and plot output file naming.
     */
    virtual std::string name() const = 0;
};

} // namespace loki
