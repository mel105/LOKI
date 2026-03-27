#pragma once

#include <loki/regression/regressor.hpp>
#include <loki/core/config.hpp>

#include <vector>

namespace loki::regression {

/**
 * @brief Polynomial regression: y = a0 + a1*x + a2*x^2 + ... + ad*x^d.
 *
 * Uses LsqSolver from loki_core/math with DesignMatrix::polynomial().
 * Optionally uses IRLS robust estimation when RegressionConfig::robust is true.
 *
 * The x-axis is x = mjd - tRef (days relative to the first observation).
 *
 * Cross-validation:
 *   - For non-robust fits: analytical LOO-CV via hat matrix diagonal (O(n),
 *     exact equivalent of refitting n times).
 *   - For robust fits: k-fold CV (k configurable via RegressionConfig::cvFolds).
 *
 * After fit(), call predict() for fitted values with confidence and prediction
 * intervals, or leaveOneOutCV() / kFoldCV() for cross-validation metrics.
 */
class PolynomialRegressor : public Regressor {
public:

    /**
     * @brief Cross-validation result.
     */
    struct CVResult {
        double rmse{0.0};   ///< CV root mean squared error.
        double mae{0.0};    ///< CV mean absolute error.
        double bias{0.0};   ///< CV mean signed error.
        int    folds{0};    ///< Number of folds used (n for LOO, k for k-fold).
    };

    /**
     * @brief Constructs a PolynomialRegressor.
     *
     * The polynomial degree is taken from RegressionConfig::polynomialDegree.
     *
     * @param cfg Full application regression configuration.
     */
    explicit PolynomialRegressor(const RegressionConfig& cfg);

    /**
     * @brief Fits y = a0 + a1*x + ... + ad*x^d to the time series.
     *
     * NaN observations are skipped. Requires at least degree + 2 valid
     * observations.
     *
     * @param ts Input time series.
     * @return   Populated RegressionResult.
     * @throws DataException      if too few valid observations.
     * @throws AlgorithmException on singular normal matrix.
     */
    RegressionResult fit(const TimeSeries& ts) override;

    /**
     * @brief Computes predictions and intervals at the given x locations.
     *
     * Must be called after fit(). x values must be mjd - tRef (days).
     *
     * @param xNew x values relative to tRef.
     * @return     Vector of PredictionPoint.
     * @throws AlgorithmException if called before fit().
     */
    std::vector<PredictionPoint> predict(const std::vector<double>& xNew) const override;

    /**
     * @brief Returns "PolynomialRegressor(degree=d)".
     */
    std::string name() const override;

    /**
     * @brief Analytical leave-one-out cross-validation (non-robust OLS only).
     *
     * Uses the hat matrix shortcut: CV_i = residual_i / (1 - h_ii).
     * This is mathematically equivalent to refitting n times but runs in O(n).
     *
     * For robust fits, falls back to kFoldCV() automatically.
     *
     * Must be called after fit().
     *
     * @return CVResult with rmse, mae, bias, folds = n.
     * @throws AlgorithmException if called before fit().
     */
    CVResult leaveOneOutCV() const;

    /**
     * @brief K-fold cross-validation.
     *
     * Splits the data into k folds, trains on k-1 folds and tests on the
     * remaining fold, repeating k times. Used for robust fits or when
     * explicitly requested.
     *
     * The number of folds is taken from RegressionConfig::cvFolds.
     * If cvFolds < 2, defaults to 10. If cvFolds > n/2, clamps to n/2
     * with a warning logged.
     *
     * Must be called after fit().
     *
     * @return CVResult with rmse, mae, bias, folds = k used.
     * @throws AlgorithmException if called before fit().
     */
    CVResult kFoldCV() const;

private:

    RegressionConfig m_cfg;
    RegressionResult m_lastResult;
    bool             m_fitted{false};

    // Raw data preserved for CV (NaN-free, x = mjd - tRef).
    Eigen::VectorXd  m_x;
    Eigen::VectorXd  m_l;

    /**
     * @brief Builds polynomial design matrix [1, x, x^2, ..., x^d].
     */
    Eigen::MatrixXd buildDesignMatrix(const Eigen::VectorXd& x) const;
};

} // namespace loki::regression
