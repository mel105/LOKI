#pragma once

#include <loki/core/config.hpp>
#include <loki/regression/regressionResult.hpp>
#include <loki/regression/regressionDiagnostics.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace loki::regression {

/**
 * @brief Produces diagnostic plots for the loki_regression pipeline.
 *
 * All plots are written to AppConfig::imgDir. Output filenames follow the
 * project naming convention:
 *   regression_[dataset]_[parameter]_[plottype].[format]
 *
 * Available plots (all controlled by PlotConfig flags):
 *   - overlay          : original series + fitted curve (regressionOverlay)
 *   - residuals        : residuals in time (regressionResiduals)
 *   - qq_bands         : QQ plot of residuals with confidence bands (regressionQqBands)
 *   - cdf              : ECDF vs theoretical normal CDF (regressionCdfPlot)
 *   - residual_acf     : ACF of residuals with 95% confidence band (regressionResidualAcf)
 *   - residual_hist    : histogram of residuals with normal fit (regressionResidualHist)
 *   - influence        : Cook's distance bar chart with 4/n threshold (regressionInfluence)
 *   - leverage         : leverage vs standardized residuals scatter (regressionLeverage)
 */
class PlotRegression {
public:
    /**
     * @brief Constructs a PlotRegression bound to the given application configuration.
     * @param cfg Application configuration. AppConfig::imgDir must be set.
     */
    explicit PlotRegression(const AppConfig& cfg);

    /**
     * @brief Renders all enabled plots for one series.
     *
     * Checks each PlotConfig flag before calling the individual plot method.
     * Failures in individual plots are caught, logged as warnings, and do not
     * abort the remaining plots.
     *
     * @param original  Raw series as loaded (used for overlay).
     * @param result    RegressionResult from any Regressor::fit().
     * @param influence InfluenceMeasures from RegressionDiagnostics (used for
     *                  influence and leverage plots). May be default-constructed
     *                  if those plots are disabled.
     */
    void plotAll(const TimeSeries&       original,
                 const RegressionResult& result,
                 const InfluenceMeasures& influence) const;

    /**
     * @brief Overlay: original series + fitted curve on one axes.
     * Output: regression_[dataset]_[param]_overlay.[fmt]
     */
    void plotOverlay(const TimeSeries&       original,
                     const RegressionResult& result) const;

    /**
     * @brief Residuals in time as a line plot with zero baseline.
     * Output: regression_[dataset]_[param]_residuals.[fmt]
     */
    void plotResiduals(const TimeSeries&       original,
                       const RegressionResult& result) const;

    /**
     * @brief QQ plot of residuals against the standard normal with confidence bands.
     * Output: regression_[dataset]_[param]_qq_bands.[fmt]
     */
    void plotQqBands(const TimeSeries&       original,
                     const RegressionResult& result) const;

    /**
     * @brief ECDF of residuals vs theoretical normal CDF.
     * Output: regression_[dataset]_[param]_cdf.[fmt]
     */
    void plotCdf(const TimeSeries&       original,
                 const RegressionResult& result) const;

    /**
     * @brief ACF of residuals with 95% confidence band (+/- 1.96/sqrt(n)).
     * Output: regression_[dataset]_[param]_residual_acf.[fmt]
     */
    void plotResidualAcf(const TimeSeries&       original,
                         const RegressionResult& result) const;

    /**
     * @brief Histogram of residuals with fitted normal curve.
     * Output: regression_[dataset]_[param]_residual_hist.[fmt]
     */
    void plotResidualHist(const TimeSeries&       original,
                          const RegressionResult& result) const;

    /**
     * @brief Cook's distance bar chart with horizontal threshold line at 4/n.
     * Output: regression_[dataset]_[param]_influence.[fmt]
     */
    void plotInfluence(const TimeSeries&        original,
                       const RegressionResult&  result,
                       const InfluenceMeasures& influence) const;

    /**
     * @brief Leverage (h_ii) vs standardized residuals scatter plot.
     *
     * Vertical reference lines at leverageThreshold and horizontal at +/-2.
     * Output: regression_[dataset]_[param]_leverage.[fmt]
     */
    void plotLeverage(const TimeSeries&        original,
                      const RegressionResult&  result,
                      const InfluenceMeasures& influence) const;

private:
    AppConfig m_cfg;

    [[nodiscard]] std::string           _baseName(const TimeSeries& series) const;
    [[nodiscard]] std::filesystem::path _outPath(const std::string& base,
                                                  const std::string& plotType) const;
    [[nodiscard]] std::string           _terminal(int widthPx = 1200,
                                                   int heightPx = 400) const;

    void _writeSeriesDat(const TimeSeries&            series,
                         const std::filesystem::path& path) const;

    void _writeVectorDat(const std::vector<double>&   x,
                         const std::vector<double>&   y,
                         const std::filesystem::path& path) const;

    [[nodiscard]] static std::vector<std::pair<double, double>>
    _computeAcf(const std::vector<double>& values, int maxLag);
};

} // namespace loki::regression
