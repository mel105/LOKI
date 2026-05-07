#include <loki/gnss/sppSolver.hpp>
#include <loki/core/exceptions.hpp>

#include <Eigen/Dense>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>

using namespace loki;
using namespace loki::gnss;

// =============================================================================
//  Constructor
// =============================================================================

SppSolver::SppSolver(Config cfg) : m_cfg(std::move(cfg)) {}

// =============================================================================
//  isEnabled
// =============================================================================

bool SppSolver::isEnabled(GnssSystem system) const {
    const std::string name = [&]() -> std::string {
        switch (system) {
            case GnssSystem::GPS:     return "GPS";
            case GnssSystem::GLONASS: return "GLONASS";
            case GnssSystem::GALILEO: return "GALILEO";
            case GnssSystem::BEIDOU:  return "BEIDOU";
            case GnssSystem::QZSS:    return "QZSS";
            default:                  return "";
        }
    }();
    return std::find(m_cfg.constellations.begin(),
                     m_cfg.constellations.end(), name)
           != m_cfg.constellations.end();
}

// =============================================================================
//  ecefToGeodetic  (Bowring iterative, 5 iterations)
// =============================================================================

void SppSolver::ecefToGeodetic(double x, double y, double z,
                                double& lat, double& lon, double& h) {
    constexpr double a  = 6378137.0;
    constexpr double e2 = 6.694379990141e-3;
    lon = std::atan2(y, x);
    const double p = std::sqrt(x*x + y*y);
    lat = std::atan2(z, p * (1.0 - e2));
    for (int i = 0; i < 5; ++i) {
        const double sinLat = std::sin(lat);
        const double N = a / std::sqrt(1.0 - e2 * sinLat * sinLat);
        lat = std::atan2(z + e2 * N * sinLat, p);
    }
    const double sinLat = std::sin(lat);
    const double N = a / std::sqrt(1.0 - e2 * sinLat * sinLat);
    h = p / std::cos(lat) - N;
}

// =============================================================================
//  selectPseudorange
//  Priority: GPS C1C > C1W > C2W > C2C > C1X
//            GAL C1X > C5X > C7X > C8X
//            GLO C1C > C1P > C2C > C2P
//            BDS C2I > C6I > C7I > C1X
// =============================================================================

double SppSolver::selectPseudorange(const SatObs& sat, GnssSystem system) {
    std::vector<std::string> priority;
    switch (system) {
        case GnssSystem::GPS:
        case GnssSystem::QZSS:
            priority = {"C1C", "C1W", "C2W", "C2C", "C1X", "C5X"};
            break;
        case GnssSystem::GALILEO:
            priority = {"C1X", "C5X", "C7X", "C8X", "C1C"};
            break;
        case GnssSystem::GLONASS:
            priority = {"C1C", "C1P", "C2C", "C2P"};
            break;
        case GnssSystem::BEIDOU:
            priority = {"C2I", "C6I", "C7I", "C1X", "C5X"};
            break;
        default:
            return 0.0;
    }
    for (const auto& code : priority) {
        const auto it = sat.obs.find(code);
        if (it != sat.obs.end() && it->second.valid &&
            it->second.value > 1.0e6)
            return it->second.value;
    }
    return 0.0;
}

// =============================================================================
//  elevationWeight  w = sin^2(el)
// =============================================================================

double SppSolver::elevationWeight(double elevRad) {
    const double s = std::sin(elevRad);
    return s * s;
}

// =============================================================================
//  klobucharDelay  (IS-GPS-200 section 20.3.3.5.2.5)
//  Returns L1 ionosphere delay [m].
// =============================================================================

