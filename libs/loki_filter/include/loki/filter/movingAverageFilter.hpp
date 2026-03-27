#pragma once

#include <loki/filter/filter.hpp>

namespace loki {

/**
 * @brief Centered simple moving average filter.
 *
 * Wraps loki::stats::movingAverage(). Edge positions produced as NaN by the
 * underlying function are filled using nearest-neighbour extrapolation:
 * forward-fill from the left edge, backward-fill from the right edge.
 *
 * The input series must be free of NaN values. Use GapFiller in the
 * pipeline before applying this filter.
 */
class MovingAverageFilter : public Filter {
public:
    /**
     * @brief Configuration for MovingAverageFilter.
     */
    struct Config {
        int window{3}; ///< Window size in samples. Must be odd >= 3; even values rounded up.
    };

    /**
     * @brief Construct with configuration.
     * @param cfg Filter configuration.
     */
    explicit MovingAverageFilter(Config cfg);

    /**
     * @brief Apply centered moving average to the series.
     * @param series Input time series. Must not contain NaN.
     * @return FilterResult with filtered series, residuals, and filter name.
     * @throws DataException   if series contains NaN.
     * @throws ConfigException if window is invalid.
     */
    FilterResult apply(const TimeSeries& series) const override;

    /** @brief Returns "MovingAverage". */
    std::string name() const override;

private:
    Config m_cfg;
};

} // namespace loki
