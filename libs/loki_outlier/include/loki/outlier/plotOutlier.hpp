#pragma once

#include <loki/core/config.hpp>
#include <loki/outlier/outlierResult.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <Eigen/Dense>

#include <cstddef>
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
 *   plotLeverages            -- DEH leverage h_ii vs time + threshold line
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
     * @param series   Original input series.
     * @param seasonal Seasonal component values (same length as series).
     */
    void plotSeasonalOverlay(const TimeSeries&          series,
                             const std::vector<double>& seasonal) const;

    /**
     * @brief Plots hat matrix leverage values h_ii over time with threshold line.
     *
     * Output: [program]_[dataset]_[param]_leverage.[fmt]
     *
     * The leverage vector has length n - arOrder; indices are offset by arOrder
     * relative to the original series. Outlier positions (h_ii > threshold) are
     * marked as filled triangles. The threshold line is drawn as a dashed red line.
     *
     * @param series        Original input series (used for MJD timestamps and naming).
     * @param leverages     Leverage values h_ii, length n - arOrder.
     * @param threshold     Detection threshold chi2Quantile(1-alpha, p) / n.
     * @param arOrder       AR lag order p (used as index offset and in title).
     * @param nOutliers     Number of detected outliers (for title).
     */
    void plotLeverages(const TimeSeries&      series,
                       const Eigen::VectorXd& leverages,
                       double                 threshold,
                       int                    arOrder,
                       std::size_t            nOutliers) const;

    /**
     * @brief Runs all enabled outlier plots based on PlotConfig flags.
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