double SppSolver::klobucharDelay(const std::array<double,4>& alpha,
                                  const std::array<double,4>& beta,
                                  double lat, double lon,
                                  double az,  double el,
                                  double gpsSow) {
    const double psi  = 0.0137 / (el / std::numbers::pi + 0.11) - 0.022;
    double phi_i = lat / std::numbers::pi + psi * std::cos(az);
    phi_i = std::clamp(phi_i, -0.416, 0.416);
    const double lambda_i = lon / std::numbers::pi
        + psi * std::sin(az) / std::cos(phi_i * std::numbers::pi);
    const double phi_m = phi_i
        + 0.064 * std::cos((lambda_i - 1.617) * std::numbers::pi);
    double t = 4.32e4 * lambda_i + gpsSow;
    t = std::fmod(t, 86400.0);
    if (t < 0.0) t += 86400.0;
    double Per = alpha[0] + alpha[1]*phi_m + alpha[2]*phi_m*phi_m
               + alpha[3]*phi_m*phi_m*phi_m;
    Per = std::max(Per, 72000.0);
    double Amp = beta[0] + beta[1]*phi_m + beta[2]*phi_m*phi_m
               + beta[3]*phi_m*phi_m*phi_m;
    Amp = std::max(Amp, 0.0);
    const double x = 2.0 * std::numbers::pi * (t - 50400.0) / Per;
    const double F = 1.0 + 16.0 * std::pow(0.53 - el / std::numbers::pi, 3.0);
    const double T_iono = (std::fabs(x) < 1.57)
        ? (5.0e-9 + Amp * (1.0 - x*x/2.0 + x*x*x*x/24.0)) * F
        : 5.0e-9 * F;
    return T_iono * SPEED_OF_LIGHT;
}

// =============================================================================
//  saastamoinenDelay
//  Returns zenith troposphere delay mapped to slant [m].
// =============================================================================

double SppSolver::saastamoinenDelay(double lat, double h, double el) {
    const double P   = 1013.25 * std::pow(1.0 - 2.2557e-5 * h, 5.2568);
    const double T   = 15.0 - 6.5e-3 * h + 273.15;
    const double e   = 6.108 * std::exp(17.15 * (T - 273.15) / (T - 38.25));
    const double Zhd = 0.0022768 * P
        / (1.0 - 0.00266 * std::cos(2.0 * lat) - 2.8e-7 * h);
    const double Zwet = 0.002277 * (1255.0 / T + 0.05) * e;
    const double sinEl = std::sin(el);
    return (Zhd + Zwet) / (sinEl < 0.05 ? 0.05 : sinEl);
}

// =============================================================================
//  solve  -- iterative weighted LSQ SPP for one epoch
// =============================================================================

