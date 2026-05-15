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
                     AntexFile                                     antex,
                     std::vector<std::unique_ptr<CorrectionModel>> corrections)
    : m_cfg(std::move(cfg))
    , m_orbit(std::move(orbit))
    , m_osb(std::move(osb))
    , m_antex(std::move(antex))
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
// =============================================================================

double PppSolver::osbCorrection(GnssSystem system, int prn,
                                 const std::string& codeL1,
                                 const std::string& codeL2) const
{
    if (!m_cfg.applyOsb || m_osb.records.empty()) return 0.0;

    constexpr double NS_TO_M = SPEED_OF_LIGHT / 1.0e9;

    if (system == GnssSystem::GPS) {
        const double osb_userL1 = m_osb.getBiasNs(system, prn, codeL1);
        const double osb_refL1  = m_osb.getBiasNs(system, prn, "C1W");
        return -GPS_ALPHA * (osb_userL1 - osb_refL1) * NS_TO_M;
    }

    if (system == GnssSystem::GALILEO) {
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
//  satPcoCorrection
//
//  SP3 gives the satellite Centre of Mass (CoM).  The CLK product is
//  generated consistent with the Antenna Phase Centre (APC).  We therefore
//  need to shift the CoM position to the APC before computing the geometric
//  range.
//
//  Satellite body frame (IGS convention, yaw-steering):
//    Z-axis: nadir (pointing from satellite to Earth centre, negative radial).
//    Y-axis: normal to orbital plane toward Sun.
//    X-axis: completes right-hand frame (along solar panels in nominal yaw).
//
//  PCO in ANTEX for satellites is given in body frame [mm]:
//    pcoE = X body, pcoN = Y body, pcoU = Z body (radial/nadir).
//
//  We apply the scalar projection onto the satellite-to-receiver LoS,
//  which avoids the need for a full rotation matrix:
//    delta_rho = dot(pco_ecef, los_unit_vector)
//  where pco_ecef is approximated using the nadir and cross-track directions.
//
//  For the IF combination:
//    pco_IF = alpha * pco_L1 - beta * pco_L2
//
//  Lookup strategy: match antennaType prefix to constellation.
//    GPS   -> antennaType starts with "BLOCK "
//    GAL   -> antennaType starts with "GAL-" or "GALILEO"
//  The first matching entry is used (type-mean calibration; adequate for
//  dm-level corrections, differentiating individual SVs is PPP-AR level).
// =============================================================================

double PppSolver::satPcoCorrection(GnssSystem system,
                                    double satX, double satY, double satZ,
                                    double rxX,  double rxY,  double rxZ) const
{
    if (m_antex.antennas.empty()) return 0.0;

    const char* typePrefix  = nullptr;
    const char* freqLabel1  = nullptr;
    const char* freqLabel2  = nullptr;
    double alpha = 0.0, beta = 0.0;

    if (system == GnssSystem::GPS) {
        typePrefix = "BLOCK ";
        freqLabel1 = "G01";
        freqLabel2 = "G02";
        alpha = GPS_ALPHA;
        beta  = GPS_BETA;
    } else if (system == GnssSystem::GALILEO) {
        typePrefix = "GAL";
        freqLabel1 = "E01";
        freqLabel2 = "E05";
        alpha = GAL_ALPHA;
        beta  = GAL_BETA;
    } else {
        return 0.0;
    }

    const AntennaCalib* found = nullptr;
    for (const auto& cal : m_antex.antennas) {
        if (!cal.satellite) continue;
        if (cal.antennaType.find(typePrefix) != std::string::npos) {
            found = &cal;
            break;
        }
    }
    if (!found) return 0.0;

    double pcoX1 = 0.0, pcoY1 = 0.0, pcoZ1 = 0.0;
    double pcoX2 = 0.0, pcoY2 = 0.0, pcoZ2 = 0.0;
    bool   haveL1 = false, haveL2 = false;

    for (const auto& freq : found->freqs) {
        if (freq.obsCode == freqLabel1) {
            pcoX1 = freq.pcoE;
            pcoY1 = freq.pcoN;
            pcoZ1 = freq.pcoU;
            haveL1 = true;
        } else if (freq.obsCode == freqLabel2) {
            pcoX2 = freq.pcoE;
            pcoY2 = freq.pcoN;
            pcoZ2 = freq.pcoU;
            haveL2 = true;
        }
    }

    if (!haveL1 && !haveL2) return 0.0;

    const double pcoX_if = alpha * pcoX1 - beta * pcoX2;
    const double pcoY_if = alpha * pcoY1 - beta * pcoY2;
    const double pcoZ_if = alpha * pcoZ1 - beta * pcoZ2;

    constexpr double MM_TO_M = 1.0e-3;
    const double px = pcoX_if * MM_TO_M;
    const double py = pcoY_if * MM_TO_M;
    const double pz = pcoZ_if * MM_TO_M;

    // Satellite body-frame Z: nadir.
    const double rsat = std::sqrt(satX*satX + satY*satY + satZ*satZ);
    if (rsat < 1.0) return 0.0;
    const double zx = -satX / rsat;
    const double zy = -satY / rsat;
    const double zz = -satZ / rsat;

    // Y: Z cross north pole (0,0,1), normalised.
    double yx = zy * 1.0 - zz * 0.0;
    double yy = zz * 0.0 - zx * 1.0;
    double yz = zx * 0.0 - zy * 0.0;
    const double rY = std::sqrt(yx*yx + yy*yy + yz*yz);
    if (rY < 1.0e-9) return 0.0;
    yx /= rY; yy /= rY; yz /= rY;

    // X = Y cross Z.
    const double xx = yy*zz - yz*zy;
    const double xy = yz*zx - yx*zz;
    const double xz = yx*zy - yy*zx;

    // PCO offset in ECEF [m].
    const double pco_ecef_x = xx*px + yx*py + zx*pz;
    const double pco_ecef_y = xy*px + yy*py + zy*pz;
    const double pco_ecef_z = xz*px + yz*py + zz*pz;

    // Line-of-sight unit vector from satellite to receiver.
    const double los_x = rxX - satX;
    const double los_y = rxY - satY;
    const double los_z = rxZ - satZ;
    const double rlos  = std::sqrt(los_x*los_x + los_y*los_y + los_z*los_z);
    if (rlos < 1.0) return 0.0;
    const double ux = los_x / rlos;
    const double uy = los_y / rlos;
    const double uz = los_z / rlos;

    return -(pco_ecef_x*ux + pco_ecef_y*uy + pco_ecef_z*uz);
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
        else              { codes = {"L5X"};                freq = GAL_F5; }
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
        for (const auto& code : {"C5X"}) {
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

    const double p_if_raw = alpha * p1 - beta * p2;
    const double corr     = osbCorrection(system, satObs.prn, codeL1, codeL2);

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

    // Approximate marker position from OBS header.
    const double markerX = obs.receiver.approxX;
    const double markerY = obs.receiver.approxY;
    const double markerZ = obs.receiver.approxZ;

    // Translate marker to Antenna Reference Point (ARP) using the antenna
    // height delta from the OBS header.  antDeltaH is the vertical offset
    // of the ARP above the marker [m] in the local geodetic frame.
    const double rMkr = std::sqrt(markerX*markerX + markerY*markerY
                                  + markerZ*markerZ);
    double approxX = markerX;
    double approxY = markerY;
    double approxZ = markerZ;
    if (rMkr > 1.0 && obs.receiver.antDeltaH != 0.0) {
        approxX += obs.receiver.antDeltaH * (markerX / rMkr);
        approxY += obs.receiver.antDeltaH * (markerY / rMkr);
        approxZ += obs.receiver.antDeltaH * (markerZ / rMkr);
    }

    const loki::geodesy::EcefPoint ep0{approxX, approxY, approxZ};
    const auto   geod0  = loki::geodesy::ecef2geod(ep0, wgs84);
    const double latRad = geod0.lat * (std::numbers::pi / 180.0);
    const double hM     = geod0.h;

    // Day of year from first epoch.
    int doy = 1;
    if (!obs.epochs.empty()) {
        const auto ts = obs.epochs.front().time.toTimeStamp();
        static const int mdays[12] = {0,31,59,90,120,151,181,212,243,273,304,334};
        const int month = ts.month(), day = ts.day(), year = ts.year();
        doy = mdays[month-1] + day;
        if (month > 2 && ((year%4==0 && year%100!=0) || year%400==0)) ++doy;
    }

    const double zhd_zen    = SaastamoinenModel::zhd(latRad, hM);
    const double zwet_prior = SaastamoinenModel::zwd(hM);

    LOKI_INFO("PppSolver: lat=" + std::to_string(geod0.lat)
              + " deg  h=" + std::to_string(hM)
              + " m  ZHD=" + std::to_string(zhd_zen)
              + " m  ZWD_prior=" + std::to_string(zwet_prior)
              + " m  OSB_records=" + std::to_string(m_osb.records.size())
              + "  ANTEX_entries=" + std::to_string(m_antex.antennas.size())
              + "  antDeltaH=" + std::to_string(obs.receiver.antDeltaH) + " m");

    // -------------------------------------------------------------------------
    //  Code-WLS bootstrap: estimate [X, Y, Z, clk] from the first few epochs
    //  using IF pseudoranges only.  Gives the filter a good clock prior so
    //  that the large clock uncertainty does not corrupt ZWD.
    // -------------------------------------------------------------------------
    double bsX = approxX, bsY = approxY, bsZ = approxZ, bsClk = 0.0;
    {
        static constexpr int    BS_EPOCHS   = 5;
        static constexpr int    BS_MAX_ITER = 10;
        static constexpr double BS_CONV_M   = 0.1;

        int epochsDone = 0;
        for (const auto& epoch : obs.epochs) {
            if (epochsDone >= BS_EPOCHS) break;

            struct BsMeas {
                double pr;
                double xs, ys, zs;
                bool   isGal;
            };
            std::vector<BsMeas> meas;
            meas.reserve(epoch.satellites.size());

            for (const auto& satObs : epoch.satellites) {
                if (!isEnabled(satObs.system)) continue;

                PppObservation o = formIfObs(satObs, satObs.system);
                if (!o.valid) continue;

                const double tof  = o.ifCode / SPEED_OF_LIGHT;
                GpsTime txTime    = epoch.time;
                txTime.sow       -= tof;
                if (txTime.sow < 0.0) { txTime.sow += 604800.0; --txTime.week; }

                const SatState sat = m_orbit->compute({}, satObs.system,
                                                       satObs.prn, txTime);
                if (!sat.valid) continue;

                const auto rot = Relativity::rotateSatPosition(
                    {sat.x, sat.y, sat.z}, tof);

                double elDeg = 0.0, azDeg = 0.0;
                SatVisibility::ecefToElevAzim(
                    {rot[0], rot[1], rot[2]}, {bsX, bsY, bsZ},
                    elDeg, azDeg);
                if (elDeg < m_cfg.elevMaskDeg) continue;

                const double elRad    = elDeg * (std::numbers::pi / 180.0);
                const double tropoSl  = (zhd_zen + zwet_prior) / std::sin(elRad);
                const double satClkM  = sat.clkBias * SPEED_OF_LIGHT;
                if (std::abs(satClkM) > 300000.0) continue;

                const double prCorr = o.ifCode + satClkM - tropoSl;
                meas.push_back({prCorr, rot[0], rot[1], rot[2],
                                satObs.system == GnssSystem::GALILEO});
            }

            if (static_cast<int>(meas.size()) < 4) continue;

            bool needIsb = false;
            for (const auto& m2 : meas) if (m2.isGal) { needIsb = true; break; }
            const int nParams = needIsb ? 5 : 4;

            double isbGal = 0.0;
            for (int iter = 0; iter < BS_MAX_ITER; ++iter) {
                const int n = static_cast<int>(meas.size());
                Eigen::MatrixXd H = Eigen::MatrixXd::Zero(n, nParams);
                Eigen::VectorXd b(n);

                for (int i = 0; i < n; ++i) {
                    const double ddx = meas[static_cast<std::size_t>(i)].xs - bsX;
                    const double ddy = meas[static_cast<std::size_t>(i)].ys - bsY;
                    const double ddz = meas[static_cast<std::size_t>(i)].zs - bsZ;
                    const double rho = std::sqrt(ddx*ddx + ddy*ddy + ddz*ddz);
                    H(i, 0) = -ddx / rho;
                    H(i, 1) = -ddy / rho;
                    H(i, 2) = -ddz / rho;
                    H(i, 3) = 1.0;
                    const double isb = (needIsb && meas[static_cast<std::size_t>(i)].isGal)
                                       ? isbGal : 0.0;
                    if (needIsb && meas[static_cast<std::size_t>(i)].isGal)
                        H(i, 4) = 1.0;
                    b(i) = meas[static_cast<std::size_t>(i)].pr
                           - (rho + bsClk + isb);
                }

                const Eigen::VectorXd dx = (H.transpose() * H).ldlt().solve(
                    H.transpose() * b);

                bsX   += dx(0);
                bsY   += dx(1);
                bsZ   += dx(2);
                bsClk += dx(3);
                if (needIsb) isbGal += dx(4);

                if (dx.head(3).norm() < BS_CONV_M) break;
            }
            ++epochsDone;
        }

        LOKI_INFO("PppSolver bootstrap: clk=" + std::to_string(bsClk)
                  + " m  pos_shift="
                  + std::to_string(std::sqrt(
                      (bsX-approxX)*(bsX-approxX) +
                      (bsY-approxY)*(bsY-approxY) +
                      (bsZ-approxZ)*(bsZ-approxZ))) + " m");
    }

    PppFilter filter(m_cfg.filter);
    filter.init(bsX, bsY, bsZ, zwet_prior);
    filter.initClock(bsClk);

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
            o.satX    = rot[0]; o.satY = rot[1]; o.satZ = rot[2];
            o.satClkM = sat.clkBias * SPEED_OF_LIGHT;

            if (std::abs(o.satClkM) > 300000.0) continue;

            double elDeg = 0.0, azDeg = 0.0;
            SatVisibility::ecefToElevAzim({o.satX, o.satY, o.satZ},
                                           {rx, ry, rz}, elDeg, azDeg);
            if (elDeg < m_cfg.elevMaskDeg) continue;
            o.elevation = elDeg * (std::numbers::pi / 180.0);

            // Satellite PCO: SP3 CoM -> APC.
            if (m_cfg.pcoPcv) {
                const double pco = satPcoCorrection(satObs.system,
                                                     o.satX, o.satY, o.satZ,
                                                     rx, ry, rz);
                o.ifCode  -= pco;
                o.ifPhase -= pco;
            }

            // Troposphere: NMF split.
            o.tropoZhdM = zhd_zen * NiellMappingFunction::hydrostatic(
                latRad, hM, doy, o.elevation);
            o.mfWet = NiellMappingFunction::wet(latRad, o.elevation);

            // Receiver PCO (ARP -> APC), vertical component only.
            if (m_cfg.pcoPcv && !m_antex.antennas.empty()) {
                const std::string& antType = obs.receiver.antennaType;
                for (const auto& cal : m_antex.antennas) {
                    if (cal.satellite) continue;
                    if (cal.antennaType.find(antType.substr(0, 8))
                            == std::string::npos) continue;
                    double pcoU1 = 0.0, pcoU2 = 0.0;
                    bool h1 = false, h2 = false;
                    for (const auto& freq : cal.freqs) {
                        if (freq.obsCode == "G01") { pcoU1 = freq.pcoU; h1 = true; }
                        if (freq.obsCode == "G02") { pcoU2 = freq.pcoU; h2 = true; }
                    }
                    if (h1 || h2) {
                        const double pcoU_if = (h1 && h2)
                            ? (GPS_ALPHA * pcoU1 - GPS_BETA * pcoU2)
                            : (h1 ? pcoU1 : pcoU2);
                        const double corr = (pcoU_if * 1.0e-3)
                                            * std::sin(o.elevation);
                        o.ifCode  -= corr;
                        o.ifPhase -= corr;
                    }
                    break;
                }
            }

            // Phase windup.
            if (m_cfg.phaseWindup) {
                const auto satVel = m_orbit->velocity(
                    satObs.system, satObs.prn, txTime);
                const double cycles = phaseWindup.compute(
                    satObs.system, satObs.prn,
                    {o.satX, o.satY, o.satZ}, satVel,
                    {rx, ry, rz},
                    epoch.time.week, epoch.time.sow);
                const double lambdaNl =
                    (satObs.system == GnssSystem::GALILEO)
                    ? GAL_LAMBDA_NL : GPS_LAMBDA_NL;
                o.phaseWindupM = cycles * lambdaNl;
            }

            pppObs.push_back(std::move(o));
        }

        if (static_cast<int>(pppObs.size()) < 4) {
            results.push_back(result);
            continue;
        }

        // Kalman update: pass residuals collector into filter.
        std::vector<PppSatResidual> resVec;
        const bool converged = filter.measurementUpdate(pppObs, &resVec);

        filter.position(result.x, result.y, result.z);
        filter.positionSigma(result.sigmaXm, result.sigmaYm, result.sigmaZm);

        result.clkBiasM  = filter.clockBiasM();
        result.sigmaClkM = filter.clkSigmaM();
        result.ztdWetM   = filter.ztdWetM();
        result.sigmaZtdM = filter.ztdSigmaM();
        result.ztdTotalM = zhd_zen + result.ztdWetM;
        result.isbGalM   = filter.isbGalM();
        result.nSats     = static_cast<int>(pppObs.size());
        result.valid     = true;
        result.converged = converged;
        result.satResiduals = std::move(resVec);

        results.push_back(result);
    }

    LOKI_INFO("PppSolver: processed "
              + std::to_string(obs.epochs.size()) + " epochs, "
              + std::to_string(results.size()) + " results.");

    return results;
}
