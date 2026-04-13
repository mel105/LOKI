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
 *   1. Gap filling (KrigingConfig::gapFillStrategy).
 *   2. Empirical variogram (loki::math::computeEmpiricalVariogram).
 *   3. Variogram model fitting (loki::math::fitVariogram).
 *   4. Kriging system build and inversion (loki::math::createKriging).
 *   5. Prediction on observation grid and optional target points.
 *   6. Leave-one-out cross-validation (optional).
 *   7. Protocol, CSV, and plot output.
 *
 * The math primitives live in loki_core/math/:
 *   krigingTypes.hpp, krigingVariogram.hpp, krigingBase.hpp,
 *   simpleKriging.hpp, ordinaryKriging.hpp, universalKriging.hpp,
 *   krigingFactory.hpp
 *
 * Usage:
 * @code
 *   loki::kriging::KrigingAnalyzer analyzer(cfg);
 *   analyzer.run(ts, "DATASET_NAME");
 * @endcode
 */
class KrigingAnalyzer {
public:

    explicit KrigingAnalyzer(const AppConfig& cfg);

    /**
     * @brief Run the Kriging pipeline on a single time series.
     *
     * @param series      Input time series (gap-filled internally).
     * @param datasetName File stem for output naming (e.g. "GNSS_STA1").
     * @throws DataException      if series too short after gap filling.
     * @throws AlgorithmException on numerical failure.
     * @throws ConfigException    if unsupported mode/method requested.
     */
    void run(const TimeSeries& series, const std::string& datasetName);

private:

    const AppConfig& m_cfg;

    std::vector<double> _buildTargetGrid(const TimeSeries& ts) const;

    static void _computeStats(const std::vector<double>& z,
                               double& mean, double& var);

    void _writeProtocol(const KrigingResult& result,
                        const TimeSeries&    ts,
                        const std::string&   datasetName) const;

    void _writeCsv(const KrigingResult& result,
                   const std::string&   datasetName,
                   const std::string&   component) const;
};

} // namespace loki::kriging
