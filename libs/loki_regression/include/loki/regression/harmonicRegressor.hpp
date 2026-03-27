#pragma once

#include <loki/regression/regressor.hpp>
#include <loki/core/config.hpp>

#include <vector>

namespace loki::regression {

/**
 * @brief Harmonic regression: y = a0 + sum_{k=1}^{K} [a_k*sin(2*pi*t/T_k) + b_k*cos(2*pi*t/T_k)].
 *
 * Fits a mean offset plus K sine/cosine pairs, one per period in
 * RegressionConfig::period (a single period) repeated harmonicTerms times
 * as sub-harmonics: T, T/2, T/3, ..., T/K.
 *
 * The x-axis is t = mjd - tRef (days relative to the first observation).
 * Design matrix is built via DesignMatrix::harmonic().
 *
 * Useful for modelling annual, semi-annual, and higher-order seasonal signals
 * in climatological or GNSS time series.
 */
class HarmonicRegressor : public Regressor {
public:

    /**
     * @brief Constructs a HarmonicRegressor.
     *
     * Parameters used from cfg:
     *   - harmonicTerms: number of sin/cos pairs K (>= 1).
     *   - period:        fundamental period in days (e.g. 365.25 for annual).
     *   - robust / robustIterations / robustWeightFn: IRLS options.
     *   - confidenceLevel: for predict() intervals.
     *
     * @param cfg Full application regression configuration.
     */
    explicit HarmonicRegressor(const RegressionConfig& cfg);

    /**
     * @brief Fits the harmonic model to the time series.
     *
     * NaN observations are skipped. Requires at least 2*K + 2 valid
     * observations (2*K + 1 parameters + 1 for dof >= 1).
     *
     * @param ts Input time series.
     * @return   Populated RegressionResult. coefficients layout:
     *           [a0, s1, c1, s2, c2, ..., sK, cK]
     *           where s_k = amplitude of sin(2*pi*t / (T/k))
     *                 c_k = amplitude of cos(2*pi*t / (T/k))
     * @throws DataException      if too few valid observations.
     * @throws AlgorithmException on singular normal matrix.
     */
    RegressionResult fit(const TimeSeries& ts) override;

    /**
     * @brief Computes predictions and intervals at the given t locations.
     *
     * Must be called after fit(). t values must be mjd - tRef (days).
     *
     * @param xNew t values relative to tRef (days).
     * @return     Vector of PredictionPoint.
     * @throws AlgorithmException if called before fit().
     */
    std::vector<PredictionPoint> predict(const std::vector<double>& xNew) const override;

    /**
     * @brief Returns "HarmonicRegressor(K=k, T=t)".
     */
    std::string name() const override;

    /**
     * @brief Returns the amplitude of the k-th harmonic term (1-indexed).
     *
     * Amplitude = sqrt(s_k^2 + c_k^2).
     * Must be called after fit().
     *
     * @param k Harmonic index in [1, harmonicTerms].
     * @return  Amplitude of the k-th harmonic.
     * @throws AlgorithmException if called before fit() or k out of range.
     */
    double amplitude(int k) const;

    /**
     * @brief Returns the phase of the k-th harmonic term in radians (1-indexed).
     *
     * Phase = atan2(s_k, c_k).
     * Must be called after fit().
     *
     * @param k Harmonic index in [1, harmonicTerms].
     * @return  Phase in [-pi, pi].
     * @throws AlgorithmException if called before fit() or k out of range.
     */
    double phase(int k) const;

private:

    RegressionConfig m_cfg;
    RegressionResult m_lastResult;
    bool             m_fitted{false};

    /**
     * @brief Builds harmonic design matrix for given t vector and periods.
     */
    Eigen::MatrixXd buildDesignMatrix(const Eigen::VectorXd& t) const;

    /**
     * @brief Returns the list of sub-harmonic periods [T, T/2, ..., T/K].
     */
    std::vector<double> periods() const;
};

} // namespace loki::regression
