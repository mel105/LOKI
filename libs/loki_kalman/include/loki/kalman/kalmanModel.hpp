#pragma once

#include <Eigen/Dense>

#include <string>

namespace loki::kalman {

/**
 * @brief State space model matrices for the Kalman filter.
 *
 * Represents a linear Gaussian state space model:
 *
 *   x[t] = F * x[t-1] + w[t],   w[t] ~ N(0, Q)
 *   y[t] = H * x[t]   + v[t],   v[t] ~ N(0, R)
 *
 * All matrices are stored as Eigen dense types for direct use in
 * KalmanFilter, RtsSmoother, and EmEstimator.
 *
 * Dimensions:
 *   n = state dimension (1 for local_level; 2 for local_trend / constant_velocity)
 *   m = observation dimension (always 1 in LOKI -- scalar measurements)
 *
 *   F  : [n x n]  state transition matrix
 *   H  : [1 x n]  observation matrix
 *   Q  : [n x n]  process noise covariance (positive semi-definite)
 *   R  : [1 x 1]  measurement noise covariance (positive definite)
 *   x0 : [n]      initial state mean
 *   P0 : [n x n]  initial state covariance (positive definite)
 */
struct KalmanModel {
    Eigen::MatrixXd F;   ///< State transition matrix  [n x n].
    Eigen::MatrixXd H;   ///< Observation matrix        [1 x n].
    Eigen::MatrixXd Q;   ///< Process noise covariance  [n x n].
    Eigen::MatrixXd R;   ///< Measurement noise cov.    [1 x 1].
    Eigen::VectorXd x0;  ///< Initial state mean        [n].
    Eigen::MatrixXd P0;  ///< Initial state covariance  [n x n].

    std::string name; ///< "local_level" | "local_trend" | "constant_velocity"
};

} // namespace loki::kalman
