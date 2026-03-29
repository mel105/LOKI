#pragma once

#include <loki/timeseries/timeSeries.hpp>

#include <Eigen/Dense>

#include <string>
#include <vector>

namespace loki::regression {

/**
 * @brief Prediction at a single x location with confidence and prediction intervals.
 *
 * x values use the same reference as the fit: x = mjd - tRef.
 * tRef is available in the RegressionResult returned by fit().
 */
struct PredictionPoint {
    double x;           ///< x = mjd - tRef (days).
    double predicted;   ///< Point estimate: A_new * x-hat.
    double confLow;     ///< Lower confidence interval bound.
    double confHigh;    ///< Upper confidence interval bound.
    double predLow;     ///< Lower prediction interval bound.
    double predHigh;    ///< Upper prediction interval bound.
};

/**
 * @brief Result of a regression fit.
 *
 * Populated by any Regressor::fit() call. All fields are always filled.
 * The x-axis convention is: x = mjd - tRef, where tRef = mjd of the first
 * valid observation. This ensures numerical stability and ms-resolution
 * compatibility across all data types.
 *
 * Prediction intervals require tRef and cofactorX -- both are preserved here
 * so that predict() can be called after fit() without re-fitting.
 */
struct RegressionResult {
    Eigen::VectorXd coefficients;     ///< Estimated parameters x-hat (n x 1).
    Eigen::VectorXd residuals;        ///< Corrections v = A*x-hat - l (m x 1).
    Eigen::MatrixXd cofactorX;        ///< Cofactor matrix (A^T P A)^-1 (n x n).
    double          sigma0{0.0};      ///< A-posteriori unit weight std deviation.
    double          rSquared{0.0};    ///< Coefficient of determination R^2.
    double          rSquaredAdj{0.0}; ///< Adjusted R^2.
    double          aic{0.0};         ///< Akaike Information Criterion.
    double          bic{0.0};         ///< Bayesian Information Criterion.
    int             dof{0};           ///< Degrees of freedom: nObs - nParams.
    bool            converged{true};  ///< False if IRLS did not converge.
    std::string     modelName;        ///< Human-readable model name for protocol.
    TimeSeries      fitted;           ///< Fitted values at observation times.
    double          tRef{0.0};        ///< MJD of first observation -- x-axis reference.
    Eigen::MatrixXd designMatrix;     ///< Design matrix X used in fit() -- required by RegressionDiagnostics.
};

} // namespace loki::regression
