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
    /// Detected structural breaks.
    std::vector<ChangePoint>     changePoints;

    /// Homogenized series (shift-corrected).
    TimeSeries                   adjustedSeries;

    /// Deseasonalized residuals after outlier cleaning (input to change point detection).
    std::vector<double>          deseasonalizedValues;

    /// Seasonal component from Deseasonalizer::Result.
    /// Use this directly for plotting -- do not recompute from original - residuals.
    std::vector<double>          seasonal;

    /// Detection report from the pre-deseasonalization outlier pass.
    loki::outlier::OutlierResult preOutlierDetection;

    /// Detection report from the post-deseasonalization outlier pass.
    loki::outlier::OutlierResult postOutlierDetection;

    /// Gap-filled series after pre-outlier removal.
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
 *   2. OutlierCleaner (pre)  -- coarse removal on raw series, never on residuals
 *   3. Deseasonalizer        -- removes seasonal signal (loki_core)
 *   4. OutlierCleaner (post) -- fine removal on residuals directly,
 *                               no seasonal passed (already removed in step 3)
 *   5. MultiChangePointDetector
 *   6. SeriesAdjuster
 */
class Homogenizer {
public:

    using Config = HomogenizerConfig;
    using Result = HomogenizerResult;

    explicit Homogenizer(HomogenizerConfig cfg = HomogenizerConfig{});

    [[nodiscard]]
    HomogenizerResult process(const TimeSeries& input) const;

private:
    HomogenizerConfig m_cfg;
};

} // namespace loki::homogeneity