#pragma once

#include <loki/core/config.hpp>
#include <loki/outlier/outlierResult.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace loki::outlier {

/**
 * @brief Generates gnuplot diagnostic plots for the outlier detection pipeline.
 *
 * Naming convention: [program]_[dataset]_[parameter]_[plottype].[format]
 *
 * Plot catalogue:
 *   plotOriginalWithOutliers -- original series + outlier markers
 *   plotCleaned              -- cleaned series line plot
 *   plotComparison           -- original vs cleaned overlay
 *   plotResiduals            -- residuals + outlier markers
 *   plotSeasonalOverlay      -- original + seasonal model overlay
 *   plotResidualsWithBounds  -- residuals + detection bound lines
 *   plotOutlierOverlay       -- pre + post detection on one plot (homogeneity)
 *   plotAll                  -- calls all enabled plots based on PlotConfig flags
 */
class PlotOutlier {
public:

    /**
     * @brief Constructs a PlotOutlier bound to the given application configuration.
     * @param cfg         Application configuration.
     * @param programName Prefix used in output filenames: "outlier" or "homogeneity".
     */
    explicit PlotOutlier(const AppConfig&   cfg,
                         const std::string& programName = "outlier");

    // -------------------------------------------------------------------------
    // Individual plots
    // -------------------------------------------------------------------------

    /**
     * @brief Plots the original series with detected outliers marked as triangles.
     */
    void plotOriginalWithOutliers(const TimeSeries&   series,
                                  const OutlierResult& detection) const;

    /**
     * @brief Plots the cleaned (outlier-replaced) series.
     */
    void plotCleaned(const TimeSeries& cleaned) const;

    /**
     * @brief Plots original and cleaned series overlaid for comparison.
     */
    void plotComparison(const TimeSeries& original,
                        const TimeSeries& cleaned) const;

    /**
     * @brief Plots deseasonalized residuals with outlier markers.
     */
    void plotResiduals(const TimeSeries&   original,
                       const TimeSeries&   residuals,
                       const OutlierResult& detection) const;

    /**
     * @brief Plots the original series with the seasonal model overlaid.
     *
     * Output: [program]_[dataset]_[param]_seasonal_overlay.[fmt]
     *
     * @param series   Original input series.
     * @param seasonal Seasonal component values (same length as series).
     */
    void plotSeasonalOverlay(const TimeSeries&          series,
                             const std::vector<double>& seasonal) const;

    /**
     * @brief Runs all enabled outlier plots based on PlotConfig flags.
     *
     * Calls individual plot methods according to cfg.plots flags.
     * plotResidualsWithBounds is called here when residualsWithBounds flag is set.
     * plotSeasonalOverlay is called here when seasonalOverlay flag is set and
     * hasComponent is true.
     *
     * @param original      Original input series.
     * @param cleaned       Cleaned series.
     * @param residuals     Residual series (may equal original if no deseasonalization).
     * @param detection     Outlier detection result.
     * @param hasComponent  True if deseasonalization was applied.
     * @param seasonal      Seasonal component (used only when hasComponent is true).
     */
    void plotAll(const TimeSeries&          original,
                 const TimeSeries&          cleaned,
                 const TimeSeries&          residuals,
                 const OutlierResult&        detection,
                 bool                        hasComponent,
                 const std::vector<double>& seasonal = {}) const;

    // -------------------------------------------------------------------------
    // Homogeneity pipeline plots
    // -------------------------------------------------------------------------

    /**
     * @brief Plots the original series with pre- and post-outlier detections overlaid.
     */
    void plotOutlierOverlay(const TimeSeries&   series,
                            const OutlierResult& preDetection,
                            const OutlierResult& postDetection) const;

    /**
     * @brief Plots residuals with detection bound lines for pre- and post-passes.
     */
    void plotResidualsWithBounds(const TimeSeries&   series,
                                 const TimeSeries&   residuals,
                                 const OutlierResult& preDetection,
                                 const OutlierResult& postDetection) const;

private:

    AppConfig   m_cfg;
    std::string m_programName;

    [[nodiscard]] std::string             _stem(const TimeSeries&  series,
                                                const std::string& plotType) const;
    [[nodiscard]] std::filesystem::path   _outPath(const std::string& stem) const;
    [[nodiscard]] std::string             _datasetName() const;
    [[nodiscard]] std::string             _terminal() const;
    [[nodiscard]] static std::string      _fwdSlash(const std::filesystem::path& p);

    void _writeSeriesDat (const TimeSeries&            series,
                          const std::filesystem::path& path) const;
    void _writeOutlierDat(const TimeSeries&            series,
                          const OutlierResult&          detection,
                          const std::filesystem::path& path) const;
    void _writeVectorDat (const std::vector<double>&   times,
                          const std::vector<double>&   values,
                          const std::filesystem::path& path) const;
};

} // namespace loki::outlier