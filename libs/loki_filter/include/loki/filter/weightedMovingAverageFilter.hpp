#pragma once

#include <loki/filter/filter.hpp>

#include <vector>

namespace loki {

/**
 * @brief Centered weighted moving average filter with a user-supplied kernel.
 *
 * Wraps loki::stats::weightedMovingAverage(). Weights are normalised internally
 * by the underlying function. Common kernels: triangular {1,2,3,2,1},
 * Gaussian-like {1,4,6,4,1}, uniform {1,1,1,1,1}.
 *
 * Edge positions produced as NaN by the underlying function are filled using
 * nearest-neighbour extrapolation: forward-fill from the left edge,
 * backward-fill from the right edge.
 *
 * The input series must be free of NaN values. Use GapFiller in the
 * pipeline before applying this filter.
 */
class WeightedMovingAverageFilter : public Filter {
public:
    /**
     * @brief Configuration for WeightedMovingAverageFilter.
     */
    struct Config {
        std::vector<double> weights; ///< Kernel weights. Size must be odd >= 3.
    };

    /**
     * @brief Construct with configuration.
     * @param cfg Filter configuration. weights must not be empty.
     * @throws ConfigException if weights are empty.
     */
    explicit WeightedMovingAverageFilter(Config cfg);

    /**
     * @brief Apply weighted moving average to the series.
     * @param series Input time series. Must not contain NaN.
     * @return FilterResult with filtered series, residuals, and filter name.
     * @throws DataException   if series contains NaN.
     * @throws ConfigException if weights are invalid.
     */
    FilterResult apply(const TimeSeries& series) const override;

    /** @brief Returns "WeightedMovingAverage". */
    std::string name() const override;

private:
    Config m_cfg;
};

} // namespace loki
