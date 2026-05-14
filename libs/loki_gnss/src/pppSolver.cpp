#include <loki/gnss/pppSolver.hpp>
#include <loki/gnss/troposphere.hpp>
#include <loki/gnss/gnssUtils.hpp>
#include <loki/gnss/relativity.hpp>
#include <loki/gnss/satVisibility.hpp>
#include <loki/geodesy/coordTransform.hpp>
#include <loki/math/ellipsoid.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <cmath>
#include <numbers>

using namespace loki;
using namespace loki::gnss;

// =============================================================================
//  Constructor
// =============================================================================

PppSolver::PppSolver(Config                                        cfg,
                     std::shared_ptr<Sp3Orbit>                     orbit,
                     OsbFile                                       osb,
                     std::vector<std::unique_ptr<CorrectionModel>> corrections)
    : m_cfg(std::move(cfg))
    , m_orbit(std::move(orbit))
    , m_osb(std::move(osb))
    , m_corrections(std::move(corrections))
{
    if (!m_orbit)
        throw ConfigException("PppSolver: Sp3Orbit must not be null.");
}

// =============================================================================
//  isEnabled
// =============================================================================

bool PppSolver::isEnabled(GnssSystem system) const
{
    const std::string name = [&]() -> std::string {
        switch (system) {
            case GnssSystem::GPS:     return "GPS";
            case GnssSystem::GALILEO: return "GALILEO";
            default:                  return "";
        }
    }();
    if (name.empty()) return false;
    return std::find(m_cfg.constellations.begin(),
                     m_cfg.constellations.end(), name)
           != m_cfg.constellations.end();
}

// =============================================================================
//  osbCorrection
//
//  CODE MGEX CLK is consistent with C1W/C2W for GPS, C1X/C5X for Galileo.
//
//  For GPS using C1C + C2W:
//    The CLK reference signals are C1W + C2W.
//    User measures C1C + C2W.
//    IF correction = -(alpha*OSB_C1C - beta*OSB_C2W) * c/1e9    [user side]
//                  + (alpha*OSB_C1W - beta*OSB_C2W) * c/1e9     [CLK side]
//                 = -alpha*(OSB_C1C - OSB_C1W) * c/1e9
//    (C2W terms cancel because same signal on both sides.)
//
//  For Galileo using C1X + C5X:
//    CLK is consistent with C1X/C5X -- correction = 0.
//    (The OSB records exist but the net effect cancels.)
// =============================================================================

double PppSolver::osbCorrection(GnssSystem system, int prn,
                                 const std::string& codeL1,
                                 const std::string& codeL2) const
{
    if (!m_cfg.applyOsb || m_osb.records.empty()) return 0.0;

    constexpr double NS_TO_M = SPEED_OF_LIGHT / 1.0e9;

    if (system == GnssSystem::GPS) {
        // CLK reference: C1W + C2W.
        // User: C1C (or other L1 code) + C2W.
        // L2 is the same (C2W) -> beta terms cancel.
        // Correction = -alpha * (OSB_userL1 - OSB_C1W) * c/ns
        const double osb_userL1 = m_osb.getBiasNs(system, prn, codeL1);
        const double osb_refL1  = m_osb.getBiasNs(system, prn, "C1W");
        return -GPS_ALPHA * (osb_userL1 - osb_refL1) * NS_TO_M;
    }

    if (system == GnssSystem::GALILEO) {
        // CLK reference: C1X + C5X.
        // User: C1X + C5X (same) -> correction = 0 for standard Galileo signals.
        // If user measures different signals, compute full correction:
        const double osb_userL1 = m_osb.getBiasNs(system, prn, codeL1);
        const double osb_refL1  = m_osb.getBiasNs(system, prn, "C1X");
        const double osb_userL2 = m_osb.getBiasNs(system, prn, codeL2);
        const double osb_refL2  = m_osb.getBiasNs(system, prn, "C5X");
        return -(GAL_ALPHA * (osb_userL1 - osb_refL1)
               - GAL_BETA  * (osb_userL2 - osb_refL2)) * NS_TO_M;
    }

    return 0.0;
}

// =============================================================================
//  selectPhase
// =============================================================================

