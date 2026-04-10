#pragma once

#include <loki/core/config.hpp>
#include <loki/kriging/krigingResult.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <string>

namespace loki::kriging {

/**
 * @brief Plot generator for the Kriging analysis pipeline.
 *
 * All plots follow the project naming convention:
 *   kriging_[dataset]_[component]_[plottype].png
 *
 * Available plots (controlled via PlotConfig):
 *   kriging_variogram     -- empirical bins + fitted theoretical model.
 *   kriging_predictions   -- original series + Kriging estimates + CI band.
 *   kriging_crossval      -- LOO errors vs time + standardised error histogram.
 *
 * FUTURE (spatial mode): add kriging_map (2-D interpolation surface) and
 * kriging_variance_map (uncertainty surface).
 */
class PlotKriging {
public:

    /**
     * @brief Construct with full application config.
     * @param cfg Application config. kriging and plots sub-configs are used.
     */
    explicit PlotKriging(const AppConfig& cfg);

    /**
     * @brief Generate all configured plots for a Kriging result.
     *
     * @param result      Full Kriging result (variogram, predictions, CV).
     * @param ts          Gap-filled time series (for original data overlay).
     * @param datasetName Dataset name stem for file naming.
     */
    void plot(const KrigingResult& result,
              const TimeSeries&    ts,
              const std::string&   datasetName) const;

private:

    const AppConfig& m_cfg;

    /**
     * @brief Plot empirical variogram bins and fitted theoretical model curve.
     *
     * X-axis: lag (days for temporal Kriging).
     * Y-axis: semi-variance (data units squared).
     * Annotates nugget, sill, and range on the plot.
     */
    void _plotVariogram(const KrigingResult& result,
                        const std::string&   datasetName) const;

    /**
     * @brief Plot original series, Kriging estimates, and CI band.
     *
     * Observation points shown as filled circles.
     * CI band shown as shaded region between ci_lower and ci_upper.
     * Forecast horizon (non-observed targets) shown in a different colour.
     */
    void _plotPredictions(const KrigingResult& result,
                          const TimeSeries&    ts,
                          const std::string&   datasetName) const;

    /**
     * @brief Plot leave-one-out cross-validation diagnostics.
     *
     * Top panel  : LOO errors vs time with +/-2 sigma envelope.
     * Bottom panel: histogram of standardised errors with N(0,1) overlay.
     */
    void _plotCrossValidation(const KrigingResult& result,
                               const std::string&   datasetName) const;

    /**
     * @brief Convert backslashes to forward slashes (Windows gnuplot path fix).
     */
    static std::string fwdSlash(const std::string& path);

    /**
     * @brief Build output path string for a plot.
     *
     * Convention: [imgDir]/kriging_[dataset]_[component]_[plottype].[format]
     */
    std::string _plotPath(const std::string& dataset,
                          const std::string& component,
                          const std::string& plottype) const;
};

} // namespace loki::kriging
