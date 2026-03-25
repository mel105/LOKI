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
 * Analogous to PlotHomogeneity in loki_homogeneity. Each method produces one
 * image file in cfg.imgDir.
 *
 * Naming convention: [program]_[dataset]_[parameter]_[plottype].[format]
 * Example: outlier_CLIM_DATA_EX1_col_3_original_with_outliers.png
 *
 * Plot catalogue:
 *   plotOriginalWithOutliers -- original series + outlier markers (triangles)
 *   plotCleaned              -- cleaned series line plot
 *   plotComparison           -- original vs cleaned overlay
 *   plotResiduals            -- deseasonalized residuals with outlier markers
 *   plotAll                  -- calls all enabled plots based on PlotConfig flags
 *
 * For use inside the homogeneity pipeline, plotOutlierOverlay() renders
 * pre-outlier and post-outlier detections on one shared axes, using different
 * marker colors so both passes are visually distinguishable.
 */
class PlotOutlier {
public:

    /**
     * @brief Constructs a PlotOutlier bound to the given application configuration.
     * @param cfg Application configuration (imgDir, plots flags, output format).
     * @param programName Prefix used in output filenames: "outlier" or "homogeneity".
     */
    explicit PlotOutlier(const AppConfig& cfg,
                         const std::string& programName = "outlier");

    // -------------------------------------------------------------------------
    // Standalone outlier app plots
    // -------------------------------------------------------------------------

    /**
     * @brief Plots the original series with detected outliers marked as triangles.
     *
     * Outlier positions are extracted from detection.points. Each outlier is
     * rendered as a red triangle above the line.
     *
     * Output: [program]_[dataset]_[param]_original_with_outliers.[fmt]
     *
     * @param series    Original input series.
     * @param detection Outlier detection result.
     */
    void plotOriginalWithOutliers(const TimeSeries&   series,
                                  const OutlierResult& detection) const;

    /**
     * @brief Plots the cleaned (outlier-replaced) series.
     *
     * Output: [program]_[dataset]_[param]_cleaned.[fmt]
     *
     * @param cleaned Cleaned series from OutlierCleaner::CleanResult::cleaned.
     */
    void plotCleaned(const TimeSeries& cleaned) const;

    /**
     * @brief Plots original and cleaned series overlaid for comparison.
     *
     * Output: [program]_[dataset]_[param]_comparison.[fmt]
     *
     * @param original Original input series.
     * @param cleaned  Cleaned series.
     */
    void plotComparison(const TimeSeries& original,
                        const TimeSeries& cleaned) const;

    /**
     * @brief Plots deseasonalized residuals with outlier markers.
     *
     * Output: [program]_[dataset]_[param]_residuals.[fmt]
     *
     * @param original  Original series (for time axis and metadata).
     * @param residuals Deseasonalized residual values.
     * @param detection Outlier detection result (positions in residual space).
     */
    void plotResiduals(const TimeSeries&   original,
                       const TimeSeries&   residuals,
                       const OutlierResult& detection) const;

    /**
     * @brief Runs all enabled outlier plots based on PlotConfig flags.
     *
     * @param original   Original input series.
     * @param cleaned    Cleaned series.
     * @param residuals  Residual series (may be same as original if no deseasonalization).
     * @param detection  Outlier detection result.
     * @param hasResiduals True if deseasonalization was applied.
     */
    void plotAll(const TimeSeries&   original,
                 const TimeSeries&   cleaned,
                 const TimeSeries&   residuals,
                 const OutlierResult& detection,
                 bool                hasResiduals) const;

    // -------------------------------------------------------------------------
    // Homogeneity pipeline overlay
    // -------------------------------------------------------------------------

    /**
     * @brief Plots the original series with pre- and post-outlier detections overlaid.
     *
     * Renders both detection passes on one plot:
     *   - Pre-outlier points  : red triangles (raw series anomalies)
     *   - Post-outlier points : blue triangles (residual anomalies, mapped back to series)
     *
     * If both detection results are empty, no file is written.
     *
     * Output: homogeneity_[dataset]_[param]_outlier_overlay.[fmt]
     *
     * @param series         Original (gap-filled) series.
     * @param preDetection   Detection result from the pre-deseasonalization pass.
     * @param postDetection  Detection result from the post-deseasonalization pass.
     */
    void plotOutlierOverlay(const TimeSeries&   series,
                            const OutlierResult& preDetection,
                            const OutlierResult& postDetection) const;

private:

    AppConfig   m_cfg;
    std::string m_programName;  ///< "outlier" or "homogeneity" -- used in filename prefix.

    // -------------------------------------------------------------------------
    // Internal helpers
    // -------------------------------------------------------------------------

    /**
     * @brief Builds the output filename stem.
     *
     * Format: [programName]_[dataset]_[parameter]_[plotType]
     * dataset   = input filename stem (without extension), e.g. "CLIM_DATA_EX1"
     * parameter = series componentName, or "col_N" if empty
     */
    [[nodiscard]]
    std::string _stem(const TimeSeries&  series,
                      const std::string& plotType) const;

    /**
     * @brief Returns the full output image path for a given stem.
     */
    [[nodiscard]]
    std::filesystem::path _outPath(const std::string& stem) const;

    /**
     * @brief Returns the dataset name derived from the input file path.
     *
     * Extracts the filename stem from cfg.input.file, e.g. "CLIM_DATA_EX1"
     * from "C:/...INPUT/CLIM_DATA_EX1.txt".
     */
    [[nodiscard]]
    std::string _datasetName() const;

    /**
     * @brief Writes a two-column (mjd, value) temporary data file.
     * @param series Source series.
     * @param path   Output path.
     */
    void _writeSeriesDat(const TimeSeries&            series,
                         const std::filesystem::path& path) const;

    /**
     * @brief Writes outlier marker data: mjd and value at outlier positions.
     *
     * @param series    Source series providing timestamps and values.
     * @param detection Detection result with point indices.
     * @param path      Output path.
     */
    void _writeOutlierDat(const TimeSeries&            series,
                          const OutlierResult&          detection,
                          const std::filesystem::path& path) const;

    /**
     * @brief Returns the gnuplot terminal string for the configured format.
     */
    [[nodiscard]]
    std::string _terminal() const;

    /**
     * @brief Converts a filesystem path to forward-slash notation for gnuplot.
     */
    [[nodiscard]]
    static std::string _fwdSlash(const std::filesystem::path& p);
};

} // namespace loki::outlier
