#pragma once

#include <loki/core/config.hpp>
#include <loki/kriging/krigingResult.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <string>
#include <vector>

namespace loki::kriging {

/**
 * @brief Orchestrator for the Kriging analysis pipeline.
 *
 * Executes the full workflow for a single time series:
 *   1. Gap filling (configured via KrigingConfig::gapFillStrategy).
 *   2. Empirical variogram computation.
 *   3. Theoretical variogram model fitting (WLS via Nelder-Mead).
 *   4. Kriging system assembly and factorisation.
 *   5. Prediction on observation grid and optional target points.
 *   6. Leave-one-out cross-validation (optional).
 *   7. Protocol, CSV, and plot output.
 *
 * Usage:
 * @code
 *   loki::kriging::KrigingAnalyzer analyzer(cfg);
 *   analyzer.run(ts, "DATASET_NAME");
 * @endcode
 */
class KrigingAnalyzer {
public:

    /**
     * @brief Construct with full application config.
     * @param cfg Application config. kriging and plots sub-configs are used.
     */
    explicit KrigingAnalyzer(const AppConfig& cfg);

    /**
     * @brief Run the Kriging pipeline on a single time series.
     *
     * @param series      Input time series (gap-filled internally).
     * @param datasetName File stem used for output naming (e.g. "GNSS_STA1").
     * @throws DataException      if the series is too short after gap filling.
     * @throws AlgorithmException on numerical failure (singular system etc.).
     * @throws ConfigException    if an unsupported mode or method is requested.
     */
    void run(const TimeSeries& series, const std::string& datasetName);

private:

    const AppConfig& m_cfg;

    // -- Pipeline steps -------------------------------------------------------

    /**
     * @brief Build prediction target grid from configuration.
     *
     * Combines:
     *   - Observation times (always included for gap-fill output).
     *   - Explicit targets from KrigingConfig::prediction.targetMjd.
     *   - Uniform horizon grid (KrigingConfig::prediction.horizonDays / nSteps).
     *
     * Result is sorted and deduplicated.
     *
     * @param ts    Gap-filled observation series.
     * @return      Sorted MJD vector of all target points.
     */
    std::vector<double> _buildTargetGrid(const TimeSeries& ts) const;

    /**
     * @brief Compute basic statistics of the observation values.
     *
     * @param z   Valid observation values.
     * @param mean   Output: sample mean.
     * @param var    Output: sample variance.
     */
    static void _computeStats(const std::vector<double>& z,
                               double& mean, double& var);

    // -- Output ---------------------------------------------------------------

    /**
     * @brief Write plain-text protocol to OUTPUT/PROTOCOLS/.
     *
     * @param result     Full Kriging result.
     * @param ts         Original (pre-gap-fill) time series.
     * @param datasetName Dataset name stem.
     */
    void _writeProtocol(const KrigingResult& result,
                        const TimeSeries&    ts,
                        const std::string&   datasetName) const;

    /**
     * @brief Write prediction CSV to OUTPUT/CSV/.
     *
     * Columns: MJD; UTC; value; variance; ci_lower; ci_upper; is_observed
     * Delimiter: semicolon (project convention).
     *
     * @param result      Full Kriging result.
     * @param datasetName Dataset name stem.
     * @param component   Series component name.
     */
    void _writeCsv(const KrigingResult& result,
                   const std::string&   datasetName,
                   const std::string&   component) const;
};

} // namespace loki::kriging
