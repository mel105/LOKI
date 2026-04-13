#pragma once

#include <loki/math/krigingBase.hpp>

namespace loki::math {

/**
 * @brief Simple Kriging (known constant mean mu).
 *
 * System:   K * lambda = k(t)
 * Estimate: Z*(t) = mu + lambda^T * (z - mu)
 * Variance: sigma^2(t) = max(nugget, C(0) - k^T * lambda)
 *
 * Most numerically stable variant. Appropriate when the mean is reliably
 * known -- e.g. deseasonalized residuals centred at zero.
 *
 * Cached: K^{-1} (n x n). LOO uses base class shortcut.
 *
 * FUTURE: locally varying known mean for spatial non-stationarity.
 */
class SimpleKriging : public KrigingBase {
public:

    /**
     * @param knownMean       Known global mean mu.
     * @param confidenceLevel Confidence level for CI bounds.
     */
    explicit SimpleKriging(double knownMean, double confidenceLevel);

    void              fit(const TimeSeries&        ts,
                          const VariogramFitResult& variogram) override;
    KrigingPrediction predictAt(double mjd) const override;

private:

    double m_mu; ///< Known global mean.
};

} // namespace loki::math
