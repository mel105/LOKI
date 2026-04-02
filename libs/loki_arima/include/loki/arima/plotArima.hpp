#pragma once

#include <loki/arima/arimaResult.hpp>
#include <loki/core/config.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace loki::arima {

/**
 * @brief Produces ARIMA-specific diagnostic plots for the loki_arima pipeline.
 *
 * Generates only plots that are not already covered by loki::Plot:
 *   - overlay     : deseasonalized residuals + ARIMA fitted values vs MJD
 *   - forecast    : last nTail observations + point forecast + shaded 95% interval
 *
 * The four-panel residual diagnostic (residuals/time, QQ, histogram, ACF) is
 * delegated to loki::Plot::residualDiagnostics() to avoid code duplication.
 *
 * Output filenames follow the project convention:
 *   arima_[dataset]_[parameter]_[plottype].[format]
 */
class PlotArima {
public:

    /**
     * @brief Constructs a PlotArima bound to the given application configuration.
     * @param cfg Application configuration. AppConfig::imgDir must be set.
     */
    explicit PlotArima(const AppConfig& cfg);

    /**
     * @brief Renders all enabled ARIMA plots for one series.
     *
     * Checks PlotConfig flags before calling individual plot methods.
     * Failures in individual plots are caught, logged as warnings, and do not
     * abort the remaining plots.
     *
     * Delegates residual diagnostics (4-panel) to loki::Plot::residualDiagnostics()
     * when cfg.plots.arimaDiagnostics is true.
     *
     * @param filled     Gap-filled TimeSeries (provides MJD timestamps).
     * @param residuals  Deseasonalized residuals aligned to filled.
     * @param result     Fitted ArimaResult.
     * @param forecast   ForecastResult. May be empty (horizon == 0).
     */
    void plotAll(const TimeSeries&          filled,
                 const std::vector<double>& residuals,
                 const ArimaResult&         result,
                 const ForecastResult&      forecast,
                 int                        forecastTail=1461) const;

    /**
     * @brief Overlay: deseasonalized residuals (grey) + ARIMA fitted values (red).
     *
     * The fitted values are aligned to the tail of the residuals series
     * (the first d + D*s observations are consumed by differencing).
     * A horizontal zero line is drawn for reference.
     *
     * Output: arima_[dataset]_[param]_overlay.[fmt]
     *
     * @param filled     Gap-filled TimeSeries (provides MJD timestamps).
     * @param residuals  Deseasonalized residuals (same length as filled).
     * @param result     Fitted ArimaResult (result.fitted aligned to tail).
     * @throws DataException if residuals or result.fitted are empty.
     * @throws IoException   on gnuplot or file I/O failure.
     */
    void plotOverlay(const TimeSeries&          filled,
                     const std::vector<double>& residuals,
                     const ArimaResult&         result) const;

    /**
     * @brief Forecast plot: last nTail observations + forecast + shaded 95% interval.
     *
     * The observed portion shows the last nTail deseasonalized residuals (blue line).
     * A vertical dashed line marks the fit/forecast boundary.
     * The forecast region shows the point forecast (red) and a shaded band for
     * the 95% prediction interval.
     * The forecast x-axis is a step index relative to the last observation.
     *
     * Output: arima_[dataset]_[param]_forecast.[fmt]
     *
     * @param filled     Gap-filled TimeSeries (provides MJD timestamps for observed).
     * @param residuals  Deseasonalized residuals aligned to filled.
     * @param result     Fitted ArimaResult.
     * @param forecast   ForecastResult with forecast, lower95, upper95.
     * @param nTail      Number of observed points to show before the forecast.
     * @throws DataException if forecast.forecast is empty.
     * @throws IoException   on gnuplot or file I/O failure.
     */
    void plotForecast(const TimeSeries&          filled,
                      const std::vector<double>& residuals,
                      const ArimaResult&         result,
                      const ForecastResult&      forecast,
                      int                        nTail = 1461) const;

private:

    AppConfig m_cfg;

    /// Build the base filename stem: "arima_[dataset]_[componentName]"
    [[nodiscard]] std::string _baseName(const TimeSeries& ts) const;

    /// Build the full output path: imgDir / base_plotType.format
    [[nodiscard]] std::filesystem::path _outPath(const std::string& base,
                                                  const std::string& plotType) const;

    /// Build gnuplot terminal string for the configured output format.
    [[nodiscard]] std::string _terminal(int widthPx = 1200, int heightPx = 400) const;

    /// Convert forward slashes for gnuplot on Windows.
    [[nodiscard]] static std::string _fwdSlash(const std::filesystem::path& p);
};

} // namespace loki::arima
