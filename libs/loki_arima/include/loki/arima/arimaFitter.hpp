#pragma once

#include <loki/arima/arimaResult.hpp>

#include <string>
#include <vector>

namespace loki::arima {

// -----------------------------------------------------------------------------
//  ArimaFitterConfig  (defined outside class -- GCC 13 aggregate-init rule)
// -----------------------------------------------------------------------------

/**
 * @brief Configuration for ArimaFitter.
 */
struct ArimaFitterConfig {
    ArimaOrder  order    {};          ///< Non-seasonal order (p, d, q).
    SarimaOrder seasonal {};          ///< Seasonal order (P, D, Q, s). s=0 disables.
    std::string method   {"css"};     ///< Fitting method. Only "css" supported now.
    int         maxIter  {200};       ///< Reserved for future MLE use.
    double      tol      {1.0e-8};    ///< Reserved for future MLE use.
};

// -----------------------------------------------------------------------------
//  ArimaFitter
// -----------------------------------------------------------------------------

/**
 * @brief Fits an ARIMA(p,d,q)(P,D,Q)[s] model using the CSS-HR method.
 *
 * The Conditional Sum of Squares (CSS) approach via Hannan-Rissanen (HR)
 * two-step approximation:
 *
 *   Step 1 -- Innovation proxy:
 *     Fit a high-order AR to the differenced series to obtain residual
 *     proxies for the unobserved innovations epsilon_t.
 *     AR order used: min(max(p + q + P + Q + 4, 10), n / 4).
 *
 *   Step 2 -- Joint OLS:
 *     Assemble a regressor matrix from:
 *       - Lagged y values at AR lags (multiplicative SARIMA expansion)
 *       - Lagged epsilon proxies at MA lags (same expansion)
 *       - Intercept column
 *     Solve via LsqSolver (ColPivHouseholderQR).
 *
 * Lag index generation follows the multiplicative Box-Jenkins convention:
 *   AR lags = { i + j*s : i in 1..p, j in 0..P } union { j*s : j in 1..P }
 *   (equivalently: Cartesian product {0..p} x {0..P} minus (0,0), lag = i+j*s)
 *   MA lags = analogous with q, Q.
 *
 * Differencing (d ordinary + D seasonal with step s) is applied internally
 * before fitting. The returned ArimaResult contains residuals and fitted
 * values on the differenced series.
 *
 * Information criteria are computed using the CSS log-likelihood approximation:
 *   logLik = -n/2 * (log(2*pi*sigma2) + 1)
 *   AIC    = -2*logLik + 2*k
 *   BIC    = -2*logLik + k*log(n)
 * where k = |arLags| + |maLags| + 1 (intercept).
 */
class ArimaFitter {
public:

    using Config = ArimaFitterConfig;

    /**
     * @brief Construct with optional configuration.
     * @param cfg Fitter configuration including order and method.
     */
    explicit ArimaFitter(Config cfg = {});

    /**
     * @brief Fit the ARIMA model to the provided series.
     *
     * Applies differencing internally (d + D seasonal passes), then runs the
     * HR two-step procedure.
     *
     * @param y  Input time series (gap-filled, deseasonalized). Must be NaN-free.
     * @return   Populated ArimaResult.
     * @throws SeriesTooShortException if y is too short for the requested order.
     * @throws AlgorithmException      if the OLS system is rank-deficient.
     */
    ArimaResult fit(const std::vector<double>& y) const;

    /**
     * @brief Compute AR lag indices for the multiplicative SARIMA expansion.
     *
     * Returns the sorted unique set of lags:
     *   { i + j*s : i in 0..p, j in 0..P } \ {0}
     *
     * For a non-seasonal model (s == 0 or P == 0) this reduces to {1, ..., p}.
     *
     * @param p  Non-seasonal AR order.
     * @param P  Seasonal AR order.
     * @param s  Seasonal period (samples). 0 means no seasonal expansion.
     * @return   Sorted vector of positive lag indices.
     */
    static std::vector<int> arLags(int p, int P, int s);

    /**
     * @brief Compute MA lag indices for the multiplicative SARIMA expansion.
     *
     * Analogous to arLags() with q and Q.
     *
     * @param q  Non-seasonal MA order.
     * @param Q  Seasonal MA order.
     * @param s  Seasonal period (samples).
     * @return   Sorted vector of positive lag indices.
     */
    static std::vector<int> maLags(int q, int Q, int s);

private:

    Config m_cfg;

    /**
     * @brief Apply d ordinary differences and D seasonal differences (step s).
     *
     * @param y  Input series.
     * @param d  Ordinary differencing order.
     * @param D  Seasonal differencing order.
     * @param s  Seasonal period.
     * @return   Differenced series.
     */
    static std::vector<double> applyDifferencing(const std::vector<double>& y,
                                                  int d, int D, int s);

    /**
     * @brief CSS fitting via Hannan-Rissanen two-step OLS.
     *
     * @param y  Differenced series (NaN-free).
     * @return   Populated ArimaResult.
     */
    ArimaResult fitCss(const std::vector<double>& y) const;

    /**
     * @brief Compute information criteria from sigma2, n, and number of params k.
     *
     * Populates result.logLik, result.aic, result.bic in-place.
     */
    static void computeInfoCriteria(ArimaResult& result, int k);
};

} // namespace loki::arima
