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

enum class TimeFormat {
    INDEX,
    GPS_TOTAL_SECONDS,
    GPS_WEEK_SOW,
    UTC,
    MJD,
    UNIX
};

// -----------------------------------------------------------------------------
//  InputMode
// -----------------------------------------------------------------------------

enum class InputMode {
    SINGLE_FILE,
    FILE_LIST,
    SCAN_DIRECTORY
};

// -----------------------------------------------------------------------------
//  MergeStrategy
// -----------------------------------------------------------------------------

enum class MergeStrategy {
    SEPARATE,
    MERGE
};

// -----------------------------------------------------------------------------
//  InputConfig
// -----------------------------------------------------------------------------

struct InputConfig {
    InputMode   mode{InputMode::SINGLE_FILE};

    std::filesystem::path file;
    std::vector<std::filesystem::path> files;
    std::filesystem::path scanDir;

    TimeFormat    timeFormat{TimeFormat::GPS_TOTAL_SECONDS};
    char          delimiter{';'};
    char          commentChar{'%'};

    std::vector<int> columns;
    std::vector<int> timeColumns;

    MergeStrategy mergeStrategy{MergeStrategy::SEPARATE};
};

// -----------------------------------------------------------------------------
//  OutputConfig
// -----------------------------------------------------------------------------

struct OutputConfig {
    LogLevel logLevel{LogLevel::INFO};
};

// -----------------------------------------------------------------------------
//  GapFillingConfig
// -----------------------------------------------------------------------------

struct GapFillingConfig {
    std::string strategy{"linear"};
    int         maxFillLength{0};
    double      gapThresholdFactor{1.5};
    int         minSeriesYears{10};
};

// -----------------------------------------------------------------------------
//  OutlierFilterConfig
// -----------------------------------------------------------------------------

struct OutlierFilterConfig {
    bool        enabled{false};
    std::string method{"mad_bounds"};
    double      madMultiplier{5.0};
    double      iqrMultiplier{1.5};
    double      zscoreThreshold{3.0};
    std::string replacementStrategy{"linear"};
    int         maxFillLength{0};
};

// -----------------------------------------------------------------------------
//  DeseasonalizationConfig
// -----------------------------------------------------------------------------

struct DeseasonalizationConfig {
    std::string strategy{"median_year"};
    int         maWindowSize{365};
    int         medianYearMinYears{5};
};

// -----------------------------------------------------------------------------
//  DetectionConfig
// -----------------------------------------------------------------------------

struct DetectionConfig {
    int         minSegmentPoints{60};
    double      minSegmentSeconds{0.0};
    std::string minSegmentDuration{""};
    double      significanceLevel{0.05};
    double      acfDependenceLimit{0.2};
    bool        correctForDependence{true};
};

// -----------------------------------------------------------------------------
//  HomogeneityConfig
// -----------------------------------------------------------------------------

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

struct OutlierConfig {

    struct DeseasonalizationSection {
        std::string strategy{"none"};
        int         maWindowSize{365};
        int         medianYearMinYears{5};
    };

    struct DetectionSection {
        std::string method{"mad"};
        double      iqrMultiplier{1.5};
        double      madMultiplier{3.0};
        double      zscoreThreshold{3.0};
    };

    struct ReplacementSection {
        std::string strategy{"linear"};
        int         maxFillLength{0};
    };

    DeseasonalizationSection deseasonalization{};
    DetectionSection         detection{};
    ReplacementSection       replacement{};
};

// -----------------------------------------------------------------------------
//  FilterMethod
// -----------------------------------------------------------------------------

enum class FilterMethodEnum {
    MOVING_AVERAGE,
    EMA,
    WEIGHTED_MA,
    KERNEL,
    LOESS,
    SAVITZKY_GOLAY
};

// -----------------------------------------------------------------------------
//  FilterConfig  (sub-configs defined outside to avoid GCC 13 aggregate init bug)
// -----------------------------------------------------------------------------

