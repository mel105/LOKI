#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace loki::arima {

/**
 * @brief Non-seasonal ARIMA order (p, d, q).
 */
struct ArimaOrder {
    int p{0};  ///< AR order.
    int d{0};  ///< Differencing order.
    int q{0};  ///< MA order.
};

/**
 * @brief Seasonal SARIMA order (P, D, Q, s).
 *
 * s == 0 disables the seasonal component entirely.
 */
struct SarimaOrder {
    int P{0};  ///< Seasonal AR order.
    int D{0};  ///< Seasonal differencing order.
    int Q{0};  ///< Seasonal MA order.
    int s{0};  ///< Seasonal period in samples (0 = no seasonal component).
};

/**
 * @brief Result of fitting an ARIMA or SARIMA model.
 *
 * Coefficients are ordered to match the lag index vectors produced by
 * ArimaFitter::arLags() and ArimaFitter::maLags():
 *   arCoeffs[i]  corresponds to lag arLags()[i]
 *   maCoeffs[i]  corresponds to lag maLags()[i]
 *
 * For a pure non-seasonal model the lag vectors are simply {1, 2, ..., p}
 * and {1, 2, ..., q}. For SARIMA they follow the multiplicative expansion.
 *
 * The residuals and fitted values are on the (differenced) series passed
 * to ArimaFitter::fit(), not on the original observations.
 */
struct ArimaResult {
    ArimaOrder          order;        ///< Non-seasonal order used.
    SarimaOrder         seasonal;     ///< Seasonal order used.
    std::vector<int>    arLags;       ///< AR lag indices (length = arCoeffs.size()).
    std::vector<int>    maLags;       ///< MA lag indices (length = maCoeffs.size()).
    std::vector<double> arCoeffs;     ///< AR coefficients phi (one per arLag).
    std::vector<double> maCoeffs;     ///< MA coefficients theta (one per maLag).
    double              intercept{0.0};
    double              sigma2{0.0};  ///< Residual variance.
    double              logLik{0.0};
    double              aic{0.0};
    double              bic{0.0};
    std::vector<double> residuals;    ///< Model residuals on the differenced series.
    std::vector<double> fitted;       ///< Fitted values on the differenced series.
    std::size_t         n{0};         ///< Number of observations used in fitting.
    std::string         method;       ///< Fitting method used: "css".
};

/**
 * @brief Result of an h-step-ahead forecast.
 *
 * All vectors have length == horizon.
 * The forecast is on the differenced series; back-transformation is the
 * caller's responsibility.
 */
struct ForecastResult {
    std::vector<double> forecast;  ///< Point forecast.
    std::vector<double> lower95;   ///< Lower 95% prediction interval bound.
    std::vector<double> upper95;   ///< Upper 95% prediction interval bound.
    int                 horizon{0};
};

} // namespace loki::arima
