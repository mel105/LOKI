#pragma once

#include "loki/core/config.hpp"
#include "loki/filter/filterResult.hpp"
#include "loki/timeseries/timeSeries.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace loki::filter {

/**
 * @brief Produces diagnostic plots for the loki_filter pipeline.
 *
 * All plots are written to AppConfig::imgDir. Output filenames follow the
 * project naming convention:
 *   filter_[dataset]_[parameter]_[plottype].[format]
 *
 * PlotFilter accepts three series per call:
 *   - original  : raw series as loaded (may contain NaN)
 *   - filled    : after GapFiller (NaN-free, input to the filter)
 *   - result    : FilterResult produced by Filter::apply()
 *
 * Generic plots (time_series, histogram, acf, qqPlot, boxplot) are rendered
 * on the original series via loki::Plot.
 *
 * Pipeline-specific plots:
 *   - overlay               : filled + filtered, two coloured lines
 *   - overlay_residuals     : two-panel multiplot (filled+filtered / residuals)
 *   - residuals             : standalone residuals line plot
 *   - residuals_acf         : ACF of residuals with 95% confidence band
 *   - residuals_histogram   : histogram of residuals
 *   - residuals_qq          : Q-Q plot of residuals
 */
class PlotFilter {
public:

    /**
     * @brief Constructs a PlotFilter bound to the given application configuration.
     * @param cfg Application configuration. AppConfig::imgDir must be set.
     */
    explicit PlotFilter(const AppConfig& cfg);

    /**
     * @brief Renders all enabled plots for one series.
     *
     * Checks each PlotConfig flag before calling the individual plot method.
     * Failures in individual plots are caught, logged as warnings, and do not
     * abort the remaining plots.
     *
     * @param original  Raw series as loaded (used for generic plots).
     * @param filled    Gap-filled series (input to the filter).
     * @param result    FilterResult from Filter::apply().
     */
    void plotAll(const TimeSeries&    original,
                 const TimeSeries&    filled,
                 const FilterResult&  result) const;

    /**
     * @brief Overlay plot: filled + filtered series on one axes.
     *
     * Output: filter_[dataset]_[param]_overlay.[fmt]
     */
    void plotOverlay(const TimeSeries&   filled,
                     const FilterResult& result) const;

    /**
     * @brief Two-panel plot: upper = overlay, lower = residuals.
     *
     * Output: filter_[dataset]_[param]_overlay_residuals.[fmt]
     */
    void plotOverlayResiduals(const TimeSeries&   filled,
                              const FilterResult& result) const;

    /**
     * @brief Standalone residuals line plot.
     *
     * Output: filter_[dataset]_[param]_residuals.[fmt]
     */
    void plotResiduals(const TimeSeries&   filled,
                       const FilterResult& result) const;

    /**
     * @brief ACF of residuals with 95% confidence band.
     *
     * Output: filter_[dataset]_[param]_residuals_acf.[fmt]
     */
    void plotResidualsAcf(const TimeSeries&   filled,
                          const FilterResult& result) const;

    /**
     * @brief Histogram of residuals.
     *
     * Output: filter_[dataset]_[param]_residuals_histogram.[fmt]
     */
    void plotResidualsHistogram(const TimeSeries&   filled,
                                const FilterResult& result) const;

    /**
     * @brief Q-Q plot of residuals against the standard normal distribution.
     *
     * Output: filter_[dataset]_[param]_residuals_qq.[fmt]
     */
    void plotResidualsQq(const TimeSeries&   filled,
                         const FilterResult& result) const;

private:

    AppConfig m_cfg;  ///< Full application configuration (copied).

    // -------------------------------------------------------------------------
    //  Helpers
    // -------------------------------------------------------------------------

    /**
     * @brief Builds the base name shared by all plots for a series.
     *
     * Format: filter_[dataset]_[parameter]
     */
    [[nodiscard]] std::string _baseName(const TimeSeries& series) const;

    /**
     * @brief Builds the absolute output path for a plot file.
     *
     * @param baseName Result of _baseName().
     * @param plotType Short descriptor, e.g. "overlay", "residuals_acf".
     */
    [[nodiscard]] std::filesystem::path _outPath(const std::string& baseName,
                                                  const std::string& plotType) const;

    /**
     * @brief Writes a two-column (mjd, value) temp file from a TimeSeries.
     */
    void _writeSeriesDat(const TimeSeries&            series,
                         const std::filesystem::path& path) const;

    /**
     * @brief Writes a two-column (mjd, value) temp file from parallel vectors.
     */
    void _writeVectorDat(const std::vector<double>&   times,
                         const std::vector<double>&   values,
                         const std::filesystem::path& path) const;

    /**
     * @brief Computes ACF up to maxLag from a residuals vector.
     *
     * Returns vector of (lag, acf) pairs.
     */
    [[nodiscard]] static std::vector<std::pair<double, double>>
    _computeAcf(const std::vector<double>& values, int maxLag);

    /**
     * @brief Returns the gnuplot terminal string for the configured format.
     *
     * @param widthPx  Canvas width in pixels.
     * @param heightPx Canvas height in pixels.
     */
    [[nodiscard]] std::string _terminal(int widthPx = 1200,
                                        int heightPx = 400) const;

    /**
     * @brief Extracts MJD values from a TimeSeries into a vector.
     */
    [[nodiscard]] static std::vector<double> _mjdVec(const TimeSeries& series);
};

} // namespace loki::filter
