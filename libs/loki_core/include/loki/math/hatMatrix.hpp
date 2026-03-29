#pragma once

#include <Eigen/Dense>

#include <optional>

namespace loki {

/**
 * @brief Computes and stores the hat (projection) matrix H = Q * Q^T,
 *        where X = Q * R is the thin QR decomposition of the design matrix X.
 *
 * The hat matrix projects the observation vector onto the column space of X:
 *   y-hat = H * y,  H = X * (X^T X)^{-1} * X^T
 *
 * Internally uses the thin QR decomposition for numerical stability.
 * Leverage values h_ii (diagonal of H) are computed eagerly in the constructor.
 * The full matrix H is computed lazily on first call to matrix().
 *
 * Used by:
 *   - RegressionDiagnostics (Cook's distance, leverage)
 *   - HatMatrixDetector in loki_outlier (Mahalanobis-based outlier detection)
 */
class HatMatrix {
public:
    /// Minimum number of rows required in X.
    static constexpr int MIN_ROWS = 2;

    /**
     * @brief Constructs the HatMatrix from a design matrix X (n x p).
     * @param X Design matrix with n observations and p parameters.
     * @throws AlgorithmException if X has fewer than MIN_ROWS rows,
     *         or if X has more columns than rows (underdetermined system).
     */
    explicit HatMatrix(const Eigen::MatrixXd& X);

    /**
     * @brief Returns the leverage values h_ii (diagonal of H), size n.
     *
     * h_ii = ||Q_i||^2, where Q_i is the i-th row of the thin Q factor.
     * Computed eagerly in the constructor -- O(n * p^2) via QR.
     */
    const Eigen::VectorXd& leverages() const;

    /**
     * @brief Returns the full n x n hat matrix H = Q * Q^T.
     *
     * Computed lazily on first call. For large n this is an n x n dense matrix --
     * use leverages() when only h_ii values are needed.
     */
    const Eigen::MatrixXd& matrix() const;

    /// Number of parameters p (columns of X).
    int rankP() const;

    /// Number of observations n (rows of X).
    int n() const;

private:
    Eigen::MatrixXd          m_Q;          ///< Thin Q factor (n x p) from QR.
    int                      m_n;          ///< Number of observations.
    int                      m_p;          ///< Number of parameters.
    Eigen::VectorXd          m_leverages;  ///< h_ii values, size n.
    mutable std::optional<Eigen::MatrixXd> m_H; ///< Full H, computed lazily.
};

} // namespace loki
