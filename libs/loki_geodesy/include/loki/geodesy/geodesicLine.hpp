#pragma once

#include <loki/math/ellipsoid.hpp>

namespace loki::geodesy {

/// @brief Result of a geodesic distance computation.
struct GeodesicResult {
    double distance;    ///< Geodesic distance [m]
    double azimuthFwd;  ///< Forward azimuth at start point [deg, 0..360)
    double azimuthRev;  ///< Reverse azimuth at end point [deg, 0..360)
};

/// @brief Result of the Vincenty direct problem.
struct DirectResult {
    double lat2;        ///< Latitude of end point [deg]
    double lon2;        ///< Longitude of end point [deg]
    double azimuthRev;  ///< Reverse azimuth at end point [deg, 0..360)
};

/**
 * @brief Vincenty inverse problem: geodesic distance and azimuths on an ellipsoid.
 *
 * Iterative solution converging to 1e-12 rad in lambda (~0.06 mm).
 * Near-antipodal points (convergence failure) are flagged with a warning;
 * the returned distance is approximate in that case.
 *
 * @param lat1Deg  Latitude of start point [deg, -90..90].
 * @param lon1Deg  Longitude of start point [deg, -180..180].
 * @param lat2Deg  Latitude of end point [deg].
 * @param lon2Deg  Longitude of end point [deg].
 * @param ell      Reference ellipsoid.
 * @return GeodesicResult with distance [m] and azimuths [deg].
 */
GeodesicResult vincentyInverse(double lat1Deg, double lon1Deg,
                                double lat2Deg, double lon2Deg,
                                const loki::math::Ellipsoid& ell);

/**
 * @brief Vincenty direct problem: end point given start, azimuth, distance.
 *
 * @param lat1Deg    Latitude of start point [deg].
 * @param lon1Deg    Longitude of start point [deg].
 * @param azimuthDeg Forward azimuth from start [deg, 0..360].
 * @param distance   Geodesic distance [m].
 * @param ell        Reference ellipsoid.
 * @return DirectResult with end point coordinates and reverse azimuth.
 */
DirectResult vincentyDirect(double lat1Deg, double lon1Deg,
                             double azimuthDeg, double distance,
                             const loki::math::Ellipsoid& ell);

/**
 * @brief Haversine great-circle distance on a sphere.
 *
 * Uses mean Earth radius 6371008.8 m (IUGG arithmetic mean).
 *
 * @param lat1Deg  Start latitude [deg].
 * @param lon1Deg  Start longitude [deg].
 * @param lat2Deg  End latitude [deg].
 * @param lon2Deg  End longitude [deg].
 * @return GeodesicResult; azimuthRev is the back-azimuth.
 */
GeodesicResult haversine(double lat1Deg, double lon1Deg,
                          double lat2Deg, double lon2Deg);

} // namespace loki::geodesy
