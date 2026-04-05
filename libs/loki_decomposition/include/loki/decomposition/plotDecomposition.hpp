#pragma once

#include <loki/decomposition/decompositionResult.hpp>
#include <loki/core/config.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <filesystem>
#include <string>

namespace loki {

/**
 * @brief Generates gnuplot-based visualizations of decomposition results.
 *
 * Produces up to three plot types:
 *   - Overlay     : original series with trend overlaid (decompOverlay flag).
 *   - Panels      : 3-panel stacked: trend / seasonal / residual (decompPanels flag).
 *   - Diagnostics : residual ACF, histogram, QQ via Plot::residualDiagnostics()
 *                   (decompDiagnostics flag).
 *
 * File naming convention: decomposition_[dataset]_[component]_[plottype].[format]
 * Output directory: AppConfig::imgDir.
 *
 * Plot errors are caught and logged as warnings so the pipeline continues.
 */
class PlotDecomposition {
public:

    /**
     * @brief Constructs the plotter with the full application configuration.
     * @param cfg Full AppConfig (plots, imgDir, output format).
     */
    explicit PlotDecomposition(const AppConfig& cfg);

    /**
     * @brief Plots original series with trend overlaid on a single axis.
     *
     * @param ts          Gap-filled original series.
     * @param result      Decomposition result.
     * @param datasetName Dataset stem (for file naming).
     * @param compName    Component name from series metadata.
     */
    void plotOverlay(const TimeSeries&          ts,
                     const DecompositionResult& result,
                     const std::string&         datasetName,
                     const std::string&         compName) const;

    /**
     * @brief Plots a 3-panel stacked figure: trend / seasonal / residual.
     *
     * @param ts          Gap-filled original series.
     * @param result      Decomposition result.
     * @param datasetName Dataset stem (for file naming).
     * @param compName    Component name from series metadata.
     */
    void plotPanels(const TimeSeries&          ts,
                    const DecompositionResult& result,
                    const std::string&         datasetName,
                    const std::string&         compName) const;

    /**
     * @brief Delegates residual diagnostics to Plot::residualDiagnostics().
     *
     * Produces a 2x2 panel: residuals vs index, Q-Q plot with bands,
     * histogram with normal fit, and ACF of residuals.
     *
     * @param result      Decomposition result (residual component used).
     * @param datasetName Dataset stem (for file naming).
     * @param compName    Component name from series metadata.
     */
    void plotDiagnostics(const DecompositionResult& result,
                          const std::string&         datasetName,
                          const std::string&         compName) const;

private:

    const AppConfig& m_cfg;

    /// Converts filesystem path to gnuplot-safe forward-slash string.
    static std::string fwdSlash(const std::filesystem::path& p);

    /// Returns the gnuplot terminal string for the configured output format.
    std::string terminal() const;

    /// Builds the output path: imgDir / decomposition_dataset_comp_type.format
    [[nodiscard]]
    std::filesystem::path outPath(const std::string& datasetName,
                                   const std::string& compName,
                                   const std::string& plotType) const;

    /// Writes a temporary data file with columns: mjd original trend seasonal residual.
    /// stem is used as part of the temp filename. Caller must remove after use.
    [[nodiscard]]
    std::filesystem::path writeTmp(const TimeSeries&          ts,
                                    const DecompositionResult& result,
                                    const std::string&         stem) const;
};

} // namespace loki
