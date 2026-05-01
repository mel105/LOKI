#pragma once

#include "loki/multivariate/multivariateResult.hpp"
#include "loki/multivariate/multivariateSeries.hpp"
#include "loki/core/config.hpp"

namespace loki::multivariate {

/**
 * @brief Canonical Correlation Analysis (CCA).
 *
 * Finds linear combinations of two variable sets X (n x p) and Y (n x q)
 * that are maximally correlated. The k-th canonical pair (u_k, v_k) satisfies:
 *
 *   u_k = X * a_k,  v_k = Y * b_k
 *   corr(u_k, v_k) = rho_k  (k-th canonical correlation)
 *
 * with rho_1 >= rho_2 >= ... >= rho_min(p,q).
 *
 * Algorithm (via SVD of the cross-covariance):
 *   1. Standardise X and Y.
 *   2. Compute within-set covariance matrices Sxx, Syy and cross-covariance Sxy.
 *   3. Compute K = Sxx^{-1/2} * Sxy * Syy^{-1/2} via Cholesky square roots.
 *   4. SVD of K: U * D * V^T. Canonical correlations = diagonal of D.
 *   5. Canonical weights: a = Sxx^{-1/2} * U, b = Syy^{-1/2} * V.
 *   6. Canonical scores: Xs * a, Ys * b.
 *
 * Uses JacobiSVD (safe in .cpp static libs on Windows/GCC 13).
 */
class Cca {
public:

    explicit Cca(const MultivariateCcaConfig& cfg);

    /**
     * @brief Computes CCA between the two channel groups.
     * @param data Full multivariate series. Groups selected via cfg.groupX/groupY.
     * @return     Populated CcaResult.
     * @throws DataException      if groups are empty or overlap.
     * @throws AlgorithmException if covariance matrices are singular.
     */
    [[nodiscard]] CcaResult compute(const MultivariateSeries& data) const;

private:

    MultivariateCcaConfig m_cfg;

    /**
     * @brief Computes symmetric matrix square root via eigen-decomposition.
     * @param S Symmetric positive definite matrix.
     * @return  S^{1/2} such that S^{1/2} * S^{1/2} = S.
     */
    static Eigen::MatrixXd _sqrtMat(const Eigen::MatrixXd& S);

    /**
     * @brief Computes symmetric matrix inverse square root.
     * @param S Symmetric positive definite matrix.
     * @return  S^{-1/2}.
     */
    static Eigen::MatrixXd _invSqrtMat(const Eigen::MatrixXd& S);
};

} // namespace loki::multivariate
