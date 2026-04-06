#pragma once

#include "loki/kalman/kalmanFilter.hpp"
#include "loki/kalman/kalmanModel.hpp"

#include <Eigen/Dense>

#include <vector>

namespace loki::kalman {

/**
 * @brief Per-epoch output of the RTS backward smoother.
 *
 * Produced by RtsSmoother::smooth() and used both for constructing
 * KalmanResult and for the EM M-step in EmEstimator.
 */
struct SmootherStep {
    Eigen::VectorXd xSmooth; ///< x[t|T] -- smoothed state mean [n].
    Eigen::MatrixXd PSmooth; ///< P[t|T] -- smoothed state cov  [n x n].
    Eigen::MatrixXd G;       ///< Smoother gain G[t] = P[t|t] * F' * inv(P[t+1|t]) [n x n].
};

/**
 * @brief Rauch-Tung-Striebel (RTS) backward smoother.
 *
 * Given the complete output of the Kalman forward pass, the RTS smoother
 * computes the full-information smoothed state estimates x[t|T] and
 * corresponding covariances P[t|T] using the backward recursion:
 *
 *   G[t]       = P[t|t] * F' * inv(P[t+1|t])
 *   x[t|T]     = x[t|t] + G[t] * (x[t+1|T] - F * x[t|t])
 *   P[t|T]     = P[t|t] + G[t] * (P[t+1|T] - P[t+1|t]) * G[t]'
 *
 * Initialisation: x[T|T] = x[T|T] (last filter step), P[T|T] = P[T|T].
 *
 * The smoother does not modify the forward pass -- it is a separate backward
 * sweep that runs after the filter is complete.
 *
 * Missing epochs (hasObservation == false) are handled correctly: the filter
 * has already set x[t|t] = x[t|t-1] and P[t|t] = P[t|t-1] for those epochs,
 * so the smoother recursion treats them uniformly.
 */
class RtsSmoother {
public:

    /**
     * @brief Constructs the smoother with the given model.
     * @param model Fully initialised KalmanModel (F is required; Q, H, R are not used).
     */
    explicit RtsSmoother(KalmanModel model);

    /**
     * @brief Runs the RTS backward pass.
     *
     * @param filterSteps Output of KalmanFilter::run() (forward pass).
     * @return Per-epoch SmootherStep vector, same length and order as filterSteps.
     *         The last element has G = 0 (initialisation terminal condition).
     * @throws DataException if filterSteps is empty.
     */
    [[nodiscard]]
    std::vector<SmootherStep> smooth(const std::vector<FilterStep>& filterSteps) const;

private:

    KalmanModel m_model;
};

} // namespace loki::kalman
