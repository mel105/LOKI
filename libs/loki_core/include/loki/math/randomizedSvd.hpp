#pragma once

#include <Eigen/Dense>

namespace loki::math {

/**
 * @brief Result of a randomized truncated SVD.
 *
 * Contains the leading k components of the decomposition X = U * diag(sv) * V^T.
 * All three matrices are thin: U is m x k, sv has length k, V is n x k.
 */
struct RandomizedSvdResult {
    Eigen::MatrixXd U;   ///< Left singular vectors  (m x k).
    Eigen::VectorXd sv;  ///< Singular values in descending order (length k).
    Eigen::MatrixXd V;   ///< Right singular vectors (n x k).
};

/**
 * @brief Randomized truncated SVD via the algorithm of Halko, Martinsson & Tropp (2011).
 *
 * Computes an approximate rank-k decomposition X ~ U * diag(sv) * V^T using
 * a randomized range finder followed by a small deterministic SVD.
 *
 * Algorithm (Algorithm 4.4 + power iteration from Halko et al. 2011):
 *   1. Draw a random Gaussian test matrix Omega (n x (k + p)).
 *   2. Form Y = X * Omega  (m x (k+p)) -- random projection of column space.
 *   3. Apply nPowerIter steps of subspace iteration to sharpen the range:
 *        Y = X * (X^T * Y)  (repeated)
 *      This improves accuracy for matrices with slowly decaying singular values.
 *   4. Orthonormalise Y via thin QR: Y = Q * R  ->  Q is m x (k+p).
 *   5. Project X into the small subspace: B = Q^T * X  ((k+p) x n).
 *   6. Compute thin SVD of the small matrix B = U_b * diag(sv) * V^T.
 *   7. Recover U = Q * U_b  and truncate to rank k.
 *
 * Complexity: O(m * n * (k + p)) where p = nOversampling (typically 5-10).
 * For SSA with m=K, n=L, k << L: this is O(K * L * k) versus O(K * L^2)
 * for the full eigendecomposition -- a factor of L/k speedup.
 *
 * Accuracy: singular values are accurate to O(sigma_{k+1}) where sigma_{k+1}
 * is the (k+1)-th singular value of X. Power iteration reduces this error
 * further for matrices with slow spectral decay.
 *
 * @param X           Input matrix (m x n). Not modified.
 * @param k           Number of singular components to compute. k >= 1.
 * @param nOversampling Extra columns in the random projection for accuracy.
 *                    Default: 10. Total working rank = k + nOversampling.
 * @param nPowerIter  Number of subspace power iterations.
 *                    Default: 2. More iterations improve accuracy at O(k) cost.
 * @param seed        Random seed for reproducibility. Default: 42.
 * @return            RandomizedSvdResult with U (m x k), sv (k), V (n x k).
 * @throws AlgorithmException if X is empty or k < 1 or k >= min(m, n).
 */
RandomizedSvdResult randomizedSvd(const Eigen::MatrixXd& X,
                                   int                    k,
                                   int                    nOversampling = 10,
                                   int                    nPowerIter    = 2,
                                   unsigned int           seed          = 42);

} // namespace loki::math
