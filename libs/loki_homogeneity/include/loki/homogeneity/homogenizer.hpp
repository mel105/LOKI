#pragma once

#include <loki/homogeneity/changePointResult.hpp>
#include <loki/homogeneity/deseasonalizer.hpp>
#include <loki/homogeneity/multiChangePointDetector.hpp>
#include <loki/homogeneity/seriesAdjuster.hpp>
#include <loki/timeseries/gapFiller.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <vector>

namespace loki::homogeneity {

/**
 * @brief Placeholder config for outlier removal (not yet implemented).
 *
 * Both pre- and post-deseasonalization outlier removal passes are planned
 * for loki_outlier. This struct reserves their place in HomogenizerConfig
 * so the interface will not change when they arrive.
 */
struct OutlierConfig {
    bool   enabled{false};  ///< Always false until loki_outlier is implemented.
    double threshold{3.0};  ///< IQR multiplier or Z-score threshold (future use).

    explicit OutlierConfig() = default;
};

/**
 * @brief Full configuration for the Homogenizer pipeline.
 *
 * Defined outside Homogenizer to work around a GCC 13 limitation:
 * structs with in-class member initializers cannot be used as default
 * arguments when nested inside another class.
 *
 * Steps are individually enabled or disabled. Disabled steps pass
 * the series through unchanged.
 */
struct HomogenizerConfig {
    bool                             applyGapFilling{true};
    GapFiller::Config                gapFiller{};

    OutlierConfig                    preOutlier{};   ///< Before deseasonalization (future).

    Deseasonalizer::Config           deseasonalizer{};

    /// Minimum valid values per slot for MedianYearSeries.
    /// Used when gapFiller.strategy == MEDIAN_YEAR or
    /// deseasonalizer.strategy == MEDIAN_YEAR.
    int                              medianYearMinYears{5};

    OutlierConfig                    postOutlier{};  ///< After deseasonalization (future).

    MultiChangePointDetector::Config detector{};

    bool                             applyAdjustment{true};

    explicit HomogenizerConfig() = default;
};

/**
 * @brief Output of the homogenization pipeline.
 */
struct HomogenizerResult {
    std::vector<ChangePoint> changePoints;          ///< Detected structural breaks.
    TimeSeries               adjustedSeries;        ///< Original-scale series with shift corrections.
    std::vector<double>      deseasonalizedValues;  ///< Residuals after seasonal removal (diagnostic).
};

/**
 * @brief Orchestrates the full homogenization pipeline for a single time series.
 *
 * Pipeline order:
 *   1. GapFiller              -- fills missing values (loki_core)
 *   2. OutlierRemover (pre)   -- coarse outlier removal before deseasonalization (future)
 *   3. Deseasonalizer         -- removes seasonal signal
 *   4. OutlierRemover (post)  -- fine outlier removal on residuals (future)
 *   5. MultiChangePointDetector -- detects structural breaks
 *   6. SeriesAdjuster         -- corrects the original series for detected shifts
 *
 * The adjusted series in HomogenizerResult contains original-scale values with
 * cumulative shift corrections applied. Deseasonalized residuals are included
 * separately for diagnostic purposes.
 */
class Homogenizer {
public:

    using Config = HomogenizerConfig;  ///< Convenience alias.
    using Result = HomogenizerResult;  ///< Convenience alias.

    /**
     * @brief Constructs a Homogenizer with the given configuration.
     * @param cfg Pipeline configuration. All fields have sensible defaults.
     */
    explicit Homogenizer(HomogenizerConfig cfg = HomogenizerConfig{});

    /**
     * @brief Runs the full homogenization pipeline on a single time series.
     *
     * If no change points are detected, adjustedSeries is a copy of the
     * (gap-filled) input and changePoints is empty.
     *
     * @param input Input time series. Must not be empty.
     * @return HomogenizerResult containing change points, adjusted series,
     *         and deseasonalized residuals.
     * @throws loki::DataException if the input series is empty.
     * @throws loki::ConfigException if pipeline configuration is incompatible
     *         with the input (e.g. MEDIAN_YEAR strategy without a calendar timestamp).
     */
    [[nodiscard]]
    HomogenizerResult process(const TimeSeries& input) const;

private:
    HomogenizerConfig m_cfg;
};

} // namespace loki::homogeneity
