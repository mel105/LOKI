#pragma once

#include <loki/arima/arimaResult.hpp>

#include <vector>

namespace loki::arima {

/**
 * @brief Produces h-step-ahead forecasts from a fitted ARIMA model.
 *
 * The forecast is computed on the differenced series (the same domain as
 * ArimaResult::fitted). Back-transformation to the original scale is the
 * caller's responsibility.
 *
 * Point forecast:
 *   Uses recursive AR propagation. For steps beyond the observed series,
 *   MA innovations are set to zero (conditional expectation).
 *   At each horizon step h:
 *     f_h = intercept
 *           + sum_i arCoeffs[i] * y_{n + h - arLags[i]}   (y from history or forecast)
 *           + sum_j maCoeffs[j] * e_{n + h - maLags[j]}   (e = 0 for future steps)
 *
 * Prediction intervals:
 *   Based on the MA(inf) representation psi-weights computed via the
 *   recursion from AR and MA coefficients. The h-step forecast variance is:
 *     Var(f_h) = sigma2 * (1 + psi_1^2 + psi_2^2 + ... + psi_{h-1}^2)
 *   95% interval: forecast +/- 1.96 * sqrt(Var(f_h))
 *
 *   The psi-weight recursion (non-seasonal model reference):
 *     psi_0 = 1
 *     psi_j = sum_{i=1}^{min(j,p)} phi_i * psi_{j-i}
 *             + (j <= q ? theta_j : 0)
 *   For SARIMA the recursion uses the full arLags / maLags index vectors.
 */
class ArimaForecaster {
public:

    /**
     * @brief Construct from a fitted ArimaResult.
     * @param result Fitted model. Must have non-empty fitted and residuals.
     * @throws AlgorithmException if result.fitted is empty.
     */
    explicit ArimaForecaster(const ArimaResult& result);

    /**
     * @brief Compute an h-step-ahead forecast.
     *
     * @param horizon Number of steps ahead (>= 1).
     * @return        ForecastResult with forecast, lower95, upper95 of length horizon.
     * @throws AlgorithmException if horizon < 1.
     */
    ForecastResult forecast(int horizon) const;

private:

    ArimaResult m_result;

    /**
     * @brief Compute psi-weights (MA-infinity coefficients) up to length horizon.
     *
     * psi[0] = 1 always. psi[h] is the coefficient on e_{t-h} in the
     * infinite MA representation, used for variance propagation.
     *
     * @param horizon Number of psi weights to compute (including psi[0]).
     * @return        Vector of length horizon.
     */
    std::vector<double> computePsiWeights(int horizon) const;
};

} // namespace loki::arima
