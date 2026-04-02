#pragma once

#include <loki/stats/hypothesis.hpp>

#include <limits>
#include <optional>
#include <string>

namespace loki::stationarity {

/**
 * @brief Result of a single unit-root or stationarity test.
 *
 * ADF and PP test H0: series has a unit root (non-stationary).
 * KPSS tests        H0: series is stationary.
 *
 * The rejected flag always means: "evidence against H0 of this test".
 */
struct TestResult {
    double      statistic    {0.0};
    double      pValue       {std::numeric_limits<double>::quiet_NaN()};
    double      critVal1pct  {0.0};
    double      critVal5pct  {0.0};
    double      critVal10pct {0.0};
    bool        rejected     {false};  ///< H0 rejected at the test's significance_level.
    std::string testName;
    std::string trendType;             ///< Model specification used ("none"/"constant"/"trend"/"level").
    int         lags         {0};      ///< Number of lags used (ADF/PP); 0 for KPSS with auto.
};

/**
 * @brief Aggregated result of the full stationarity analysis pipeline.
 *
 * isStationary reflects the joint conclusion:
 *   - ADF rejected (unit root rejected) AND KPSS not rejected (stationarity not rejected)
 *     -> stationary
 *   - ADF not rejected AND KPSS rejected
 *     -> non-stationary
 *   - conflicting or all disabled -> conclusion contains explanation
 *
 * recommendedDiff is 0, 1, or 2 -- the suggested differencing order d for ARIMA(p,d,q).
 */
struct StationarityResult {
    std::optional<TestResult>            adf;
    std::optional<TestResult>            kpss;
    std::optional<TestResult>            pp;
    std::optional<loki::stats::HypothesisResult> runsTest;

    int         recommendedDiff {0};
    bool        isStationary    {false};
    std::string conclusion;
    std::size_t n               {0};   ///< Series length used (after gap filling / differencing).
};

} // namespace loki::stationarity
