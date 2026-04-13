#pragma once

#include <loki/core/config.hpp>
#include <loki/math/krigingTypes.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <vector>

namespace loki::math {

// -----------------------------------------------------------------------------
//  Empirical variogram
// -----------------------------------------------------------------------------

/**
 * @brief Compute the empirical variogram from a temporal time series.
 *
 * Bins all observation pairs (i, j) by lag h = |t_i - t_j| (MJD days)
 * into nLagBins equal-width bins over [0, maxLag].
 *
 * Only valid (non-NaN) observations are used. The series should be gap-filled
 * before calling this function.
 *
 * Semi-variance per bin:
 *   gamma_k = 0.5 * (1/N_k) * sum[ (z_i - z_j)^2 ]
 *
 * Bins with fewer than 2 pairs are excluded from the result.
 *
 * FUTURE (spatial): replace temporal lag with Euclidean distance and extend
 * to directional variograms for anisotropy analysis.
 *
 * @param ts  Time series (gap-filled).
 * @param cfg Variogram configuration (nLagBins, maxLag).
 * @return    Vector of VariogramPoint, sorted by ascending lag.
 * @throws DataException if fewer than 3 valid observations are present.
 */
std::vector<VariogramPoint> computeEmpiricalVariogram(
    const TimeSeries&             ts,
    const KrigingVariogramConfig& cfg);

// -----------------------------------------------------------------------------
//  Theoretical variogram models
// -----------------------------------------------------------------------------

/**
 * @brief Spherical variogram model.
 *
 * gamma(h) = nugget + (sill-nugget) * [1.5*(h/range) - 0.5*(h/range)^3]  h <= range
 *          = nugget + (sill-nugget)                                         h >  range
 *
 * Bounded -- reaches sill exactly at h = range. Most commonly used.
 */
double variogramSpherical(double h, double nugget, double sill, double range);

/**
 * @brief Exponential variogram model.
 *
 * gamma(h) = nugget + (sill-nugget) * [1 - exp(-h/range)]
 *
 * Approaches sill asymptotically. Practical range ~= 3 * range parameter.
 */
double variogramExponential(double h, double nugget, double sill, double range);

/**
 * @brief Gaussian variogram model.
 *
 * gamma(h) = nugget + (sill-nugget) * [1 - exp(-(h/range)^2)]
 *
 * Very smooth near origin (twice differentiable). Practical range ~= sqrt(3)*range.
 * Recommended for smooth sensor signals (velocity, acceleration).
 * May cause numerical instability for large n with closely spaced observations.
 */
double variogramGaussian(double h, double nugget, double sill, double range);

/**
 * @brief Power variogram model (unbounded, intrinsic hypothesis).
 *
 * gamma(h) = nugget + sill * h^range
 *
 * Non-standard parameter usage: sill = scaling coefficient, range = exponent in (0,2).
 * Suitable for fractal-like processes.
 */
double variogramPower(double h, double nugget, double sill, double range);

/**
 * @brief Pure nugget variogram model (no spatial correlation).
 *
 * gamma(h) = nugget  for h > 0
 *          = 0       for h = 0
 */
double variogramNugget(double h, double nugget);

/**
 * @brief Dispatch to any supported theoretical variogram model.
 *
 * @param h   Lag value.
 * @param fit Fitted variogram parameters (model name + nugget/sill/range).
 * @throws AlgorithmException if fit.model is not recognised.
 */
double variogramEval(double h, const VariogramFitResult& fit);

// -----------------------------------------------------------------------------
//  Variogram fitting
// -----------------------------------------------------------------------------

/**
 * @brief Fit a theoretical variogram model to empirical bins via WLS.
 *
 * Minimises the weighted least-squares objective:
 *   Q = sum_k [ N_k * (gamma_k - gamma_model(h_k))^2 ]
 *
 * Uses loki::math::nelderMead for minimisation. Initial parameters are derived
 * automatically from the empirical variogram unless overridden in cfg
 * (non-zero cfg.nugget / cfg.sill / cfg.range are used as-is).
 *
 * Constraints (penalty-based):
 *   nugget >= 0,  sill > nugget,  range > 0
 *
 * @param empirical Empirical variogram (from computeEmpiricalVariogram).
 * @param cfg       Variogram configuration (model, optional initial params).
 * @return          Fitted parameters and WLS RMSE.
 * @throws DataException       if empirical has fewer than 3 bins.
 * @throws AlgorithmException  if cfg.model is not recognised.
 */
VariogramFitResult fitVariogram(
    const std::vector<VariogramPoint>& empirical,
    const KrigingVariogramConfig&      cfg);

} // namespace loki::math
