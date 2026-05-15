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
 *   gnss_<station>_ppp_<name>.png
 *
 * X-axis convention: all time-series plots use UTC HH:MM on the x-axis.
 * The UTC date (DD-MM-YYYY) appears in each plot title.
 *
 * PPP diagnostic plots (always produced when PPP is enabled):
 *   gnss_GOPE_ppp_sky.png               Sky distribution (per-PRN)
 *   gnss_GOPE_ppp_coord_lat.png         Latitude difference + sigma
 *   gnss_GOPE_ppp_coord_lon.png         Longitude difference + sigma
 *   gnss_GOPE_ppp_coord_hgt.png         Height difference + sigma
 *   gnss_GOPE_ppp_troposphere.png       ZTD total + sigma envelope
 *   gnss_GOPE_ppp_clock.png             Station clock offset [ns] + sigma
 *   gnss_GOPE_ppp_satcount.png          Tracked satellites + ambiguity reset %
 *   gnss_GOPE_ppp_residuals_phase.png   Carrier-phase residuals per PRN
 *   gnss_GOPE_ppp_residuals_code.png    Pseudorange residuals per PRN
 *   gnss_GOPE_ppp_ambiguity.png         Phase ambiguity status (Gantt)
 *
 * SPP / observation plots (flag-controlled):
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
 */
class PlotGnss {
public:
    explicit PlotGnss(const loki::AppConfig& cfg, const std::string& station);

    /**
     * @brief Produces all plots for the completed GnssResult.
     *
     * PPP diagnostic plots are always produced when result.hasPpp == true,
     * regardless of the figures flags (for debugging convenience).
     */
    void plotAll(const GnssResult& result,
                 const NavFile&    nav,
                 const ObsFile&    obs) const;

    // -------------------------------------------------------------------------
    //  Observation-level plots
    // -------------------------------------------------------------------------

    void plotSatCount(const ObsFile& obs) const;
    void plotElevation(const ObsFile& obs, const NavFile& nav) const;
    void plotSkyplot(const ObsFile& obs, const NavFile& nav) const;

    // -------------------------------------------------------------------------
    //  SPP plot methods
    // -------------------------------------------------------------------------

    void plotDop(const std::vector<SppResult>& spp) const;
    void plotClockBias(const std::vector<SppResult>& spp) const;
    void plotResiduals(const std::vector<SppResult>& spp) const;
    void plotIsb(const std::vector<SppResult>& spp) const;
    void plotPositionEcef(const std::vector<SppResult>& spp) const;
    void plotPositionError(const std::vector<SppResult>& spp,
                           const SppSummary& summary) const;
    void plotPositionScatter(const std::vector<SppResult>& spp,
                             const SppSummary& summary) const;

    // -------------------------------------------------------------------------
    //  PPP diagnostic plots (always produced for debugging)
    // -------------------------------------------------------------------------

    /// Sky distribution, per-PRN coloured, with legend box.
    void plotPppSky(const ObsFile& obs, const NavFile& nav) const;

    /**
     * @brief Latitude / longitude / height difference vs reference + sigma.
     *
     * Three separate single-panel plots.  Blue line = solution, orange line
     * = 1-sigma formal error (right y-axis).  Convergence period in grey.
     * Only produced when pppSummary.hasReference == true.
     */
    void plotPppCoordDiff(const std::vector<PppResult>& ppp,
                           const PppSummary& summary) const;

    /**
     * @brief ZTD total time series with 1-sigma shaded envelope.
     */
    void plotPppTroposphere(const std::vector<PppResult>& ppp) const;

    /**
     * @brief Station clock offset [ns] + 1-sigma (right axis).
     */
    void plotPppClock(const std::vector<PppResult>& ppp) const;

    /**
     * @brief Number of tracked satellites per epoch (left) and
     *        percentage of ambiguity resets (right, red crosses).
     */
    void plotPppSatCount(const std::vector<PppResult>& ppp) const;

    /**
     * @brief Carrier-phase post-fit residuals, one colour per PRN.
     *
     * Only satellites from the configured constellations are plotted.
     */
    void plotPppResidualsPhase(const std::vector<PppResult>& ppp) const;

    /**
     * @brief Pseudorange post-fit residuals, one colour per PRN.
     */
    void plotPppResidualsCode(const std::vector<PppResult>& ppp) const;

    /**
     * @brief Phase ambiguity status Gantt chart.
     *
     * One row per satellite.  Colour indicates ambiguity state:
     *   green  = converged arc
     *   yellow = float / pre-convergence arc
     *   red    = new arc (reset)
     *
     * The chart is drawn using thin horizontal bars and vertical tick marks
     * at epoch boundaries where a reset occurs.
     */
    void plotPppAmbiguity(const std::vector<PppResult>& ppp) const;

    // Legacy combined PPP plots (kept for backwards compatibility).
    void plotPppPositionError(const std::vector<PppResult>& ppp,
                               const PppSummary& summary) const;
    void plotPppClockBias(const std::vector<PppResult>& ppp) const;

private:
    loki::AppConfig m_cfg;
    std::string     m_station;
    std::string     m_outDir;

    // Shared sky-plot engine.
    void _plotSkyplotImpl(const ObsFile& obs, const NavFile& nav,
                          bool perPrn) const;

    std::string outPath(const std::string& param,
                        const std::string& plotType) const;
    static std::string fwdSlash(const std::string& p);

    // Converts a GpsTime to a fractional hour value (0-24) for gnuplot
    // time axis.  Returns seconds-since-midnight as a double.
    static double toSecOfDay(const GpsTime& t);

    // Returns the UTC date string "DD-MM-YYYY" for a GpsTime.
    static std::string utcDateStr(const GpsTime& t);

    // Emits gnuplot commands to set up a UTC HH:MM x-axis.
    // dayStartSec is toSecOfDay() of the first epoch (anchors the axis).
    static std::string xAxisUtcSetup(const std::string& xlabel = "UTC");
};

} // namespace loki::gnss
