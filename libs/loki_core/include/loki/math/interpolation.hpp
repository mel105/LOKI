#pragma once

#include <array>
#include <vector>

namespace loki::math {

/**
 * @brief Lagrange polynomial interpolation.
 *
 * Standard IGS implementation for SP3 precise orbit interpolation.
 * The interpolation window of (order+1) points is centred on the
 * epoch nearest to t.  When t is near the file boundary the window
 * is shifted inward so it never extends outside the available data.
 *
 * Recommended order: 9 (IGS standard for 5-min SP3 epochs, ~1 mm).
 * For 15-min SP3 epochs use order 10 or 11.
 *
 * All arrays must be sorted in ascending time order.
 * times and values must have equal length >= order+1.
 */

/**
 * @brief Interpolates a scalar series to epoch t.
 *
 * @param times   Tabulated epoch values (any consistent unit, e.g. GPS sow).
 * @param values  Tabulated function values, same length as times.
 * @param t       Query epoch (same unit as times).
 * @param order   Polynomial order.  Window size = order+1.
 * @return        Interpolated value.
 * @throws        AlgorithmException if times.size() < order+1.
 */
double lagrangeInterp(const std::vector<double>& times,
                      const std::vector<double>& values,
                      double t,
                      int order = 9);

/**
 * @brief Interpolates a 3-D position vector to epoch t.
 *
 * Applies lagrangeInterp independently to each coordinate.
 *
 * @param times      Tabulated epoch values.
 * @param positions  Tabulated 3-D vectors, same length as times.
 * @param t          Query epoch.
 * @param order      Polynomial order.
 * @return           Interpolated [x, y, z].
 */
std::array<double, 3> lagrangeInterp3(
    const std::vector<double>&              times,
    const std::vector<std::array<double,3>>& positions,
    double t,
    int order = 9);

} // namespace loki::math
