#include <loki/gnss/satVisibility.hpp>
#include <loki/geodesy/coordTransform.hpp>
#include <loki/math/ellipsoid.hpp>
#include <loki/core/exceptions.hpp>

#include <Eigen/Dense>
#include <cmath>
#include <numbers>

using namespace loki;
using namespace loki::gnss;

// =============================================================================
//  ecefToElevAzim
//  Uses loki_geodesy ecef2geod (Vermeille) + ecefEnuRotMat for the
//  ECEF->ENU rotation. Replaces the previous Bowring iteration.
// =============================================================================

void SatVisibility::ecefToElevAzim(const std::array<double,3>& satEcef,
                                    const std::array<double,3>& staEcef,
                                    double& elevation,
                                    double& azimuth)
{
    static const loki::math::Ellipsoid wgs84 =
        loki::math::makeEllipsoid(loki::math::EllipsoidModel::WGS84);

    // Geodetic coordinates of the station (degrees).
    const loki::geodesy::EcefPoint staEp{staEcef[0], staEcef[1], staEcef[2]};
    const loki::geodesy::GeodPoint staGeod = loki::geodesy::ecef2geod(staEp, wgs84);

    // ECEF->ENU rotation matrix at the station.
    const Eigen::Matrix3d R = loki::geodesy::ecefEnuRotMat(staGeod.lat, staGeod.lon);

    // Baseline vector in ECEF [m].
    const Eigen::Vector3d dEcef{
        satEcef[0] - staEcef[0],
        satEcef[1] - staEcef[1],
        satEcef[2] - staEcef[2]
    };

    // ENU components.
    const Eigen::Vector3d enu = R * dEcef;
    const double e = enu(0);
    const double n = enu(1);
    const double u = enu(2);

    const double horiz = std::sqrt(e*e + n*n);
    elevation = std::atan2(u, horiz) * (180.0 / std::numbers::pi);
    azimuth   = std::atan2(e, n)     * (180.0 / std::numbers::pi);
    if (azimuth < 0.0) azimuth += 360.0;
}

// =============================================================================
//  computeDop
// =============================================================================

void SatVisibility::computeDop(VisibilityEpoch& epoch)
{
    std::vector<Eigen::RowVector4d> rows;
    rows.reserve(epoch.satellites.size());

    for (const auto& sat : epoch.satellites) {
        if (!sat.visible || !sat.state.valid) continue;

        const double el = sat.elevation * (std::numbers::pi / 180.0);
        const double az = sat.azimuth   * (std::numbers::pi / 180.0);

        const double e = std::cos(el) * std::sin(az);
        const double n = std::cos(el) * std::cos(az);
        const double u = std::sin(el);

        rows.push_back(Eigen::RowVector4d{e, n, u, 1.0});
    }

    epoch.nVisible = static_cast<int>(rows.size());

    if (static_cast<int>(rows.size()) < 4) {
        epoch.gdop = epoch.pdop = epoch.hdop = epoch.vdop = 99.9;
        return;
    }

    Eigen::MatrixXd H(static_cast<int>(rows.size()), 4);
    for (int i = 0; i < static_cast<int>(rows.size()); ++i)
        H.row(i) = rows[static_cast<std::size_t>(i)];

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

            sea.state = KeplerOrbit::compute(nav, satObs.system,
                                              satObs.prn, obsEpoch.time);
            if (!sea.state.valid) {
                visEpoch.satellites.push_back(sea);
                continue;
            }

            const std::array<double,3> satEcef{
                sea.state.x, sea.state.y, sea.state.z};

            ecefToElevAzim(satEcef, stationEcef, sea.elevation, sea.azimuth);
            sea.visible = (sea.elevation >= elevMaskDeg);
            visEpoch.satellites.push_back(sea);
        }

        computeDop(visEpoch);
        result.push_back(std::move(visEpoch));
    }

    return result;
}
