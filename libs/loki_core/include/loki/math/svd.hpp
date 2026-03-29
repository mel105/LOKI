#pragma once

#include <Eigen/SVD>

namespace loki {

/**
 * @brief Wrapper around Eigen's BDCSVD providing higher-level SVD operations.
 *
 * Computes the singular value decomposition M = U * S * V^T, where:
 *   U  -- left singular vectors  (m x p, thin) or (m x m, full)
 *   S  -- singular values in descending order (p x 1)
 *   V  -- right singular vectors (n x p, thin) or (n x n, full)
 *   p  = min(m, n)
 *
 * Uses Eigen::BDCSVD (divide-and-conquer), which is recommended for matrices
 * larger than ~16x16. Thin U and V are computed by default.
 *
 * Intended uses:
 *   - CalibrationRegressor (TLS via smallest singular vector)
 *   - loki_svd module (SSA, PCA decomposition of time series)
 *   - Pseudoinverse for ill-conditioned systems
 */
class SvdDecomposition {
public:
    /**
     * @brief Constructs the SVD of matrix M.
     * @param M         Input matrix (m x n).
     * @param computeFullU If true, compute full m x m U matrix. Default: thin (m x p).
     * @param computeFullV If true, compute full n x n V matrix. Default: thin (n x p).
     * @throws AlgorithmException if M is empty or SVD fails to converge.
     */
    explicit SvdDecomposition(const Eigen::MatrixXd& M,
                               bool computeFullU = false,
                               bool computeFullV = false);

    /**
     * @brief Returns the left singular vectors U (m x p thin, or m x m full).
     */
    const Eigen::MatrixXd& U() const;

    /**
     * @brief Returns the singular values in descending order (p x 1).
     */
    const Eigen::VectorXd& singularValues() const;

    /**
     * @brief Returns the right singular vectors V (n x p thin, or n x n full).
     *
     * Note: Eigen stores V, not V^T. Use V().transpose() to get V^T.
     */
    const Eigen::MatrixXd& V() const;

    /// Number of rows m of the original matrix.
    int rows() const;

    /// Number of columns n of the original matrix.
    int cols() const;

    /**
     * @brief Estimates the numerical rank of M.
     * @param tol Threshold for singular values. If tol < 0, uses the standard
     *            estimate: eps * max(m, n) * s_max.
     * @return Number of singular values exceeding tol.
     */
    int rank(double tol = -1.0) const;

    /**
     * @brief Returns the condition number s_max / s_min.
     *
     * Large condition number indicates a numerically ill-conditioned matrix.
     * Returns infinity if s_min is zero (rank-deficient matrix).
     */
    double condition() const;

    /**
     * @brief Reconstructs M using only the top-k singular components.
     *
     * Returns U_k * diag(S_k) * V_k^T, the rank-k approximation of M.
     * Used for SSA and PCA truncation.
     *
     * @param k Number of components to retain (1 <= k <= p).
     * @throws AlgorithmException if k is out of range.
     */
    Eigen::MatrixXd truncate(int k) const;

    /**
     * @brief Returns the cumulative explained variance ratio per component.
     *
     * Ratio_i = s_i^2 / sum(s_j^2). Length p.
     * Useful for scree plots and choosing truncation rank.
     */
    Eigen::VectorXd explainedVarianceRatio() const;

    /**
     * @brief Computes the Moore-Penrose pseudoinverse of M.
     *
     * Pseudoinverse = V * diag(1/s_i) * U^T, where only singular values
     * exceeding tol are inverted (others contribute zero).
     *
     * @param tol Threshold for inversion. If tol < 0, uses same auto-estimate as rank().
     * @return Pseudoinverse matrix (n x m).
     */
    Eigen::MatrixXd pseudoinverse(double tol = -1.0) const;

private:
    Eigen::BDCSVD<Eigen::MatrixXd> m_svd;
    int                             m_rows;
    int                             m_cols;

    /// Computes the auto tolerance: eps * max(m,n) * s_max.
    double autoTol() const;
};

} // namespace loki
