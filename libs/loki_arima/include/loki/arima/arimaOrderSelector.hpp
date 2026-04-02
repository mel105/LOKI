#pragma once

#include <loki/arima/arimaResult.hpp>

#include <string>
#include <vector>

namespace loki::arima {

// -----------------------------------------------------------------------------
//  ArimaOrderSelectorConfig  (defined outside class -- GCC 13 rule)
// -----------------------------------------------------------------------------

/**
 * @brief Configuration for ArimaOrderSelector.
 */
struct ArimaOrderSelectorConfig {
    int         maxP      {5};      ///< Upper bound for non-seasonal AR order p.
    int         maxQ      {5};      ///< Upper bound for non-seasonal MA order q.
    std::string criterion {"aic"};  ///< Selection criterion: "aic" or "bic".
    std::string method    {"css"};  ///< Fitting method passed to ArimaFitter.
};

// -----------------------------------------------------------------------------
//  ArimaOrderSelector
// -----------------------------------------------------------------------------

/**
 * @brief Selects the optimal non-seasonal ARIMA order (p, q) by grid search.
 *
 * Performs an exhaustive search over the grid [0..maxP] x [0..maxQ],
 * fitting each candidate model via ArimaFitter (CSS) and selecting the
 * order that minimises AIC or BIC.
 *
 * The differencing order d is supplied externally (from StationarityAnalyzer
 * or from config). The seasonal order (P, D, Q, s) is passed through
 * unchanged to ArimaFitter and is not part of the grid search.
 *
 * ARIMA(0, d, 0) is always included in the grid as the baseline.
 * Models that fail to fit (singular OLS, series too short) are silently
 * skipped; a warning is logged for each failure.
 *
 * If all models fail, an AlgorithmException is thrown.
 */
class ArimaOrderSelector {
public:

    using Config = ArimaOrderSelectorConfig;

    /**
     * @brief Construct with optional configuration.
     * @param cfg Selector configuration.
     */
    explicit ArimaOrderSelector(Config cfg = {});

    /**
     * @brief Select the best (p, q) order for the given differenced series.
     *
     * @param y        Series after applying d ordinary differences and D seasonal
     *                 differences (i.e. the same input that ArimaFitter::fit()
     *                 would receive -- differencing is NOT applied here again).
     * @param d        Ordinary differencing order (used to set result.d only).
     * @param seasonal Seasonal order passed through to ArimaFitter unchanged.
     * @return         ArimaOrder with the best (p, d, q). d is copied from the
     *                 argument unchanged.
     * @throws AlgorithmException if no candidate model could be fitted.
     * @throws SeriesTooShortException if y is too short for even ARIMA(0,d,0).
     */
    ArimaOrder select(const std::vector<double>& y,
                      int                        d,
                      const SarimaOrder&         seasonal = {}) const;

private:

    Config m_cfg;
};

} // namespace loki::arima
