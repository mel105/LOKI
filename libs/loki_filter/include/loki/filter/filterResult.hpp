#pragma once

#include <loki/timeseries/timeSeries.hpp>

#include <string>

namespace loki {

/**
 * @brief Result of a filter operation.
 *
 * Contains the filtered series, the residuals (original minus filtered),
 * the name of the filter that produced the result, and the effective
 * window/bandwidth used (for logging and diagnostics).
 */
struct FilterResult {
    TimeSeries  filtered;             ///< Filtered output series (same timestamps as input).
    TimeSeries  residuals;            ///< Residuals: original - filtered.
    std::string filterName;           ///< Name of the filter (for logging and plot output naming).
    int         effectiveWindow{0};   ///< Effective window in samples (0 if not applicable, e.g. EMA).
    double      effectiveBandwidth{0.0}; ///< Effective bandwidth as fraction of series length (0 if not applicable).
};

} // namespace loki
