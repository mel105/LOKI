#pragma once

#include <loki/core/config.hpp>
#include <loki/qc/qcResult.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <filesystem>
#include <string>

namespace loki::qc {

/**
 * @brief Generates gnuplot diagnostic plots for the loki_qc pipeline.
 *
 * Plot catalogue (controlled by PlotConfig flags):
 *   plotCoverage  (qcCoverage)  -- time-axis bar chart: green/orange/red per day (or epoch).
 *   plotHistogram (qcHistogram) -- value histogram with normal fit; delegated to loki::Plot.
 *   plotAcf       (qcAcf)       -- ACF of valid observations; delegated to loki::Plot.
 *
 * Naming convention: qc_[dataset]_[component]_[plottype].[format]
 */
class PlotQc {
public:

    /**
     * @brief Constructs a PlotQc bound to the given application configuration.
     * @param cfg Application configuration.
     */
    explicit PlotQc(const AppConfig& cfg);

    // -------------------------------------------------------------------------
    // Individual plots
    // -------------------------------------------------------------------------

    /**
     * @brief Coverage bar chart: one bar per day, coloured by worst flag in that day.
     *
     * Colour priority: red = gap, orange = outlier, green = valid.
     * For series with fewer than 1000 epochs, one bar per epoch is shown instead.
     *
     * Output: qc_[dataset]_[component]_coverage.[fmt]
     */
    void plotCoverage(const TimeSeries& series, const QcResult& result) const;

    /**
     * @brief Value histogram with normal fit overlay.
     *
     * Delegates to loki::Plot::histogram(). Uses bins from PlotConfig::options
     * if set (> 0), otherwise defaults to 30.
     *
     * Output: qc_[dataset]_[component]_histogram.[fmt]
     */
    void plotHistogram(const TimeSeries& series, const QcResult& result) const;

    /**
     * @brief ACF of valid observations up to PlotConfig::options.acfMaxLag lags.
     *
     * Delegates to loki::Plot::acf(). Skips NaN values before computing ACF.
     *
     * Output: qc_[dataset]_[component]_acf.[fmt]
     */
    void plotAcf(const TimeSeries& series, const QcResult& result) const;

    /**
     * @brief Runs all enabled plots based on PlotConfig flags.
     */
    void plotAll(const TimeSeries& series, const QcResult& result) const;

private:

    AppConfig m_cfg;

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    /// Builds output stem: "qc_[dataset]_[component]_[plotType]"
    [[nodiscard]] std::string _stem(const QcResult& result,
                                    const std::string& plotType) const;

    /// Builds absolute output path from stem.
    [[nodiscard]] std::filesystem::path _outPath(const std::string& stem) const;

    /// Returns gnuplot terminal string for configured output format.
    [[nodiscard]] std::string _terminal(int widthPx = 1400, int heightPx = 400) const;

    /// Converts backslashes to forward slashes for gnuplot on Windows.
    [[nodiscard]] static std::string _fwdSlash(const std::filesystem::path& p);
};

} // namespace loki::qc
