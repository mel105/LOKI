#pragma once

#include <loki/ssa/ssaResult.hpp>
#include <loki/core/config.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace loki::ssa {

/**
 * @brief Produces SSA-specific diagnostic plots for the loki_ssa pipeline.
 *
 * Four plot types are provided:
 *   - scree        : eigenvalue spectrum (log scale) + cumulative variance curve.
 *   - wcorr        : r x r w-correlation matrix rendered as a colour heatmap.
 *   - components   : first N elementary reconstructed components vs time (MJD).
 *   - reconstruction: original series + per-group reconstructions overlaid.
 *
 * Output filenames follow the project convention:
 *   ssa_[dataset]_[parameter]_[plottype].[format]
 *
 * All plots are gated by PlotConfig flags (ssaScree, ssaWCorr, ssaComponents,
 * ssaReconstruction). Failures in individual plots are caught, logged as
 * warnings, and do not abort remaining plots.
 */
class PlotSsa {
public:

    /**
     * @brief Constructs a PlotSsa bound to the given application configuration.
     * @param cfg Application configuration. AppConfig::imgDir must be set.
     */
    explicit PlotSsa(const AppConfig& cfg);

    /**
     * @brief Renders all enabled SSA plots for one series.
     *
     * @param filled    Gap-filled TimeSeries (provides MJD timestamps).
     * @param result    Populated SsaResult from SsaAnalyzer::analyze().
     * @param nComponents Number of elementary components to show in the
     *                    components plot. Default: 8.
     */
    void plotAll(const TimeSeries& filled,
                 const SsaResult&  result,
                 int               nComponents = 8) const;

    /**
     * @brief Scree plot: eigenvalue spectrum on log scale + cumulative variance.
     *
     * Left y-axis: eigenvalues (log scale, bar chart).
     * Right y-axis: cumulative variance fraction (line, 0-100%).
     * A horizontal dashed line marks the 95% cumulative variance threshold.
     *
     * Output: ssa_[dataset]_[param]_scree.[fmt]
     *
     * @param result SsaResult with eigenvalues and varianceFractions filled.
     * @throws DataException if eigenvalues are empty.
     * @throws IoException   on gnuplot or file I/O failure.
     */
    void plotScree(const TimeSeries& filled,
                   const SsaResult&  result) const;

    /**
     * @brief W-correlation heatmap: r x r matrix as a filled colour map.
     *
     * Colours range from white (0, well-separated) to dark blue (1, mixed).
     * The plot is square; axis labels show eigentriple indices.
     *
     * Output: ssa_[dataset]_[param]_wcorr.[fmt]
     *
     * @param result SsaResult with wCorrMatrix filled.
     * @throws DataException if wCorrMatrix is empty.
     * @throws IoException   on gnuplot or file I/O failure.
     */
    void plotWCorr(const TimeSeries& filled,
                   const SsaResult&  result) const;

    /**
     * @brief Elementary components plot: first nComponents series vs MJD.
     *
     * Each component is plotted in a separate panel stacked vertically.
     * The variance fraction is annotated in the panel title.
     *
     * Output: ssa_[dataset]_[param]_components.[fmt]
     *
     * @param filled      Gap-filled TimeSeries (MJD timestamps).
     * @param result      SsaResult with components filled.
     * @param nComponents Number of components to plot (clamped to r).
     * @throws DataException if components are empty.
     * @throws IoException   on gnuplot or file I/O failure.
     */
    void plotComponents(const TimeSeries& filled,
                        const SsaResult&  result,
                        int               nComponents = 8) const;

    /**
     * @brief Reconstruction overlay: original + per-group reconstructions.
     *
     * Top panel:    original series (grey) + trend (red) + all non-noise groups.
     * Bottom panel: noise reconstruction (if a "noise" group exists).
     * If no "noise" group: single-panel layout.
     *
     * Output: ssa_[dataset]_[param]_reconstruction.[fmt]
     *
     * @param filled  Gap-filled TimeSeries (original values + MJD timestamps).
     * @param result  SsaResult with groups and reconstructions filled.
     * @throws DataException if groups are empty.
     * @throws IoException   on gnuplot or file I/O failure.
     */
    void plotReconstruction(const TimeSeries& filled,
                            const SsaResult&  result) const;

private:

    AppConfig m_cfg;

    /// Build base filename stem: "ssa_[dataset]_[componentName]"
    [[nodiscard]] std::string _baseName(const TimeSeries& ts) const;

    /// Build full output path: imgDir / base_plotType.format
    [[nodiscard]] std::filesystem::path _outPath(const std::string& base,
                                                  const std::string& plotType) const;

    /// Build gnuplot terminal string for the configured output format.
    [[nodiscard]] std::string _terminal(int widthPx = 1200,
                                        int heightPx = 600) const;

    /// Convert backslashes to forward slashes for gnuplot on Windows.
    [[nodiscard]] static std::string _fwdSlash(const std::filesystem::path& p);
};

} // namespace loki::ssa
