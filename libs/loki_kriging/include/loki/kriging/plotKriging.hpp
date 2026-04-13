#pragma once

#include <loki/core/config.hpp>
#include <loki/kriging/krigingResult.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <string>

namespace loki::kriging {

/**
 * @brief Plot generator for the Kriging analysis pipeline.
 *
 * All plots follow the naming convention:
 *   kriging_[dataset]_[component]_[plottype].[format]
 *
 * Available plots (controlled via PlotConfig):
 *   kriging_variogram   -- empirical bins + fitted theoretical curve.
 *   kriging_predictions -- series + Kriging estimate + CI band (pink fill).
 *   kriging_crossval    -- LOO errors vs sample index + std error histogram.
 *
 * FUTURE (spatial): kriging_map and kriging_variance_map (2-D surfaces).
 */
class PlotKriging {
public:

    explicit PlotKriging(const AppConfig& cfg);

    void plot(const KrigingResult& result,
              const TimeSeries&    ts,
              const std::string&   datasetName) const;

private:

    const AppConfig& m_cfg;

    void _plotVariogram      (const KrigingResult& result,
                               const std::string&   datasetName) const;
    void _plotPredictions    (const KrigingResult& result,
                               const TimeSeries&    ts,
                               const std::string&   datasetName) const;
    void _plotCrossValidation(const KrigingResult& result,
                               const std::string&   datasetName) const;

    static std::string fwdSlash(const std::string& path);

    std::string _plotPath(const std::string& dataset,
                          const std::string& component,
                          const std::string& plottype) const;
};

} // namespace loki::kriging
