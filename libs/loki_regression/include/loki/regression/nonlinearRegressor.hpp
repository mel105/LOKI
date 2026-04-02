#pragma once

#include <loki/regression/regressionResult.hpp>
#include <loki/core/config.hpp>
#include <loki/math/lm.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <Eigen/Dense>

#include <string>
#include <vector>

namespace loki::regression {

/**
 * @brief Nonlinear least-squares regressor using the Levenberg-Marquardt algorithm.
 *
 * Fits a parametric nonlinear model f(x, p) to a time series by minimising
 * the sum of squared residuals. The Jacobian is evaluated numerically via
 * central differences.
 *
 * Built-in models (selected via NonlinearModelEnum in config):
 *   EXPONENTIAL : f(x, [a, b])          = a * exp(b * x)
 *   LOGISTIC    : f(x, [L, k, x0])      = L / (1 + exp(-k * (x - x0)))
 *   GAUSSIAN    : f(x, [A, mu, sigma])  = A * exp(-((x - mu)^2) / (2 * sigma^2))
 *
 * For CUSTOM models, use the two-argument constructor and supply a ModelFn
 * and initial parameter vector directly.
 *
 * Convention: x-axis is always x = mjd - tRef (days relative to first valid
 * observation), identical to all other regressors in loki_regression.
 *
 * fit() always returns a result even when LM does not converge (best-effort).
 * The caller should check result.converged and log a warning accordingly.
 *
 * This class does NOT inherit from Regressor because it requires model-function
 * and initial-parameter inputs that are incompatible with the base-class interface.
 */
class NonlinearRegressor {
public:

    /**
     * @brief Constructor for built-in models.
     *
     * The model function and default initial parameter estimates are determined
     * from cfg.nonlinear.model. If cfg.nonlinear.initialParams is non-empty,
     * those values override the built-in defaults.
     *
     * @param cfg Full regression config. nonlinear sub-config is used.
     * @throws ConfigException if initialParams has wrong size for the chosen model.
     */
    explicit NonlinearRegressor(const RegressionConfig& cfg);

    /**
     * @brief Constructor for CUSTOM models.
     *
     * @param cfg      Full regression config (LM tuning parameters are taken from
     *                 cfg.nonlinear; model enum is ignored).
     * @param modelFn  User-supplied model function f(x, params).
     * @param p0       Initial parameter vector. Must be non-empty.
     * @throws ConfigException if p0 is empty.
     */
    NonlinearRegressor(const RegressionConfig& cfg,
                       loki::ModelFn           modelFn,
                       Eigen::VectorXd         p0);

    /**
     * @brief Fit the nonlinear model to the given time series.
     *
     * NaN observations are silently skipped. At least p+1 valid observations
     * are required, where p is the number of parameters.
     *
     * @param ts Input time series.
     * @return   Fully populated RegressionResult.
     *           result.designMatrix contains the Jacobian at the solution.
     *           result.cofactorX    contains the approximate covariance matrix.
     *           result.converged    is false if LM reached maxIterations.
     * @throws DataException      if ts has fewer than p+1 valid observations.
     * @throws AlgorithmException on numerical failure inside LmSolver.
     */
    RegressionResult fit(const TimeSeries& ts);

    /**
     * @brief Compute predictions and approximate intervals at given x locations.
     *
     * Must be called after fit(). Intervals are linearisation-based (delta method):
     * they use the Jacobian-derived covariance from fit() and are therefore
     * approximate for strongly nonlinear models.
     *
     * @param xNew x values in days relative to tRef (= mjd - tRef).
     * @return     Vector of PredictionPoint, one per element of xNew.
     * @throws AlgorithmException if called before fit().
     */
    std::vector<PredictionPoint> predict(const std::vector<double>& xNew) const;

    /**
     * @brief Returns a human-readable model name including parameter values.
     */
    std::string name() const;

private:

    RegressionConfig m_cfg;
    loki::ModelFn    m_modelFn;
    Eigen::VectorXd  m_p0;
    RegressionResult m_lastResult;
    bool             m_fitted {false};

    /**
     * @brief Returns the built-in ModelFn for the given enum value.
     */
    static loki::ModelFn makeBuiltinModel(NonlinearModelEnum model);

    /**
     * @brief Returns default initial parameters for built-in models.
     *
     * These are rough heuristic starting points. Providing explicit
     * initial_params in the config is strongly recommended.
     */
    static Eigen::VectorXd defaultP0(NonlinearModelEnum model);

    /**
     * @brief Returns a string describing parameter names for the model.
     * Used to compose result.modelName.
     */
    static std::string paramNamesStr(NonlinearModelEnum model);
};

} // namespace loki::regression
