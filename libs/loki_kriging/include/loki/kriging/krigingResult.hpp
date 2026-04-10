#pragma once

#include <string>
#include <vector>

namespace loki::kriging {

// -----------------------------------------------------------------------------
//  VariogramPoint
// -----------------------------------------------------------------------------

/**
 * @brief One bin of the empirical variogram.
 *
 * The empirical semi-variance is:
 *   gamma(h) = 0.5 * mean[ (Z(t+h) - Z(t))^2 ]
 * aggregated over all pairs whose lag |t_i - t_j| falls into this bin.
 */
struct VariogramPoint {
    double lag;   ///< Bin-centre lag (MJD days for temporal; coord units for spatial).
    double gamma; ///< Empirical semi-variance for this bin.
    int    count; ///< Number of observation pairs contributing to this bin.
};

// -----------------------------------------------------------------------------
//  VariogramFitResult
// -----------------------------------------------------------------------------

/**
 * @brief Result of fitting a theoretical variogram model to empirical data.
 *
 * Parameters (nugget, sill, range) are estimated via weighted least squares
 * using Nelder-Mead minimisation. Weights are proportional to the number
 * of pairs per bin (standard geostatistical WLS weighting).
 *
 * For the "power" model the "sill" parameter stores the power exponent
 * and "range" stores the scaling coefficient (non-standard usage --
 * documented per model in variogram.hpp).
 *
 * FUTURE: anisotropy parameters (angle, anisotropy ratio) will be added
 * here when spatial / space-time Kriging is implemented.
 */
struct VariogramFitResult {
    std::string model;      ///< Model name: "spherical"|"exponential"|"gaussian"|"power"|"nugget"
    double      nugget;     ///< Fitted nugget (discontinuity at h=0).
    double      sill;       ///< Fitted sill (total variance asymptote).
    double      range;      ///< Fitted range (correlation length).
    double      rmse;       ///< WLS residual RMSE (goodness of fit indicator).
    bool        converged;  ///< True if Nelder-Mead converged within tolerance.
};

// -----------------------------------------------------------------------------
//  KrigingPrediction
// -----------------------------------------------------------------------------

/**
 * @brief Kriging estimate at a single target point.
 *
 * In temporal mode the target is a MJD time value.
 * In spatial mode (FUTURE) the target would be an (x, y) coordinate pair.
 *
 * The Kriging variance (sigma^2) quantifies the uncertainty of the estimate.
 * Confidence interval bounds assume a Gaussian distribution:
 *   CI = estimate +/- z * sqrt(variance)
 * where z = quantile of N(0,1) at the configured confidence level.
 *
 * Note: Kriging variance depends only on the variogram and data configuration,
 * not on the observed values. It is zero at observation locations (for ordinary
 * and simple Kriging without nugget).
 */
struct KrigingPrediction {
    double mjd;         ///< Target time (MJD). FUTURE: add x, y for spatial.
    double value;       ///< Kriging estimate Z*(t).
    double variance;    ///< Kriging variance sigma^2_K(t).
    double ciLower;     ///< Lower confidence bound.
    double ciUpper;     ///< Upper confidence bound.
    bool   isObserved;  ///< True if this target coincides with an observation.
};

// -----------------------------------------------------------------------------
//  CrossValidationResult
// -----------------------------------------------------------------------------

/**
 * @brief Leave-one-out cross-validation diagnostics.
 *
 * Each observation is temporarily removed and re-estimated from its neighbours.
 * The standardised error e*_i = e_i / sqrt(sigma^2_i) should follow N(0,1)
 * if the variogram model is correctly specified.
 *
 * Diagnostics:
 *   RMSE    -- root mean squared error (same units as data).
 *   MAE     -- mean absolute error.
 *   MSE     -- mean squared prediction error.
 *   meanSE  -- mean standardised error (should be ~0 for unbiased estimator).
 *   meanSSE -- mean squared standardised error (should be ~1 for correct variance).
 */
struct CrossValidationResult {
    double              rmse;          ///< sqrt( mean(e_i^2) )
    double              mae;           ///< mean( |e_i| )
    double              mse;           ///< mean( e_i^2 )
    double              meanSE;        ///< mean( e_i / sigma_i ) -- bias indicator
    double              meanSSE;       ///< mean( (e_i / sigma_i)^2 ) -- variance calibration
    std::vector<double> errors;        ///< Raw LOO errors e_i = z_i - z_hat_i
    std::vector<double> stdErrors;     ///< Standardised: e_i / sqrt(sigma^2_i)
    std::vector<double> mjd;           ///< MJD of each LOO point (for plotting)
};

// -----------------------------------------------------------------------------
//  KrigingResult
// -----------------------------------------------------------------------------

/**
 * @brief Top-level result container for the Kriging pipeline.
 *
 * Produced by KrigingAnalyzer::run() and consumed by PlotKriging and
 * the protocol/CSV writers.
 */
struct KrigingResult {
    std::string                    mode;               ///< "temporal"|"spatial"|"space_time"
    std::string                    method;             ///< "simple"|"ordinary"|"universal"
    int                            nObs;               ///< Number of valid observations used.
    double                         meanValue;          ///< Sample mean of observations.
    double                         sampleVariance;     ///< Sample variance of observations.
    VariogramFitResult             variogram;          ///< Fitted variogram parameters.
    std::vector<VariogramPoint>    empiricalVariogram; ///< Empirical variogram bins.
    std::vector<KrigingPrediction> predictions;        ///< Estimates on obs grid + targets.
    CrossValidationResult          crossValidation;    ///< LOO cross-validation diagnostics.
    std::string                    componentName;      ///< Series component name (for output).
};

} // namespace loki::kriging
