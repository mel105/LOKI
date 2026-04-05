#pragma once

#include <loki/decomposition/decompositionResult.hpp>
#include <loki/core/config.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <vector>

namespace loki {

/**
 * @brief Classical additive decomposition: moving-average trend + per-slot seasonal.
 *
 * Algorithm:
 *   1. Trend T[t]:    centered moving average of width = period.
 *                     NaN edges are filled by bfill/ffill via GapFiller (LINEAR).
 *   2. Detrended D[t] = Y[t] - T[t].
 *   3. Seasonal S[t]: for each slot s = (i mod period), compute the median
 *                     (or mean) of all D[t] that fall in slot s, then
 *                     normalize so that the sum over one period equals zero.
 *   4. Residual R[t] = Y[t] - T[t] - S[t].
 *
 * The input series must be free of NaN. Use GapFiller in the pipeline before
 * calling decompose().
 *
 * Reference: Makridakis, Wheelwright & Hyndman (1998). Forecasting: Methods
 * and Applications. Chapter 3.
 */
class ClassicalDecomposer {
public:

    using Config = ClassicalDecompositionConfig;

    /**
     * @brief Constructs with configuration.
     * @param cfg Decomposition configuration (trend filter, seasonal type).
     */
    explicit ClassicalDecomposer(Config cfg = Config{});

    /**
     * @brief Decomposes the series into trend, seasonal, and residual.
     *
     * @param ts     Input time series. Must be free of NaN. Must have
     *               at least 2 * period observations.
     * @param period Period in samples (>= 2).
     * @return DecompositionResult with all three components.
     * @throws DataException   if ts contains NaN or has fewer than 2*period points.
     * @throws ConfigException if period < 2.
     */
    [[nodiscard]]
    DecompositionResult decompose(const TimeSeries& ts, int period) const;

private:

    Config m_cfg;

    /**
     * @brief Estimates the trend via centered moving average, then fills
     *        NaN edges using GapFiller(LINEAR) bfill/ffill.
     *
     * @param ts     Input series (NaN-free).
     * @param period Window width for moving average.
     * @return Trend vector of length ts.size(), fully filled (no NaN).
     */
    [[nodiscard]]
    std::vector<double> estimateTrend(const TimeSeries& ts, int period) const;

    /**
     * @brief Computes per-slot seasonal component from detrended values.
     *
     * For each slot s in [0, period), collects all detrended[i] where
     * (i mod period) == s, then aggregates by median or mean according to
     * m_cfg.seasonalType. The slot values are then normalized so that their
     * sum over one complete period equals zero.
     *
     * @param detrended  Y[t] - T[t] for all t.
     * @param period     Period in samples.
     * @return Seasonal vector of length detrended.size().
     */
    [[nodiscard]]
    std::vector<double> estimateSeasonal(const std::vector<double>& detrended,
                                         int period) const;
};

} // namespace loki
