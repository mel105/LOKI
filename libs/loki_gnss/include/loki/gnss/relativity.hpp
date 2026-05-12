#pragma once

#include <loki/gnss/gnssTypes.hpp>

#include <array>

namespace loki::gnss {

/**
 * @brief GNSS relativity corrections.
 *
 * Two relativistic effects are relevant for GNSS:
 *
 * 1. Relativistic satellite clock correction (dtr):
 *    dtr = F * e * sqrtA * sin(E)
 *    where F = -4.442807633e-10 s/m^0.5.
 *    This term is part of the satellite clock model and is computed
 *    inside KeplerOrbit::_keplerPropagate(). It is NOT handled here.
 *
 * 2. Sagnac (Earth-rotation) effect:
 *    During signal travel, Earth rotates. The satellite position at
 *    transmission must be rotated to match the receiver frame at reception.
 *    Equivalent path-length change [m]:
 *      delta = (OmegaE / c) * (xs * yr - ys * xr)
 *    where (xs, ys) is satellite ECEF and (xr, yr) is receiver ECEF.
 *
 *    Alternatively, the satellite position can be physically rotated
 *    by theta = OmegaE * tof before computing the range.
 *    Both approaches are numerically equivalent to better than 1 mm.
 *    We expose both: rotateSatPosition() for explicit rotation,
 *    sagnacDelay() for the scalar correction.
 */
namespace Relativity {

/// Earth rotation rate [rad/s] (WGS-84).
constexpr double OMEGA_E   = 7.2921151467e-5;
constexpr double SPEED_OF_LIGHT = 299792458.0;

/**
 * @brief Rotates a satellite ECEF position to account for Earth rotation
 *        during signal propagation.
 *
 * Applies a rotation of theta = OmegaE * tof around the Z-axis.
 * Only X and Y are affected (Z is unchanged).
 *
 * @param satEcef   Satellite ECEF [m] at signal transmission time.
 * @param tof       Signal time of flight [s].
 * @return          Rotated satellite ECEF [m].
 */
std::array<double, 3> rotateSatPosition(const std::array<double, 3>& satEcef,
                                         double tof);

/**
 * @brief Computes the Sagnac scalar path-length correction [m].
 *
 * delta = (OmegaE / c) * (xs * yr - ys * xr)
 *
 * Positive value means the apparent pseudorange is shorter than the
 * geometric range (subtract from pseudorange).
 *
 * @param satEcef   Satellite ECEF [m] (already rotated or at tx time).
 * @param rcvEcef   Receiver ECEF [m].
 * @return          Sagnac delay [m].
 */
double sagnacDelay(const std::array<double, 3>& satEcef,
                   const std::array<double, 3>& rcvEcef);

} // namespace Relativity

} // namespace loki::gnss
