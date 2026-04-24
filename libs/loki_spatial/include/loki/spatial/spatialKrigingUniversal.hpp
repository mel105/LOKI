#pragma once
#include <memory>
#include <loki/spatial/spatialKrigingBase.hpp>
#include <vector>

namespace loki::spatial {

/**
 * @brief Spatial Universal Kriging (polynomial drift in x, y).
 *
 * Mean model (degree 1): mu(x,y) = b0 + b1*x + b2*y
 * Mean model (degree 2): mu(x,y) = b0 + b1*x + b2*y + b3*x^2 + b4*xy + b5*y^2
 *
 * Extended system (n + nDrift):
 *   [ K   F ] [ lambda ]   [ k(x,y) ]
 *   [ F^T 0 ] [  beta  ] = [ f(x,y) ]
 *
 * F is the (n x nDrift) drift matrix; nDrift = (d+1)(d+2)/2 for degree d.
 * Variance: sigma^2 = max(nugget, C(0) - k^T*lambda - f^T*beta)
 */
class SpatialUniversalKriging : public SpatialKrigingBase {
public:
    /**
     * @param trendDegree     Polynomial degree of the drift (1 or 2).
     * @param confidenceLevel Confidence level for CI bounds.
     */
    explicit SpatialUniversalKriging(int trendDegree, double confidenceLevel);

    std::unique_ptr<SpatialKrigingBase> _cloneEmpty() const override;

    void fit(const std::vector<loki::math::SpatialPoint>& points,
             const loki::math::VariogramFitResult&        variogram) override;

    loki::math::SpatialPrediction predictAt(double x, double y) const override;

private:
    int    m_degree;
    double m_xRef = 0.0; ///< Centroid X (numerical stability).
    double m_yRef = 0.0; ///< Centroid Y.

    /// Evaluate drift basis at shifted (x - xRef, y - yRef).
    std::vector<double> _driftBasis(double x, double y) const;
};

} // namespace loki::spatial
