#include <loki/gnss/satVisibility.hpp>
#include <loki/core/exceptions.hpp>

#include <Eigen/Dense>
#include <cmath>
#include <numbers>

using namespace loki;
using namespace loki::gnss;

// =============================================================================
//  ecefToElevAzim
//  Converts ECEF satellite position to elevation and azimuth as seen from
//  a ground station. Uses the standard ECEF -> ENU rotation.
// =============================================================================

void SatVisibility::ecefToElevAzim(const std::array<double,3>& satEcef,
                                    const std::array<double,3>& staEcef,
                                    double& elevation,
                                    double& azimuth) {
    // Station geodetic coordinates (needed for ENU rotation matrix)
    const double X = staEcef[0];
    const double Y = staEcef[1];
    const double Z = staEcef[2];

    // Station geodetic latitude and longitude from ECEF
    const double lon = std::atan2(Y, X);
    const double p   = std::sqrt(X*X + Y*Y);
    const double lat = std::atan2(Z, p * (1.0 - 6.694379990141e-3)); // WGS-84 e^2

    // Iterate for precise geodetic latitude (Bowring, 1 iteration sufficient)
    {
        const double e2  = 6.694379990141e-3;
        const double a   = 6378137.0;
        double phi = lat;
        for (int i = 0; i < 5; ++i) {
            const double sinPhi = std::sin(phi);
            const double N = a / std::sqrt(1.0 - e2 * sinPhi * sinPhi);
            phi = std::atan2(Z + e2 * N * sinPhi, p);
        }
        // final lat
        const double sinPhi = std::sin(phi);
        const double cosPhi = std::cos(phi);
        const double sinLon = std::sin(lon);
        const double cosLon = std::cos(lon);

        // ECEF -> ENU rotation matrix rows:
        // e = [-sinLon,         cosLon,        0       ]
        // n = [-sinPhi*cosLon, -sinPhi*sinLon,  cosPhi ]
        // u = [ cosPhi*cosLon,  cosPhi*sinLon,  sinPhi ]

        const double dx = satEcef[0] - staEcef[0];
        const double dy = satEcef[1] - staEcef[1];
        const double dz = satEcef[2] - staEcef[2];

        const double e = -sinLon*dx + cosLon*dy;
        const double n = -sinPhi*cosLon*dx - sinPhi*sinLon*dy + cosPhi*dz;
        const double u =  cosPhi*cosLon*dx + cosPhi*sinLon*dy + sinPhi*dz;

        const double horiz = std::sqrt(e*e + n*n);
        elevation = std::atan2(u, horiz) * (180.0 / std::numbers::pi);
        azimuth   = std::atan2(e, n)     * (180.0 / std::numbers::pi);
        if (azimuth < 0.0) azimuth += 360.0;
    }
}

// =============================================================================
//  computeDop
//  Builds the geometry matrix H from unit vectors to visible satellites
//  and computes DOP values from Q = (H^T H)^-1.
// =============================================================================

void SatVisibility::computeDop(VisibilityEpoch& epoch) {
    // Collect unit vectors for visible satellites
    std::vector<Eigen::RowVector4d> rows;
    rows.reserve(epoch.satellites.size());

    for (const auto& sat : epoch.satellites) {
        if (!sat.visible || !sat.state.valid) continue;

        // Unit vector from station to satellite in ECEF
        // (approximation: use satellite ECEF directly with unit normalization)
        // For DOP, the direction cosines in ENU suffice.
        const double el = sat.elevation * (std::numbers::pi / 180.0);
        const double az = sat.azimuth   * (std::numbers::pi / 180.0);

        // ENU unit vector
        const double e = std::cos(el) * std::sin(az);
        const double n = std::cos(el) * std::cos(az);
        const double u = std::sin(el);

        rows.push_back(Eigen::RowVector4d{e, n, u, 1.0});
    }

    epoch.nVisible = static_cast<int>(rows.size());

    if (static_cast<int>(rows.size()) < 4) {
        // Cannot compute DOP with fewer than 4 satellites
        epoch.gdop = epoch.pdop = epoch.hdop = epoch.vdop = 99.9;
        return;
    }

    // Build H matrix
    Eigen::MatrixXd H(static_cast<int>(rows.size()), 4);
    for (int i = 0; i < static_cast<int>(rows.size()); ++i)
        H.row(i) = rows[static_cast<std::size_t>(i)];

    // Q = (H^T H)^-1
    const Eigen::Matrix4d HtH = H.transpose() * H;
    const Eigen::Matrix4d Q   = HtH.inverse();

    epoch.gdop = std::sqrt(Q(0,0) + Q(1,1) + Q(2,2) + Q(3,3));
    epoch.pdop = std::sqrt(Q(0,0) + Q(1,1) + Q(2,2));
    epoch.hdop = std::sqrt(Q(0,0) + Q(1,1));
    epoch.vdop = std::sqrt(Q(2,2));
}

// =============================================================================
//  compute
// =============================================================================

std::vector<VisibilityEpoch> SatVisibility::compute(
    const ObsFile&              obs,
    const NavFile&              nav,
    const std::array<double,3>& stationEcef,
    double                      elevMaskDeg) const
{
    std::vector<VisibilityEpoch> result;
    result.reserve(obs.epochs.size());

    for (const auto& obsEpoch : obs.epochs) {
        VisibilityEpoch visEpoch;
        visEpoch.time = obsEpoch.time;
        visEpoch.satellites.reserve(obsEpoch.satellites.size());

        for (const auto& satObs : obsEpoch.satellites) {
            SatElevAzim sea;
            sea.system = satObs.system;
            sea.prn    = satObs.prn;

            // Compute satellite ECEF position at signal transmission time.
            // Approximate: use epoch reception time (light-travel correction
            // applied in SPP solver, not needed for visibility/DOP).
            sea.state = KeplerOrbit::compute(nav, satObs.system,
                                              satObs.prn, obsEpoch.time);

            if (!sea.state.valid) {
                visEpoch.satellites.push_back(sea);
                continue;
            }

            const std::array<double,3> satEcef{
                sea.state.x, sea.state.y, sea.state.z};

            ecefToElevAzim(satEcef, stationEcef,
                            sea.elevation, sea.azimuth);

            sea.visible = (sea.elevation >= elevMaskDeg);
            visEpoch.satellites.push_back(sea);
        }

        computeDop(visEpoch);
        result.push_back(std::move(visEpoch));
    }

    return result;
}
