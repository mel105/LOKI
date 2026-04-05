#pragma once

#include <loki/decomposition/decompositionResult.hpp>
#include <loki/core/config.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <string>

namespace loki {

/**
 * @brief Orchestrates the full decomposition pipeline for one time series.
 *
 * Pipeline:
 *   1. Gap filling (GapFiller, strategy from DecompositionConfig).
 *   2. Validate: series length >= 2 * period.
 *   3. Decompose (ClassicalDecomposer or StlDecomposer).
 *   4. Log descriptive statistics for each component.
 *   5. Write protocol to OUTPUT/PROTOCOLS/.
 *   6. Write CSV (trend, seasonal, residual columns) to OUTPUT/CSV/.
 *   7. Generate plots via PlotDecomposition.
 */
class DecompositionAnalyzer {
public:

    /**
     * @brief Constructs the analyzer with the full application configuration.
     * @param cfg Full AppConfig (decomposition, plots, paths).
     */
    explicit DecompositionAnalyzer(const AppConfig& cfg);

    /**
     * @brief Runs the full decomposition pipeline on one series.
     *
     * @param ts          Input time series (may contain gaps).
     * @param datasetName Stem of the input file, used for output naming.
     * @throws DataException      if series is too short after gap filling.
     * @throws ConfigException    if decomposition config is invalid.
     * @throws AlgorithmException if the decomposer fails numerically.
     */
    void run(const TimeSeries& ts, const std::string& datasetName) const;

private:

    const AppConfig& m_cfg;

    /**
     * @brief Writes a text protocol summarising the decomposition result.
     *
     * @param ts         Original (gap-filled) series.
     * @param result     Decomposition result.
     * @param datasetName Dataset stem name.
     * @param compName   Component name from series metadata.
     */
    void writeProtocol(const TimeSeries&          ts,
                       const DecompositionResult& result,
                       const std::string&         datasetName,
                       const std::string&         compName) const;

    /**
     * @brief Writes a semicolon-delimited CSV with columns:
     *        time_mjd ; original ; trend ; seasonal ; residual
     */
    void writeCsv(const TimeSeries&          ts,
                  const DecompositionResult& result,
                  const std::string&         datasetName,
                  const std::string&         compName) const;
};

} // namespace loki
