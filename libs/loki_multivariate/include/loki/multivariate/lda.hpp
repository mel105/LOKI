#pragma once

#include "loki/multivariate/multivariateResult.hpp"
#include "loki/multivariate/multivariateSeries.hpp"
#include "loki/core/config.hpp"

#include <string>
#include <vector>

namespace loki::multivariate {

/**
 * @brief Linear / Quadratic Discriminant Analysis.
 *
 * LDA finds linear combinations of channels that best separate predefined
 * groups. QDA allows per-class covariance matrices (more flexible, less stable).
 *
 * LDA algorithm:
 *   1. Compute within-class scatter Sw and between-class scatter Sb.
 *   2. Solve the generalised eigenvalue problem: Sw^{-1} * Sb * w = lambda * w.
 *   3. Project data onto the top (nClasses-1) discriminant axes.
 *   4. Assign each observation to the nearest class centroid in projected space.
 *
 * Group labels are supplied as a separate integer vector (0-based class indices)
 * of length n. The analyzer extracts them from the channel named groupsColumn
 * (interpreted as integer values) or from the last column if groupsColumn is
 * a 1-based numeric string.
 *
 * Uses Eigen::GeneralizedSelfAdjointEigenSolver for numerical stability.
 */
class Lda {
public:

    explicit Lda(const MultivariateLdaConfig& cfg);

    /**
     * @brief Runs LDA/QDA on the multivariate series.
     *
     * @param data   Multivariate series (n x p).
     * @param labels Integer group labels (length n, 0-based).
     * @return       Populated LdaResult.
     * @throws DataException      if fewer than 2 classes or n != labels.size().
     * @throws AlgorithmException if Sw is singular.
     */
    [[nodiscard]] LdaResult compute(const MultivariateSeries& data,
                                    const std::vector<int>&   labels) const;

private:

    MultivariateLdaConfig m_cfg;

    /**
     * @brief Predicts class for each observation using nearest centroid in
     *        discriminant space (LDA) or Mahalanobis distance (QDA).
     */
    [[nodiscard]]
    std::vector<int> _predict(const Eigen::MatrixXd&              scores,
                               const std::vector<Eigen::VectorXd>& centroids) const;
};

} // namespace loki::multivariate
