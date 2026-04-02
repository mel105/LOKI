#pragma once

#include <Eigen/Dense>

#include <vector>

namespace loki::math {

/**
 * @brief Builds the AR(p) lag design matrix (Gamma) from a residual series.
 *
 * Each row i of the result corresponds to the state vector at time i+p:
 *   row i = [ y[i+p-1], y[i+p-2], ..., y[i] ]
 *
 * The matrix has dimensions (n - p) x p, where n = y.size().
 * The first p observations are dropped (standard practice -- no zero-padding).
 * This is the matrix Gamma from Hau & Tong (1989), used to compute the
 * hat matrix H = Gamma * (Gamma^T Gamma)^{-1} * Gamma^T for DEH-based
 * outlier detection.
 *
 * @param y Input residual series (NaN-free).
 * @param p AR lag order (number of columns). Must satisfy p >= 1 and p < n.
 * @return  (n - p) x p design matrix.
 * @throws SeriesTooShortException if y.size() <= static_cast<std::size_t>(p).
 * @throws AlgorithmException      if p < 1.
 */
Eigen::MatrixXd buildLagMatrix(const std::vector<double>& y, int p);

} // namespace loki::math