struct FilterGapFillingConfig {
    std::string strategy{"linear"};
    int         maxFillLength{0};
};

struct MovingAverageFilterConfig {
    int window{365};
};

struct EmaFilterConfig {
    double alpha{0.1};
};

struct WeightedMaFilterConfig {
    std::vector<double> weights{1.0, 2.0, 3.0, 2.0, 1.0};
};

struct KernelFilterConfig {
    double      bandwidth{0.1};
    std::string kernelType{"epanechnikov"};
    double      gaussianCutoff{3.0};
};

struct LoessFilterConfig {
    double      bandwidth{0.25};
    int         degree{1};
    std::string kernelType{"tricube"};
    bool        robust{false};
    int         robustIterations{3};
};

struct SavitzkyGolayFilterConfig {
    int window{11};
    int degree{2};
};

/**
 * @brief Top-level configuration for the loki_filter pipeline.
 */
struct FilterConfig {
    FilterGapFillingConfig    gapFilling{};
    FilterMethodEnum          method{FilterMethodEnum::KERNEL};
    std::string               autoWindowMethod{"silverman_mad"};
    MovingAverageFilterConfig movingAverage{};
    EmaFilterConfig           ema{};
    WeightedMaFilterConfig    weightedMa{};
    KernelFilterConfig        kernel{};
    LoessFilterConfig         loess{};
    SavitzkyGolayFilterConfig savitzkyGolay{};
};

// -----------------------------------------------------------------------------
//  PlotOptionsConfig
// -----------------------------------------------------------------------------

/**
 * @brief Fine-grained options for individual plot types.
 */
struct PlotOptionsConfig {
    /// Maximum lag for ACF plot. 0 = auto: min(N/10, 200).
    int acfMaxLag{0};

    /// Number of bins for histogram. 0 = auto: Sturges rule ceil(log2(N)+1).
    int histogramBins{0};
};

// -----------------------------------------------------------------------------
//  PlotConfig
// -----------------------------------------------------------------------------

/**
 * @brief Configuration for plot output.
 */
struct PlotConfig {
    std::string outputFormat{"png"};
    std::string timeFormat{""};

    PlotOptionsConfig options{};

    // -- Generic plots --------------------------------------------------------
    bool timeSeries {true};
    bool comparison {false};
    bool histogram  {true};
    bool acf        {false};
    bool qqPlot     {false};
    bool boxplot    {false};

    // -- Outlier pipeline plots -----------------------------------------------
    bool originalSeries      {true};
    bool adjustedSeries      {true};
    bool homogComparison     {true};
    bool deseasonalized      {true};
    bool seasonalOverlay     {true};
    bool residualsWithBounds {true};
    bool outlierOverlay      {false};

    // -- Homogeneity pipeline plots -------------------------------------------
    bool changePoints     {true};
    bool shiftMagnitudes  {true};
    bool correctionCurve  {true};

    // -- Filter pipeline plots ------------------------------------------------
    bool filterOverlay           {true};
    bool filterOverlayResiduals  {true};
    bool filterResiduals         {false};
    bool filterResidualsAcf      {false};
    bool residualsHistogram      {false};
    bool residualsQq             {false};
};

// -----------------------------------------------------------------------------
//  StatsConfig
// -----------------------------------------------------------------------------

struct StatsConfig {
    bool      enabled   {true};
    NanPolicy nanPolicy {NanPolicy::SKIP};
    bool      hurst     {true};
};

// -----------------------------------------------------------------------------
//  AppConfig
// -----------------------------------------------------------------------------

struct AppConfig {
    std::filesystem::path workspace;

    InputConfig       input;
    OutputConfig      output;
    PlotConfig        plots;
    HomogeneityConfig homogeneity;
    StatsConfig       stats;
    OutlierConfig     outlier;
    FilterConfig      filter;

    std::filesystem::path logDir;
    std::filesystem::path csvDir;
    std::filesystem::path imgDir;
};

} // namespace loki