SppResult SppSolver::solve(
    const ObsEpoch&              epoch,
    const NavFile&               nav,
    const std::array<double,3>&  approxPos,
    const std::array<double,4>&  ionoAlpha,
    const std::array<double,4>&  ionoBeta) const
{
    SppResult result;
    result.time = epoch.time;

    // Working state: ECEF position + receiver clock bias [m]
    double rx      = approxPos[0];
    double ry      = approxPos[1];
    double rz      = approxPos[2];
    double rcv_clk = 0.0;

    // Geodetic coords of current position estimate (for correction models)
    double lat0 = 0.0, lon0 = 0.0, h0 = 0.0;
    ecefToGeodetic(rx, ry, rz, lat0, lon0, h0);

    // clkBias stored separately for debug output
    struct SatMeas {
        double     range;       // corrected pseudorange [m]
        double     prRaw;       // raw pseudorange [m] before clock correction
        double     clkBias;     // satellite clock bias [s]
        double     xs, ys, zs; // satellite ECEF [m]
        double     elevation;   // [rad]
        double     azimuth;     // [rad]
        double     weight;
        GnssSystem system;
        int        prn;
    };

    for (int iter = 0; iter < m_cfg.maxIterations; ++iter) {

        std::vector<SatMeas> meas;
        meas.reserve(epoch.satellites.size());

        for (const auto& satObs : epoch.satellites) {
            if (!isEnabled(satObs.system)) continue;

            const double pr = selectPseudorange(satObs, satObs.system);
            if (pr < 1.0e6) continue;

            // Approximate signal transmission time
            const double tof = pr / SPEED_OF_LIGHT;
            GpsTime txTime   = epoch.time;
            txTime.sow      -= tof;
            if (txTime.sow < 0.0) { txTime.sow += 604800.0; --txTime.week; }

            // Satellite ECEF position + clock at transmission time
            const SatState sat = KeplerOrbit::compute(
                nav, satObs.system, satObs.prn, txTime);
            if (!sat.valid) continue;

            // Sagnac correction (Earth rotation during signal travel)
            double xs = sat.x, ys = sat.y, zs = sat.z;
            if (m_cfg.sagnac) {
                constexpr double OE = 7.2921151467e-5;
                const double theta  = OE * tof;
                xs =  sat.x * std::cos(theta) + sat.y * std::sin(theta);
                ys = -sat.x * std::sin(theta) + sat.y * std::cos(theta);
                zs =  sat.z;
            }

            // Elevation and azimuth from current position estimate
            double el = 0.0, az = 0.0;
            SatVisibility::ecefToElevAzim({xs, ys, zs}, {rx, ry, rz}, el, az);
            const double elRad = el * (std::numbers::pi / 180.0);
            const double azRad = az * (std::numbers::pi / 180.0);

            if (el < m_cfg.elevMaskDeg) continue;

            // Build corrected pseudorange.
            // Pseudorange observation model: P = rho + c*dt_rcv - c*dt_sat + ...
            // To isolate (rho + c*dt_rcv) we add c*dt_sat to both sides:
            //   P + c*dt_sat = rho + c*dt_rcv
            // clkBias = dt_sat [s], so: prCorr = pr + clkBias * c
            double prCorr = pr;
            prCorr += sat.clkBias * SPEED_OF_LIGHT;

            if (m_cfg.ionosphere == "klobuchar") {
                prCorr -= klobucharDelay(ionoAlpha, ionoBeta,
                                          lat0, lon0, azRad, elRad,
                                          epoch.time.sow);
            }

            if (m_cfg.troposphere == "saastamoinen") {
                prCorr -= saastamoinenDelay(lat0, h0, elRad);
            }

            double w = 1.0;
            if (m_cfg.weighting == "elevation") {
                w = elevationWeight(elRad);
                if (w < 1.0e-6) continue;
            }

            meas.push_back({prCorr, pr, sat.clkBias,
                             xs, ys, zs, elRad, azRad, w,
                             satObs.system, satObs.prn});
        }

        if (static_cast<int>(meas.size()) < 4) break;

        const int n = static_cast<int>(meas.size());
        Eigen::MatrixXd H(n, 4);
        Eigen::VectorXd b(n);
        Eigen::VectorXd W(n);

        for (int i = 0; i < n; ++i) {
            const std::size_t si = static_cast<std::size_t>(i);
            const double ddx = meas[si].xs - rx;
            const double ddy = meas[si].ys - ry;
            const double ddz = meas[si].zs - rz;
            const double rho = std::sqrt(ddx*ddx + ddy*ddy + ddz*ddz);

            H(i, 0) = -ddx / rho;
            H(i, 1) = -ddy / rho;
            H(i, 2) = -ddz / rho;
            H(i, 3) = 1.0;

            b(i) = meas[si].range - (rho + rcv_clk);
            W(i) = meas[si].weight;
        }

        // Weighted LSQ: dx = (H^T W H)^-1 H^T W b
        const Eigen::MatrixXd Hw   = H.array().colwise() * W.array();
        const Eigen::Matrix4d HtWH = Hw.transpose() * H;
        const Eigen::Vector4d HtWb = Hw.transpose() * b;
        const Eigen::Vector4d dx   = HtWH.ldlt().solve(HtWb);

        rx      += dx(0);
        ry      += dx(1);
        rz      += dx(2);
        rcv_clk += dx(3);

        // Update geodetic for next iteration
        ecefToGeodetic(rx, ry, rz, lat0, lon0, h0);

        result.iterations = iter + 1;

        const double norm = dx.head(3).norm();
        if (norm < m_cfg.convergenceThresholdM) {
            result.converged = true;

            // Post-fit residuals
            result.residuals.reserve(static_cast<std::size_t>(n));
            result.usedSats.reserve(static_cast<std::size_t>(n));

            for (int i = 0; i < n; ++i) {
                const std::size_t si = static_cast<std::size_t>(i);
                const double ddx  = meas[si].xs - rx;
                const double ddy  = meas[si].ys - ry;
                const double ddz  = meas[si].zs - rz;
                const double rho2 = std::sqrt(ddx*ddx + ddy*ddy + ddz*ddz);
                result.residuals.push_back(meas[si].range - (rho2 + rcv_clk));

                SppResult::UsedSat us;
                us.system    = meas[si].system;
                us.prn       = meas[si].prn;
                us.elevation = meas[si].elevation * (180.0 / std::numbers::pi);
                us.weight    = meas[si].weight;
                result.usedSats.push_back(us);
            }

            // PDOP from unweighted geometry matrix
            const Eigen::Matrix4d Q = (H.transpose() * H).inverse();
            result.pdop = std::sqrt(Q(0,0) + Q(1,1) + Q(2,2));

            // ----------------------------------------------------------------
            // DEBUG -- print detailed table for the first converged epoch only
            // ----------------------------------------------------------------
            static bool s_debugDone = false;
            if (!s_debugDone) {
                s_debugDone = true;
                std::fprintf(stderr,
                    "\n=== SPP DEBUG: first converged epoch ===\n");
                std::fprintf(stderr,
                    "  Converged: X=%12.3f  Y=%12.3f  Z=%12.3f  clk=%12.3f m"
                    "  (%.3f us)\n",
                    rx, ry, rz,
                    rcv_clk, rcv_clk / SPEED_OF_LIGHT * 1.0e6);
                std::fprintf(stderr,
                    "  Reference: X=%12.3f  Y=%12.3f  Z=%12.3f\n",
                    3979316.439, 1050312.253, 4857066.904);
                const double dX = rx - 3979316.439;
                const double dY = ry - 1050312.253;
                const double dZ = rz - 4857066.904;
                std::fprintf(stderr,
                    "  Error:    dX=%9.3f  dY=%9.3f  dZ=%9.3f"
                    "  3D=%9.3f [m]\n",
                    dX, dY, dZ, std::sqrt(dX*dX + dY*dY + dZ*dZ));
                std::fprintf(stderr,
                    "\n  %-4s  %15s  %13s  %13s  %13s"
                    "  %14s  %14s  %11s  %8s  %8s\n",
                    "PRN", "pr_raw[m]", "clkBias[us]",
                    "clk_corr[m]", "pr_corr[m]",
                    "rho[m]", "resid[m]", "el[deg]",
                    "weight", "tof[ms]");
                for (int j = 0; j < n; ++j) {
                    const std::size_t sj = static_cast<std::size_t>(j);
                    const double ddx2 = meas[sj].xs - rx;
                    const double ddy2 = meas[sj].ys - ry;
                    const double ddz2 = meas[sj].zs - rz;
                    const double rho2 = std::sqrt(
                        ddx2*ddx2 + ddy2*ddy2 + ddz2*ddz2);
                    const double res2 =
                        meas[sj].range - (rho2 + rcv_clk);
                    const double clkUs  =
                        meas[sj].clkBias * 1.0e6;
                    const double clkM   =
                        meas[sj].clkBias * SPEED_OF_LIGHT;
                    const double tofMs  =
                        meas[sj].prRaw / SPEED_OF_LIGHT * 1.0e3;
                    const std::string sys = [&]() -> std::string {
                        switch (meas[sj].system) {
                            case GnssSystem::GPS:     return "G";
                            case GnssSystem::GALILEO: return "E";
                            case GnssSystem::GLONASS: return "R";
                            case GnssSystem::BEIDOU:  return "C";
                            default:                  return "?";
                        }
                    }();
                    std::fprintf(stderr,
                        "  %-4s  %15.3f  %13.6f  %13.3f  %13.3f"
                        "  %14.3f  %14.3f  %11.4f  %8.4f  %8.4f\n",
                        (sys + std::to_string(meas[sj].prn)).c_str(),
                        meas[sj].prRaw,
                        clkUs,
                        clkM,
                        meas[sj].range,
                        rho2,
                        res2,
                        meas[sj].elevation * (180.0 / std::numbers::pi),
                        meas[sj].weight,
                        tofMs);
                }
                std::fprintf(stderr,
                    "========================================\n\n");
            }
            // ----------------------------------------------------------------

            break;
        }
    }

    if (!result.converged) return result;

    result.x        = rx;
    result.y        = ry;
    result.z        = rz;
    result.clkBiasM = rcv_clk;
    result.nSats    = static_cast<int>(result.usedSats.size());
    result.valid    = true;
    return result;
}

// =============================================================================
//  solveAll
// =============================================================================

std::vector<SppResult> SppSolver::solveAll(
    const ObsFile&    obs,
    const NavFile&    nav,
    const GnssConfig& cfg) const
{
    const std::array<double,3> approxPos{
        obs.receiver.approxX,
        obs.receiver.approxY,
        obs.receiver.approxZ
    };

    std::vector<SppResult> results;
    results.reserve(obs.epochs.size());

    for (const auto& epoch : obs.epochs)
        results.push_back(solve(epoch, nav, approxPos,
                                nav.ionoAlpha, nav.ionoBeta));

    return results;
}
