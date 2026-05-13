#include <loki/gnss/sppSolver.hpp>
#include <loki/gnss/gnssUtils.hpp>
#include <loki/gnss/relativity.hpp>
#include <loki/geodesy/coordTransform.hpp>
#include <loki/math/ellipsoid.hpp>
#include <loki/core/exceptions.hpp>

#include <Eigen/Dense>
#include <algorithm>
#include <cmath>
#include <numbers>

using namespace loki;
using namespace loki::gnss;

// =============================================================================
//  Constructor
// =============================================================================

SppSolver::SppSolver(Config                                        cfg,
                     std::unique_ptr<OrbitModel>                   orbit,
                     std::vector<std::unique_ptr<CorrectionModel>> corrections)
    : m_cfg(std::move(cfg))
    , m_orbit(std::move(orbit))
    , m_corrections(std::move(corrections))
{
    if (!m_orbit) {
        throw ConfigException("SppSolver: orbit model must not be null.");
    }
}

// =============================================================================
//  isEnabled
// =============================================================================

bool SppSolver::isEnabled(GnssSystem system) const
{
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
//  solve  -- iterative weighted LSQ SPP for one epoch
// =============================================================================

SppResult SppSolver::solve(
    const ObsEpoch&              epoch,
    const NavFile&               nav,
    const std::array<double,3>&  approxPos) const
{
    SppResult result;
    result.time = epoch.time;

    // Working position estimate [m].
    double rx = approxPos[0];
    double ry = approxPos[1];
    double rz = approxPos[2];

    // WGS-84 ellipsoid for ECEF -> geodetic conversion.
    const loki::math::Ellipsoid wgs84 = loki::math::makeEllipsoid(
        loki::math::EllipsoidModel::WGS84);

    // clk[0] = GPS clock [m], clk[1] = GLO ISB, clk[2] = GAL ISB, clk[3] = BDS ISB.
    std::array<double, 4> clk{0.0, 0.0, 0.0, 0.0};

    // Fixed ISB column mapping (stable across all iterations):
    //   0 = GPS / QZSS (reference clock)
    //   1 = GLONASS ISB
    //   2 = GALILEO ISB
    //   3 = BEIDOU ISB
    auto sysToFixedCol = [](GnssSystem sys) -> int {
        switch (sys) {
            case GnssSystem::GLONASS: return 1;
            case GnssSystem::GALILEO: return 2;
            case GnssSystem::BEIDOU:  return 3;
            default:                  return 0;
        }
    };

    // Per-measurement working structure used inside each iteration.
    struct SatMeas {
        double     range;           ///< Corrected pseudorange [m].
        double     xs, ys, zs;     ///< Satellite ECEF [m] (Sagnac-rotated if enabled).
        double     elevation;       ///< [rad]
        double     azimuth;         ///< [rad]
        double     weight;
        int        fixedCol;        ///< 0=GPS, 1=GLO, 2=GAL, 3=BDS
        GnssSystem system;
        int        prn;
    };

    // Geodetic coordinates of current position (recomputed each iteration
    // for correction models that depend on station position).
    loki::geodesy::GeodPoint geod{0.0, 0.0, 0.0};

    for (int iter = 0; iter < m_cfg.maxIterations; ++iter) {

        // Update geodetic position.
        {
            const loki::geodesy::EcefPoint ep{rx, ry, rz};
            geod = loki::geodesy::ecef2geod(ep, wgs84);
            // ecef2geod returns degrees; convert to radians for correction models.
        }
        const double latRad = geod.lat * (std::numbers::pi / 180.0);
        const double lonRad = geod.lon * (std::numbers::pi / 180.0);
        const double hM     = geod.h;

        std::vector<SatMeas> meas;
        meas.reserve(epoch.satellites.size());

        for (const auto& satObs : epoch.satellites) {
            if (!isEnabled(satObs.system)) continue;

            const double pr = selectPseudorange(satObs, satObs.system);
            if (pr == 0.0) continue;

            // Approximate signal time of flight.
            const double tof  = pr / SPEED_OF_LIGHT;
            GpsTime txTime    = epoch.time;
            txTime.sow       -= tof;
            if (txTime.sow < 0.0) { txTime.sow += 604800.0; --txTime.week; }

            // Satellite state at transmission time.
            const SatState sat = m_orbit->compute(nav, satObs.system,
                                                    satObs.prn, txTime);
            if (!sat.valid) continue;

            // Optionally rotate satellite position for Sagnac effect.
            double xs = sat.x, ys = sat.y, zs = sat.z;
            if (m_cfg.sagnac) {
                const auto rotated = Relativity::rotateSatPosition(
                    {sat.x, sat.y, sat.z}, tof);
                xs = rotated[0]; ys = rotated[1]; zs = rotated[2];
            }

            // Elevation and azimuth from current position estimate.
            double elDeg = 0.0, azDeg = 0.0;
            SatVisibility::ecefToElevAzim({xs, ys, zs}, {rx, ry, rz},
                                           elDeg, azDeg);
            if (elDeg < m_cfg.elevMaskDeg) continue;

            const double elRad = elDeg * (std::numbers::pi / 180.0);
            const double azRad = azDeg * (std::numbers::pi / 180.0);

            // Apply satellite clock correction (ADD: positive clkBias shortens range).
            double prCorr = pr + sat.clkBias * SPEED_OF_LIGHT;

            // Apply injected correction models (subtract each delay).
            if (!m_corrections.empty()) {
                CorrectionInput ci;
                ci.lat       = latRad;
                ci.lon       = lonRad;
                ci.h         = hM;
                ci.elevation = elRad;
                ci.azimuth   = azRad;
                ci.gpsSow    = epoch.time.sow;
                ci.gpsWeek   = epoch.time.week;
                // ECEF positions for geometry-dependent models (solid tides, PCO/PCV)
                ci.satX = xs;  ci.satY = ys;  ci.satZ = zs;
                ci.staX = rx;  ci.staY = ry;  ci.staZ = rz;

                for (const auto& model : m_corrections) {
                    prCorr -= model->delay(ci);
                }
            }

            // Elevation-dependent weight.
            double w = 1.0;
            if (m_cfg.weighting == "elevation") {
                w = elevationWeight(elRad);
                if (w < 1.0e-6) continue;
            }

            meas.push_back({prCorr, xs, ys, zs,
                             elRad, azRad, w,
                             sysToFixedCol(satObs.system),
                             satObs.system, satObs.prn});
        }

        // Count satellites per fixed column.
        int colCount[4] = {0, 0, 0, 0};
        for (const auto& m2 : meas)
            colCount[m2.fixedCol]++;

        // Map fixed column -> compact parameter index.
        // X,Y,Z = 0,1,2; GPS clock = 3; non-GPS ISB added if >= 2 satellites.
        int compactIdx[4] = {3, -1, -1, -1};
        int nParams = 4;
        for (int c = 1; c <= 3; ++c) {
            if (colCount[c] >= 2)
                compactIdx[c] = nParams++;
        }

        // Drop measurements whose constellation ISB column was not activated.
        meas.erase(
            std::remove_if(meas.begin(), meas.end(),
                [&](const SatMeas& m2) {
                    return compactIdx[m2.fixedCol] < 0;
                }),
            meas.end());

        const int n = static_cast<int>(meas.size());
        if (n < nParams) break;

        // Build weighted LSQ system H * dx = b, W = diag(weights).
        Eigen::MatrixXd H = Eigen::MatrixXd::Zero(n, nParams);
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
            H(i, compactIdx[meas[si].fixedCol]) = 1.0;

            const double ct = clk[0]
                + (meas[si].fixedCol > 0 ? clk[meas[si].fixedCol] : 0.0);
            b(i) = meas[si].range - (rho + ct);
            W(i) = meas[si].weight;
        }

        const Eigen::MatrixXd Hw   = H.array().colwise() * W.array();
        const Eigen::MatrixXd HtWH = Hw.transpose() * H;
        const Eigen::VectorXd HtWb = Hw.transpose() * b;
        const Eigen::VectorXd dx   = HtWH.ldlt().solve(HtWb);

        rx     += dx(0);
        ry     += dx(1);
        rz     += dx(2);
        clk[0] += dx(3);
        for (int c = 1; c <= 3; ++c) {
            if (compactIdx[c] >= 4)
                clk[c] += dx(compactIdx[c]);
        }

        // Abort if estimate has diverged far from Earth surface.
        if (rx*rx + ry*ry + rz*rz > 4.5e13) break;

        result.iterations = iter + 1;

        if (dx.head(3).norm() < m_cfg.convergenceThresholdM) {
            result.converged = true;

            result.residuals.reserve(static_cast<std::size_t>(n));
            result.usedSats.reserve(static_cast<std::size_t>(n));

            for (int i = 0; i < n; ++i) {
                const std::size_t si = static_cast<std::size_t>(i);
                const double ddx  = meas[si].xs - rx;
                const double ddy  = meas[si].ys - ry;
                const double ddz  = meas[si].zs - rz;
                const double rho2 = std::sqrt(ddx*ddx + ddy*ddy + ddz*ddz);
                const double ct   = clk[0]
                    + (meas[si].fixedCol > 0 ? clk[meas[si].fixedCol] : 0.0);
                result.residuals.push_back(meas[si].range - (rho2 + ct));

                SppResult::UsedSat us;
                us.system    = meas[si].system;
                us.prn       = meas[si].prn;
                us.elevation = meas[si].elevation * (180.0 / std::numbers::pi);
                us.weight    = meas[si].weight;
                result.usedSats.push_back(us);
            }

            const Eigen::MatrixXd Q = HtWH.inverse();
            result.pdop = std::sqrt(Q(0,0) + Q(1,1) + Q(2,2));

            const std::array<std::string, 3> isbNames{"GLONASS", "GALILEO", "BEIDOU"};
            for (int c = 1; c <= 3; ++c) {
                if (compactIdx[c] >= 4)
                    result.isbM[isbNames[static_cast<std::size_t>(c - 1)]] = clk[c];
            }

            break;
        }
    }

    if (!result.converged) return result;

    result.x        = rx;
    result.y        = ry;
    result.z        = rz;
    result.clkBiasM = clk[0];
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
    const GnssConfig& /*cfg*/) const
{
    const std::array<double, 3> approxPos{
        obs.receiver.approxX,
        obs.receiver.approxY,
        obs.receiver.approxZ
    };

    std::vector<SppResult> results;
    results.reserve(obs.epochs.size());

    for (const auto& epoch : obs.epochs)
        results.push_back(solve(epoch, nav, approxPos));

    return results;
}
