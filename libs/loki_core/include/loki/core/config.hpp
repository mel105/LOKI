#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "loki/core/logger.hpp"
#include "loki/core/nanPolicy.hpp"

namespace loki {

// -----------------------------------------------------------------------------
//  TimeFormat
// -----------------------------------------------------------------------------

/**
 * @brief Specifies how the time column in an input file is encoded.
 */
enum class TimeFormat {
    INDEX,              ///< Sequential integer index (no absolute time).
    GPS_TOTAL_SECONDS,  ///< Seconds since GPS epoch 1980-01-06 (e.g. 1424771514).
    GPS_WEEK_SOW,       ///< Two columns: GPS week number + seconds of week.
    UTC,                ///< ISO string: "YYYY-MM-DD hh:mm:ss[.sss]".
    MJD,                ///< Modified Julian Date as a floating-point number.
    UNIX                ///< Seconds since Unix epoch 1970-01-01.
};

// -----------------------------------------------------------------------------
//  InputMode
// -----------------------------------------------------------------------------

/**
 * @brief Controls how the Loader discovers input files.
 */
enum class InputMode {
    SINGLE_FILE,    ///< Load exactly one file specified by InputConfig::file.
    FILE_LIST,      ///< Load an explicit list of files (InputConfig::files).
    SCAN_DIRECTORY  ///< Scan InputConfig::scanDir for all recognised extensions.
};

// -----------------------------------------------------------------------------
//  MergeStrategy
// -----------------------------------------------------------------------------

/**
 * @brief Controls how multiple input files are combined.
 *
 * Only relevant when InputMode is FILE_LIST or SCAN_DIRECTORY.
 */
enum class MergeStrategy {
    SEPARATE,  ///< Each file produces an independent TimeSeries.
    MERGE      ///< All files are concatenated and sorted chronologically.
};

// -----------------------------------------------------------------------------
//  InputConfig
// -----------------------------------------------------------------------------

/**
 * @brief Configuration for data loading.
 */
struct InputConfig {

    InputMode   mode{InputMode::SINGLE_FILE};

    std::filesystem::path file;
    std::vector<std::filesystem::path> files;
    std::filesystem::path scanDir;

    TimeFormat    timeFormat{TimeFormat::GPS_TOTAL_SECONDS};
    char          delimiter{';'};
    char          commentChar{'%'};

    /// 1-based column indices for value columns. Empty = load all.
    std::vector<int> columns;

    /// 0-based field indices that together form the time token.
    /// Default empty = field 0 only.
    /// Use [0, 1] when date and time are split across two fields,
    /// e.g. "1990-01-01" "00:00:00" with time_format: "utc".
    /// The fields are joined with a single space before parsing.
    std::vector<int> timeColumns;

    MergeStrategy mergeStrategy{MergeStrategy::SEPARATE};
};

// -----------------------------------------------------------------------------
//  OutputConfig
// -----------------------------------------------------------------------------

/**
 * @brief Configuration for program output.
 */
struct OutputConfig {
    LogLevel logLevel{LogLevel::INFO};
};

// -----------------------------------------------------------------------------
//  GapFillingConfig
// -----------------------------------------------------------------------------

/**
 * @brief Configuration for the gap filling step.
 */
struct GapFillingConfig {
    std::string strategy{"linear"};       ///< linear | forward_fill | mean | none
    int         maxFillLength{0};         ///< 0 = unlimited
    double      gapThresholdFactor{1.5};  ///< Multiplier for expected step to detect gap.
    int         minSeriesYears{10};       ///< Minimum span for MEDIAN_YEAR gap filling.
};

// -----------------------------------------------------------------------------
//  OutlierFilterConfig
// -----------------------------------------------------------------------------

/**
 * @brief Configuration for a single outlier removal pass.
 *
 * Used for both pre-deseasonalization (coarse) and post-deseasonalization
 * (fine) outlier removal inside the homogeneity pipeline.
 *
 * ### Methods
 * - mad_bounds : Global MAD-based symmetric bounds. Robust to unknown domain.
 *                Suitable for pre-outlier on raw series of any type.
 *                Flags values outside [median +/- k * MAD].
 *                Use a large multiplier (e.g. 5.0) for coarse removal only.
 * - iqr        : IQR-based detection on residuals. Classic box-plot rule.
 * - mad        : MAD-based detection on residuals (normalised by 0.6745).
 * - zscore     : Z-score detection on residuals. Assumes approximate normality.
 *
 * ### Replacement
 * Detected outliers are replaced with NaN, then filled by GapFiller.
 * Strategies: linear | forward_fill | mean
 */
struct OutlierFilterConfig {
    bool        enabled{false};

