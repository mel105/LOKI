// ============================================================
//  spatialKrigingSimple.hpp
// ============================================================
#pragma once
#include <memory>
#include <loki/spatial/spatialKrigingBase.hpp>

namespace loki::spatial {

/**
 * @brief Spatial Simple Kriging (known constant mean mu).
 *
 * System:   K * lambda = k(x, y)
 * Estimate: Z*(x,y) = mu + lambda^T * (z - mu)
 * Variance: sigma^2  = max(nugget, C(0) - k^T * lambda)
 */
class SpatialSimpleKriging : public SpatialKrigingBase {
public:
    explicit SpatialSimpleKriging(double knownMean, double confidenceLevel);

    std::unique_ptr<SpatialKrigingBase> _cloneEmpty() const override;

    void fit(const std::vector<loki::math::SpatialPoint>& points,
             const loki::math::VariogramFitResult&        variogram) override;

    loki::math::SpatialPrediction predictAt(double x, double y) const override;

private:
    double m_mu;
};

} // namespace loki::spatial