double PppSolver::selectPhase(const SatObs& satObs,
                               GnssSystem    system,
                               int           freqIdx)
{
    std::vector<std::string> codes;
    double freq = 0.0;

    if (system == GnssSystem::GPS) {
        if (freqIdx == 1) { codes = {"L1C", "L1W", "L1X"}; freq = GPS_F1; }
        else              { codes = {"L2W", "L2C", "L2X"}; freq = GPS_F2; }
    } else if (system == GnssSystem::GALILEO) {
        if (freqIdx == 1) { codes = {"L1X", "L1C"};        freq = GAL_F1; }
        else              { codes = {"L5X", "L7X", "L8X"}; freq = GAL_F5; }
    } else {
        return 0.0;
    }

    if (freq == 0.0) return 0.0;
    const double lambda = SPEED_OF_LIGHT / freq;

    for (const auto& code : codes) {
        const auto it = satObs.obs.find(code);
        if (it != satObs.obs.end() && it->second.valid && it->second.value != 0.0)
            return it->second.value * lambda;
    }
    return 0.0;
}

// =============================================================================
//  formIfObs
// =============================================================================

PppObservation PppSolver::formIfObs(const SatObs& satObs,
                                     GnssSystem    system) const
{
    PppObservation o;
    o.system = system;
    o.prn    = satObs.prn;

    double p1 = 0.0, p2 = 0.0;
    std::string codeL1, codeL2;

    if (system == GnssSystem::GPS) {
        for (const auto& code : {"C1C", "C1W", "C1X"}) {
            const auto it = satObs.obs.find(code);
            if (it != satObs.obs.end() && it->second.valid && it->second.value > 0.0)
                { p1 = it->second.value; codeL1 = code; break; }
        }
        for (const auto& code : {"C2W", "C2C", "C2X"}) {
            const auto it = satObs.obs.find(code);
            if (it != satObs.obs.end() && it->second.valid && it->second.value > 0.0)
                { p2 = it->second.value; codeL2 = code; break; }
        }
    } else if (system == GnssSystem::GALILEO) {
        for (const auto& code : {"C1X", "C1C"}) {
            const auto it = satObs.obs.find(code);
            if (it != satObs.obs.end() && it->second.valid && it->second.value > 0.0)
                { p1 = it->second.value; codeL1 = code; break; }
        }
        for (const auto& code : {"C5X", "C7X", "C8X"}) {
            const auto it = satObs.obs.find(code);
            if (it != satObs.obs.end() && it->second.valid && it->second.value > 0.0)
                { p2 = it->second.value; codeL2 = code; break; }
        }
    }

    if (p1 == 0.0 || p2 == 0.0) return o;

    const double l1 = selectPhase(satObs, system, 1);
    const double l2 = selectPhase(satObs, system, 2);
    if (l1 == 0.0 || l2 == 0.0) return o;

    const double alpha = (system == GnssSystem::GPS) ? GPS_ALPHA : GAL_ALPHA;
    const double beta  = (system == GnssSystem::GPS) ? GPS_BETA  : GAL_BETA;

    // Raw IF pseudorange.
    const double p_if_raw = alpha * p1 - beta * p2;

    // Apply OSB correction: aligns user observations with CLK reference signals.
    const double corr = osbCorrection(system, satObs.prn, codeL1, codeL2);

    o.ifCode  = p_if_raw + corr;
    o.ifPhase = alpha * l1 - beta * l2;
    o.gfObs   = l1 - l2;
    o.valid   = true;
    return o;
}

// =============================================================================
//  solveAll
// =============================================================================

