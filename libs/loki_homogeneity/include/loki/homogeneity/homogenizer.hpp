#pragma once

#include <loki/homogeneity/changePointResult.hpp>
#include <loki/homogeneity/deseasonalizer.hpp>
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
 *
 * Used for both pre-deseasonalization (coarse) and post-deseasonalization
 * (fine) passes. Each pass has its own OutlierConfig instance.
 *
 * ### Methods
 * - mad_bounds : Global MAD-based symmetric bounds on the raw series.
 *                Best for pre-outlier (coarse removal of physically impossible values).
 *                Threshold = median +/- madMultiplier * MAD.
 *                Use a large multiplier (default 5.0).
 * - iqr        : IQR-based detection on residuals (box-plot rule).
 * - mad        : MAD-based detection on residuals (normalised by 0.6745).
 * - zscore     : Z-score detection on residuals. Assumes approximate normality.
 *
 * ### Replacement
 * Detected outliers are set to NaN, then filled by GapFiller.
 * Strategies: linear | forward_fill | mean
 */
struct OutlierConfig {
    bool        enabled{false};

    /// Detection method: mad_bounds | iqr | mad | zscore
    std::string method{"mad_bounds"};

    /// MAD multiplier for mad_bounds and mad detectors.
    /// Default 5.0 for pre-outlier (coarse). Use 3.0 for post-outlier (fine).
    double      madMultiplier{5.0};

    /// IQR multiplier for iqr detector. Default 1.5 (standard box-plot rule).
    double      iqrMultiplier{1.5};

    /// Z-score threshold. Default 3.0.
    double      zscoreThreshold{3.0};

    /// Replacement strategy for detected outliers: linear | forward_fill | mean
    std::string replacementStrategy{"linear"};

    /// Maximum consecutive NaN positions to fill. 0 = unlimited.
    int         maxFillLength{0};

    explicit OutlierConfig() = default;
};

// ----------------------------------------------------------------------------
//  HomogenizerConfig
// ----------------------------------------------------------------------------

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

    /// Coarse outlier removal on the raw gap-filled series.
    /// Runs before deseasonalization. Default method: mad_bounds, k=5.0.
    OutlierConfig                    preOutlier{};

    Deseasonalizer::Config           deseasonalizer{};

    /// Minimum valid values per slot for MedianYearSeries.
    /// Used when gapFiller.strategy == MEDIAN_YEAR or
    /// deseasonalizer.strategy == MEDIAN_YEAR.
    int                              medianYearMinYears{5};

    /// Fine outlier removal on the deseasonalized residuals.
    /// Runs after deseasonalization, before change point detection.
    /// Default method: mad, k=3.0.
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
 *
 * Contains the primary results (change points, adjusted series) plus
 * diagnostic data for logging and plotting:
 * - deseasonalizedValues : residuals after seasonal removal and outlier cleaning.
 * - preOutlierDetection  : outliers found in the raw series (Step 2).
 * - postOutlierDetection : outliers found in the residuals (Step 4).
 * - preOutlierCleaned    : gap-filled series after pre-outlier removal.
 *                          Useful for overlay plots (original vs cleaned).
 *
 * All outlier fields are empty/default when the corresponding step is disabled.
 */
struct HomogenizerResult {
    /// Detected structural breaks in global series coordinates.
    std::vector<ChangePoint>    changePoints;

    /// Original-scale series with cumulative shift corrections applied.
    TimeSeries                  adjustedSeries;

    /// Deseasonalized residuals after outlier cleaning (input to change point detection).
    std::vector<double>         deseasonalizedValues;

    /// Detection report from the pre-deseasonalization outlier pass.
    /// Empty if preOutlier.enabled == false.
    loki::outlier::OutlierResult preOutlierDetection;

    /// Detection report from the post-deseasonalization outlier pass.
    /// Empty if postOutlier.enabled == false.
    loki::outlier::OutlierResult postOutlierDetection;

    /// Gap-filled working series after pre-outlier removal.
    /// Same as gap-filled input when preOutlier.enabled == false.
    /// Useful for plotting: original vs pre-outlier-cleaned overlay.
    TimeSeries                  preOutlierCleaned;
};

// ----------------------------------------------------------------------------
//  Homogenizer
// ----------------------------------------------------------------------------

/**
 * @brief Orchestrates the full homogenization pipeline for a single time series.
 *
 * Pipeline order:
 *   1. GapFiller              -- fills missing values (loki_core)
 *   2. OutlierCleaner (pre)   -- coarse outlier removal on raw series (loki_outlier)
 *   3. Deseasonalizer         -- removes seasonal signal
 *   4. OutlierCleaner (post)  -- fine outlier removal on residuals (loki_outlier)
 *   5. MultiChangePointDetector -- detects structural breaks
 *   6. SeriesAdjuster         -- corrects the original series for detected shifts
 *
 * The adjusted series in HomogenizerResult contains original-scale values with
 * cumulative shift corrections applied. Deseasonalized residuals (after outlier
 * cleaning) are included separately for diagnostic purposes.
 */
class Homogenizer {
public:

    using Config = HomogenizerConfig;
    using Result = HomogenizerResult;

    /**
     * @brief Constructs a Homogenizer with the given configuration.
     * @param cfg Pipeline configuration. All fields have sensible defaults.
     */
    explicit Homogenizer(HomogenizerConfig cfg = HomogenizerConfig{});

    /**
     * @brief Runs the full homogenization pipeline on a single time series.
     *
     * @param input Input time series. Must not be empty.
     * @return HomogenizerResult with change points, adjusted series, residuals,
     *         and outlier detection diagnostics.
     * @throws loki::DataException    if the input series is empty.
     * @throws loki::ConfigException  if pipeline configuration is incompatible
     *         with the input (e.g. MEDIAN_YEAR without a calendar timestamp).
     */
    [[nodiscard]]
    HomogenizerResult process(const TimeSeries& input) const;

private:
    HomogenizerConfig m_cfg;
};

} // namespace loki::homogeneity
