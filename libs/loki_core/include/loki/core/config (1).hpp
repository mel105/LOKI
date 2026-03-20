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
 * @brief Configuration for the gap filling step of the homogeneity pipeline.
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
 * @brief Placeholder for pre/post outlier removal (loki_outlier, future).
 */
struct OutlierFilterConfig {
    bool   enabled{false};
    double threshold{3.0};
};

// -----------------------------------------------------------------------------
//  DeseasonalizationConfig
// -----------------------------------------------------------------------------

/**
 * @brief Configuration for the deseasonalization step.
 */
struct DeseasonalizationConfig {
    std::string strategy{"median_year"};    ///< median_year | moving_average | none
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

    // -- Generic plots (apps/loki and apps/loki_homogeneity) ------------------
    bool timeSeries {true};
    bool comparison {false};
    bool histogram  {true};
    bool acf        {false};
    bool qqPlot     {false};
    bool boxplot    {false};

    // -- Homogeneity plots (apps/loki_homogeneity) ----------------------------
    bool originalSeries   {true};   ///< Original input series.
    bool seasonalOverlay  {true};   ///< Original + seasonal component overlay.
    bool deseasonalized   {true};   ///< Deseasonalized residuals.
    bool changePoints     {true};   ///< Residuals with change point markers.
    bool adjustedSeries   {true};   ///< Homogenized (adjusted) series.
    bool homogComparison  {true};   ///< Original vs adjusted overlay.
    bool shiftMagnitudes  {true};   ///< Bar chart of shift magnitudes.
    bool correctionCurve  {true};   ///< Original series + cumulative correction step function.
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
 */
struct AppConfig {

    std::filesystem::path workspace;

    InputConfig       input;
    OutputConfig      output;
    PlotConfig        plots;
    HomogeneityConfig homogeneity;
    StatsConfig       stats;

    // -- Derived paths (computed by ConfigLoader) -----------------------------
    std::filesystem::path logDir;
    std::filesystem::path csvDir;
    std::filesystem::path imgDir;
};

} // namespace loki
