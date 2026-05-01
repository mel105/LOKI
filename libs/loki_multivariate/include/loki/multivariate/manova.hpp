#pragma once

#include "loki/multivariate/multivariateResult.hpp"
#include "loki/multivariate/multivariateSeries.hpp"
#include "loki/core/config.hpp"

#include <vector>

namespace loki::multivariate {

/**
 * @brief One-way Multivariate Analysis of Variance (MANOVA).
 *
 * Tests H0: all group mean vectors are equal, against H1: at least one differs.
 *
 * Four test statistics are computed from the eigenvalues lambda_i of
 * E^{-1} * H (where H = between-group SSCP, E = within-group SSCP):
 *
 *   Wilks lambda:     Lambda = prod(1 / (1 + lambda_i))  = det(E) / det(H+E)
 *   Pillai trace:     V = sum(lambda_i / (1 + lambda_i))
 *   Hotelling trace:  T = sum(lambda_i)
 *   Roy maximum root: Theta = max(lambda_i) / (1 + max(lambda_i))
 *
 * Wilks Lambda is converted to an approximate F statistic via the
 * Rao (1951) formula, which is exact for p <= 2 or nClasses <= 3.
 *
 * Pillai trace F approximation (Pillai & Jayachandran 1967):
 *   F = (V/s) / ((m - V/s)) * (df2 / df1)
 * where s = min(p, g-1), m = (|p - g + 1| - 1) / 2, n_bar = (N - g - p - 1)/2.
 *
 * Uses Eigen::GeneralizedSelfAdjointEigenSolver for E^{-1} H.
 */
class Manova {
public:

    explicit Manova(const MultivariateManovaConfig& cfg);

    /**
     * @brief Runs one-way MANOVA.
     *
     * @param data   Multivariate series (n x p).
     * @param labels Group labels (length n, 0-based integers).
     * @return       Populated ManovaResult.
     * @throws DataException if fewer than 2 groups or n < p + nGroups.
     */
    [[nodiscard]] ManovaResult compute(const MultivariateSeries& data,
                                       const std::vector<int>&   labels) const;

private:

    MultivariateManovaConfig m_cfg;

    /**
     * @brief Approximate F p-value using the F distribution.
     * Reuses the same Lentz continued fraction from var.cpp logic --
     * implemented here independently to keep classes self-contained.
     */
    static double _fPvalue(double fStat, double df1, double df2);
};

} // namespace loki::multivariate
