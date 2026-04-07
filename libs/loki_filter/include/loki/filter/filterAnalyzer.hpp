#pragma once

#include <loki/core/config.hpp>
#include <loki/filter/filterResult.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <string>

namespace loki {

/**
 * @brief Orchestrates the full filtering pipeline for one time series.
 *
 * Pipeline:
 *   1. Gap filling (GapFiller, strategy from FilterConfig).
 *   2. Build and apply the selected filter.
 *   3. Log descriptive statistics for residuals.
 *   4. Write protocol to OUTPUT/PROTOCOLS/.
 *   5. Write CSV (original, filtered, residual columns) to OUTPUT/CSV/.
 *   6. Generate plots via PlotFilter.
 */
class FilterAnalyzer {
public:

    /**
     * @brief Constructs the analyzer with the full application configuration.
     * @param cfg Full AppConfig (filter, plots, paths).
     */
    explicit FilterAnalyzer(const AppConfig& cfg);

    /**
     * @brief Runs the full filtering pipeline on one series.
     *
     * @param ts          Input time series (may contain gaps).
     * @param datasetName Stem of the input file, used for output naming.
     * @throws DataException      if series is empty after gap filling.
     * @throws ConfigException    if filter config is invalid.
     * @throws AlgorithmException if the filter fails numerically.
     */
    void run(const TimeSeries& ts, const std::string& datasetName) const;

private:

    const AppConfig& m_cfg;

    /**
     * @brief Writes a text protocol summarising the filter result.
     *
     * Includes filter parameters, residual statistics (mean, std, MAD, RMSE,
     * min, max), residual diagnostics (Jarque-Bera, Durbin-Watson, ACF lag-1),
     * and method-specific tuning hints.
     *
     * @param ts          Gap-filled input series.
     * @param result      Filter result (filtered series + residuals).
     * @param datasetName Dataset stem name.
     * @param compName    Component name from series metadata.
     */
    void _writeProtocol(const TimeSeries&   ts,
                        const FilterResult& result,
                        const std::string&  datasetName,
                        const std::string&  compName) const;

    /**
     * @brief Writes a semicolon-delimited CSV with columns:
     *        time_mjd ; original ; filtered ; residual
     */
    void _writeCsv(const TimeSeries&   ts,
                   const FilterResult& result,
                   const std::string&  datasetName,
                   const std::string&  compName) const;
};

} // namespace loki
