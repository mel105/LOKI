#pragma once

#include "loki/kalman/kalmanFilter.hpp"
#include "loki/kalman/kalmanModel.hpp"
#include "loki/kalman/rtsSmoother.hpp"

#include <utility>
#include <vector>

namespace loki::kalman {

/**
 * @brief Summary of EM estimation results.
 */
struct EmResult {
    double estimatedQ;         ///< Final process noise scalar.
    double estimatedR;         ///< Final measurement noise scalar.
    double finalLogLikelihood; ///< Log-likelihood after last iteration.
    int    iterations;         ///< Number of EM iterations performed.
    bool   converged;          ///< True if relative log-likelihood change < tol.
};

/**
 * @brief Expectation-Maximisation estimator for Kalman noise covariances.
 *
 * Iterates the Kalman forward pass (E-step) and RTS backward smoother
 * (also part of the E-step) to compute the expected sufficient statistics,
 * then updates Q and R via the M-step closed-form expressions.
 *
 * ### Scalar noise parameterisation
 * Q and R are both constrained to scaled identity matrices:
 *   Q = q * I_n,   R = r (scalar, 1x1)
 * This keeps the M-step well-posed and avoids degenerate solutions for
 * the 2-state models (local_trend, constant_velocity) without additional
 * regularisation.
 *
 * ### M-step update equations
 * Let T = number of observed epochs, n = state dimension.
 *
 *   E_xx[t]  = P[t|T] + x[t|T] * x[t|T]'
 *   E_xx1[t] = P[t,t-1|T] + x[t|T] * x[t-1|T]'  (cross-covariance)
 *
 * where P[t,t-1|T] = G[t-1] * P[t|T]  (lag-1 smoothed cross-covariance)
 *
 *   q_new = (1 / (n * T)) * trace(
 *             sum_t [ E_xx[t] - F * E_xx1[t]' - E_xx1[t] * F' + F * E_xx[t-1] * F' ]
 *           )
 *
 *   r_new = (1 / T_obs) * sum_{observed t} [
 *             (y[t] - H * x[t|T])^2 + H * P[t|T] * H'
 *           ]
 *
 * ### Convergence criterion
 * Iteration stops when:
 *   |logL[k] - logL[k-1]| / (1 + |logL[k-1]|) < tol
 *
 * or when maxIter iterations are reached.
 *
 * ### Safety bounds
 * q and r are clamped to [1e-12, 1e12] after each M-step to prevent
 * numerical collapse.
 */
class EmEstimator {
public:

    /**
     * @brief Constructs the estimator.
     *
     * @param model   Initial model (F, H, x0, P0 are used as-is; Q and R are
     *                the starting guess for EM).
     * @param maxIter Maximum number of EM iterations.
     * @param tol     Convergence tolerance on relative log-likelihood change.
     */
    explicit EmEstimator(KalmanModel model, int maxIter = 100, double tol = 1.0e-6);

    /**
     * @brief Runs the EM algorithm and returns the updated model and summary.
     *
     * The returned KalmanModel has Q and R updated to the EM estimates;
     * all other matrices (F, H, x0, P0) are unchanged from the input model.
     *
     * @param measurements Scalar measurements. NaN = missing epoch (predict-only).
     * @return Pair of (updated KalmanModel, EmResult summary).
     * @throws DataException if measurements has fewer than 4 finite values.
     */
    [[nodiscard]]
    std::pair<KalmanModel, EmResult> estimate(
        const std::vector<double>& measurements) const;

private:

    KalmanModel m_model;
    int         m_maxIter;
    double      m_tol;

    // ---- M-step helpers -----------------------------------------------------

    /**
     * @brief Computes the lag-1 smoothed cross-covariance P[t, t-1|T].
     *
     * P[t, t-1|T] = G[t-1] * P[t|T]
     *
     * @param G_prev  Smoother gain from step t-1.
     * @param PSmooth Smoothed covariance from step t.
     */
    [[nodiscard]]
    static Eigen::MatrixXd lag1CrossCov(const Eigen::MatrixXd& G_prev,
                                        const Eigen::MatrixXd& PSmooth);

    /**
     * @brief Clamps a scalar noise value to [lo, hi].
     */
    [[nodiscard]]
    static double clampNoise(double v, double lo = 1.0e-12, double hi = 1.0e12);
};

} // namespace loki::kalman
