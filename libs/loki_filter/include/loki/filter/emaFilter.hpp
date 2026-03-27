#pragma once

#include <loki/filter/filter.hpp>

namespace loki {

/**
 * @brief Exponential moving average filter (single forward pass).
 *
 * Wraps loki::stats::exponentialMovingAverage(). EMA is causal (one-directional),
 * so no edge NaN are produced on the right. The first output value equals
 * the first input value (no NaN fill needed).
 *
 * The input series must be free of NaN values. Use GapFiller in the
 * pipeline before applying this filter.
 */
class EmaFilter : public Filter {
public:
    /**
     * @brief Configuration for EmaFilter.
     */
    struct Config {
        double alpha{0.1}; ///< Smoothing factor in (0, 1]. alpha=1 returns a copy of input.
    };

    /**
     * @brief Construct with configuration.
     * @param cfg Filter configuration.
     */
    explicit EmaFilter(Config cfg);

    /**
     * @brief Apply exponential moving average to the series.
     * @param series Input time series. Must not contain NaN.
     * @return FilterResult with filtered series, residuals, and filter name.
     * @throws DataException   if series contains NaN.
     * @throws ConfigException if alpha is not in (0, 1].
     */
    FilterResult apply(const TimeSeries& series) const override;

    /** @brief Returns "EMA". */
    std::string name() const override;

private:
    Config m_cfg;
};

} // namespace loki
