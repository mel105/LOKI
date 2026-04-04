#pragma once

#include <Eigen/Dense>

#include <vector>

namespace loki::stats {

/**
 * @brief Computes the w-correlation matrix for SSA component grouping.
 *
 * Given L elementary reconstructed series F_0, F_1, ..., F_{L-1}, each of
 * length n, the w-correlation matrix W is an L x L matrix with entries:
 *
 *   W[i][j] = |<F_i, F_j>_w| / (||F_i||_w * ||F_j||_w)
 *
 * where the weighted inner product is defined as:
 *
 *   <F_i, F_j>_w = sum_{k=0}^{n-1} w_k * F_i[k] * F_j[k]
 *
 * The Hankel weights w_k are determined by the overlap count of the
 * trajectory matrix anti-diagonals:
 *
 *   w_k = min(k+1, L, K, n-k)   where K = n - L + 1
 *
 * These weights give more importance to the central observations that
 * contribute to the most trajectory matrix rows.
 *
 * W[i][j] in [0, 1]. Values close to 1 indicate that components i and j
 * are separable (no mixing). Values close to 0 indicate strong mixing and
 * suggest grouping i and j together.
 *
 * @param components L elementary reconstructed series, each of length n.
 *                   Produced by diagonal averaging of rank-1 elementary matrices.
 * @param L          Window length used in SSA embedding. Must equal components.size().
 * @return           L x L w-correlation matrix.
 * @throws AlgorithmException if components is empty, L does not match
 *                            components.size(), or series lengths are inconsistent.
 */
Eigen::MatrixXd computeWCorrelation(
    const std::vector<std::vector<double>>& components,
    int L);

} // namespace loki::stats
