#pragma once

#include <loki/core/config.hpp>
#include <loki/math/krigingTypes.hpp>
#include <loki/math/spatialTypes.hpp>

#include <cmath>
#include <vector>

namespace loki::math {

// =============================================================================
//  Empirical spatial variogram
// =============================================================================

/**
 * @brief Compute the isotropic empirical variogram from a 2-D scatter dataset.
 *
 * Bins all observation pairs (i, j) by Euclidean lag
 *   h_ij = sqrt( (xi - xj)^2 + (yi - yj)^2 )
 * into nLagBins equal-width bins over [0, maxLag].
 *
 * Semi-variance per bin:
 *   gamma_k = 0.5 * (1/N_k) * sum[ (z_i - z_j)^2 ]
 *
 * Bins with fewer than 2 pairs are excluded from the result.
 *
 * @param points   Input scatter observations (x, y, value). Must have >= 3.
 * @param nLagBins Number of equally-spaced lag bins (>= 3).
 * @param maxLag   Maximum lag for binning. 0.0 = auto (half of max pair distance).
 * @return         Vector of VariogramPoint sorted by ascending lag.
 * @throws DataException if fewer than 3 points are provided.
 */
std::vector<VariogramPoint> computeSpatialVariogram(
    const std::vector<SpatialPoint>& points,
    int                              nLagBins,
    double                           maxLag);

// =============================================================================
//  Variogram model fitting  (reuses krigingVariogram models)
// =============================================================================

/**
 * @brief Fit a theoretical variogram model to empirical spatial bins via WLS.
 *
 * Delegates to fitVariogram() from krigingVariogram.hpp -- same models,
 * same WLS + Nelder-Mead minimisation. Only the lag computation differs
 * (handled upstream in computeSpatialVariogram).
 *
 * @param empirical Empirical spatial variogram bins.
 * @param cfg       Variogram configuration (model name + optional initial params).
 * @return          Fitted VariogramFitResult.
 * @throws DataException      if fewer than 3 bins.
 * @throws AlgorithmException if cfg.model is not recognised.
 */
VariogramFitResult fitSpatialVariogram(
    const std::vector<VariogramPoint>& empirical,
    const KrigingVariogramConfig&      cfg);

// =============================================================================
//  Euclidean distance helpers
// =============================================================================

/**
 * @brief Compute Euclidean distance between two spatial points.
 */
inline double euclideanDistance(const SpatialPoint& a, const SpatialPoint& b)
{
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

/**
 * @brief Compute Euclidean distance between two (x, y) coordinate pairs.
 */
inline double euclideanDistance(double x1, double y1, double x2, double y2)
{
    const double dx = x1 - x2;
    const double dy = y1 - y2;
    return std::sqrt(dx * dx + dy * dy);
}

} // namespace loki::math
