#pragma once
#include <memory>
#include <loki/spatial/spatialKrigingBase.hpp>

namespace loki::spatial {

/**
 * @brief Spatial Ordinary Kriging (unknown local mean, sum-of-weights = 1).
 *
 * Extended system:
 *   [ K   1 ] [ lambda ]   [ k(x,y) ]
 *   [ 1^T 0 ] [   mu   ] = [   1    ]
 *
 * Estimate: Z*(x,y) = lambda^T * z
 * Variance: sigma^2  = max(nugget, C(0) - k^T*lambda - mu_lagrange)
 */
class SpatialOrdinaryKriging : public SpatialKrigingBase {
public:
    explicit SpatialOrdinaryKriging(double confidenceLevel);

    std::unique_ptr<SpatialKrigingBase> _cloneEmpty() const override;

    void fit(const std::vector<loki::math::SpatialPoint>& points,
             const loki::math::VariogramFitResult&        variogram) override;

    loki::math::SpatialPrediction predictAt(double x, double y) const override;
};

} // namespace loki::spatial
