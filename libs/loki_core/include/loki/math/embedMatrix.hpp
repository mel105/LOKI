#pragma once

#include <Eigen/Dense>

#include <vector>

namespace loki::math {

/**
 * @brief Builds the Hankel trajectory matrix for Singular Spectrum Analysis.
 *
 * Given a time series y[0..n-1] and window length L, constructs the
 * (K x L) trajectory matrix X where K = n - L + 1 and:
 *
 *   X[i, j] = y[i + j]   for i = 0..K-1,  j = 0..L-1
 *
 * Each row is a lagged window of length L. Each anti-diagonal of X
 * contains the same value of y (Hankel structure).
 *
 * This is the standard embedding step of single-channel SSA.
 * The SVD of X then provides the elementary matrices used for
 * reconstruction and grouping.
 *
 * Dimension conventions:
 *   - L  : window length (number of columns). Choose L <= n/2.
 *   - K  : number of rows = n - L + 1.
 *   - n  : length of the input series.
 *
 * @param y Input time series (NaN-free). Length n.
 * @param L Window length. Must satisfy 2 <= L <= n - 1.
 * @return  K x L trajectory matrix, K = n - L + 1.
 * @throws AlgorithmException      if L < 2 or L >= n.
 * @throws SeriesTooShortException if y.size() < 3.
 */
Eigen::MatrixXd buildEmbedMatrix(const std::vector<double>& y, int L);

} // namespace loki::math
