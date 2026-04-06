#pragma once

#include "loki/kalman/kalmanResult.hpp"

#include "loki/core/config.hpp"

#include <filesystem>
#include <string>

namespace loki::kalman {

/**
 * @brief Produces all plots for the loki_kalman pipeline.
 *
 * All plots use inline gnuplot data (no temporary files).
 * Methods are gated by the corresponding PlotConfig flag -- the caller
 * must check the flag before calling, or use the plotAll() convenience method.
 *
 * Output file naming follows the convention:
 *   kalman_<dataset>_<component>_<plottype>.<format>
 *
 * ### Plot types
 *   overlay       -- original + filtered + (smoothed) + confidence band
 *   innovations   -- innovations vs time with +/-2sigma bounds
 *   gain          -- Kalman gain K[t][0] vs time
 *   uncertainty   -- filter/smoother std dev sqrt(P[t]) vs time
 *   forecast      -- filtered + forecast with growing prediction interval
 *   diagnostics   -- 4-panel residual diagnostics via loki::Plot::residualDiagnostics
 *
 * Gnuplot API: gp("command")  -- no << operator.
 * Terminal: pngcairo noenhanced font 'Sans,12'
 */
class PlotKalman {
public:

    /**
     * @brief Constructs the plotter bound to the given configuration.
     * @param cfg Full application configuration (imgDir, PlotConfig, format).
     */
    explicit PlotKalman(const AppConfig& cfg);

    /**
     * @brief Calls all enabled plot methods in sequence.
     *
     * Checks PlotConfig flags and skips disabled plots silently.
     *
     * @param result      Populated KalmanResult from KalmanAnalyzer::run().
     * @param dataset     Dataset name stem (for filename).
     * @param component   Series component name (for filename and title).
     */
    void plotAll(const KalmanResult&  result,
                 const std::string&   dataset,
                 const std::string&   component) const;

    /**
     * @brief Overlay plot: original + filtered + smoothed (if available) + confidence band.
     *
     * Confidence band is drawn at filtered +/- 2 * filteredStd.
     * If smoothed data is available, it is drawn on top in a distinct colour.
     */
    void plotOverlay(const KalmanResult& result,
                     const std::string&  dataset,
                     const std::string&  component) const;

    /**
     * @brief Innovations (one-step-ahead residuals) vs time with +/-2sigma envelope.
     *
     * Only epochs with hasObservation == true are plotted.
     */
    void plotInnovations(const KalmanResult& result,
                         const std::string&  dataset,
                         const std::string&  component) const;

    /**
     * @brief Kalman gain K[t][0] vs time.
     *
     * Useful for diagnosing convergence: gain should stabilise quickly for
     * stationary models when Q and R are correctly specified.
     */
    void plotGain(const KalmanResult& result,
                  const std::string&  dataset,
                  const std::string&  component) const;

    /**
     * @brief Filter (and smoother) uncertainty sqrt(P[t]) vs time.
     *
     * If smoothed std is available, both curves are overlaid.
     */
    void plotUncertainty(const KalmanResult& result,
                         const std::string&  dataset,
                         const std::string&  component) const;

    /**
     * @brief Forecast plot: filtered series + forecast with 95% prediction interval.
     *
     * Only drawn when result.forecastState is non-empty.
     * The last portion of the filtered series is shown for context.
     */
    void plotForecast(const KalmanResult& result,
                      const std::string&  dataset,
                      const std::string&  component) const;

    /**
     * @brief Four-panel residual diagnostics delegated to loki::Plot::residualDiagnostics.
     *
     * Uses innovations as residuals and predictedState as fitted values.
     */
    void plotDiagnostics(const KalmanResult& result,
                         const std::string&  dataset,
                         const std::string&  component) const;

private:

    AppConfig m_cfg;

    // ---- helpers ------------------------------------------------------------

    /**
     * @brief Builds the output path for a plot file.
     *   kalman_<dataset>_<component>_<plottype>.<format>
     */
    [[nodiscard]]
    std::filesystem::path outputPath(const std::string& dataset,
                                     const std::string& component,
                                     const std::string& plotType) const;

    /// @brief Converts a filesystem path to forward-slash form for gnuplot on Windows.
    [[nodiscard]]
    static std::string fwdSlash(const std::filesystem::path& p);

    /// @brief Builds the gnuplot terminal/output preamble.
    [[nodiscard]]
    std::string terminalCmd(const std::filesystem::path& outPath,
                            int widthPx  = 1400,
                            int heightPx = 600) const;
};

} // namespace loki::kalman
