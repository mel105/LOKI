#pragma once

#include "loki/multivariate/multivariateResult.hpp"
#include "loki/multivariate/multivariateSeries.hpp"
#include "loki/core/config.hpp"

namespace loki::multivariate {

/**
 * @brief Factor Analysis with optional Varimax rotation.
 *
 * Extracts nFactors latent factors from the (n x p) data matrix using the
 * principal axis factoring method (PAF):
 *   1. Start from the correlation matrix R (standardised data).
 *   2. Replace diagonal with communality estimates (initial: squared multiple
 *      correlations, i.e. R^2 from regressing each variable on all others).
 *   3. Iteratively extract factors via eigen-decomposition of the reduced
 *      correlation matrix until communalities converge.
 *   4. Optionally apply Varimax rotation to the loading matrix.
 *
 * Varimax rotation maximises sum of variances of squared loadings per factor,
 * producing a simpler, more interpretable structure (Kaiser 1958).
 *
 * Reuses Eigen::SelfAdjointEigenSolver (symmetric matrix -- no BDCSVD needed).
 */
class FactorAnalysis {
public:

    explicit FactorAnalysis(const MultivariateFactorConfig& cfg);

    /**
     * @brief Runs factor analysis on the multivariate series.
     * @param data Synchronised series (n x p). Should be standardised.
     * @return     Populated FactorAnalysisResult.
     * @throws DataException      if p < nFactors or n < p.
     * @throws AlgorithmException if communalities do not converge.
     */
    [[nodiscard]] FactorAnalysisResult compute(const MultivariateSeries& data) const;

private:

    MultivariateFactorConfig m_cfg;

    /**
     * @brief Applies Varimax rotation to the loading matrix.
     *
     * Implements the Kaiser (1958) algorithm:
     *   Iteratively rotate pairs of factors to maximise sum of variances
     *   of squared loadings (normalised by communalities).
     *
     * @param L  Loading matrix (p x k). Modified in place.
     */
    static void _varimax(Eigen::MatrixXd& L, int maxIter, double tol);

    /**
     * @brief Computes squared multiple correlations as initial communalities.
     *
     * h2_j = R^2 of regressing variable j on all other variables.
     * = 1 - 1/R_jj^{-1}  where R^{-1} is the inverse of the correlation matrix.
     *
     * @param R  Correlation matrix (p x p).
     * @return   Vector of initial communality estimates (p).
     */
    static Eigen::VectorXd _initialCommunalities(const Eigen::MatrixXd& R);
};

} // namespace loki::multivariate
