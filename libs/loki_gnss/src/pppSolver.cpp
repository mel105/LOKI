#include <loki/gnss/pppSolver.hpp>
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
                     std::vector<std::unique_ptr<CorrectionModel>> corrections)
    : m_cfg(std::move(cfg))
    , m_orbit(std::move(orbit))
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
//  selectPhase
// =============================================================================

double PppSolver::selectPhase(const SatObs& satObs,
                               GnssSystem    system,
                               int           freqIdx)
{
    // Returns phase in metres (RINEX stores in cycles * wavelength = metres).
    // RINEX 3 carrier-phase unit: full cycles stored as metres? No:
    // RINEX 3 phase observations are stored in CYCLES (not metres).
    // Conversion: metres = cycles * (c / frequency).

    // Observation code priorities.
    std::vector<std::string> codes;
    if (system == GnssSystem::GPS) {
        if (freqIdx == 1) codes = {"L1C", "L1W", "L1X"};
        else              codes = {"L2W", "L2C", "L2X"};
    } else if (system == GnssSystem::GALILEO) {
        if (freqIdx == 1) codes = {"L1X", "L1C"};
        else              codes = {"L5X", "L7X", "L8X"};
    } else {
        return 0.0;
    }

    // Frequency for cycles -> metres conversion.
    double freq = 0.0;
    if (system == GnssSystem::GPS) {
        freq = (freqIdx == 1) ? GPS_F1 : GPS_F2;
    } else {
        freq = (freqIdx == 1) ? GAL_F1 : GAL_F5;
    }
    if (freq == 0.0) return 0.0;
    const double lambda = SPEED_OF_LIGHT / freq;

    for (const auto& code : codes) {
        const auto it = satObs.obs.find(code);
        if (it != satObs.obs.end() && it->second.valid && it->second.value != 0.0)
            return it->second.value * lambda;  // cycles * lambda = metres
    }
    return 0.0;
}

// =============================================================================
//  formIfObs
// =============================================================================

PppObservation PppSolver::formIfObs(const SatObs& satObs, GnssSystem system)
{
    PppObservation o;
    o.system = system;
    o.prn    = satObs.prn;

    // Select pseudoranges on two frequencies.
    double p1 = 0.0, p2 = 0.0;
    if (system == GnssSystem::GPS) {
        // P1 (L1): C1C > C1W
        for (const auto& code : {"C1C", "C1W", "C1X"}) {
            const auto it = satObs.obs.find(code);
            if (it != satObs.obs.end() && it->second.valid && it->second.value > 0.0) {
                p1 = it->second.value; break;
            }
        }
        // P2 (L2): C2W > C2C
        for (const auto& code : {"C2W", "C2C", "C2X"}) {
            const auto it = satObs.obs.find(code);
            if (it != satObs.obs.end() && it->second.valid && it->second.value > 0.0) {
                p2 = it->second.value; break;
            }
        }
    } else if (system == GnssSystem::GALILEO) {
        for (const auto& code : {"C1X", "C1C"}) {
            const auto it = satObs.obs.find(code);
            if (it != satObs.obs.end() && it->second.valid && it->second.value > 0.0) {
                p1 = it->second.value; break;
            }
        }
        for (const auto& code : {"C5X", "C7X", "C8X"}) {
            const auto it = satObs.obs.find(code);
            if (it != satObs.obs.end() && it->second.valid && it->second.value > 0.0) {
                p2 = it->second.value; break;
            }
        }
    }

    if (p1 == 0.0 || p2 == 0.0) return o;  // valid remains false

    // Select carrier-phase on two frequencies [m].
    const double l1 = selectPhase(satObs, system, 1);
    const double l2 = selectPhase(satObs, system, 2);
    if (l1 == 0.0 || l2 == 0.0) return o;

    // Form IF combinations.
    const double alpha = (system == GnssSystem::GPS) ? GPS_ALPHA : GAL_ALPHA;
    const double beta  = (system == GnssSystem::GPS) ? GPS_BETA  : GAL_BETA;

    o.ifCode  = alpha * p1 - beta * p2;
    o.ifPhase = alpha * l1 - beta * l2;

    // Geometry-free combination for cycle slip detection: L1 - L2 [m].
    o.gfObs = l1 - l2;

    o.valid = true;
    return o;
}

// =============================================================================
//  saastamoinenZhd
// =============================================================================

double PppSolver::saastamoinenZhd(double lat_rad, double h_m,
                                   double elevation_rad)
{
    const double P = 1013.25 * std::pow(1.0 - 2.2557e-5 * h_m, 5.2568);
    const double zhd = 0.0022768 * P
                     / (1.0 - 0.00266 * std::cos(2.0 * lat_rad)
                            - 2.8e-7  * h_m);
    const double mf = 1.0 / std::sin(std::max(elevation_rad, 0.05));
    return zhd * mf;
}

// =============================================================================
//  solveAll
// =============================================================================