    /// Detection method: mad_bounds | iqr | mad | zscore
    std::string method{"mad_bounds"};

    /// MAD multiplier for mad_bounds and mad detectors.
    /// Default 5.0 for pre-outlier (coarse). Use 3.0 for post-outlier (fine).
    double      madMultiplier{5.0};

    /// IQR multiplier for iqr detector. Default 1.5 (standard box-plot rule).
    double      iqrMultiplier{1.5};

    /// Z-score threshold. Default 3.0 (corresponds to ~0.3% of Gaussian tail).
    double      zscoreThreshold{3.0};

    /// How to fill NaN values left by outlier removal.
    /// Options: linear | forward_fill | mean
    std::string replacementStrategy{"linear"};

    /// Maximum consecutive NaN samples to fill. 0 = unlimited.
    int         maxFillLength{0};
};

// -----------------------------------------------------------------------------
//  DeseasonalizationConfig
// -----------------------------------------------------------------------------

/**
 * @brief Configuration for the deseasonalization step.
 */
struct DeseasonalizationConfig {
    std::string strategy{"median_year"};  ///< median_year | moving_average | none
    int         maWindowSize{365};
    int         medianYearMinYears{5};
};

// -----------------------------------------------------------------------------
//  DetectionConfig
// -----------------------------------------------------------------------------

/**
 * @brief Configuration for change point detection.
 */
struct DetectionConfig {
    int    minSegmentPoints{60};
    double minSegmentSeconds{0.0};

    /// Human-readable minimum segment duration. Parsed and converted to
    /// minSegmentSeconds by ConfigLoader. Takes precedence over minSegmentSeconds
    /// if non-empty. Supported units: y (years), d (days), h (hours), m (minutes), s (seconds).
    /// Examples: "1y", "180d", "6h", "30m", "60s".
    std::string minSegmentDuration{""};

    double significanceLevel{0.05};
    double acfDependenceLimit{0.2};
    bool   correctForDependence{true};
};

// -----------------------------------------------------------------------------
//  HomogeneityConfig
// -----------------------------------------------------------------------------

/**
 * @brief Full configuration for the homogeneity analysis pipeline.
 *
 * Maps 1:1 to the "homogeneity" section of the JSON config file.
 * ConfigLoader builds HomogenizerConfig from this struct in main.
 */
struct HomogeneityConfig {
    bool                    applyGapFilling{true};
    GapFillingConfig        gapFilling{};
    OutlierFilterConfig     preOutlier{};
    DeseasonalizationConfig deseasonalization{};
    OutlierFilterConfig     postOutlier{};
    DetectionConfig         detection{};
    bool                    applyAdjustment{true};
};

// -----------------------------------------------------------------------------
//  OutlierConfig
// -----------------------------------------------------------------------------

/**
 * @brief Full configuration for the standalone outlier analysis pipeline.
 *
 * Maps 1:1 to the "outlier" section of outlier.json.
 * Used exclusively by apps/loki_outlier.
 */
struct OutlierConfig {

    /**
     * @brief Deseasonalization applied before outlier detection.
     *
     * Strategies:
     * - median_year    : Subtracts median annual profile. For climatological /
     *                    GNSS data with known annual periodicity.
     * - moving_average : Subtracts a centred moving average. For sensor / signal
     *                    data with no strict annual period.
     * - none           : No deseasonalization. Use for already-detrended residuals
     *                    or aperiodic series (economic, count data).
     */
    struct DeseasonalizationSection {
        std::string strategy{"none"};  ///< median_year | moving_average | none
        int         maWindowSize{365};
        int         medianYearMinYears{5};
    };

