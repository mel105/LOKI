#pragma once

#include <loki/regression/regressor.hpp>
#include <loki/core/config.hpp>

#include <vector>

namespace loki::regression {

/**
 * @brief Simple linear regression: y = a0 + a1 * x.
 *
 * Uses LsqSolver from loki_core/math. Optionally uses IRLS robust estimation
 * when RegressionConfig::robust is true.
 *
 * The x-axis is x = mjd - tRef (days relative to the first observation),
 * which ensures numerical stability for both ms-resolution and 6h data.
 *
 * After fit(), call predict() to obtain fitted values with confidence and
 * prediction intervals at arbitrary x locations.
 */
class LinearRegressor : public Regressor {
public:

    /**
     * @brief Constructs a LinearRegressor with the given configuration.
     * @param cfg Full application regression configuration.
     */
    explicit LinearRegressor(const RegressionConfig& cfg);

    /**
     * @brief Fits y = a0 + a1 * x to the time series.
     *
     * NaN observations are skipped. Requires at least 3 valid observations.
     * Stores the result internally so that predict() can be called afterwards.
     *
     * @param ts Input time series.
     * @return   Populated RegressionResult with modelName, fitted TimeSeries,
     *           tRef, R^2, adjusted R^2, AIC, BIC, sigma0, dof.
     * @throws DataException      if fewer than 3 valid observations.
     * @throws AlgorithmException on singular normal matrix.
     */
    RegressionResult fit(const TimeSeries& ts) override;

    /**
     * @brief Computes predictions and intervals at the given x locations.
     *
     * Must be called after fit(). Uses the t-distribution with dof degrees
     * of freedom and the confidence level from RegressionConfig.
     *
     * @param xNew x values in days relative to tRef (= mjd - tRef).
     * @return     Vector of PredictionPoint, one per element of xNew.
     * @throws AlgorithmException if called before fit().
     */
    std::vector<PredictionPoint> predict(const std::vector<double>& xNew) const override;

    /**
     * @brief Returns "LinearRegressor".
     */
    std::string name() const override;

private:

    RegressionConfig m_cfg;
    RegressionResult m_lastResult;
    bool             m_fitted{false};

    /**
     * @brief Builds the 2-column design matrix [1, x] for linear regression.
     */
    static Eigen::MatrixXd buildDesignMatrix(const Eigen::VectorXd& x);

    /**
     * @brief Computes R^2, adjusted R^2, AIC and BIC from the lsq result.
     */
    static void computeGoodnessOfFit(RegressionResult&      result,
                                     const Eigen::VectorXd& l,
                                     int                    nParams);
};

} // namespace loki::regression
