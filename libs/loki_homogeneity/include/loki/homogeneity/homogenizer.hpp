#pragma once

#include <loki/homogeneity/changePointResult.hpp>
#include <loki/timeseries/deseasonalizer.hpp>
#include <loki/homogeneity/multiChangePointDetector.hpp>
#include <loki/homogeneity/seriesAdjuster.hpp>
#include <loki/outlier/outlierResult.hpp>
#include <loki/timeseries/gapFiller.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <string>
#include <vector>

namespace loki::homogeneity {

// ----------------------------------------------------------------------------
//  OutlierConfig
// ----------------------------------------------------------------------------

/**
 * @brief Configuration for one outlier removal pass inside the homogeneity pipeline.
 */
struct OutlierConfig {
    bool        enabled{false};
    std::string method{"mad_bounds"};
    double      madMultiplier{5.0};
    double      iqrMultiplier{1.5};
    double      zscoreThreshold{3.0};
    std::string replacementStrategy{"linear"};
    int         maxFillLength{0};

    explicit OutlierConfig() = default;
};

// ----------------------------------------------------------------------------
//  HomogenizerConfig
// ----------------------------------------------------------------------------

/**
 * @brief Full configuration for the Homogenizer pipeline.
 */
struct HomogenizerConfig {
    bool                             applyGapFilling{true};
    GapFiller::Config                gapFiller{};

    OutlierConfig                    preOutlier{};

    /// Uses loki::Deseasonalizer (moved to loki_core).
    loki::Deseasonalizer::Config     deseasonalizer{};

    int                              medianYearMinYears{5};

    OutlierConfig                    postOutlier{};

    MultiChangePointDetector::Config detector{};

    bool                             applyAdjustment{true};

    explicit HomogenizerConfig() = default;
};

// ----------------------------------------------------------------------------
//  HomogenizerResult
// ----------------------------------------------------------------------------

/**
 * @brief Output of the homogenization pipeline.
 */
struct HomogenizerResult {
    std::vector<ChangePoint>     changePoints;
    TimeSeries                   adjustedSeries;
    std::vector<double>          deseasonalizedValues;
    loki::outlier::OutlierResult preOutlierDetection;
    loki::outlier::OutlierResult postOutlierDetection;
    TimeSeries                   preOutlierCleaned;
};

// ----------------------------------------------------------------------------
//  Homogenizer
// ----------------------------------------------------------------------------

/**
 * @brief Orchestrates the full homogenization pipeline for a single time series.
 *
 * Pipeline order:
 *   1. GapFiller
 *   2. OutlierCleaner (pre)  -- coarse removal on raw series
 *   3. Deseasonalizer        -- removes seasonal signal (loki_core)
 *   4. OutlierCleaner (post) -- fine removal on residuals
 *   5. MultiChangePointDetector
 *   6. SeriesAdjuster
 */
class Homogenizer {
public:

    using Config = HomogenizerConfig;
    using Result = HomogenizerResult;

    /**
     * @brief Constructs a Homogenizer with the given configuration.
     */
    explicit Homogenizer(HomogenizerConfig cfg = HomogenizerConfig{});

    /**
     * @brief Runs the full homogenization pipeline on a single time series.
     *
     * @param input Input time series. Must not be empty.
     * @return HomogenizerResult.
     * @throws loki::DataException   if the input series is empty.
     * @throws loki::ConfigException if pipeline configuration is incompatible.
     */
    [[nodiscard]]
    HomogenizerResult process(const TimeSeries& input) const;

private:
    HomogenizerConfig m_cfg;
};

} // namespace loki::homogeneity