#pragma once

#include <loki/regression/regressor.hpp>
#include <loki/core/config.hpp>

#include <vector>

namespace loki::regression {

/**
 * @brief Weighted and robust regression via IRLS.
 *
 * Wraps any linear model (polynomial degree d) with explicit IRLS robust
 * estimation. Unlike PolynomialRegressor with robust=true, RobustRegressor
 * also supports external observation weights and exposes per-observation
 * final IRLS weights for diagnostics.
 *
 * Model: y = a0 + a1*x + ... + ad*x^d
 * Estimation: IRLS with Huber or Bisquare weight function.
 *
 * The x-axis is x = mjd - tRef (days relative to the first observation).
 *
 * Typical use case: fitting in the presence of outliers where IQR/MAD
 * detection is not desirable (e.g. correlated data, leverage points).
 */
class RobustRegressor : public Regressor {
public:

    /**
     * @brief Per-observation IRLS weight after convergence.
     *
     * Observations with low weight were downweighted as potential outliers.
     * w == 1.0 means the observation had full influence.
     * w ~~ 0.0 means the observation was effectively excluded.
     */
    struct WeightedObservation {
        std::size_t index;   ///< Index in the original (NaN-filtered) series.
        double      t;       ///< x = mjd - tRef (days).
        double      y;       ///< Observed value.
        double      fitted;  ///< Fitted value.
        double      residual;///< y - fitted.
        double      weight;  ///< Final IRLS weight in [0, 1].
    };

    /**
     * @brief Constructs a RobustRegressor.
     *
     * Parameters used from cfg:
     *   - polynomialDegree: model degree (default 1 = linear).
     *   - robust:           must be true; forced on if false, with a warning.
     *   - robustIterations: IRLS iteration limit.
     *   - robustWeightFn:   "huber" or "bisquare".
     *   - confidenceLevel:  for predict() intervals.
     *
     * @param cfg Full application regression configuration.
     */
    explicit RobustRegressor(const RegressionConfig& cfg);

    /**
     * @brief Fits the robust polynomial model to the time series.
     *
     * NaN observations are skipped. Requires at least degree + 2 valid
     * observations.
     *
     * @param ts Input time series.
     * @return   Populated RegressionResult. sigma0 reflects robust estimate.
     * @throws DataException      if too few valid observations.
     * @throws AlgorithmException on singular normal matrix or non-convergence
     *                             after maxIterations.
     */
    RegressionResult fit(const TimeSeries& ts) override;

    /**
     * @brief Computes predictions and intervals at the given x locations.
     *
     * Must be called after fit(). x values must be mjd - tRef (days).
     *
     * @param xNew x values relative to tRef (days).
     * @return     Vector of PredictionPoint.
     * @throws AlgorithmException if called before fit().
     */
    std::vector<PredictionPoint> predict(const std::vector<double>& xNew) const override;

    /**
     * @brief Returns "RobustRegressor(degree=d, fn=f)".
     */
    std::string name() const override;

    /**
     * @brief Returns per-observation IRLS weights after fit().
     *
     * Useful for identifying observations that were downweighted (outliers).
     * Must be called after fit().
     *
     * @return Vector of WeightedObservation, one per valid (non-NaN) observation.
     * @throws AlgorithmException if called before fit().
     */
    const std::vector<WeightedObservation>& weightedObservations() const;

private:

    RegressionConfig                m_cfg;
    RegressionResult                m_lastResult;
    bool                            m_fitted{false};
    std::vector<WeightedObservation> m_weights;

    /**
     * @brief Builds polynomial design matrix [1, x, ..., x^d].
     */
    Eigen::MatrixXd buildDesignMatrix(const Eigen::VectorXd& x) const;

    /**
     * @brief Computes Huber or Bisquare IRLS weights from standardised residuals.
     */
    static Eigen::VectorXd irlsWeights(const Eigen::VectorXd& residuals,
                                        double                  sigma,
                                        LsqWeightFunction       fn);
};

} // namespace loki::regression
