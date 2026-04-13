#pragma once

#include <loki/math/krigingBase.hpp>

#include <vector>

namespace loki::math {

/**
 * @brief Universal Kriging (polynomial drift in time).
 *
 * Mean model: mu(t) = beta_0 + beta_1*t + ... + beta_p*t^p
 * where t = mjd - tRef (shifted for numerical stability).
 *
 * Extended system (n + p + 1):
 *   [ K   F ] [ lambda ]   [ k(t) ]
 *   [ F^T 0 ] [  beta  ] = [ f(t) ]
 *
 * F is the (n x (p+1)) drift matrix; f(t) the (p+1) basis at target.
 * Variance: sigma^2(t) = max(nugget, C(0) - k^T*lambda - f^T*beta)
 *
 * m_Kinv stores the full (n+p+1) x (n+p+1) extended inverse.
 * LOO shortcut uses the upper-left n x n block.
 *
 * Appropriate when a deterministic trend is present in the data
 * (e.g. GNSS coordinate velocity, sensor drift).
 *
 * FUTURE: arbitrary user-defined drift basis functions.
 */
class UniversalKriging : public KrigingBase {
public:

    /**
     * @param trendDegree     Polynomial degree of the drift (1 or 2 recommended).
     * @param confidenceLevel Confidence level for CI bounds.
     */
    explicit UniversalKriging(int trendDegree, double confidenceLevel);

    void              fit(const TimeSeries&        ts,
                          const VariogramFitResult& variogram) override;
    KrigingPrediction predictAt(double mjd) const override;

private:

    int    m_degree; ///< Polynomial degree.
    double m_tRef;   ///< Reference MJD (first observation) for numerical stability.

    /// Evaluate drift basis [1, t, t^2, ..., t^p] at shifted time t = mjd - m_tRef.
    std::vector<double> _driftBasis(double mjd) const;
};

} // namespace loki::math
