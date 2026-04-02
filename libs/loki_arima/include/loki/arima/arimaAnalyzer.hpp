#pragma once

#include <loki/arima/arimaResult.hpp>
#include <loki/core/config.hpp>

#include <vector>

namespace loki::arima {

/**
 * @brief Top-level orchestrator for the ARIMA analysis pipeline.
 *
 * Executes the following steps on a pre-processed (gap-filled, deseasonalized)
 * series:
 *
 *   1. Determine differencing order d:
 *        If cfg.d >= 0: use cfg.d directly.
 *        If cfg.d == -1: run StationarityAnalyzer and use recommendedDiff.
 *
 *   2. Apply d ordinary differences and cfg.seasonal.D seasonal differences.
 *
 *   3. Select (p, q) order:
 *        If cfg.autoOrder == true:  ArimaOrderSelector grid search.
 *        If cfg.autoOrder == false: use cfg.p, cfg.q directly.
 *
 *   4. Fit ArimaFitter::fit() -> ArimaResult.
 *
 *   5. Ljung-Box test on model residuals (logged as diagnostic, does not
 *      affect the returned result).
 *
 * The returned ArimaResult contains residuals and fitted values on the
 * differenced series. The caller (main.cpp) is responsible for forecasting,
 * CSV export, protocol, and plotting.
 */
class ArimaAnalyzer {
public:

    using Config = loki::ArimaConfig;

    /**
     * @brief Construct with optional configuration.
     * @param cfg Full ArimaConfig from AppConfig.
     */
    explicit ArimaAnalyzer(Config cfg = {});

    /**
     * @brief Run the full ARIMA pipeline on a pre-processed series.
     *
     * @param y  Univariate series (gap-filled, deseasonalized, NaN-free).
     *           Must have at least 10 observations.
     * @return   Fitted ArimaResult.
     * @throws DataException          if y is too short.
     * @throws AlgorithmException     if fitting fails.
     */
    ArimaResult analyze(const std::vector<double>& y) const;

private:

    Config m_cfg;

    /**
     * @brief Determine differencing order: from config or via StationarityAnalyzer.
     * @param y  Differenced series candidate (used only if d == -1).
     * @return   Non-negative differencing order d.
     */
    int determineDifferencingOrder(const std::vector<double>& y) const;
};

} // namespace loki::arima
