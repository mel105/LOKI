#pragma once

#include <loki/homogeneity/changePointResult.hpp>
#include <loki/homogeneity/homogenizer.hpp>
#include <loki/timeseries/timeSeries.hpp>
#include <loki/core/config.hpp>
#include <loki/io/gnuplot.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace loki::homogeneity {

/**
 * @brief Generates gnuplot diagnostic plots for the homogenization pipeline.
 *
 * Each plot method writes a .plt script and invokes gnuplot to produce
 * the output image. Output files land in cfg.imgDir.
 *
 * Naming convention: <stationId>_<component>_<plotType>.<format>
 *
 * Plot catalogue:
 *   plotOriginal        -- original input series (line)
 *   plotSeasonalOverlay -- original + seasonal component (two lines)
 *   plotDeseasonalized  -- deseasonalized residuals (line)
 *   plotChangePoints    -- residuals + vertical lines at change points
 *   plotAdjusted        -- homogenized series (line)
 *   plotComparison      -- original vs adjusted overlay (two lines)
 *   plotShiftMagnitudes -- bar chart of shift magnitudes per change point
 *   plotAll             -- calls all enabled plots based on PlotConfig flags
 */
class PlotHomogeneity {
public:

    /**
     * @brief Constructs a PlotHomogeneity instance.
     * @param cfg Application configuration (imgDir, plots flags, output format).
     */
    explicit PlotHomogeneity(const AppConfig& cfg);

    /**
     * @brief Plots the original (gap-filled) input series.
     * @param series  Input time series.
     */
    void plotOriginal(const TimeSeries& series) const;

    /**
     * @brief Plots the original series with the seasonal component overlaid.
     * @param series   Original series.
     * @param seasonal Seasonal component values (same length as series).
     */
    void plotSeasonalOverlay(const TimeSeries&          series,
                             const std::vector<double>& seasonal) const;

    /**
     * @brief Plots the deseasonalized residual series.
     * @param series    Original series (used for time axis and metadata).
     * @param residuals Deseasonalized values.
     */
    void plotDeseasonalized(const TimeSeries&          series,
                            const std::vector<double>& residuals) const;

    /**
     * @brief Plots residuals with vertical lines marking detected change points.
     * @param series       Original series (time axis and metadata).
     * @param residuals    Deseasonalized values.
     * @param changePoints Detected change points.
     */
    void plotChangePoints(const TimeSeries&              series,
                          const std::vector<double>&     residuals,
                          const std::vector<ChangePoint>& changePoints) const;

    /**
     * @brief Plots the homogenized (adjusted) series.
     * @param adjusted Adjusted output series from SeriesAdjuster.
     */
    void plotAdjusted(const TimeSeries& adjusted) const;

    /**
     * @brief Plots original and adjusted series overlaid for comparison.
     * @param original Original (gap-filled) series.
     * @param adjusted Adjusted series.
     */
    void plotComparison(const TimeSeries& original,
                        const TimeSeries& adjusted) const;

    /**
     * @brief Plots a bar chart of shift magnitudes at each change point.
     * @param series       Original series (used for metadata / naming).
     * @param changePoints Detected change points with shift values.
     */
    void plotShiftMagnitudes(const TimeSeries&               series,
                             const std::vector<ChangePoint>&  changePoints) const;

    /**
     * @brief Runs all enabled plots based on PlotConfig flags.
     *
     * This is the main entry point called from main.cpp.
     *
     * @param original     Original (gap-filled) series.
     * @param result       Full homogenization result.
     * @param seasonal     Seasonal component (from Deseasonalizer::Result).
     */
    void plotAll(const TimeSeries&       original,
                 const HomogenizerResult& result,
                 const std::vector<double>& seasonal) const;

private:

    AppConfig m_cfg;

    // -- Internal helpers -----------------------------------------------------

    /**
     * @brief Returns the base output name for a series: "<stationId>_<component>".
     */
    std::string _baseName(const TimeSeries& series) const;

    /**
     * @brief Returns the full output path for a plot file.
     * @param baseName  Station+component identifier.
     * @param plotType  Short descriptor, e.g. "original", "deseas".
     */
    std::filesystem::path _outPath(const std::string& baseName,
                                   const std::string& plotType) const;

    /**
     * @brief Writes a temporary gnuplot data file from a TimeSeries.
     * @param series  Source series.
     * @param path    Output path for the .dat file.
     */
    void _writeSeriesDat(const TimeSeries&          series,
                         const std::filesystem::path& path) const;

    /**
     * @brief Writes a temporary gnuplot data file from MJD + values vectors.
     * @param times   MJD time axis.
     * @param values  Corresponding values.
     * @param path    Output path for the .dat file.
     */
    void _writeVectorDat(const std::vector<double>&   times,
                         const std::vector<double>&   values,
                         const std::filesystem::path& path) const;

    /**
     * @brief Invokes gnuplot on a script file. Logs a warning on failure.
     * @param scriptPath Path to the .plt script.
     */
    void _runGnuplot(const std::filesystem::path& scriptPath) const;

    /**
     * @brief Returns the gnuplot terminal string for the configured format.
     * E.g. "pngcairo noenhanced font 'Sans,12'" for png output.
     */
    std::string _terminal() const;
};

} // namespace loki::homogeneity