std::vector<PppResult> PppSolver::solveAll(
    const ObsFile&    obs,
    const NavFile&    /*nav*/,
    const GnssConfig& gnssCfg) const
{
    std::vector<PppResult> results;
    results.reserve(obs.epochs.size());

    const loki::math::Ellipsoid wgs84 = loki::math::makeEllipsoid(
        loki::math::EllipsoidModel::WGS84);

    const std::array<double,3> approxPos{
        obs.receiver.approxX,
        obs.receiver.approxY,
        obs.receiver.approxZ
    };

    // Convert approximate position to geodetic for a priori troposphere.
    const loki::geodesy::EcefPoint ep0{approxPos[0], approxPos[1], approxPos[2]};
    const auto geod0 = loki::geodesy::ecef2geod(ep0, wgs84);
    const double latRad0 = geod0.lat * (std::numbers::pi / 180.0);
    const double hM0     = geod0.h;

    // Initialise filter.
    // ZTD wet prior from Saastamoinen (full model at zenith).
    const double T_K   = 288.15 - 6.5e-3 * hM0;
    const double e_hPa = 6.108 * std::exp(17.15 * (T_K - 273.15) / (T_K - 38.25));
    const double zwet_prior = 0.002277 * (1255.0 / T_K + 0.05) * e_hPa;

    PppFilter filter(m_cfg.filter);
    filter.init(approxPos[0], approxPos[1], approxPos[2], zwet_prior);

    PhaseWindup phaseWindup;

    double prevEpochSec = -1.0;

    for (const auto& epoch : obs.epochs) {
        PppResult result;
        result.time = epoch.time;

        const double epochSec = epoch.time.totalSeconds();
        const double dt = (prevEpochSec > 0.0) ? (epochSec - prevEpochSec)
                                               : obs.interval;
        prevEpochSec = epochSec;

        // Time update.
        if (dt > 0.0 && dt < 3600.0)
            filter.timeUpdate(dt);

        // Get current filter position for geometry computations.
        double rx, ry, rz;
        filter.position(rx, ry, rz);

        // Convert current position to geodetic.
        const loki::geodesy::EcefPoint epCur{rx, ry, rz};
        const auto geodCur = loki::geodesy::ecef2geod(epCur, wgs84);
        const double latRad = geodCur.lat * (std::numbers::pi / 180.0);
        const double hM     = geodCur.h;

        // Build PppObservation vector.
        std::vector<PppObservation> pppObs;
        pppObs.reserve(epoch.satellites.size());

        for (const auto& satObs : epoch.satellites) {
            if (!isEnabled(satObs.system)) continue;

            PppObservation o = formIfObs(satObs, satObs.system);
            if (!o.valid) continue;

            // Approximate signal time of flight.
            const double tof = o.ifCode / SPEED_OF_LIGHT;
            GpsTime txTime   = epoch.time;
            txTime.sow      -= tof;
            if (txTime.sow < 0.0) { txTime.sow += 604800.0; --txTime.week; }

            // Satellite state from SP3.
            const SatState sat = m_orbit->compute({}, satObs.system,
                                                    satObs.prn, txTime);
            if (!sat.valid) continue;

            // Sagnac rotation.
            const auto rotated = Relativity::rotateSatPosition(
                {sat.x, sat.y, sat.z}, tof);
            o.satX = rotated[0];
            o.satY = rotated[1];
            o.satZ = rotated[2];

            // Satellite clock [m].
            o.satClkM = sat.clkBias * SPEED_OF_LIGHT;

            // Elevation and azimuth.
            double elDeg = 0.0, azDeg = 0.0;
            SatVisibility::ecefToElevAzim({o.satX, o.satY, o.satZ},
                                           {rx, ry, rz}, elDeg, azDeg);
            if (elDeg < m_cfg.elevMaskDeg) continue;

            o.elevation = elDeg * (std::numbers::pi / 180.0);

            // A priori ZHD (dry troposphere, mapped to slant).
            o.tropoM = saastamoinenZhd(latRad, hM, o.elevation);

            // Phase windup [m].
            if (m_cfg.phaseWindup) {
                const auto satVel = m_orbit->velocity(satObs.system,
                                                       satObs.prn, txTime);
                const double cycles = phaseWindup.compute(
                    satObs.system, satObs.prn,
                    {o.satX, o.satY, o.satZ},
                    satVel,
                    {rx, ry, rz},
                    epoch.time.week, epoch.time.sow);

                // Wavelength of IF combination (approximate: GPS L1).
                const double lambda_if = SPEED_OF_LIGHT / GPS_F1;
                o.phaseWindupM = cycles * lambda_if;
            }

            // Apply any injected correction models (e.g. placeholder for future
            // PCO/PCV -- currently empty for PPP).
            if (!m_corrections.empty()) {
                CorrectionInput ci;
                ci.lat       = latRad;
                ci.lon       = geodCur.lon * (std::numbers::pi / 180.0);
                ci.h         = hM;
                ci.elevation = o.elevation;
                ci.gpsSow    = epoch.time.sow;
                ci.gpsWeek   = epoch.time.week;
                ci.satX = o.satX; ci.satY = o.satY; ci.satZ = o.satZ;
                ci.staX = rx;     ci.staY = ry;     ci.staZ = rz;
                for (const auto& model : m_corrections)
                    o.tropoM += model->delay(ci);
            }

            pppObs.push_back(std::move(o));
        }

        if (static_cast<int>(pppObs.size()) < 4) {
            results.push_back(result);
            continue;
        }

        // Measurement update.
        const bool converged = filter.measurementUpdate(pppObs);

        // Fill result.
        double x, y, z;
        filter.position(x, y, z);
        result.x         = x;
        result.y         = y;
        result.z         = z;
        result.clkBiasM  = filter.clockBiasM();
        result.ztdWetM   = filter.ztdWetM();
        result.nSats     = static_cast<int>(pppObs.size());
        result.valid     = true;
        result.converged = converged;

        // Total ZTD = a priori ZHD + estimated ZWD (both at zenith).
        const double P_hPa = 1013.25 * std::pow(1.0 - 2.2557e-5 * hM, 5.2568);
        const double zhd_zen = 0.0022768 * P_hPa
                             / (1.0 - 0.00266 * std::cos(2.0 * latRad)
                                    - 2.8e-7  * hM);
        result.ztdTotalM = zhd_zen + result.ztdWetM;

        results.push_back(result);
    }

    LOKI_INFO("PppSolver: processed " + std::to_string(obs.epochs.size())
              + " epochs, " + std::to_string(results.size()) + " results.");

    return results;
}
