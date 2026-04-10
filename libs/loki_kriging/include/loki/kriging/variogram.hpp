#pragma once

#include <loki/core/config.hpp>
#include <loki/kriging/krigingResult.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <vector>

namespace loki::kriging {

// -----------------------------------------------------------------------------
//  Empirical variogram
// -----------------------------------------------------------------------------

/**
 * @brief Compute the empirical variogram from a temporal time series.
 *
 * Bins all observation pairs (i, j) by lag h = |t_i - t_j| (in MJD days)
 * into nLagBins equal-width bins over [0, maxLag].
 *
 * Only valid (non-NaN) observations are used. The series is assumed to be
 * gap-filled before calling this function.
 *
 * The empirical semi-variance per bin is:
 *   gamma_k = 0.5 * (1/N_k) * sum[ (z_i - z_j)^2 ]
 * where N_k is the number of pairs in bin k.
 *
 * Bins with fewer than 2 pairs are excluded from the returned vector.
 *
 * FUTURE (spatial mode): replace temporal lag with Euclidean distance
 * sqrt((x_i-x_j)^2 + (y_i-y_j)^2) and extend to directional variograms
 * (anisotropy) by binning pairs by both distance and direction angle.
 *
 * @param ts  Time series (gap-filled, valid observations only used).
 * @param cfg Variogram configuration (nLagBins, maxLag).
 * @return    Vector of VariogramPoint, sorted by ascending lag.
 * @throws DataException if fewer than 3 valid observations are present.
 */
std::vector<VariogramPoint> computeEmpiricalVariogram(
    const TimeSeries&          ts,
    const KrigingVariogramConfig& cfg);

// -----------------------------------------------------------------------------
//  Theoretical variogram models
// -----------------------------------------------------------------------------

/**
 * @brief Evaluate the spherical variogram model.
 *
 * gamma(h) = nugget + (sill - nugget) * [1.5*(h/range) - 0.5*(h/range)^3]  for h <= range
 *          = nugget + (sill - nugget)                                         for h >  range
 *
 * Bounded: reaches sill at h = range. Most commonly used in practice.
 *
 * @param h      Lag value (>= 0).
 * @param nugget Nugget effect (discontinuity at h=0).
 * @param sill   Sill (asymptote, total variance contribution).
 * @param range  Range (distance at which sill is reached).
 */
double variogramSpherical(double h, double nugget, double sill, double range);

/**
 * @brief Evaluate the exponential variogram model.
 *
 * gamma(h) = nugget + (sill - nugget) * [1 - exp(-h / range)]
 *
 * Approaches sill asymptotically (practical range ~= 3*range parameter).
 * Appropriate when correlation decays smoothly without a hard cutoff.
 */
double variogramExponential(double h, double nugget, double sill, double range);

/**
 * @brief Evaluate the Gaussian variogram model.
 *
 * gamma(h) = nugget + (sill - nugget) * [1 - exp(-(h/range)^2)]
 *
 * Very smooth near origin (twice differentiable process).
 * Practical range ~= sqrt(3)*range parameter.
 * Can cause numerical instabilities in the Kriging system for large datasets
 * -- use with care for n > 500 or closely-spaced observations.
 */
double variogramGaussian(double h, double nugget, double sill, double range);

/**
 * @brief Evaluate the power variogram model.
 *
 * gamma(h) = nugget + sill * h^range
 *
 * Unbounded (intrinsic hypothesis, no finite sill).
 * Parameter mapping (non-standard): sill = scaling coefficient (a),
 * range = power exponent (alpha, typically in (0, 2)).
 * Suitable for fractal-like processes (Brownian motion = power with alpha=1).
 *
 * FUTURE: consider renaming parameters to avoid confusion with bounded models.
 */
double variogramPower(double h, double nugget, double sill, double range);

/**
 * @brief Evaluate the pure nugget variogram model.
 *
 * gamma(h) = nugget  for h > 0
 *          = 0       for h = 0
 *
 * Represents pure measurement noise / white noise. No spatial correlation.
 * Rarely useful alone; can be added to another model as a nugget component.
 */
double variogramNugget(double h, double nugget);

/**
 * @brief Evaluate any supported theoretical variogram model.
 *
 * Dispatches to the appropriate model function based on fit.model.
 *
 * @param h   Lag value.
 * @param fit Fitted variogram parameters including model name.
 * @throws AlgorithmException if fit.model is not recognised.
 */
double variogramEval(double h, const VariogramFitResult& fit);

// -----------------------------------------------------------------------------
//  Variogram fitting
// -----------------------------------------------------------------------------

/**
 * @brief Fit a theoretical variogram model to empirical variogram data via WLS.
 *
 * Minimises the weighted least-squares objective:
 *   Q = sum_k [ N_k * (gamma_k - gamma_model(h_k; params))^2 ]
 * where N_k is the pair count (standard geostatistical WLS weighting scheme).
 *
 * Minimisation uses loki::math::nelderMead. Initial parameter estimates are
 * derived from the empirical variogram (nugget from near-zero lag bins, sill
 * from overall sample variance, range from midpoint of empirical variogram)
 * unless overridden by cfg.nugget / cfg.sill / cfg.range (non-zero = use as-is).
 *
 * Constraints enforced during minimisation:
 *   nugget >= 0
 *   sill   > nugget
 *   range  > 0
 * Violated parameter combinations incur a large penalty (not hard rejection).
 *
 * @param empirical Empirical variogram bins (from computeEmpiricalVariogram).
 * @param cfg       Variogram configuration (model name, optional initial params).
 * @return          Fitted variogram parameters and RMSE.
 * @throws DataException       if empirical has fewer than 3 valid bins.
 * @throws AlgorithmException  if cfg.model is not recognised.
 */
VariogramFitResult fitVariogram(
    const std::vector<VariogramPoint>& empirical,
    const KrigingVariogramConfig&      cfg);

} // namespace loki::kriging