    /**
     * @brief Outlier detection parameters.
     *
     * Methods:
     * - iqr        : IQR-based (box-plot rule). Multiplier controls fence width.
     * - mad        : MAD-based (robust, Gaussian-normalised by 0.6745).
     * - zscore     : Z-score (assumes approximate normality of residuals).
     * - mad_bounds : Global MAD bounds on raw series. No deseasonalization needed.
     *                Best for coarse filtering of physically impossible values.
     */
    struct DetectionSection {
        std::string method{"mad"};            ///< iqr | mad | zscore | mad_bounds
        double      iqrMultiplier{1.5};       ///< Fence = Q1/Q3 +/- multiplier * IQR
        double      madMultiplier{3.0};       ///< Threshold = median +/- k * MAD / 0.6745
        double      zscoreThreshold{3.0};     ///< Flag if |z| > threshold
    };

    /**
     * @brief Replacement strategy for detected outliers.
     *
     * Detected outliers are set to NaN, then filled by GapFiller from loki_core.
     * Strategies:
     * - linear       : Linear interpolation between valid neighbours.
     * - forward_fill : Copy last valid value forward.
     * - mean         : Replace with mean of the full series.
     */
    struct ReplacementSection {
        std::string strategy{"linear"};  ///< linear | forward_fill | mean
        int         maxFillLength{0};    ///< 0 = unlimited
    };

    DeseasonalizationSection deseasonalization{};
    DetectionSection         detection{};
    ReplacementSection       replacement{};
};

// -----------------------------------------------------------------------------
//  PlotConfig
// -----------------------------------------------------------------------------

/**
 * @brief Configuration for plot output.
 *
 * Generic plots (timeSeries, histogram, etc.) are used by apps/loki.
 * Homogeneity-specific plots are used by apps/loki_homogeneity.
 */
struct PlotConfig {
    std::string outputFormat{"png"};  ///< png | eps | svg
    std::string timeFormat{""};       ///< X-axis time format. Empty = inherit from input.

    // -- Generic plots --------------------------------------------------------
    bool timeSeries {true};
    bool comparison {false};
    bool histogram  {true};
    bool acf        {false};
    bool qqPlot     {false};
    bool boxplot    {false};

    // -- Homogeneity plots ----------------------------------------------------
    bool originalSeries   {true};
    bool seasonalOverlay  {true};
    bool deseasonalized   {true};
    bool changePoints     {true};
    bool adjustedSeries   {true};
    bool homogComparison  {true};
    bool shiftMagnitudes  {true};
    bool correctionCurve  {true};
};

// -----------------------------------------------------------------------------
//  StatsConfig
// -----------------------------------------------------------------------------

/**
 * @brief Configuration for descriptive statistics computation.
 */
struct StatsConfig {
    bool      enabled   {true};
    NanPolicy nanPolicy {NanPolicy::SKIP};
    bool      hurst     {true};
};

// -----------------------------------------------------------------------------
//  AppConfig
// -----------------------------------------------------------------------------

/**
 * @brief Top-level application configuration.
 *
 * Populated by ConfigLoader::load() from a JSON file.
 * All file paths inside sub-configs are absolute.
 *
 * Expected directory layout under workspace:
 * @code
 *   <workspace>/
 *   +-- INPUT/
 *   +-- OUTPUT/
 *       +-- LOG/
 *       +-- CSV/
 *       +-- IMG/
 * @endcode
 *
 * Each application reads only the sections it needs:
 * - apps/loki_homogeneity : input, output, plots, stats, homogeneity
 * - apps/loki_outlier     : input, output, plots, stats, outlier
 */
struct AppConfig {

    std::filesystem::path workspace;

    InputConfig       input;
    OutputConfig      output;
    PlotConfig        plots;
    HomogeneityConfig homogeneity;
    StatsConfig       stats;
    OutlierConfig     outlier;

    // -- Derived paths (computed by ConfigLoader) -----------------------------
    std::filesystem::path logDir;
    std::filesystem::path csvDir;
    std::filesystem::path imgDir;
};

} // namespace loki