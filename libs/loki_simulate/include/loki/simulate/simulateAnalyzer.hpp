#pragma once

#include <loki/simulate/simulationResult.hpp>
#include <loki/core/config.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <string>

namespace loki::simulate {

/**
 * @brief Orchestrates the full loki_simulate pipeline.
 *
 * Two public entry points:
 *
 *   runSynthetic(datasetName)
 *     -- No input series required. Generates nSim independent realizations
 *        from the ARIMA or Kalman model parameters in the config.
 *        datasetName is used only for output file naming.
 *
 *   run(series, datasetName)
 *     -- Bootstrap mode. Fits the configured model on the (gap-filled,
 *        deseasonalized) input series, then generates B bootstrap replicas
 *        from the fitted model. Collects bootstrap CIs for key parameters.
 *
 * Both modes:
 *   - Optionally inject anomalies (outliers, gaps, shifts) into each replica.
 *   - Compute the percentile envelope across all simulations.
 *   - Write a plain-text protocol to protocolsDir.
 *   - Write a CSV summary (envelope + parameter CIs) to csvDir.
 *   - Produce gnuplot plots via PlotSimulate.
 */
class SimulateAnalyzer {
public:

    /**
     * @brief Constructs the analyzer with the full application configuration.
     * @param cfg AppConfig (simulate section, paths, plot flags).
     */
    explicit SimulateAnalyzer(const AppConfig& cfg);

    /**
     * @brief Runs the synthetic generation pipeline (no input series).
     *
     * @param datasetName Label used in output file names (e.g. "synthetic").
     * @throws ConfigException    if model or mode fields are unrecognised.
     * @throws AlgorithmException if generation fails.
     */
    void runSynthetic(const std::string& datasetName);

    /**
     * @brief Runs the bootstrap pipeline on a real input series.
     *
     * @param series      Input time series (raw, possibly with gaps).
     * @param datasetName Stem of the input file.
     * @throws DataException      if the series is too short.
     * @throws ConfigException    if model or bootstrap method is unrecognised.
     * @throws AlgorithmException if fitting or bootstrap fails.
     */
    void run(const TimeSeries& series, const std::string& datasetName);

private:

    AppConfig m_cfg;

    // ---- synthetic sub-pipelines --------------------------------------------

    /** @brief Generates nSim ARIMA/AR realizations from parametric config. */
    [[nodiscard]] SimulationResult _runArimaSynthetic(
        const std::string& datasetName) const;

    /** @brief Generates nSim Kalman realizations from parametric config. */
    [[nodiscard]] SimulationResult _runKalmanSynthetic(
        const std::string& datasetName) const;

    // ---- bootstrap sub-pipelines --------------------------------------------

    /** @brief Fits ARIMA on series, generates B replicas, collects parameter CIs. */
    [[nodiscard]] SimulationResult _runArimaBootstrap(
        const TimeSeries&  series,
        const std::string& datasetName) const;

    /** @brief Fits Kalman on series, generates B replicas, collects parameter CIs. */
    [[nodiscard]] SimulationResult _runKalmanBootstrap(
        const TimeSeries&  series,
        const std::string& datasetName) const;

    // ---- shared helpers -----------------------------------------------------

    /**
     * @brief Optionally injects outliers, gaps, and/or mean shifts into each replica.
     *
     * Controlled by cfg.simulate.injectOutliers / injectGaps / injectShifts.
     * Applied independently to every simulation using seed-derived sub-seeds.
     *
     * @param simulations Batch of simulations to modify in place.
     * @param baseSeed    Base seed; each simulation uses baseSeed + index.
     */
    void _injectAnomalies(
        std::vector<std::vector<double>>& simulations,
        uint64_t baseSeed) const;

    /**
     * @brief Computes the percentile envelope across all simulations.
     *
     * At each time step t, sorts the values across all simulations
     * and extracts the 5/25/50/75/95 percentiles.
     *
     * @param result SimulationResult with simulations populated; fills env05..env95.
     */
    static void _computeEnvelope(SimulationResult& result);

    /**
     * @brief Computes cross-simulation summary statistics.
     *
     * Fills simMeanMean, simMeanStd, simStdMean, simStdStd.
     *
     * @param result SimulationResult with simulations populated.
     */
    static void _computeSummaryStats(SimulationResult& result);

    /**
     * @brief Writes a plain-text protocol to cfg.protocolsDir.
     * @param result Completed SimulationResult.
     */
    void _writeProtocol(const SimulationResult& result) const;

    /**
     * @brief Writes a CSV summary (envelope + parameter CIs) to cfg.csvDir.
     *
     * Full simulation matrix is written only if nSimulations <= 50.
     *
     * @param result Completed SimulationResult.
     */
    void _writeCsv(const SimulationResult& result) const;

    /**
     * @brief Fills gaps in the series according to cfg.simulate.gapFillStrategy.
     * @param series Input series (may contain NaN / time gaps).
     * @return Gap-filled series.
     */
    [[nodiscard]] TimeSeries _fillGaps(const TimeSeries& series) const;

    /**
     * @brief Extracts valid (non-NaN) double values from a TimeSeries.
     * @param series Input series.
     * @return Vector of valid values (NaN observations skipped).
     */
    static std::vector<double> _extractValues(const TimeSeries& series);
};

} // namespace loki::simulate
