#include <loki/geodesy/coordTransform.hpp>
#include <loki/core/exceptions.hpp>

#include <cmath>
#include <numbers>
#include <stdexcept>

using namespace loki::geodesy;
using namespace loki::math;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

constexpr double DEG2RAD = std::numbers::pi / 180.0;
constexpr double RAD2DEG = 180.0 / std::numbers::pi;

inline double toRad(double deg) noexcept { return deg * DEG2RAD; }
inline double toDeg(double rad) noexcept { return rad * RAD2DEG; }

// Clamp latitude to avoid singularities at the exact poles.
// Vincenty / Vermeille become degenerate at lat = +/-90 exactly.
inline double clampLat(double latRad) noexcept
{
    constexpr double POLE_OFFSET = 1e-10;
    constexpr double HALF_PI     = std::numbers::pi / 2.0;
    if (latRad >  HALF_PI - POLE_OFFSET) return  HALF_PI - POLE_OFFSET;
    if (latRad < -HALF_PI + POLE_OFFSET) return -HALF_PI + POLE_OFFSET;
    return latRad;
}

// Build the block-diagonal Jacobian for an extended 6x6 state vector from
// a 3x3 Jacobian applied identically to position and velocity blocks.
Eigen::MatrixXd blockDiag6(const Eigen::Matrix3d& J3)
{
    Eigen::MatrixXd J6 = Eigen::MatrixXd::Zero(6, 6);
    J6.block<3, 3>(0, 0) = J3;
    J6.block<3, 3>(3, 3) = J3;
    return J6;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Rotation matrix ECEF <-> ENU
// ---------------------------------------------------------------------------

Eigen::Matrix3d loki::geodesy::ecefEnuRotMat(double latDeg, double lonDeg)
{
    double lat = toRad(latDeg);
    double lon = toRad(lonDeg);

    double slat = std::sin(lat);
    double clat = std::cos(lat);
    double slon = std::sin(lon);
    double clon = std::cos(lon);

    // Row 0: East direction
    // Row 1: North direction
    // Row 2: Up direction
    Eigen::Matrix3d R;
    R << -slon,        clon,        0.0,
         -slat * clon, -slat * slon,  clat,
          clat * clon,  clat * slon,  slat;
    return R;
}

// ---------------------------------------------------------------------------
// Jacobians
// ---------------------------------------------------------------------------

Eigen::Matrix3d loki::geodesy::jacobianEcefGeod(double latDeg, double lonDeg,
                                                  double h,
                                                  const Ellipsoid& ell)
{
    double lat = toRad(latDeg);
    double lon = toRad(lonDeg);

    double sinLat = std::sin(lat);
    double cosLat = std::cos(lat);
    double sinLon = std::sin(lon);
    double cosLon = std::cos(lon);

    double Nv    = ell.N(lat);
    double dNdl  = ell.dNdLat(lat);

    // dX/dlat, dX/dlon, dX/dh
    double dXdLat = cosLon * (dNdl * cosLat - sinLat * (Nv + h));
    double dXdLon = -(Nv + h) * cosLat * sinLon;
    double dXdH   = cosLat * cosLon;

    // dY/dlat, dY/dlon, dY/dh
    double dYdLat = sinLon * (dNdl * cosLat - sinLat * (Nv + h));
    double dYdLon = (Nv + h) * cosLat * cosLon;
    double dYdH   = cosLat * sinLon;

    // dZ/dlat, dZ/dlon, dZ/dh
    double b2a2   = (ell.b * ell.b) / (ell.a * ell.a);
    double dZdLat = b2a2 * (sinLat * dNdl + cosLat * Nv) + h * cosLat;
    double dZdLon = 0.0;
    double dZdH   = sinLat;

    Eigen::Matrix3d J;
    J << dXdLat, dXdLon, dXdH,
         dYdLat, dYdLon, dYdH,
         dZdLat, dZdLon, dZdH;
    return J;
}

Eigen::Matrix3d loki::geodesy::jacobianSphereEcef(const EcefPoint& p)
{
    double R   = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
    double rxy = std::sqrt(p.x * p.x + p.y * p.y);

    double sqp = std::sqrt(1.0 - (p.z * p.z) / (R * R));
    double tpp = R * R * R;

    // dLat/dX, dLat/dY, dLat/dZ  (lat in radians)
    double dLatdX = -(p.x * p.z) / (tpp * sqp);
    double dLatdY = -(p.y * p.z) / (tpp * sqp);
    double dLatdZ = ((1.0 / R) - (p.z * p.z / tpp)) / sqp;

    // dLon/dX, dLon/dY, dLon/dZ  (lon in radians)
    double dLondX = -p.y / (rxy * rxy);
    double dLondY =  p.x / (rxy * rxy);
    double dLondZ = 0.0;

    // dR/dX, dR/dY, dR/dZ
    double dRdX = p.x / R;
    double dRdY = p.y / R;
    double dRdZ = p.z / R;

    Eigen::Matrix3d J;
    J << dLatdX, dLatdY, dLatdZ,
         dLondX, dLondY, dLondZ,
         dRdX,   dRdY,   dRdZ;
    return J;
}

Eigen::Matrix3d loki::geodesy::jacobianEcefSphere(const SpherePoint& p)
{
    double lat = toRad(p.lat);
    double lon = toRad(p.lon);
    double R   = p.radius;

    double sinLat = std::sin(lat);
    double cosLat = std::cos(lat);
    double sinLon = std::sin(lon);
    double cosLon = std::cos(lon);

    // d(X,Y,Z)/d(lat_rad, lon_rad, R)
    Eigen::Matrix3d J;
    J << -R * sinLat * cosLon, -R * cosLat * sinLon, cosLat * cosLon,
         -R * sinLat * sinLon,  R * cosLat * cosLon, cosLat * sinLon,
          R * cosLat,           0.0,                  sinLat;
    return J;
}

// ---------------------------------------------------------------------------
// Scaling matrices
// ---------------------------------------------------------------------------

Eigen::MatrixXd loki::geodesy::angularScaleDeg2Rad(int n)
{
    if (n != 3 && n != 6)
        throw loki::ConfigException("angularScaleDeg2Rad: n must be 3 or 6");

    Eigen::VectorXd diag(n);
    for (int i = 0; i < n; ++i)
        diag(i) = ((i % 3) < 2) ? DEG2RAD : 1.0;

    return diag.asDiagonal();
}

Eigen::MatrixXd loki::geodesy::angularScaleRad2Deg(int n)
{
    if (n != 3 && n != 6)
        throw loki::ConfigException("angularScaleRad2Deg: n must be 3 or 6");

    Eigen::VectorXd diag(n);
    for (int i = 0; i < n; ++i)
        diag(i) = ((i % 3) < 2) ? RAD2DEG : 1.0;

    return diag.asDiagonal();
}

// ---------------------------------------------------------------------------
// GEOD <-> ECEF
// ---------------------------------------------------------------------------

EcefPoint loki::geodesy::geod2ecef(const GeodPoint& p, const Ellipsoid& ell)
{
    double lat = toRad(p.lat);
    double lon = toRad(p.lon);

    double Nv     = ell.N(lat);
    double cosLat = std::cos(lat);
    double sinLat = std::sin(lat);
    double cosLon = std::cos(lon);
    double sinLon = std::sin(lon);

    double b2a2 = (ell.b * ell.b) / (ell.a * ell.a);

    return {
        (Nv + p.h) * cosLat * cosLon,
        (Nv + p.h) * cosLat * sinLon,
        (b2a2 * Nv + p.h) * sinLat
    };
}

GeodPoint loki::geodesy::ecef2geod(const EcefPoint& p, const Ellipsoid& ell)
{
    // Vermeille (2002) non-iterative method.
    double a   = ell.a;
    double eSq = ell.eSq;
    double eSq2 = eSq * eSq;

    double pxy = std::hypot(p.x, p.y);

    double pn = (pxy * pxy) / (a * a);
    double qn = (1.0 - eSq) * (p.z * p.z) / (a * a);
    double rn = (pn + qn - eSq2) / 6.0;

    double e4pq = eSq2 * pn * qn;
    double t    = 8.0 * rn * rn * rn + e4pq;

    double lat, hgt;

    if (t > 0.0 || (t <= 0.0 && qn != 0.0)) {
        double u;
        if (t > 0.0) {
            double li = std::cbrt(std::sqrt(t) + std::sqrt(e4pq));
            u = 1.5 * rn * rn / (li * li) + 0.5 * (li + rn / li) * (li + rn / li);
        } else {
            double uAux = (2.0 / 3.0) * std::atan2(std::sqrt(e4pq),
                                                     std::sqrt(-t) + std::sqrt(-8.0 * rn * rn * rn));
            u = -4.0 * rn * std::sin(uAux) * std::cos(std::numbers::pi / 6.0 + uAux);
        }
        double v = std::sqrt(u * u + eSq2 * qn);
        double w = eSq * (u + v - qn) / (2.0 * v);
        double k = (u + v) / (std::sqrt(w * w + u + v) + w);
        double D = k * pxy / (k + eSq);

        hgt = (k + eSq - 1.0) * std::hypot(D, p.z) / k;
        lat = 2.0 * std::atan2(p.z, D + std::hypot(D, p.z));
    } else {
        // qn == 0 && p <= eSq2  (point near equatorial plane)
        double e2p  = std::sqrt(eSq - pn);
        double me2  = std::sqrt(1.0 - eSq);
        hgt = -(a * me2 * e2p) / ell.e;
        lat = 2.0 * std::atan2(std::sqrt(eSq2 - pn),
                                ell.e * e2p + me2 * std::sqrt(pn));
    }

    double lon = std::atan2(p.y, p.x);
    return { toDeg(lat), toDeg(lon), hgt };
}

// ---------------------------------------------------------------------------
// SPHERE <-> ECEF
// ---------------------------------------------------------------------------

EcefPoint loki::geodesy::sphere2ecef(const SpherePoint& p)
{
    double lat = toRad(p.lat);
    double lon = toRad(p.lon);
    return {
        p.radius * std::cos(lat) * std::cos(lon),
        p.radius * std::cos(lat) * std::sin(lon),
        p.radius * std::sin(lat)
    };
}

SpherePoint loki::geodesy::ecef2sphere(const EcefPoint& p)
{
    double R   = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
    double lat = toDeg(std::asin(p.z / R));
    double lon = toDeg(std::atan2(p.y, p.x));
    return { lat, lon, R };
}

// ---------------------------------------------------------------------------
// ECEF <-> ENU
// ---------------------------------------------------------------------------

EnuPoint loki::geodesy::ecef2enu(const EcefPoint& p,
                                   const GeodPoint& origin,
                                   RefBody ref,
                                   const Ellipsoid& ell)
{
    EcefPoint o;
    if (ref == RefBody::ELLIPSOID) {
        o = geod2ecef(origin, ell);
    } else {
        // Treat origin.h as sphere radius
        o = sphere2ecef({ origin.lat, origin.lon, origin.h });
    }

    Eigen::Matrix3d R = ecefEnuRotMat(origin.lat, origin.lon);
    Eigen::Vector3d dp{ p.x - o.x, p.y - o.y, p.z - o.z };
    Eigen::Vector3d enu = R * dp;
    return { enu(0), enu(1), enu(2) };
}

EcefPoint loki::geodesy::enu2ecef(const EnuPoint& p,
                                   const GeodPoint& origin,
                                   RefBody ref,
                                   const Ellipsoid& ell)
{
    EcefPoint o;
    if (ref == RefBody::ELLIPSOID) {
        o = geod2ecef(origin, ell);
    } else {
        o = sphere2ecef({ origin.lat, origin.lon, origin.h });
    }

    Eigen::Matrix3d R = ecefEnuRotMat(origin.lat, origin.lon);
    Eigen::Vector3d enu{ p.e, p.n, p.u };
    Eigen::Vector3d ecef = R.transpose() * enu;
    return { ecef(0) + o.x, ecef(1) + o.y, ecef(2) + o.z };
}

// ---------------------------------------------------------------------------
// Convenience chains
// ---------------------------------------------------------------------------

EnuPoint loki::geodesy::geod2enu(const GeodPoint& p,
                                   const GeodPoint& origin,
                                   const Ellipsoid& ell)
{
    return ecef2enu(geod2ecef(p, ell), origin, RefBody::ELLIPSOID, ell);
}

GeodPoint loki::geodesy::enu2geod(const EnuPoint& p,
                                   const GeodPoint& origin,
                                   const Ellipsoid& ell)
{
    return ecef2geod(enu2ecef(p, origin, RefBody::ELLIPSOID, ell), ell);
}

SpherePoint loki::geodesy::geod2sphere(const GeodPoint& p, const Ellipsoid& ell)
{
    return ecef2sphere(geod2ecef(p, ell));
}

GeodPoint loki::geodesy::sphere2geod(const SpherePoint& p, const Ellipsoid& ell)
{
    return ecef2geod(sphere2ecef(p), ell);
}

EnuPoint loki::geodesy::sphere2enu(const SpherePoint& p,
                                    const GeodPoint& origin,
                                    const Ellipsoid& ell)
{
    return ecef2enu(sphere2ecef(p), origin, RefBody::SPHERE, ell);
}

SpherePoint loki::geodesy::enu2sphere(const EnuPoint& p,
                                       const GeodPoint& origin,
                                       const Ellipsoid& ell)
{
    return ecef2sphere(enu2ecef(p, origin, RefBody::SPHERE, ell));
}

// ---------------------------------------------------------------------------
// AES conversions
// ---------------------------------------------------------------------------

AesPoint loki::geodesy::enu2aes(const EnuPoint& p)
{
    double slant = std::sqrt(p.e * p.e + p.n * p.n + p.u * p.u);
    double el    = toDeg(std::atan2(p.u, std::hypot(p.e, p.n)));
    double az    = toDeg(std::atan2(p.e, p.n));
    if (az < 0.0) az += 360.0;
    return { az, el, slant };
}

EnuPoint loki::geodesy::aes2enu(const AesPoint& p)
{
    double az    = toRad(p.az);
    double el    = toRad(p.el);
    double cosEl = std::cos(el);
    return {
        p.slant * cosEl * std::sin(az),
        p.slant * cosEl * std::cos(az),
        p.slant * std::sin(el)
    };
}

// ---------------------------------------------------------------------------
// Covariance propagation helpers
// ---------------------------------------------------------------------------

namespace {

// Build 3x3 Jacobian for a single block, then assemble NxN (N=3 or 6).
// J3 is applied to both position and velocity blocks when N=6.
Eigen::MatrixXd buildBlockJacobian(const Eigen::Matrix3d& J3, int N)
{
    if (N == 3) return J3;
    return blockDiag6(J3);
}

// Propagate: Sigma_out = J * Sigma_in * J^T
Eigen::MatrixXd propagate(const Eigen::MatrixXd& J, const Eigen::MatrixXd& sigma)
{
    return J * sigma * J.transpose();
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// GEOD -> ECEF covariance
// ---------------------------------------------------------------------------

void loki::geodesy::trGeod2Ecef(const Eigen::VectorXd& geodPos,
                                  const Eigen::MatrixXd& geodCov,
                                  const Ellipsoid& ell,
                                  Eigen::VectorXd& ecefPos,
                                  Eigen::MatrixXd& ecefCov)
{
    int N = static_cast<int>(geodPos.size());
    if (N != 3 && N != 6)
        throw loki::DataException("trGeod2Ecef: state vector must be size 3 or 6");

    // Transform position
    GeodPoint gp{ geodPos(0), geodPos(1), geodPos(2) };
    EcefPoint ep = geod2ecef(gp, ell);
    ecefPos.resize(N);
    ecefPos(0) = ep.x; ecefPos(1) = ep.y; ecefPos(2) = ep.z;

    if (N == 6) {
        // Velocity block: treat geodPos(3..5) as velocity in geod coords,
        // transform identically using same Jacobian
        GeodPoint gv{ geodPos(3), geodPos(4), geodPos(5) };
        EcefPoint ev = geod2ecef(gv, ell);
        ecefPos(3) = ev.x; ecefPos(4) = ev.y; ecefPos(5) = ev.z;
    }

    // Convert GEOD covariance from deg to rad (angular components)
    Eigen::MatrixXd T   = angularScaleDeg2Rad(N);
    Eigen::MatrixXd sigRad = T * geodCov * T.transpose();

    // Jacobian d(ECEF)/d(GEOD_rad)
    Eigen::Matrix3d J3 = jacobianEcefGeod(gp.lat, gp.lon, gp.h, ell);
    Eigen::MatrixXd J  = buildBlockJacobian(J3, N);

    ecefCov = propagate(J, sigRad);
}

// ---------------------------------------------------------------------------
// ECEF -> GEOD covariance
// ---------------------------------------------------------------------------

void loki::geodesy::trEcef2Geod(const Eigen::VectorXd& ecefPos,
                                  const Eigen::MatrixXd& ecefCov,
                                  const Ellipsoid& ell,
                                  Eigen::VectorXd& geodPos,
                                  Eigen::MatrixXd& geodCov)
{
    int N = static_cast<int>(ecefPos.size());
    if (N != 3 && N != 6)
        throw loki::DataException("trEcef2Geod: state vector must be size 3 or 6");

    EcefPoint ep{ ecefPos(0), ecefPos(1), ecefPos(2) };
    GeodPoint gp = ecef2geod(ep, ell);
    geodPos.resize(N);
    geodPos(0) = gp.lat; geodPos(1) = gp.lon; geodPos(2) = gp.h;

    if (N == 6) {
        EcefPoint ev{ ecefPos(3), ecefPos(4), ecefPos(5) };
        GeodPoint gv = ecef2geod(ev, ell);
        geodPos(3) = gv.lat; geodPos(4) = gv.lon; geodPos(5) = gv.h;
    }

    // Jacobian d(ECEF)/d(GEOD_rad)
    Eigen::Matrix3d J3  = jacobianEcefGeod(gp.lat, gp.lon, gp.h, ell);
    Eigen::MatrixXd J   = buildBlockJacobian(J3, N);

    // Sigma_GEOD_rad = J^{-1} * Sigma_ECEF * (J^{-1})^T
    Eigen::MatrixXd Jinv = J.inverse();
    Eigen::MatrixXd sigRad = propagate(Jinv, ecefCov);

    // Convert from rad to deg
    Eigen::MatrixXd T = angularScaleRad2Deg(N);
    geodCov = T * sigRad * T.transpose();
}

// ---------------------------------------------------------------------------
// ECEF -> ENU covariance
// ---------------------------------------------------------------------------

void loki::geodesy::trEcef2Enu(const Eigen::VectorXd& ecefPos,
                                 const Eigen::MatrixXd& ecefCov,
                                 const GeodPoint& origin,
                                 RefBody /*ref*/,
                                 const Ellipsoid& ell,
                                 Eigen::VectorXd& enuPos,
                                 Eigen::MatrixXd& enuCov)
{
    int N = static_cast<int>(ecefPos.size());
    if (N != 3 && N != 6)
        throw loki::DataException("trEcef2Enu: state vector must be size 3 or 6");

    EcefPoint ep{ ecefPos(0), ecefPos(1), ecefPos(2) };
    EnuPoint  en = ecef2enu(ep, origin, RefBody::ELLIPSOID, ell);
    enuPos.resize(N);
    enuPos(0) = en.e; enuPos(1) = en.n; enuPos(2) = en.u;

    if (N == 6) {
        // Velocity: apply rotation only (no translation offset)
        Eigen::Matrix3d R = ecefEnuRotMat(origin.lat, origin.lon);
        Eigen::Vector3d vel{ ecefPos(3), ecefPos(4), ecefPos(5) };
        Eigen::Vector3d velEnu = R * vel;
        enuPos(3) = velEnu(0); enuPos(4) = velEnu(1); enuPos(5) = velEnu(2);
    }

    // Jacobian is the rotation matrix (same for position and velocity)
    Eigen::Matrix3d J3 = ecefEnuRotMat(origin.lat, origin.lon);
    Eigen::MatrixXd J  = buildBlockJacobian(J3, N);

    enuCov = propagate(J, ecefCov);
}

// ---------------------------------------------------------------------------
// ENU -> ECEF covariance
// ---------------------------------------------------------------------------

void loki::geodesy::trEnu2Ecef(const Eigen::VectorXd& enuPos,
                                 const Eigen::MatrixXd& enuCov,
                                 const GeodPoint& origin,
                                 RefBody ref,
                                 const Ellipsoid& ell,
                                 Eigen::VectorXd& ecefPos,
                                 Eigen::MatrixXd& ecefCov)
{
    int N = static_cast<int>(enuPos.size());
    if (N != 3 && N != 6)
        throw loki::DataException("trEnu2Ecef: state vector must be size 3 or 6");

    EnuPoint en{ enuPos(0), enuPos(1), enuPos(2) };
    EcefPoint ep = enu2ecef(en, origin, ref, ell);
    ecefPos.resize(N);
    ecefPos(0) = ep.x; ecefPos(1) = ep.y; ecefPos(2) = ep.z;

    if (N == 6) {
        Eigen::Matrix3d R = ecefEnuRotMat(origin.lat, origin.lon);
        Eigen::Vector3d vel{ enuPos(3), enuPos(4), enuPos(5) };
        Eigen::Vector3d velEcef = R.transpose() * vel;
        ecefPos(3) = velEcef(0); ecefPos(4) = velEcef(1); ecefPos(5) = velEcef(2);
    }

    // Jacobian for ENU->ECEF is R^T
    Eigen::Matrix3d J3 = ecefEnuRotMat(origin.lat, origin.lon).transpose();
    Eigen::MatrixXd J  = buildBlockJacobian(J3, N);

    ecefCov = propagate(J, enuCov);
}

// ---------------------------------------------------------------------------
// Chained propagations
// ---------------------------------------------------------------------------

void loki::geodesy::trGeod2Enu(const Eigen::VectorXd& geodPos,
                                 const Eigen::MatrixXd& geodCov,
                                 const GeodPoint& origin,
                                 const Ellipsoid& ell,
                                 Eigen::VectorXd& enuPos,
                                 Eigen::MatrixXd& enuCov)
{
    Eigen::VectorXd ecefPos;
    Eigen::MatrixXd ecefCov;
    trGeod2Ecef(geodPos, geodCov, ell, ecefPos, ecefCov);
    trEcef2Enu(ecefPos, ecefCov, origin, RefBody::ELLIPSOID, ell, enuPos, enuCov);
}

void loki::geodesy::trEnu2Geod(const Eigen::VectorXd& enuPos,
                                 const Eigen::MatrixXd& enuCov,
                                 const GeodPoint& origin,
                                 const Ellipsoid& ell,
                                 Eigen::VectorXd& geodPos,
                                 Eigen::MatrixXd& geodCov)
{
    Eigen::VectorXd ecefPos;
    Eigen::MatrixXd ecefCov;
    trEnu2Ecef(enuPos, enuCov, origin, RefBody::ELLIPSOID, ell, ecefPos, ecefCov);
    trEcef2Geod(ecefPos, ecefCov, ell, geodPos, geodCov);
}

// ---------------------------------------------------------------------------
// SPHERE <-> ECEF covariance
// ---------------------------------------------------------------------------

void loki::geodesy::trSphere2Ecef(const Eigen::VectorXd& sphPos,
                                    const Eigen::MatrixXd& sphCov,
                                    Eigen::VectorXd& ecefPos,
                                    Eigen::MatrixXd& ecefCov)
{
    int N = static_cast<int>(sphPos.size());
    if (N != 3 && N != 6)
        throw loki::DataException("trSphere2Ecef: state vector must be size 3 or 6");

    SpherePoint sp{ sphPos(0), sphPos(1), sphPos(2) };
    EcefPoint   ep = sphere2ecef(sp);
    ecefPos.resize(N);
    ecefPos(0) = ep.x; ecefPos(1) = ep.y; ecefPos(2) = ep.z;

    if (N == 6) {
        SpherePoint sv{ sphPos(3), sphPos(4), sphPos(5) };
        EcefPoint   ev = sphere2ecef(sv);
        ecefPos(3) = ev.x; ecefPos(4) = ev.y; ecefPos(5) = ev.z;
    }

    // Convert sphere covariance from deg to rad
    Eigen::MatrixXd T      = angularScaleDeg2Rad(N);
    Eigen::MatrixXd sigRad = T * sphCov * T.transpose();

    Eigen::Matrix3d J3 = jacobianEcefSphere(sp);
    Eigen::MatrixXd J  = buildBlockJacobian(J3, N);

    ecefCov = propagate(J, sigRad);
}

void loki::geodesy::trEcef2Sphere(const Eigen::VectorXd& ecefPos,
                                    const Eigen::MatrixXd& ecefCov,
                                    Eigen::VectorXd& sphPos,
                                    Eigen::MatrixXd& sphCov)
{
    int N = static_cast<int>(ecefPos.size());
    if (N != 3 && N != 6)
        throw loki::DataException("trEcef2Sphere: state vector must be size 3 or 6");

    EcefPoint   ep{ ecefPos(0), ecefPos(1), ecefPos(2) };
    SpherePoint sp = ecef2sphere(ep);
    sphPos.resize(N);
    sphPos(0) = sp.lat; sphPos(1) = sp.lon; sphPos(2) = sp.radius;

    if (N == 6) {
        EcefPoint   ev{ ecefPos(3), ecefPos(4), ecefPos(5) };
        SpherePoint sv = ecef2sphere(ev);
        sphPos(3) = sv.lat; sphPos(4) = sv.lon; sphPos(5) = sv.radius;
    }

    Eigen::Matrix3d J3     = jacobianSphereEcef(ep);
    Eigen::MatrixXd J      = buildBlockJacobian(J3, N);
    Eigen::MatrixXd sigRad = propagate(J, ecefCov);

    // Convert from rad to deg
    Eigen::MatrixXd T = angularScaleRad2Deg(N);
    sphCov = T * sigRad * T.transpose();
}

// ---------------------------------------------------------------------------
// Chained GEOD <-> SPHERE, SPHERE <-> ENU
// ---------------------------------------------------------------------------

void loki::geodesy::trGeod2Sphere(const Eigen::VectorXd& geodPos,
                                    const Eigen::MatrixXd& geodCov,
                                    const Ellipsoid& ell,
                                    Eigen::VectorXd& sphPos,
                                    Eigen::MatrixXd& sphCov)
{
    Eigen::VectorXd ecefPos;
    Eigen::MatrixXd ecefCov;
    trGeod2Ecef(geodPos, geodCov, ell, ecefPos, ecefCov);
    trEcef2Sphere(ecefPos, ecefCov, sphPos, sphCov);
}

void loki::geodesy::trSphere2Geod(const Eigen::VectorXd& sphPos,
                                    const Eigen::MatrixXd& sphCov,
                                    const Ellipsoid& ell,
                                    Eigen::VectorXd& geodPos,
                                    Eigen::MatrixXd& geodCov)
{
    Eigen::VectorXd ecefPos;
    Eigen::MatrixXd ecefCov;
    trSphere2Ecef(sphPos, sphCov, ecefPos, ecefCov);
    trEcef2Geod(ecefPos, ecefCov, ell, geodPos, geodCov);
}

void loki::geodesy::trSphere2Enu(const Eigen::VectorXd& sphPos,
                                   const Eigen::MatrixXd& sphCov,
                                   const GeodPoint& origin,
                                   const Ellipsoid& ell,
                                   Eigen::VectorXd& enuPos,
                                   Eigen::MatrixXd& enuCov)
{
    Eigen::VectorXd ecefPos;
    Eigen::MatrixXd ecefCov;
    trSphere2Ecef(sphPos, sphCov, ecefPos, ecefCov);
    trEcef2Enu(ecefPos, ecefCov, origin, RefBody::SPHERE, ell, enuPos, enuCov);
}

void loki::geodesy::trEnu2Sphere(const Eigen::VectorXd& enuPos,
                                   const Eigen::MatrixXd& enuCov,
                                   const GeodPoint& origin,
                                   const Ellipsoid& ell,
                                   Eigen::VectorXd& sphPos,
                                   Eigen::MatrixXd& sphCov)
{
    Eigen::VectorXd ecefPos;
    Eigen::MatrixXd ecefCov;
    trEnu2Ecef(enuPos, enuCov, origin, RefBody::SPHERE, ell, ecefPos, ecefCov);
    trEcef2Sphere(ecefPos, ecefCov, sphPos, sphCov);
}
