#pragma once

#include <string>
#include <vector>

namespace loki::kalman {

/**
 * @brief Holds all outputs produced by the Kalman filter pipeline.
 *
 * Populated by KalmanAnalyzer::run(). Fields that are not computed
 * (e.g. smoothed state when the RTS smoother is disabled, or forecast
 * when steps == 0) are left as empty vectors.
 *
 * All time vectors use MJD. State vectors contain only the first
 * component of the state vector (position / level / velocity),
 * which is the physically observable quantity in all three built-in models.
 */
struct KalmanResult {

    // ---- Input / time axis --------------------------------------------------

    std::vector<double> times;      ///< MJD timestamps of all epochs.
    std::vector<double> original;   ///< Raw measurements (NaN where missing).

    // ---- Filter output (forward pass) ---------------------------------------

    std::vector<double> filteredState; ///< x_hat[t|t]  -- first state component.
    std::vector<double> filteredStd;   ///< sqrt(P[t|t][0,0]) -- filter uncertainty.

    // ---- Smoother output (RTS backward pass) --------------------------------

    std::vector<double> smoothedState; ///< x_hat[t|T]  -- empty if smoother disabled.
    std::vector<double> smoothedStd;   ///< sqrt(P[t|T][0,0]) -- empty if disabled.

    // ---- Predictor (one-step-ahead) -----------------------------------------

    std::vector<double> predictedState; ///< x_hat[t|t-1] -- one-step-ahead prediction.

    // ---- Innovations (residuals of predictor) --------------------------------

    std::vector<double> innovations;    ///< y[t] - H * x_hat[t|t-1]. NaN for missing epochs.
    std::vector<double> innovationStd;  ///< sqrt(S[t]) -- innovation std dev. NaN for missing.

    // ---- Kalman gain --------------------------------------------------------

    std::vector<double> gains; ///< K[t][0] -- first-row gain scalar. NaN for missing epochs.

    // ---- Forecast (beyond last observation) ---------------------------------

    std::vector<double> forecastTimes; ///< MJD timestamps of forecast epochs.
    std::vector<double> forecastState; ///< Predicted state, growing uncertainty.
    std::vector<double> forecastStd;   ///< Prediction std dev (growing with each step).

    // ---- Summary scalars ----------------------------------------------------

    double logLikelihood {0.0}; ///< Sum of log N(innovation; 0, S) over observed epochs.
    double estimatedQ    {0.0}; ///< Process noise variance (from EM or manual).
    double estimatedR    {0.0}; ///< Measurement noise variance (from EM or manual).
    int    emIterations  {0};   ///< EM iterations performed (0 if not using EM).
    bool   emConverged   {false}; ///< True if EM converged within tolerance.

    // ---- Metadata -----------------------------------------------------------

    std::string modelName;   ///< "local_level" | "local_trend" | "constant_velocity"
    std::string noiseMethod; ///< "manual" | "heuristic" | "em"
    std::string smoother;    ///< "none" | "rts"
};

} // namespace loki::kalman
