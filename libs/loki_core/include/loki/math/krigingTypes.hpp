#pragma once

#include <string>
#include <vector>

namespace loki::math {

// -----------------------------------------------------------------------------
//  VariogramPoint
// -----------------------------------------------------------------------------

/**
 * @brief One bin of the empirical variogram.
 *
 * The empirical semi-variance is:
 *   gamma(h) = 0.5 * mean[ (Z(t+h) - Z(t))^2 ]
 * aggregated over all pairs whose lag |t_i - t_j| falls into this bin.
 *
 * In temporal Kriging the lag unit is MJD days.
 * FUTURE (spatial): lag unit will be the coordinate unit (degrees or metres).
 */
struct VariogramPoint {
    double lag;   ///< Bin-centre lag value.
    double gamma; ///< Empirical semi-variance for this bin.
    int    count; ///< Number of observation pairs contributing to this bin.
};

// -----------------------------------------------------------------------------
//  VariogramFitResult
// -----------------------------------------------------------------------------

/**
 * @brief Result of fitting a theoretical variogram model to empirical data.
 *
 * Parameters are estimated via weighted least squares using Nelder-Mead.
 * Weights are proportional to the number of pairs per bin (standard WLS).
 *
 * For the "power" model the "sill" parameter stores the scaling coefficient
 * and "range" stores the power exponent -- non-standard, see krigingVariogram.hpp.
 *
 * FUTURE: anisotropy parameters (angle, ratio) for spatial / space-time Kriging.
 */
struct VariogramFitResult {
    std::string model;     ///< "spherical"|"exponential"|"gaussian"|"power"|"nugget"
    double      nugget;    ///< Fitted nugget (discontinuity at h=0).
    double      sill;      ///< Fitted sill (total variance asymptote).
    double      range;     ///< Fitted range (correlation length).
    double      rmse;      ///< WLS residual RMSE (goodness of fit indicator).
    bool        converged; ///< True if Nelder-Mead converged within tolerance.
};

// -----------------------------------------------------------------------------
//  KrigingPrediction
// -----------------------------------------------------------------------------

/**
 * @brief Kriging estimate at a single target point.
 *
 * In temporal mode the target is identified by its MJD value.
 * FUTURE (spatial): add x, y coordinate fields.
 *
 * The Kriging variance sigma^2 quantifies prediction uncertainty.
 * CI bounds assume a Gaussian distribution:
 *   CI = value +/- z * sqrt(variance)
 * where z is the N(0,1) quantile at the configured confidence level.
 *
 * With a nugget effect, variance at observation locations equals the nugget
 * (not zero), reflecting measurement noise that is always present.
 */
struct KrigingPrediction {
    double mjd;        ///< Target time (MJD). FUTURE: add x, y for spatial.
    double value;      ///< Kriging estimate Z*(t).
    double variance;   ///< Kriging variance sigma^2_K(t).
    double ciLower;    ///< Lower confidence bound.
    double ciUpper;    ///< Upper confidence bound.
    bool   isObserved; ///< True if this target coincides with an observation.
};

// -----------------------------------------------------------------------------
//  CrossValidationResult
// -----------------------------------------------------------------------------

/**
 * @brief Leave-one-out cross-validation diagnostics.
 *
 * Computed via the O(n^2) LOO shortcut (Dubrule 1983):
 *   e_i = alpha_i / [K^{-1}]_{ii}
 * where alpha = K^{-1} * z. No re-factorisation per left-out point.
 *
 * Ideal values:
 *   meanSE  ~= 0  (unbiased estimator)
 *   meanSSE ~= 1  (correctly calibrated variance)
 *
 * meanSSE > 1: variance underestimated (CI bands too narrow).
 * meanSSE < 1: variance overestimated (CI bands too wide).
 */
struct CrossValidationResult {
    double              rmse;      ///< sqrt( mean(e_i^2) )
    double              mae;       ///< mean( |e_i| )
    double              mse;       ///< mean( e_i^2 )
    double              meanSE;    ///< mean( e_i / sigma_i ) -- bias indicator
    double              meanSSE;   ///< mean( (e_i / sigma_i)^2 ) -- variance calibration
    std::vector<double> errors;    ///< Raw LOO errors e_i = z_i - z_hat_i
    std::vector<double> stdErrors; ///< Standardised: e_i / sqrt(sigma^2_i)
    std::vector<double> mjd;       ///< MJD of each LOO point (for plotting)
};

} // namespace loki::math
