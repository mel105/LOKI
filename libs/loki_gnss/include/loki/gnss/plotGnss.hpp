#pragma once

#include <loki/gnss/gnssResult.hpp>
#include <loki/gnss/gnssTypes.hpp>
#include <loki/gnss/keplerOrbit.hpp>
#include <loki/gnss/satVisibility.hpp>
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
 * Current plots:
 *   gnss_GOPE_satcount_timeseries.png    -- satellites tracked per epoch, per constellation
 *   gnss_GOPE_elevation_timeseries.png   -- mean satellite elevation per epoch
 *   gnss_GOPE_skyplot_polar.png          -- full-day sky coverage (polar)
 *   gnss_GOPE_dop_timeseries.png         -- GDOP/PDOP/HDOP/VDOP time series
 *   gnss_GOPE_spp_clockbias.png          -- GPS receiver clock bias [m] and [us]
 *   gnss_GOPE_spp_residuals.png          -- pseudorange residuals per epoch (mean + RMS)
 *   gnss_GOPE_spp_isb.png               -- inter-system biases GLO/GAL/BDS [m]
 *   gnss_GOPE_spp_position_ecef.png      -- SPP ECEF X/Y/Z time series
 *   gnss_GOPE_spp_position_error.png     -- dX/dY/dZ + 3D error vs reference
 *   gnss_GOPE_spp_position_scatter.png   -- horizontal scatter (dE vs dN)
 *
 * Planned (PPP phase):
 *   gnss_GOPE_snr_timeseries.png
 *   gnss_GOPE_iono_delay.png
 *   gnss_GOPE_tropo_delay.png
 *   gnss_GOPE_ppp_position_error.png
 *
 * All plots: pngcairo terminal, font 'Sans,12', noenhanced.
 * Datablocks ($name << EOD) used inside multiplot; plot '-' elsewhere.
 */
class PlotGnss {
public:
    explicit PlotGnss(const loki::AppConfig& cfg, const std::string& station);

    /**
     * @brief Produces all plots for the completed GnssResult.
     * Methods that have no data return immediately without creating a file.
     */
    void plotAll(const GnssResult& result,
                 const NavFile&    nav,
                 const ObsFile&    obs) const;

    // -------------------------------------------------------------------------
    //  Individual plot methods (public for debugging / selective re-run)
    // -------------------------------------------------------------------------

    /// Satellites tracked per epoch, one line per constellation.
    void plotSatCount(const ObsFile& obs) const;

    /// Mean satellite elevation per epoch [deg].
    void plotElevation(const ObsFile& obs, const NavFile& nav) const;

    /// Full-day sky coverage polar plot (azimuth vs elevation).
    void plotSkyplot(const ObsFile& obs, const NavFile& nav) const;

    /// GDOP / PDOP / HDOP / VDOP time series (4-panel).
    void plotDop(const std::vector<SppResult>& spp) const;

    /// GPS receiver clock bias [m] and [us] (2-panel).
    void plotClockBias(const std::vector<SppResult>& spp) const;

    /// Pseudorange residuals: mean absolute + RMS per epoch.
    void plotResiduals(const std::vector<SppResult>& spp) const;

    /// Inter-system biases for GLONASS / Galileo / BeiDou [m].
    void plotIsb(const std::vector<SppResult>& spp) const;

    /// SPP ECEF X, Y, Z time series (3-panel).
    void plotPositionEcef(const std::vector<SppResult>& spp) const;

    /// dX, dY, dZ components + 3D error vs reference (4-panel).
    /// Only produced if summary.hasReference == true.
    void plotPositionError(const std::vector<SppResult>& spp,
                           const SppSummary&             summary) const;

    /// Horizontal scatter plot dE vs dN in local ENU frame.
    /// Only produced if summary.hasReference == true.
    void plotPositionScatter(const std::vector<SppResult>& spp,
                             const SppSummary&             summary) const;

private:
    loki::AppConfig m_cfg;
    std::string     m_station;
    std::string     m_outDir;

    /// Shared skyplot engine (perPrn=false -> constellation colours, true -> PRN colours).
    void _plotSkyplotImpl(const ObsFile& obs, const NavFile& nav, bool perPrn) const;

    std::string outPath(const std::string& param, const std::string& plotType) const;
    static std::string fwdSlash(const std::string& p);
};

} // namespace loki::gnss
