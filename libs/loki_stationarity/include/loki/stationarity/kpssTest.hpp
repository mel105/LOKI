#pragma once

#include <loki/stationarity/stationarityResult.hpp>
#include <loki/core/config.hpp>

#include <vector>

namespace loki::stationarity {

/**
 * @brief KPSS test for stationarity (Kwiatkowski, Phillips, Schmidt, Shin 1992).
 *
 * Tests H0: the series is stationary (around a constant or a linear trend).
 * H1: the series has a unit root.
 *
 * This is complementary to ADF: ADF tests the unit root hypothesis, KPSS tests
 * the stationarity hypothesis. Used together they provide a more complete picture.
 *
 * The test statistic is:
 *   eta = (1/n^2) * sum_{t=1}^{n} S_t^2 / s^2
 *
 * where S_t = sum_{i=1}^{t} e_i are the partial sums of the OLS residuals,
 * and s^2 is the Newey-West long-run variance estimate.
 *
 * Critical values are asymptotic from Kwiatkowski et al. (1992), Table 1.
 * Reject H0 (stationarity) if eta > critical value.
 *
 * trendType:
 *   "level" -- regress on constant only (eta_mu statistic)
 *   "trend" -- regress on constant and linear trend (eta_tau statistic)
 */
class KpssTest {
public:
    using Config = KpssConfig;

    /**
     * @brief Construct with optional configuration.
     * @param cfg KpssConfig (from AppConfig or default-constructed).
     */
    explicit KpssTest(Config cfg = {});

    /**
     * @brief Run the KPSS test on the provided series.
     * @param y  Univariate time series. Must have at least 5 observations.
     * @return   TestResult with statistic, critical values, trendType, and lags used.
     *           pValue is NaN (asymptotic critical values are tabulated, not a continuous function).
     * @throws DataException if y is too short.
     */
    TestResult test(const std::vector<double>& y) const;

private:
    Config m_cfg;
};

} // namespace loki::stationarity
