#pragma once

#include <loki/math/krigingBase.hpp>

namespace loki::math {

/**
 * @brief Ordinary Kriging (unknown local mean, sum-of-weights = 1).
 *
 * Extended system:
 *   [ K   1 ] [ lambda ]   [ k(t) ]
 *   [ 1^T 0 ] [   mu   ] = [  1   ]
 *
 * Estimate: Z*(t) = lambda^T * z
 * Variance: sigma^2(t) = max(nugget, C(0) - k^T*lambda - mu_lagrange)
 *
 * Default method. No assumption about the mean required.
 * m_Kinv stores the full (n+1) x (n+1) extended inverse.
 * LOO shortcut uses the upper-left n x n block.
 *
 * FUTURE: neighbourhood search (k nearest neighbours) for n > 2000.
 */
class OrdinaryKriging : public KrigingBase {
public:

    /**
     * @param confidenceLevel Confidence level for CI bounds.
     */
    explicit OrdinaryKriging(double confidenceLevel);

    void              fit(const TimeSeries&        ts,
                          const VariogramFitResult& variogram) override;
    KrigingPrediction predictAt(double mjd) const override;
};

} // namespace loki::math