std::vector<PppResult> PppSolver::solveAll(
    const ObsFile&    obs,
    const NavFile&    /*nav*/,
    const GnssConfig& /*gnssCfg*/) const
{
    std::vector<PppResult> results;
    results.reserve(obs.epochs.size());

    const loki::math::Ellipsoid wgs84 =
        loki::math::makeEllipsoid(loki::math::EllipsoidModel::WGS84);

    const std::array<double,3> approxPos{
        obs.receiver.approxX, obs.receiver.approxY, obs.receiver.approxZ};

    const loki::geodesy::EcefPoint ep0{approxPos[0], approxPos[1], approxPos[2]};
    const auto   geod0  = loki::geodesy::ecef2geod(ep0, wgs84);
    const double latRad = geod0.lat * (std::numbers::pi / 180.0);
    const double hM     = geod0.h;

    int doy = 75;
    if (!obs.epochs.empty()) {
        const auto ts = obs.epochs.front().time.toTimeStamp();
        static const int mdays[12] = {0,31,59,90,120,151,181,212,243,273,304,334};
        int m = ts.month(), d = ts.day(), y = ts.year();
        doy = mdays[m-1] + d;
        if (m > 2 && ((y%4==0 && y%100!=0) || y%400==0)) ++doy;
    }

    const double zhd_zen    = SaastamoinenModel::zhd(latRad, hM);
    const double zwet_prior = SaastamoinenModel::zwd(hM);

    LOKI_INFO("PppSolver: lat=" + std::to_string(geod0.lat)
              + " deg  h=" + std::to_string(hM)
              + " m  ZHD=" + std::to_string(zhd_zen)
              + " m  ZWD_prior=" + std::to_string(zwet_prior)
              + " m  OSB_records=" + std::to_string(m_osb.records.size()));

    PppFilter filter(m_cfg.filter);
    filter.init(approxPos[0], approxPos[1], approxPos[2], zwet_prior);

    PhaseWindup phaseWindup;
    double prevEpochSec = -1.0;

    for (const auto& epoch : obs.epochs) {
        PppResult result;
        result.time = epoch.time;

        const double epochSec = epoch.time.totalSeconds();
        const double dt = (prevEpochSec > 0.0)
            ? (epochSec - prevEpochSec) : obs.interval;
        prevEpochSec = epochSec;

        if (dt > 0.0 && dt < 3600.0)
            filter.timeUpdate(dt);

        double rx, ry, rz;
        filter.position(rx, ry, rz);

        std::vector<PppObservation> pppObs;
        pppObs.reserve(epoch.satellites.size());

        for (const auto& satObs : epoch.satellites) {
            if (!isEnabled(satObs.system)) continue;

            PppObservation o = formIfObs(satObs, satObs.system);
            if (!o.valid) continue;

            const double tof = o.ifCode / SPEED_OF_LIGHT;
            GpsTime txTime   = epoch.time;
            txTime.sow      -= tof;
            if (txTime.sow < 0.0) { txTime.sow += 604800.0; --txTime.week; }

            const SatState sat = m_orbit->compute({}, satObs.system,
                                                   satObs.prn, txTime);
            if (!sat.valid) continue;

            const auto rot = Relativity::rotateSatPosition(
                {sat.x, sat.y, sat.z}, tof);
            o.satX = rot[0]; o.satY = rot[1]; o.satZ = rot[2];
            o.satClkM = sat.clkBias * SPEED_OF_LIGHT;

            double elDeg = 0.0, azDeg = 0.0;
            SatVisibility::ecefToElevAzim({o.satX, o.satY, o.satZ},
                                           {rx, ry, rz}, elDeg, azDeg);
            if (elDeg < m_cfg.elevMaskDeg) continue;
            o.elevation = elDeg * (std::numbers::pi / 180.0);

            // Troposphere: NMF ZHD/ZWD split.
            o.tropoZhdM = zhd_zen * NiellMappingFunction::hydrostatic(
                latRad, hM, doy, o.elevation);
            o.mfWet = NiellMappingFunction::wet(latRad, o.elevation);

            if (m_cfg.phaseWindup) {
                const auto satVel = m_orbit->velocity(
                    satObs.system, satObs.prn, txTime);
                const double cycles = phaseWindup.compute(
                    satObs.system, satObs.prn,
                    {o.satX, o.satY, o.satZ}, satVel,
                    {rx, ry, rz},
                    epoch.time.week, epoch.time.sow);
                o.phaseWindupM = cycles * (SPEED_OF_LIGHT / GPS_F1);
            }

            pppObs.push_back(std::move(o));
        }

        if (static_cast<int>(pppObs.size()) < 4) {
            results.push_back(result);
            continue;
        }

        const bool converged = filter.measurementUpdate(pppObs);

        double x, y, z;
        filter.position(x, y, z);
        result.x         = x;
        result.y         = y;
        result.z         = z;
        result.clkBiasM  = filter.clockBiasM();
        result.ztdWetM   = filter.ztdWetM();
        result.isbGalM   = filter.isbGalM();
        result.nSats     = static_cast<int>(pppObs.size());
        result.valid     = true;
        result.converged = converged;
        result.ztdTotalM = zhd_zen + result.ztdWetM;

        results.push_back(result);
    }

    LOKI_INFO("PppSolver: processed "
              + std::to_string(obs.epochs.size()) + " epochs, "
              + std::to_string(results.size()) + " results.");

    return results;
}
