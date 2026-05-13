#pragma once

#include <loki/gnss/gnssResult.hpp>
#include <loki/gnss/gnssTypes.hpp>
#include <loki/gnss/keplerOrbit.hpp>
#include <loki/gnss/satVisibility.hpp>
#include <loki/gnss/pppSolver.hpp>
#include <loki/math/ellipsoid.hpp>
#include <loki/core/config.hpp>

#include <string>

namespace loki::gnss {

/**
 * @brief Gnuplot-based plotter for all loki_gnss pipeline outputs.
 *
 * Naming convention (LOKI standard):
 *   gnss_<station>_<parameter>_<plottype>.png
 *
 * SPP plots:
 *   gnss_GOPE_satcount_timeseries.png
 *   gnss_GOPE_elevation_timeseries.png
 *   gnss_GOPE_skyplot_polar.png
 *   gnss_GOPE_skyplot_prn.png
 *   gnss_GOPE_dop_timeseries.png
 *   gnss_GOPE_spp_clockbias.png
 *   gnss_GOPE_spp_residuals.png
 *   gnss_GOPE_spp_isb.png
 *   gnss_GOPE_spp_position_ecef.png
 *   gnss_GOPE_spp_position_error.png
 *   gnss_GOPE_spp_position_scatter.png
 *
 * PPP plots:
 *   gnss_GOPE_ppp_position_error.png
 *   gnss_GOPE_ppp_troposphere.png
 *   gnss_GOPE_ppp_clockbias.png
 *
 * All plots: pngcairo terminal, font 'Sans,12', noenhanced.
 * Datablocks ($name << EOD) used inside multiplot; plot '-' elsewhere.
 */
class PlotGnss {
public:
    explicit PlotGnss(const loki::AppConfig& cfg, const std::string& station);

    /**
     * @brief Produces all plots for the completed GnssResult.
     * Methods that have no data or disabled flags return immediately.
     */
    void plotAll(const GnssResult& result,
                 const NavFile&    nav,
                 const ObsFile&    obs) const;

    // -------------------------------------------------------------------------
    //  Observation-level plots (independent of positioning method)
    // -------------------------------------------------------------------------

    /// Satellites tracked per epoch, one line per constellation.
    void plotSatCount(const ObsFile& obs) const;

    /// Mean satellite elevation per epoch [deg].
    void plotElevation(const ObsFile& obs, const NavFile& nav) const;

    /// Full-day sky coverage plots (constellation colours + per-PRN colours).
    void plotSkyplot(const ObsFile& obs, const NavFile& nav) const;

    // -------------------------------------------------------------------------
    //  SPP plot methods
    // -------------------------------------------------------------------------

    /// GDOP / PDOP / HDOP / VDOP time series.
    void plotDop(const std::vector<SppResult>& spp) const;

    /// GPS receiver clock bias [m] and [us] (2-panel).
    void plotClockBias(const std::vector<SppResult>& spp) const;

    /// Pseudorange residuals: mean absolute + RMS per epoch.
    void plotResiduals(const std::vector<SppResult>& spp) const;

    /// Inter-system biases for GLONASS / Galileo / BeiDou [m].
    void plotIsb(const std::vector<SppResult>& spp) const;

    /// SPP ECEF X, Y, Z time series (3-panel).
    void plotPositionEcef(const std::vector<SppResult>& spp) const;

    /// dX, dY, dZ + 3D error vs reference (4-panel).
    void plotPositionError(const std::vector<SppResult>& spp,
                           const SppSummary& summary) const;

    /// Horizontal scatter plot dE vs dN in local ENU frame.
    void plotPositionScatter(const std::vector<SppResult>& spp,
                             const SppSummary& summary) const;

    // -------------------------------------------------------------------------
    //  PPP plot methods
    // -------------------------------------------------------------------------

    /**
     * @brief dX/dY/dZ and 3D error vs reference (4-panel).
     *
     * Pre-convergence epochs shown in grey; converged epochs in colour.
     * Only produced when pppSummary.hasReference == true.
     */
    void plotPppPositionError(const std::vector<PppResult>& ppp,
                               const PppSummary& summary) const;

    /**
     * @brief Kalman-estimated ZTD wet and ZTD total (2-panel).
     *
     * Pre-convergence in grey; converged in colour.
     */
    void plotPppTroposphere(const std::vector<PppResult>& ppp) const;

    /**
     * @brief Receiver clock bias [m] and [us] (2-panel).
     *
     * Pre-convergence in grey; converged in colour.
     */
    void plotPppClockBias(const std::vector<PppResult>& ppp) const;

private:
    loki::AppConfig m_cfg;
    std::string     m_station;
    std::string     m_outDir;

    /// Shared sky-plot engine (perPrn=false -> constellation colours).
    void _plotSkyplotImpl(const ObsFile& obs, const NavFile& nav, bool perPrn) const;

    std::string outPath(const std::string& param, const std::string& plotType) const;
    static std::string fwdSlash(const std::string& p);
};

} // namespace loki::gnss
