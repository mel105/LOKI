#pragma once

#include <loki/regression/regressionResult.hpp>
#include <loki/timeseries/timeSeries.hpp>
#include <loki/core/config.hpp>

#include <string>

namespace loki::regression {

/**
 * @brief Decomposition of a time series into trend, seasonal, and residual components.
 *
 * Fits the combined model:
 *   y = trend(t) + seasonal(t) + residual
 *
 * where:
 *   trend(t)    = a0 + a1*t  (linear, using LinearRegressor internally)
 *   seasonal(t) = sum of K harmonic pairs (using HarmonicRegressor internally)
 *
 * The two components are estimated jointly in a single LSQ fit, not sequentially.
 * This avoids the bias that arises from removing a seasonal signal before
 * estimating the trend.
 *
 * After fit(), the decomposed components are accessible as TimeSeries via
 * trend(), seasonal(), and residuals().
 */
class TrendEstimator {
public:

    /**
     * @brief Full decomposition result.
     */
    struct DecompositionResult {
        RegressionResult regression; ///< Full regression result (all coefficients).
        TimeSeries       trend;      ///< Linear trend component at observation times.
        TimeSeries       seasonal;   ///< Harmonic seasonal component.
        TimeSeries       residuals;  ///< Residuals after trend + seasonal removal.
        double           trendSlope{0.0};     ///< Trend slope in units/day.
        double           trendIntercept{0.0}; ///< Trend intercept at tRef.
    };

    /**
     * @brief Constructs a TrendEstimator.
     *
     * Parameters used from cfg:
     *   - harmonicTerms: K harmonic pairs for seasonal component.
     *   - period:        fundamental period in days.
     *   - robust / robustIterations / robustWeightFn: IRLS options.
     *   - confidenceLevel: for predict() intervals.
     *
     * @param cfg Full application regression configuration.
     */
    explicit TrendEstimator(const RegressionConfig& cfg);

    /**
     * @brief Fits trend + seasonal model jointly to the time series.
     *
     * NaN observations are skipped. Requires at least 2*K + 3 valid
     * observations (2*K + 2 parameters + 1 for dof >= 1).
     *
     * Coefficient layout: [a0, a1, s1, c1, s2, c2, ..., sK, cK]
     *   a0, a1  -- linear trend intercept and slope
     *   s_k, c_k -- sin/cos amplitudes for k-th harmonic
     *
     * @param ts Input time series.
     * @return   DecompositionResult with all components populated.
     * @throws DataException      if too few valid observations.
     * @throws AlgorithmException on singular normal matrix.
     */
    DecompositionResult fit(const TimeSeries& ts);

    /**
     * @brief Returns "TrendEstimator(K=k, T=t)".
     */
    std::string name() const;

private:

    RegressionConfig m_cfg;

    /**
     * @brief Builds combined [trend | harmonic] design matrix.
     *
     * Columns: [1, t, sin(2pi*t/T1), cos(2pi*t/T1), ..., sin(2pi*t/TK), cos(2pi*t/TK)]
     */
    Eigen::MatrixXd buildDesignMatrix(const Eigen::VectorXd& t) const;

    /**
     * @brief Returns the list of sub-harmonic periods [T, T/2, ..., T/K].
     */
    std::vector<double> periods() const;
};

} // namespace loki::regression
