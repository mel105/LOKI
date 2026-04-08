#pragma once

#include <loki/core/config.hpp>
#include <loki/homogeneity/homogenizer.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <string>

namespace loki::homogeneity {

/**
 * @brief Orchestrates the full homogeneity analysis pipeline for one time series.
 *
 * Pipeline (mirrors Homogenizer::process internals, driven from AppConfig):
 *   1. Build HomogenizerConfig from AppConfig.
 *   2. Run Homogenizer::process (gap fill, outlier removal, deseasonalization,
 *      change point detection, series adjustment).
 *   3. Write text protocol to OUTPUT/PROTOCOLS/.
 *   4. Write CSV  to OUTPUT/CSV/.
 *   5. Generate plots via PlotHomogeneity.
 *
 * Calling pattern (same as FilterAnalyzer):
 * @code
 *   HomogeneityAnalyzer analyzer{cfg};
 *   for (const auto& r : loadResults)
 *       for (const auto& ts : r.series)
 *           analyzer.run(ts, r.filePath.stem().string());
 * @endcode
 */
class HomogeneityAnalyzer {
public:

    /**
     * @brief Constructs the analyzer with the full application configuration.
     * @param cfg Full AppConfig (homogeneity, plots, paths).
     */
    explicit HomogeneityAnalyzer(const AppConfig& cfg);

    /**
     * @brief Runs the full homogeneity pipeline on one time series.
     *
     * @param ts          Input time series (may contain gaps).
     * @param datasetName Stem of the input file, used for output naming.
     * @throws DataException      if the series is empty.
     * @throws AlgorithmException if the detector or adjuster fails.
     * @throws IoException        if protocol or CSV cannot be written.
     */
    void run(const TimeSeries& ts, const std::string& datasetName) const;

private:

    const AppConfig& m_cfg;

    /**
     * @brief Builds a HomogenizerConfig from the AppConfig homogeneity section.
     */
    HomogenizerConfig _buildHomogenizerConfig() const;

    /**
     * @brief Writes a text protocol summarising the full homogenization result.
     *
     * Sections:
     *   - Pre-processing (gap filling, outlier removal, deseasonalization)
     *   - Detection parameters (method-specific)
     *   - Change points table (index, MJD, UTC date, shift, p-value)
     *   - Segment statistics (date range, n, mean, std dev per segment)
     *   - Adjustments applied (cumulative corrections per segment)
     *   - Residual diagnostics (J-B, D-W, ACF lag-1)
     *   - Homogenized series statistics
     *
     * @param original    Gap-filled input series (before adjustment).
     * @param result      Full HomogenizerResult from Homogenizer::process.
     * @param hcfg        HomogenizerConfig used for the run.
     * @param datasetName Dataset stem name.
     * @param compName    Component name from series metadata.
     */
    void _writeProtocol(const TimeSeries&       original,
                        const HomogenizerResult& result,
                        const HomogenizerConfig& hcfg,
                        const std::string&       datasetName,
                        const std::string&       compName) const;

    /**
     * @brief Writes a semicolon-delimited CSV with columns:
     *        mjd ; original ; seasonal ; deseasonalized ; adjusted ; flag ; change_point
     */
    void _writeCsv(const TimeSeries&       original,
                   const HomogenizerResult& result,
                   const std::string&       datasetName,
                   const std::string&       compName) const;
};

} // namespace loki::homogeneity