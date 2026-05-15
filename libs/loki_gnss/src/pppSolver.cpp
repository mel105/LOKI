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

    const double osb_userL1 = m_osb.getBiasNs(system, prn, codeL1);
    const double osb_refL1  = m_osb.getBiasNs(system, prn, "C1W");

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

    // Determine antenna type prefix and frequency labels for lookup.
    const char* typePrefix  = nullptr;
    const char* freqLabel1  = nullptr;
    const char* freqLabel2  = nullptr;
    double alpha = 0.0, beta = 0.0;

    if (system == GnssSystem::GPS) {
        typePrefix = "BLOCK ";
        freqLabel1 = "G01";   // L1 in ANTEX satellite freq labels
        freqLabel2 = "G02";   // L2
        alpha = GPS_ALPHA;
        beta  = GPS_BETA;
    } else if (system == GnssSystem::GALILEO) {
        typePrefix = "GAL";
        freqLabel1 = "E01";   // E1 in ANTEX satellite freq labels
        freqLabel2 = "E05";   // E5a
        alpha = GAL_ALPHA;
        beta  = GAL_BETA;
    } else {
        return 0.0;
    }

    // Find the first matching satellite antenna entry.
    // ANTEX satellite entries have calib.satellite == true.
    const AntennaCalib* found = nullptr;
    for (const auto& cal : m_antex.antennas) {
        if (!cal.satellite) continue;
        if (cal.antennaType.find(typePrefix) != std::string::npos) {
            found = &cal;
            break;
        }
    }
    if (!found) return 0.0;

    // Extract PCO for each frequency [mm].
    double pcoX1 = 0.0, pcoY1 = 0.0, pcoZ1 = 0.0;
    double pcoX2 = 0.0, pcoY2 = 0.0, pcoZ2 = 0.0;
    bool   haveL1 = false, haveL2 = false;

    for (const auto& freq : found->freqs) {
        if (freq.obsCode == freqLabel1) {
            pcoX1 = freq.pcoE;  // X body [mm]
            pcoY1 = freq.pcoN;  // Y body [mm]
            pcoZ1 = freq.pcoU;  // Z body [mm] (radial/nadir)
            haveL1 = true;
        } else if (freq.obsCode == freqLabel2) {
            pcoX2 = freq.pcoE;
            pcoY2 = freq.pcoN;
            pcoZ2 = freq.pcoU;
            haveL2 = true;
        }
    }

    if (!haveL1 && !haveL2) return 0.0;

    // IF-combined PCO in body frame [mm].
    const double pcoX_if = alpha * pcoX1 - beta * pcoX2;
    const double pcoY_if = alpha * pcoY1 - beta * pcoY2;
    const double pcoZ_if = alpha * pcoZ1 - beta * pcoZ2;

    // Convert to metres.
    constexpr double MM_TO_M = 1.0e-3;
    const double px = pcoX_if * MM_TO_M;
    const double py = pcoY_if * MM_TO_M;
    const double pz = pcoZ_if * MM_TO_M;

    // Build satellite body frame unit vectors from orbital geometry.
    //
    // Z_body (nadir): unit vector from satellite to Earth centre.
    const double rsat = std::sqrt(satX*satX + satY*satY + satZ*satZ);
    if (rsat < 1.0) return 0.0;
    const double zx = -satX / rsat;
    const double zy = -satY / rsat;
    const double zz = -satZ / rsat;

    // For a Sun-pointing Y-axis we would need the Sun position.
    // Approximation: Y_body = Z_body cross (0,0,1) normalised.
    // This is adequate for the dominant Z (nadir/radial) PCO component
    // which is what matters most for range corrections.
    // The X and Y PCO components are typically < 30 mm for GPS Block II
    // and their cross-track/along-track projection onto LoS averages near
    // zero over a pass, contributing < 1 cm systematic error.
    double yx, yy, yz;
    // Cross product of Z_body with north pole (0,0,1).
    yx = zy * 1.0 - zz * 0.0;   //  zy*1 - zz*0
    yy = zz * 0.0 - zx * 1.0;   //  zz*0 - zx*1
    yz = zx * 0.0 - zy * 0.0;   //  zx*0 - zy*0
    const double rY = std::sqrt(yx*yx + yy*yy + yz*yz);
    if (rY < 1.0e-9) return 0.0;  // satellite near pole -- degenerate
    yx /= rY; yy /= rY; yz /= rY;

    // X_body = Y_body cross Z_body.
    const double xx = yy*zz - yz*zy;
    const double xy = yz*zx - yx*zz;
    const double xz = yx*zy - yy*zx;

    // PCO offset in ECEF [m].
    const double pco_ecef_x = xx*px + yx*py + zx*pz;
    const double pco_ecef_y = xy*px + yy*py + zy*pz;
    const double pco_ecef_z = xz*px + yz*py + zz*pz;

    // Line-of-sight unit vector from satellite (APC approx = CoM) to receiver.
    const double los_x = rxX - satX;
    const double los_y = rxY - satY;
    const double los_z = rxZ - satZ;
    const double rlos  = std::sqrt(los_x*los_x + los_y*los_y + los_z*los_z);
    if (rlos < 1.0) return 0.0;
    const double ux = los_x / rlos;
    const double uy = los_y / rlos;
    const double uz = los_z / rlos;

    // Scalar projection: range increases when APC is farther from receiver
    // than CoM along LoS.  The correction is added to modeled range.
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
        else              { codes = {"L5X"};               freq = GAL_F5; }
        //else              { codes = {"L5X", "L7X", "L8X"}; freq = GAL_F5; }
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
        //for (const auto& code : {"C5X", "C7X", "C8X"}) {
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
    const double corr = osbCorrection(system, satObs.prn, codeL1, codeL2);

    o.ifCode  = p_if_raw + corr;
    o.ifPhase = alpha * l1 - beta * l2;
    o.gfObs   = l1 - l2;
    o.valid   = true;

    /*
    if (satObs.prn == 15 && system == GnssSystem::GPS) {
    LOKI_INFO("PPP_DEBUG G15: p1=" + std::to_string(p1)
              + " p2=" + std::to_string(p2)
              + " l1=" + std::to_string(l1)
              + " l2=" + std::to_string(l2)
              + " ifCode_raw=" + std::to_string(alpha*p1 - beta*p2)
              + " corr=" + std::to_string(osbCorrection(system, satObs.prn, codeL1, codeL2))
              + " ifPhase=" + std::to_string(alpha*l1 - beta*l2)
              + " codeL1=" + codeL1 + " codeL2=" + codeL2);
    }
    */
    // debug
    static bool osbLogged = false;
    if (!osbLogged && system == GnssSystem::GPS) {
        LOKI_INFO("PPP_DEBUG OSB prn=" + std::to_string(satObs.prn)
                + " corr=" + std::to_string(osbCorrection(system, satObs.prn, codeL1, codeL2)));
    }
    if (satObs.prn == 30) osbLogged = true;
    // end debug

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
    // Convert to ECEF: the Up direction at the marker is the normalised
    // ECEF position vector (geocentric approximation, error < 0.2 mm).
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

    int doy = 75;
    if (!obs.epochs.empty()) {
        const auto ts = obs.epochs.front().time.toTimeStamp();
        static const int mdays[12] = {0,31,59,90,120,151,181,212,243,273,304,334};
        int month = ts.month(), day = ts.day(), year = ts.year();
        doy = mdays[month-1] + day;
        if (month > 2 && ((year%4==0 && year%100!=0) || year%400==0)) ++doy;
    }

    const double zhd_zen    = SaastamoinenModel::zhd(latRad, hM);
    const double zwet_prior = SaastamoinenModel::zwd(hM);

    // DEBUG 
    LOKI_INFO("[!!! NOVY DEBUG]PPP_DEBUG tropo: ZHD_zen=" + std::to_string(zhd_zen)
          + " ZWD_prior=" + std::to_string(zwet_prior)
          + " lat_deg=" + std::to_string(geod0.lat)
          + " h_m=" + std::to_string(hM));

    // END DEBUG
    LOKI_INFO("PppSolver: lat=" + std::to_string(geod0.lat)
              + " deg  h=" + std::to_string(hM)
              + " m  ZHD=" + std::to_string(zhd_zen)
              + " m  ZWD_prior=" + std::to_string(zwet_prior)
              + " m  OSB_records=" + std::to_string(m_osb.records.size())
              + "  ANTEX_entries=" + std::to_string(m_antex.antennas.size())
              + "  antDeltaH=" + std::to_string(obs.receiver.antDeltaH) + " m");

    // -------------------------------------------------------------------------
    //  Code-WLS bootstrap: estimate [X, Y, Z, clk] from the first few epochs
    //  using IF pseudoranges only, no ZWD free parameter (ZHD applied as fixed
    //  correction).  This gives the filter a good clock prior so that the large
    //  clock uncertainty (~-107 km for TRIMBLE ALLOY) does not corrupt ZWD on
    //  the first Kalman update.
    //
    //  The bootstrap does NOT estimate ZWD -- it uses the Saastamoinen prior as
    //  a fixed correction: tropo_slant = (ZHD + ZWD_prior) / sin(el).
    //  The error in this approximation is < 5 cm, well within code noise.
    // -------------------------------------------------------------------------
    double bsX = approxX, bsY = approxY, bsZ = approxZ, bsClk = 0.0;
    {
        static constexpr int    BS_EPOCHS    = 5;
        static constexpr int    BS_MAX_ITER  = 10;
        static constexpr double BS_CONV_M    = 0.1;

        int epochsDone = 0;
        for (const auto& epoch : obs.epochs) {
            if (epochsDone >= BS_EPOCHS) break;

            // Collect IF code observations with satellite positions.
            struct BsMeas {
                double pr;                  // corrected IF pseudorange [m]
                double xs, ys, zs;          // satellite ECEF [m]
                bool   isGal;               // needs ISB column
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
                const double tropoSl  = (zhd_zen + zwet_prior)
                                        / std::sin(elRad);
                const double satClkM  = sat.clkBias * SPEED_OF_LIGHT;

                // Skip satellites with missing/sentinel clock values.
                if (std::abs(satClkM) > 300000.0) continue;

                // Corrected pseudorange: subtract satellite clock and tropo.
                const double prCorr = o.ifCode + satClkM - tropoSl;

                meas.push_back({prCorr, rot[0], rot[1], rot[2],
                                satObs.system == GnssSystem::GALILEO});
            }

            if (static_cast<int>(meas.size()) < 4) continue;

            // Determine whether Galileo ISB column is needed.
            bool needIsb = false;
            for (const auto& m2 : meas) if (m2.isGal) { needIsb = true; break; }
            const int nParams = needIsb ? 5 : 4;   // X Y Z clk [isbGal]

            // Iterative WLS: state = [dX, dY, dZ, dClk, (dIsb)].
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

                const Eigen::MatrixXd HtH = H.transpose() * H;
                const Eigen::VectorXd Htb = H.transpose() * b;
                const Eigen::VectorXd dx  = HtH.ldlt().solve(Htb);

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
        // debug
        LOKI_INFO("[!!! NOVY DEBUG] PPP_DEBUG bootstrap: bsX=" + std::to_string(bsX)
          + " bsY=" + std::to_string(bsY)
          + " bsZ=" + std::to_string(bsZ)
          + " bsClk=" + std::to_string(bsClk));
        // end debug
    }

    PppFilter filter(m_cfg.filter);
    // Initialise with bootstrapped position and clock so the first Kalman
    // update starts with a near-correct clock (~0 m residual vs ~-107 km
    // without bootstrap) and ZWD is not corrupted by clock uncertainty.
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
            o.satX = rot[0]; o.satY = rot[1]; o.satZ = rot[2];
            o.satClkM = sat.clkBias * SPEED_OF_LIGHT;

            // Reject satellites with physically impossible clock values.
            // Valid satellite clock bias: |dt_s| < 1 ms = 300 km.
            // Larger values indicate a missing or sentinel CLK record.
            if (std::abs(o.satClkM) > 300000.0) continue;

            double elDeg = 0.0, azDeg = 0.0;
            SatVisibility::ecefToElevAzim({o.satX, o.satY, o.satZ},
                                           {rx, ry, rz}, elDeg, azDeg);
            if (elDeg < m_cfg.elevMaskDeg) continue;
            o.elevation = elDeg * (std::numbers::pi / 180.0);

            // Satellite PCO correction: SP3 CoM -> APC [m].
            // Added to ifCode and ifPhase because both are referenced to APC,
            // while the geometric range rho is computed to the CoM position.
            if (m_cfg.pcoPcv) {
                const double pco = satPcoCorrection(satObs.system,
                                                     o.satX, o.satY, o.satZ,
                                                     rx, ry, rz);
                // Apply to both code and phase: the filter model is
                // rho + clk + tropo + N, where rho is CoM distance.
                // Subtracting the PCO correction from observations brings
                // them to the CoM reference frame, consistent with the
                // filter's geometric range computation.
                o.ifCode  -= pco;
                o.ifPhase -= pco;
            }

            // Troposphere: NMF split.
            o.tropoZhdM = zhd_zen * NiellMappingFunction::hydrostatic(
                latRad, hM, doy, o.elevation);
            o.mfWet = NiellMappingFunction::wet(latRad, o.elevation);

            // Receiver PCO correction (ARP -> APC).
            if (m_cfg.pcoPcv && !m_antex.antennas.empty()) {
                const std::string& antType = obs.receiver.antennaType;
                for (const auto& cal : m_antex.antennas) {
                    if (cal.satellite) continue;
                    if (cal.antennaType.find(antType.substr(0, 8)) == std::string::npos) continue;
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
                        // pcoU is in mm, Up direction, project onto LOS
                        const double corr = (pcoU_if * 1e-3) * std::sin(o.elevation);
                        o.ifCode  -= corr;
                        o.ifPhase -= corr;
                    }
                    break;
                }
            }

            // debug
            static bool mfLogged = false;
            if (!mfLogged) {
                const double mf = NiellMappingFunction::hydrostatic(latRad, hM, doy, o.elevation);
                LOKI_INFO("PPP_DEBUG MF: el_deg=" + std::to_string(o.elevation * 180.0 / std::numbers::pi)
                        + " el_rad=" + std::to_string(o.elevation)
                        + " latRad=" + std::to_string(latRad)
                        + " hM=" + std::to_string(hM)
                        + " doy=" + std::to_string(doy)
                        + " MF_h=" + std::to_string(mf)
                        + " ZHD=" + std::to_string(zhd_zen)
                        + " tropoZhdM=" + std::to_string(o.tropoZhdM));
                mfLogged = true;
            }
            // end debug

            // Phase windup correction using narrow-lane wavelength.
            if (m_cfg.phaseWindup) {
                const auto satVel = m_orbit->velocity(
                    satObs.system, satObs.prn, txTime);
                const double cycles = phaseWindup.compute(
                    satObs.system, satObs.prn,
                    {o.satX, o.satY, o.satZ}, satVel,
                    {rx, ry, rz},
                    epoch.time.week, epoch.time.sow);

                // Use narrow-lane wavelength: lambda_NL = c/(f1+f2).
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

        // DEBUG
        static int dbgEpoch = 0;
        if (dbgEpoch < 5 || (dbgEpoch % 100 == 0 && dbgEpoch < 1000)) {
            const auto& dbgO = pppObs.front();
            LOKI_INFO("PPP_DEBUG ep=" + std::to_string(dbgEpoch)
                    + " prn=" + std::to_string(dbgO.prn)
                    + " ifCode=" + std::to_string(dbgO.ifCode)
                    + " ifPhase=" + std::to_string(dbgO.ifPhase)
                    + " diff=" + std::to_string(dbgO.ifPhase - dbgO.ifCode)
                    + " tropoZhdM=" + std::to_string(dbgO.tropoZhdM)
                    + " mfWet=" + std::to_string(dbgO.mfWet)
                    + " el_deg=" + std::to_string(dbgO.elevation * 180.0 / std::numbers::pi));
        }
        ++dbgEpoch;
        // END DEBUG

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
