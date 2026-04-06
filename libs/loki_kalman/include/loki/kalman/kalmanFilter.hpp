#pragma once

#include "loki/kalman/kalmanModel.hpp"

#include <Eigen/Dense>

#include <vector>

namespace loki::kalman {

/**
 * @brief Per-epoch data produced by the Kalman forward pass.
 *
 * Retained for use by the RTS smoother and the EM estimator.
 * All Eigen fields are sized according to the model state dimension n.
 */
struct FilterStep {
    Eigen::VectorXd xPred; ///< x[t|t-1] -- predicted state mean   [n].
    Eigen::MatrixXd PPred; ///< P[t|t-1] -- predicted state cov.   [n x n].
    Eigen::VectorXd xFilt; ///< x[t|t]   -- filtered state mean    [n].
    Eigen::MatrixXd PFilt; ///< P[t|t]   -- filtered state cov.    [n x n].

    double innovation;    ///< y[t] - H * x[t|t-1]. NaN when hasObservation == false.
    double innovationVar; ///< S[t] = H * P[t|t-1] * H' + R.  NaN when no observation.
    double kalmanGain0;   ///< K[t][0] -- first scalar of the Kalman gain vector.
                          ///< NaN when hasObservation == false.

    bool hasObservation; ///< False for NaN measurement epochs (predict-only step).
};

/**
 * @brief Linear Kalman filter -- forward (predict + update) pass.
 *
 * Implements the standard discrete-time Kalman filter equations:
 *
 *   Predict:
 *     x[t|t-1] = F * x[t-1|t-1]
 *     P[t|t-1] = F * P[t-1|t-1] * F' + Q
 *
 *   Update (skipped when measurement is NaN):
 *     S[t]     = H * P[t|t-1] * H' + R
 *     K[t]     = P[t|t-1] * H' / S[t]
 *     x[t|t]   = x[t|t-1] + K[t] * (y[t] - H * x[t|t-1])
 *     P[t|t]   = (I - K[t] * H) * P[t|t-1]
 *
 * When a measurement is NaN the update step is skipped entirely:
 * x[t|t] = x[t|t-1] and P[t|t] = P[t|t-1]. The innovation and
 * Kalman gain for that epoch are stored as NaN in the FilterStep.
 *
 * The Joseph form of the covariance update is used for numerical
 * stability: P[t|t] = (I - K*H) * P[t|t-1] * (I - K*H)' + K * R * K'
 */
class KalmanFilter {
public:

    /**
     * @brief Constructs the filter with the given model.
     * @param model Fully initialised KalmanModel (F, H, Q, R, x0, P0).
     */
    explicit KalmanFilter(KalmanModel model);

    /**
     * @brief Runs the full forward pass over a measurement sequence.
     *
     * @param measurements Scalar measurement values. NaN entries are treated
     *                     as missing observations (predict-only step).
     * @return Per-epoch FilterStep vector (same length as measurements).
     * @throws DataException if measurements is empty.
     */
    [[nodiscard]]
    std::vector<FilterStep> run(const std::vector<double>& measurements) const;

    /**
     * @brief Computes the log-likelihood of the measurement sequence.
     *
     * Uses the innovations and innovation variances from a completed forward pass:
     *   log L = -0.5 * sum_t [ log(2*pi*S[t]) + innovation[t]^2 / S[t] ]
     * Summation is over observed epochs only (hasObservation == true).
     *
     * @param steps Output of a prior call to run().
     * @return Log-likelihood scalar.
     */
    [[nodiscard]]
    double logLikelihood(const std::vector<FilterStep>& steps) const;

    /// @brief Read-only access to the model (needed by EmEstimator).
    [[nodiscard]]
    const KalmanModel& model() const;

    /**
     * @brief Returns a new KalmanFilter with updated Q and R scalars.
     *
     * Used by EmEstimator to create an updated filter without modifying
     * the original. Q and R are replaced with q * I and r * I respectively;
     * F, H, x0, P0 are preserved.
     *
     * @param q New process noise scalar.
     * @param r New measurement noise scalar.
     * @throws ConfigException if q <= 0 or r <= 0.
     */
    [[nodiscard]]
    KalmanFilter withNoise(double q, double r) const;

private:

    KalmanModel m_model;

    // ---- helpers ------------------------------------------------------------

    /**
     * @brief Joseph-form covariance update for numerical stability.
     *
     * P_new = (I - K*H) * P * (I - K*H)' + K * R * K'
     */
    [[nodiscard]]
    Eigen::MatrixXd josephUpdate(const Eigen::MatrixXd& P,
                                 const Eigen::MatrixXd& K) const;
};

} // namespace loki::kalman
