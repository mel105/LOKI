#pragma once

#include <loki/stationarity/stationarityResult.hpp>
#include <loki/core/config.hpp>

#include <vector>

namespace loki::stationarity {

/**
 * @brief Phillips-Perron (PP) unit root test (Phillips & Perron 1988).
 *
 * Tests H0: the series has a unit root (non-stationary).
 * H1: the series is stationary.
 *
 * The PP test is a non-parametric alternative to ADF. Instead of adding
 * lagged differences to control for serial correlation (as ADF does), PP
 * applies a semi-parametric correction to the standard DF t-statistic using
 * the Newey-West long-run variance estimator.
 *
 * The corrected Z(t_alpha) statistic is:
 *
 *   Z(t) = t_alpha * sqrt(s^2 / lambda^2)
 *          - 0.5 * (lambda^2 - s^2) * (n * se_alpha) / (lambda^2 * sqrt(XtX_inv_gamma))
 *
 * where:
 *   t_alpha  = OLS t-ratio of the lagged level coefficient (from simple DF)
 *   s^2      = OLS residual variance (sigma^2)
 *   lambda^2 = Newey-West long-run variance of residuals
 *   se_alpha = OLS standard error of the gamma coefficient
 *
 * Critical values are from MacKinnon (1994), identical to ADF (same distribution).
 * The trendType specification is the same as for ADF.
 */
class PpTest {
public:
    using Config = PpConfig;

    /**
     * @brief Construct with optional configuration.
     * @param cfg PpConfig (from AppConfig or default-constructed).
     */
    explicit PpTest(Config cfg = {});

    /**
     * @brief Run the PP test on the provided series.
     * @param y  Univariate time series. Must have at least 10 observations.
     * @return   TestResult with corrected Z(t) statistic, critical values,
     *           trendType, and lags used for the Newey-West correction.
     *           pValue is NaN (response surface gives critical values only).
     * @throws DataException      if y is too short.
     * @throws AlgorithmException if the OLS system is singular.
     */
    TestResult test(const std::vector<double>& y) const;

private:
    Config m_cfg;
};

} // namespace loki::stationarity
