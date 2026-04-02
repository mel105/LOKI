#pragma once

#include <loki/stationarity/stationarityResult.hpp>
#include <loki/core/config.hpp>

#include <vector>

namespace loki::stationarity {

/**
 * @brief Augmented Dickey-Fuller (ADF) unit root test.
 *
 * Tests H0: the series has a unit root (is non-stationary / I(1)).
 * H1: the series is stationary (I(0)).
 *
 * The ADF regression fitted is one of:
 *   - "none":     dy_t = gamma*y_{t-1} + sum beta_i*dy_{t-i} + e_t
 *   - "constant": dy_t = alpha + gamma*y_{t-1} + sum beta_i*dy_{t-i} + e_t
 *   - "trend":    dy_t = alpha + delta*t + gamma*y_{t-1} + sum beta_i*dy_{t-i} + e_t
 *
 * The test statistic is the t-ratio of gamma (the coefficient on y_{t-1}).
 * Critical values are from MacKinnon (1994) via adfCriticalValue().
 *
 * Lag selection:
 *   - "aic"   : minimise AIC over lags 0..maxLags
 *   - "bic"   : minimise BIC over lags 0..maxLags
 *   - "fixed" : use exactly maxLags lags (maxLags must be >= 0)
 *
 * When maxLags == -1 (auto), the upper bound is floor(12*(n/100)^0.25),
 * the rule of thumb from Schwert (1989).
 */
class AdfTest {
public:
    using Config = AdfConfig;

    /**
     * @brief Construct with optional configuration.
     * @param cfg AdfConfig (from AppConfig or default-constructed).
     */
    explicit AdfTest(Config cfg = {});

    /**
     * @brief Run the ADF test on the provided series.
     * @param y  Univariate time series (raw values, not differenced).
     *           Must have at least 10 observations.
     * @return   TestResult with statistic, p-value (NaN -- not available from
     *           response surface), critical values, trendType, and lags used.
     * @throws DataException      if y is too short.
     * @throws AlgorithmException if the OLS system is singular.
     */
    TestResult test(const std::vector<double>& y) const;

private:
    Config m_cfg;

    /// Compute the upper lag bound from sample size (Schwert 1989).
    static int autoMaxLag(std::size_t n);

    /// Fit one ADF regression with given lag count, return {tau, aic, bic}.
    struct FitResult { double tau; double aic; double bic; };
    FitResult fitRegression(const std::vector<double>& dy,
                            const std::vector<double>& y,
                            int lag) const;
};

} // namespace loki::stationarity
