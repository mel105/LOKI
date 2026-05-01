#pragma once

#include "loki/multivariate/multivariateResult.hpp"
#include "loki/multivariate/multivariateSeries.hpp"
#include "loki/core/config.hpp"

namespace loki::multivariate {

/**
 * @brief Multivariate outlier detection via Mahalanobis distance.
 *
 * Computes the squared Mahalanobis distance for each observation:
 *
 *   D^2_i = (x_i - mu)^T * S^{-1} * (x_i - mu)
 *
 * where mu is the sample mean and S is the sample covariance matrix.
 *
 * Under multivariate normality, D^2_i ~ Chi^2(p) asymptotically.
 * An observation is flagged as an outlier if D^2_i > chi2_crit(p, alpha),
 * where chi2_crit is the critical value at significance level alpha.
 *
 * Robust option (recommended): uses the Minimum Covariance Determinant (MCD)
 * estimator for mu and S to avoid masking effects where outliers inflate S.
 * MCD is approximated here via a C-step algorithm (subset of size h = 0.75*n).
 *
 * For large p (p > n/2), the sample covariance S may be singular. In that
 * case, the Moore-Penrose pseudoinverse is used automatically (logged as
 * a warning).
 */
class Mahalanobis {
public:

    explicit Mahalanobis(const MultivariateMahalanobisConfig& cfg);

    /**
     * @brief Computes Mahalanobis distances and flags outliers.
     * @param data Multivariate series (n x p).
     * @return     Populated MahalanobisResult.
     * @throws DataException if n < p + 1.
     */
    [[nodiscard]] MahalanobisResult compute(const MultivariateSeries& data) const;

private:

    MultivariateMahalanobisConfig m_cfg;

    /**
     * @brief Critical value of the Chi^2(p) distribution at level alpha.
     *
     * Uses the Wilson-Hilferty approximation:
     *   chi2_crit(p, alpha) approx p * (1 - 2/(9p) + z_alpha * sqrt(2/(9p)))^3
     * where z_alpha is the standard normal quantile.
     */
    static double _chi2Critical(int p, double alpha);

    /**
     * @brief Computes pseudoinverse of a symmetric matrix via eigendecomposition.
     */
    static Eigen::MatrixXd _pseudoinverse(const Eigen::MatrixXd& S, double tol = 1.0e-10);
};

} // namespace loki::multivariate
