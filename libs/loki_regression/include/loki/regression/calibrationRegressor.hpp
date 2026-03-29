#pragma once

#include <loki/regression/regressor.hpp>
#include <loki/regression/regressionResult.hpp>
#include <loki/core/config.hpp>

#include <Eigen/Dense>

namespace loki::regression {

/**
 * @brief Orthogonal (Total Least Squares) regression via SVD.
 *
 * Unlike OLS which minimises vertical distances (errors in y only),
 * TLS minimises the sum of squared orthogonal distances from each point
 * to the fitted line. This is appropriate when both the predictor (x)
 * and the response (y) are subject to measurement error -- e.g. when
 * comparing two sensors or calibrating one instrument against another.
 *
 * Algorithm (simple linear case, one predictor):
 *   1. Centre x and y by their means.
 *   2. Form the n x 2 matrix Z = [x_c, y_c].
 *   3. Compute SVD: Z = U * S * V^T.
 *   4. The last right singular vector v = V[:, 1] defines the line:
 *      v[0] * x_c + v[1] * y_c = 0  =>  slope = -v[0] / v[1].
 *   5. Intercept recovered from means: a0 = y_bar - slope * x_bar.
 *
 * Coefficient layout: [a0, a1]  (intercept, slope) -- same as LinearRegressor.
 *
 * Residuals stored in RegressionResult are orthogonal distances (not vertical).
 * sigma0 is the RMS orthogonal residual.
 * cofactorX is left empty (TLS does not yield a standard covariance matrix).
 * designMatrix is stored for use with RegressionDiagnostics::computeInfluence().
 *
 * Prediction via predict() uses the fitted line to evaluate at new x values,
 * returning vertical prediction intervals based on sigma0 (approximate).
 */
class CalibrationRegressor : public Regressor {
public:
    explicit CalibrationRegressor(const RegressionConfig& cfg);

    /**
     * @brief Fits the TLS line to the time series.
     *
     * The x-axis is mjd - tRef (days from first valid observation),
     * consistent with all other LOKI regressors.
     *
     * @param ts Input time series. NaN values are skipped.
     * @return RegressionResult with TLS coefficients [a0, a1], orthogonal
     *         residuals, sigma0, and goodness-of-fit metrics.
     * @throws DataException if fewer than 3 valid observations are available.
     * @throws AlgorithmException if SVD fails or the line is undefined
     *         (e.g. all x values identical).
     */
    RegressionResult fit(const TimeSeries& ts) override;

    /**
     * @brief Evaluates the fitted line at new x positions.
     *
     * Prediction intervals are approximate (based on sigma0 from TLS fit,
     * treated as vertical error for interval computation).
     *
     * @param xNew x values in the same reference frame as fit() (mjd - tRef).
     * @return Vector of PredictionPoint with predicted values and intervals.
     * @throws AlgorithmException if fit() has not been called.
     */
    std::vector<PredictionPoint> predict(const std::vector<double>& xNew) const override;

    /// Returns "CalibrationRegressor (TLS)".
    std::string name() const override;

private:
    RegressionConfig m_cfg;
    RegressionResult m_lastResult;
    bool             m_fitted{false};
};

} // namespace loki::regression
