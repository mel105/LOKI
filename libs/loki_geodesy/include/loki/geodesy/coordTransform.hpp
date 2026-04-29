#pragma once

#include <loki/math/ellipsoid.hpp>

#include <Eigen/Dense>
#include <string>

namespace loki::geodesy {

/**
 * @brief Coordinate transformation functions between ECEF, GEOD, SPHERE, ENU and AES.
 *
 * All angle inputs and outputs are in DEGREES unless the function name or
 * parameter explicitly states radians.  Height and Cartesian coordinates
 * are always in metres.
 *
 * Covariance matrices follow the law of error propagation:
 *   Sigma_out = J * Sigma_in * J^T
 *
 * For ECEF<->GEOD the Jacobian J = d(ECEF)/d(GEOD) is used.
 * The GEOD covariance matrix is assumed to be in DEGREES (lat, lon) and
 * metres (h).  Internal scaling to radians is handled automatically.
 *
 * Extended state vectors of size 6 (position + velocity) are supported.
 * The 6x6 block-diagonal Jacobian applies the same 3x3 rotation to both
 * the position and velocity blocks.
 *
 * Reference bodies:
 *   ELLIPSOID - rotation ellipsoid (default, uses Ellipsoid parameter)
 *   SPHERE    - mean sphere; hgt0 / radius parameter interpreted as sphere radius
 */
enum class RefBody { ELLIPSOID, SPHERE };

// ---------------------------------------------------------------------------
// Point structs
// ---------------------------------------------------------------------------

/// @brief ECEF Cartesian position [m].
struct EcefPoint { double x, y, z; };

/// @brief Geodetic position: lat [deg], lon [deg], h [m].
struct GeodPoint { double lat, lon, h; };

/// @brief Spherical position: lat [deg], lon [deg], radius [m].
struct SpherePoint { double lat, lon, radius; };

/// @brief Local topocentric ENU [m].
struct EnuPoint { double e, n, u; };

/// @brief Polar AES coordinates: azimuth [deg], elevation [deg], slant [m].
struct AesPoint { double az, el, slant; };

// ---------------------------------------------------------------------------
// Basic coordinate transformations
// ---------------------------------------------------------------------------

/**
 * @brief Geodetic -> ECEF.
 * @param p    Geodetic point (lat/lon in deg, h in m).
 * @param ell  Reference ellipsoid.
 */
EcefPoint geod2ecef(const GeodPoint& p, const loki::math::Ellipsoid& ell);

/**
 * @brief ECEF -> Geodetic (Vermeille 2002 non-iterative method).
 * @param p    ECEF point [m].
 * @param ell  Reference ellipsoid.
 * @return Geodetic point (lat/lon in deg, h in m).
 */
GeodPoint ecef2geod(const EcefPoint& p, const loki::math::Ellipsoid& ell);

/**
 * @brief Sphere -> ECEF.
 * @param p  Spherical point (lat/lon in deg, radius in m).
 */
EcefPoint sphere2ecef(const SpherePoint& p);

/**
 * @brief ECEF -> Sphere.
 * @param p  ECEF point [m].
 * @return Spherical point (lat/lon in deg, radius in m).
 */
SpherePoint ecef2sphere(const EcefPoint& p);

/**
 * @brief ECEF -> ENU.
 * @param p      ECEF point to transform [m].
 * @param origin Origin of ENU frame (geodetic or spherical depending on ref).
 * @param ref    Reference body (ELLIPSOID or SPHERE).
 * @param ell    Ellipsoid (used only when ref == ELLIPSOID).
 */
EnuPoint ecef2enu(const EcefPoint& p,
                  const GeodPoint& origin,
                  RefBody ref,
                  const loki::math::Ellipsoid& ell);

/**
 * @brief ENU -> ECEF.
 * @param p      ENU point [m].
 * @param origin Origin of ENU frame (geodetic).
 * @param ref    Reference body.
 * @param ell    Ellipsoid.
 */
EcefPoint enu2ecef(const EnuPoint& p,
                   const GeodPoint& origin,
                   RefBody ref,
                   const loki::math::Ellipsoid& ell);

/**
 * @brief Geodetic -> ENU (convenience, chains geod2ecef + ecef2enu).
 */
EnuPoint geod2enu(const GeodPoint& p,
                  const GeodPoint& origin,
                  const loki::math::Ellipsoid& ell);

/**
 * @brief ENU -> Geodetic (convenience, chains enu2ecef + ecef2geod).
 */
GeodPoint enu2geod(const EnuPoint& p,
                   const GeodPoint& origin,
                   const loki::math::Ellipsoid& ell);

/**
 * @brief Geodetic -> Spherical (chains geod2ecef + ecef2sphere).
 */
SpherePoint geod2sphere(const GeodPoint& p, const loki::math::Ellipsoid& ell);

/**
 * @brief Spherical -> Geodetic (chains sphere2ecef + ecef2geod).
 */
GeodPoint sphere2geod(const SpherePoint& p, const loki::math::Ellipsoid& ell);

/**
 * @brief Spherical -> ENU.
 */
EnuPoint sphere2enu(const SpherePoint& p,
                    const GeodPoint& origin,
                    const loki::math::Ellipsoid& ell);

/**
 * @brief ENU -> Spherical.
 */
SpherePoint enu2sphere(const EnuPoint& p,
                       const GeodPoint& origin,
                       const loki::math::Ellipsoid& ell);

// ---------------------------------------------------------------------------
// AES (Azimuth-Elevation-Slant) conversions
// ---------------------------------------------------------------------------

/**
 * @brief ENU -> AES polar coordinates.
 * @return AES point: az [deg, 0..360), el [deg], slant [m].
 */
AesPoint enu2aes(const EnuPoint& p);

/**
 * @brief AES -> ENU.
 * @param p  AES point: az [deg], el [deg], slant [m].
 */
EnuPoint aes2enu(const AesPoint& p);

// ---------------------------------------------------------------------------
// Rotation matrix ECEF <-> ENU
// ---------------------------------------------------------------------------

/**
 * @brief Rotation matrix R such that enu_vec = R * (ecef_vec - ecef_origin).
 *
 * This is also the Jacobian for ECEF->ENU covariance propagation:
 *   Sigma_ENU = R * Sigma_ECEF * R^T
 *
 * @param latDeg  Latitude of ENU origin [deg].
 * @param lonDeg  Longitude of ENU origin [deg].
 */
Eigen::Matrix3d ecefEnuRotMat(double latDeg, double lonDeg);

// ---------------------------------------------------------------------------
// Jacobians
// ---------------------------------------------------------------------------

/**
 * @brief Jacobian d(ECEF)/d(GEOD) evaluated at (lat, lon, h).
 *
 * J[i][j] = d(X,Y,Z)[i] / d(lat_rad, lon_rad, h)[j]
 *
 * Note: the Jacobian is with respect to lat/lon in RADIANS and h in metres.
 * Callers must apply the degree-to-radian scaling matrix T to the GEOD
 * covariance matrix before propagation when working in degrees.
 *
 * @param latDeg  Geodetic latitude [deg].
 * @param lonDeg  Geodetic longitude [deg].
 * @param h       Ellipsoidal height [m].
 * @param ell     Reference ellipsoid.
 */
Eigen::Matrix3d jacobianEcefGeod(double latDeg, double lonDeg, double h,
                                  const loki::math::Ellipsoid& ell);

/**
 * @brief Jacobian d(SPHERE)/d(ECEF) evaluated at ECEF point.
 *
 * J[i][j] = d(lat_rad, lon_rad, R)[i] / d(X, Y, Z)[j]
 */
Eigen::Matrix3d jacobianSphereEcef(const EcefPoint& p);

/**
 * @brief Jacobian d(ECEF)/d(SPHERE) evaluated at spherical point.
 *
 * J[i][j] = d(X, Y, Z)[i] / d(lat_rad, lon_rad, R)[j]
 */
Eigen::Matrix3d jacobianEcefSphere(const SpherePoint& p);

// ---------------------------------------------------------------------------
// Scaling matrices for degree <-> radian covariance conversion
// ---------------------------------------------------------------------------

/**
 * @brief Scaling matrix T that converts a covariance matrix from deg to rad.
 *
 * For a 3x3 GEOD/SPHERE covariance where rows/cols 0,1 are angular (deg)
 * and row/col 2 is metric:
 *   T = diag(pi/180, pi/180, 1)
 *   Sigma_rad = T * Sigma_deg * T^T
 *
 * @param n  Matrix dimension (3 or 6).
 */
Eigen::MatrixXd angularScaleDeg2Rad(int n);

/**
 * @brief Scaling matrix that converts a covariance matrix from rad to deg.
 *
 * T = diag(180/pi, 180/pi, 1)  (size 3 or 6)
 */
Eigen::MatrixXd angularScaleRad2Deg(int n);

// ---------------------------------------------------------------------------
// Covariance propagation with extended state vector support
// ---------------------------------------------------------------------------

/**
 * @brief Transform covariance: GEOD -> ECEF.
 *
 * Input covariance is in [deg, deg, m] (and optionally [deg/s, deg/s, m/s]).
 * Output covariance is in [m, m, m].
 *
 * @param geodPos    Position vector (lat, lon, h) in deg/m, size 3 or 6.
 * @param geodCov    Input covariance, size NxN (N=3 or 6).
 * @param ell        Reference ellipsoid.
 * @param ecefPos    [out] Transformed position vector.
 * @param ecefCov    [out] Transformed covariance.
 */
void trGeod2Ecef(const Eigen::VectorXd& geodPos,
                 const Eigen::MatrixXd& geodCov,
                 const loki::math::Ellipsoid& ell,
                 Eigen::VectorXd& ecefPos,
                 Eigen::MatrixXd& ecefCov);

/**
 * @brief Transform covariance: ECEF -> GEOD.
 *
 * Output covariance is in [deg, deg, m].
 * Uses J^{-1} * Sigma_ECEF * (J^{-1})^T.
 */
void trEcef2Geod(const Eigen::VectorXd& ecefPos,
                 const Eigen::MatrixXd& ecefCov,
                 const loki::math::Ellipsoid& ell,
                 Eigen::VectorXd& geodPos,
                 Eigen::MatrixXd& geodCov);

/**
 * @brief Transform covariance: ECEF -> ENU.
 *
 * @param ecefPos   ECEF state vector, size 3 or 6.
 * @param ecefCov   ECEF covariance.
 * @param origin    ENU origin in geodetic coordinates.
 * @param ref       Reference body.
 * @param ell       Ellipsoid.
 * @param enuPos    [out] ENU state vector.
 * @param enuCov    [out] ENU covariance.
 */
void trEcef2Enu(const Eigen::VectorXd& ecefPos,
                const Eigen::MatrixXd& ecefCov,
                const GeodPoint& origin,
                RefBody ref,
                const loki::math::Ellipsoid& ell,
                Eigen::VectorXd& enuPos,
                Eigen::MatrixXd& enuCov);

/**
 * @brief Transform covariance: ENU -> ECEF.
 */
void trEnu2Ecef(const Eigen::VectorXd& enuPos,
                const Eigen::MatrixXd& enuCov,
                const GeodPoint& origin,
                RefBody ref,
                const loki::math::Ellipsoid& ell,
                Eigen::VectorXd& ecefPos,
                Eigen::MatrixXd& ecefCov);

/**
 * @brief Transform covariance: GEOD -> ENU (chains GEOD->ECEF->ENU).
 */
void trGeod2Enu(const Eigen::VectorXd& geodPos,
                const Eigen::MatrixXd& geodCov,
                const GeodPoint& origin,
                const loki::math::Ellipsoid& ell,
                Eigen::VectorXd& enuPos,
                Eigen::MatrixXd& enuCov);

/**
 * @brief Transform covariance: ENU -> GEOD (chains ENU->ECEF->GEOD).
 */
void trEnu2Geod(const Eigen::VectorXd& enuPos,
                const Eigen::MatrixXd& enuCov,
                const GeodPoint& origin,
                const loki::math::Ellipsoid& ell,
                Eigen::VectorXd& geodPos,
                Eigen::MatrixXd& geodCov);

/**
 * @brief Transform covariance: SPHERE -> ECEF.
 *
 * Input covariance is in [deg, deg, m].
 */
void trSphere2Ecef(const Eigen::VectorXd& sphPos,
                   const Eigen::MatrixXd& sphCov,
                   Eigen::VectorXd& ecefPos,
                   Eigen::MatrixXd& ecefCov);

/**
 * @brief Transform covariance: ECEF -> SPHERE.
 *
 * Output covariance is in [deg, deg, m].
 */
void trEcef2Sphere(const Eigen::VectorXd& ecefPos,
                   const Eigen::MatrixXd& ecefCov,
                   Eigen::VectorXd& sphPos,
                   Eigen::MatrixXd& sphCov);

/**
 * @brief Transform covariance: GEOD -> SPHERE (chains GEOD->ECEF->SPHERE).
 */
void trGeod2Sphere(const Eigen::VectorXd& geodPos,
                   const Eigen::MatrixXd& geodCov,
                   const loki::math::Ellipsoid& ell,
                   Eigen::VectorXd& sphPos,
                   Eigen::MatrixXd& sphCov);

/**
 * @brief Transform covariance: SPHERE -> GEOD (chains SPHERE->ECEF->GEOD).
 */
void trSphere2Geod(const Eigen::VectorXd& sphPos,
                   const Eigen::MatrixXd& sphCov,
                   const loki::math::Ellipsoid& ell,
                   Eigen::VectorXd& geodPos,
                   Eigen::MatrixXd& geodCov);

/**
 * @brief Transform covariance: SPHERE -> ENU (chains SPHERE->ECEF->ENU).
 */
void trSphere2Enu(const Eigen::VectorXd& sphPos,
                  const Eigen::MatrixXd& sphCov,
                  const GeodPoint& origin,
                  const loki::math::Ellipsoid& ell,
                  Eigen::VectorXd& enuPos,
                  Eigen::MatrixXd& enuCov);

/**
 * @brief Transform covariance: ENU -> SPHERE (chains ENU->ECEF->SPHERE).
 */
void trEnu2Sphere(const Eigen::VectorXd& enuPos,
                  const Eigen::MatrixXd& enuCov,
                  const GeodPoint& origin,
                  const loki::math::Ellipsoid& ell,
                  Eigen::VectorXd& sphPos,
                  Eigen::MatrixXd& sphCov);

} // namespace loki::geodesy
