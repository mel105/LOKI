#pragma once

#include <loki/stationarity/stationarityResult.hpp>
#include <loki/stationarity/adfTest.hpp>
#include <loki/stationarity/kpssTest.hpp>
#include <loki/stationarity/ppTest.hpp>
#include <loki/core/config.hpp>

#include <vector>

namespace loki::stationarity {

/**
 * @brief Top-level orchestrator for stationarity analysis.
 *
 * Runs all enabled tests (ADF, KPSS, PP, Runs) and synthesises a joint
 * conclusion with a recommended differencing order for ARIMA modelling.
 *
 * Joint conclusion logic:
 *   The primary signal is the ADF/PP vs KPSS agreement:
 *
 *   ADF/PP rejected (unit root rejected)  AND  KPSS not rejected (stationary):
 *     -> isStationary = true,  recommendedDiff = 0
 *
 *   ADF/PP not rejected (unit root present) AND  KPSS rejected (non-stationary):
 *     -> isStationary = false, recommendedDiff = 1
 *
 *   Conflicting (both reject or both fail to reject):
 *     -> isStationary = false (conservative), recommendedDiff = 1
 *        with a note in the conclusion string.
 *
 *   If only one test family is enabled, its result is used directly.
 *
 * The runs test provides supplementary evidence and is noted in the conclusion
 * but does not influence isStationary or recommendedDiff.
 */
class StationarityAnalyzer {
public:
    using Config = StationarityConfig;

    /**
     * @brief Construct with optional configuration.
     * @param cfg Full StationarityConfig (from AppConfig or default-constructed).
     */
    explicit StationarityAnalyzer(Config cfg = {});

    /**
     * @brief Run all enabled tests and return the aggregated result.
     *
     * @param y  Univariate time series (already gap-filled and deseasonalized
     *           by the caller). Must have at least 10 observations.
     * @return   StationarityResult with per-test results and joint conclusion.
     * @throws DataException if y is too short.
     */
    StationarityResult analyze(const std::vector<double>& y) const;

private:
    Config m_cfg;

    /// Build the human-readable conclusion string from individual results.
    static std::string buildConclusion(const StationarityResult& r,
                                       bool adfPpEvidence,
                                       bool kpssEvidence);
};

} // namespace loki::stationarity
