#pragma once

#include <loki/core/config.hpp>
#include <loki/evt/evtResult.hpp>

#include <string>

namespace loki::evt {

/**
 * @brief Gnuplot-based plotter for EVT analysis outputs.
 *
 * Produces up to five plot types depending on PlotConfig flags:
 *   - evtMeanExcess   : Mean excess plot with selected threshold marker.
 *   - evtStability    : xi and sigma stability plots vs threshold candidates.
 *   - evtReturnLevels : Return level plot (estimate + CI band) on log T axis.
 *   - evtExceedances  : Empirical exceedance rate vs threshold (diagnostic).
 *   - evtGpdFit       : Empirical CDF of exceedances vs fitted GPD (Q-Q style).
 *
 * All plots follow LOKI naming convention:
 *   evt_<dataset>_<component>_<plottype>.<format>
 */
class PlotEvt {
public:

    /**
     * @brief Construct with application config.
     * @param cfg Application config. plots and evt sub-configs are used.
     */
    explicit PlotEvt(const AppConfig& cfg);

    /**
     * @brief Produce all configured plots for one EVT result.
     *
     * @param result      Completed EvtResult from EvtAnalyzer.
     * @param datasetName File stem (e.g. "CLIM_DATA_EX1").
     * @param component   Series component name (e.g. "iwv").
     */
    void plot(const EvtResult& result,
               const std::string& datasetName,
               const std::string& component) const;

private:

    const AppConfig& m_cfg;

    void _plotMeanExcess  (const EvtResult& r,
                            const std::string& base) const;

    void _plotStability   (const EvtResult& r,
                            const std::string& base) const;

    void _plotReturnLevels(const EvtResult& r,
                            const std::string& base) const;

    void _plotExceedances (const EvtResult& r,
                            const std::string& base) const;

    void _plotGpdFit      (const EvtResult& r,
                            const std::string& base) const;

    /// Convert backslashes to forward slashes for gnuplot on Windows.
    static std::string fwdSlash(const std::string& p);

    /// Build output path: imgDir / filename.
    std::string _outPath(const std::string& filename) const;
};

} // namespace loki::evt
