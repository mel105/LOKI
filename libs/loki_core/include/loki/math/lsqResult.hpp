#pragma once

#include <Eigen/Dense>

namespace loki {

/**
 * @brief Result of a least-squares adjustment.
 *
 * Populated by LsqSolver::solve(). All fields are always filled.
 * Check `converged` when using robust (IRLS) mode.
 *
 * Consistency check: for a correct adjustment, the weighted sum of
 * corrections v^T * P * v equals sigma0^2 * dof (by definition of sigma0).
 * Additionally, for an unweighted model without a constant term, sum(v) ~ 0.
 */
struct LsqResult {
    Eigen::VectorXd coefficients; ///< Estimated parameters x-hat (n x 1).
    Eigen::VectorXd residuals;    ///< Corrections v = A * x-hat - l (m x 1).
    double sigma0{0.0};           ///< A-posteriori unit weight std deviation: sqrt(v^T P v / dof).
    double vTPv{0.0};             ///< Weighted sum of squared residuals v^T * P * v.
    Eigen::MatrixXd cofactorX;   ///< Cofactor matrix of unknowns: (A^T P A)^{-1} (n x n).
    int nObs{0};                  ///< Number of observations (rows of A).
    int nParams{0};               ///< Number of unknowns (columns of A).
    int dof{0};                   ///< Degrees of freedom: nObs - nParams.
    bool converged{true};         ///< False if IRLS reached maxIterations without convergence.
};

} // namespace loki
