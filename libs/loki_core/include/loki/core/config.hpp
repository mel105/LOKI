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

    struct HatMatrixSection {
        int    arOrder           {5};
        double significanceLevel {0.05};
        bool   enabled           {true};
    };

    DeseasonalizationSection deseasonalization{};
    DetectionSection         detection{};
    ReplacementSection       replacement{};
    HatMatrixSection         hatMatrix{};
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
//  RegressionMethod
// -----------------------------------------------------------------------------

enum class RegressionMethodEnum {
    LINEAR,
    POLYNOMIAL,
    HARMONIC,
    TREND,
    ROBUST,
    CALIBRATION,
    NONLINEAR
};

// -----------------------------------------------------------------------------
//  NonlinearModel
// -----------------------------------------------------------------------------

enum class NonlinearModelEnum {
    EXPONENTIAL,  ///< a * exp(b * x)
    LOGISTIC,     ///< L / (1 + exp(-k * (x - x0)))
    GAUSSIAN      ///< A * exp(-((x - mu)^2) / (2 * sigma^2))
};

// -----------------------------------------------------------------------------
//  NonlinearConfig  (defined outside RegressionConfig to avoid GCC 13 bug)
// -----------------------------------------------------------------------------

/**
 * @brief Configuration for NonlinearRegressor (Levenberg-Marquardt).
 */
struct NonlinearConfig {
    NonlinearModelEnum  model          {NonlinearModelEnum::EXPONENTIAL};
    std::vector<double> initialParams  {};
    int                 maxIterations  {100};
    double              gradTol        {1.0e-8};
    double              stepTol        {1.0e-8};
    double              lambdaInit     {1.0e-3};
    double              lambdaFactor   {10.0};
    double              confidenceLevel{0.95};
};

// -----------------------------------------------------------------------------
//  RegressionConfig  (sub-configs defined outside to avoid GCC 13 aggregate init bug)
// -----------------------------------------------------------------------------

struct RegressionGapFillingConfig {
    std::string strategy{"linear"};
    int         maxFillLength{0};
};

/**
 * @brief Top-level configuration for the loki_regression pipeline.
 */
struct RegressionConfig {
    RegressionGapFillingConfig gapFilling{};
    RegressionMethodEnum       method{RegressionMethodEnum::LINEAR};
    int                        polynomialDegree{1};
    int                        harmonicTerms{2};
    double                     period{365.25};
    bool                       robust{false};
    int                        robustIterations{10};
    std::string                robustWeightFn{"bisquare"};
    bool                       computePrediction{false};
    double                     predictionHorizon{0.0};
    double                     confidenceLevel{0.95};
    double                     significanceLevel{0.05};
    int                        cvFolds{10};
    NonlinearConfig            nonlinear{};
};

// -----------------------------------------------------------------------------
//  PlotOptionsConfig
// -----------------------------------------------------------------------------

/**
 * @brief Fine-grained options for individual plot types.
 */
struct PlotOptionsConfig {
    int acfMaxLag{0};
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
    bool leveragePlot        {true};   ///< DEH leverage vs time (HatMatrixDetector).

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

    // -- Regression pipeline plots --------------------------------------------
    bool regressionOverlay       {true};
    bool regressionResiduals     {true};
    bool regressionCdfPlot       {false};
    bool regressionQqBands       {true};
    bool regressionResidualAcf   {false};
    bool regressionResidualHist  {false};
    bool regressionInfluence     {false};
    bool regressionLeverage      {false};

    // -- Stationarity pipeline plots ------------------------------------------
    bool pacfPlot                {true};   ///< PACF of residuals with confidence band.
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
//  StationarityConfig  (sub-configs defined outside to avoid GCC 13 aggregate init bug)
// -----------------------------------------------------------------------------

struct AdfConfig {
    std::string trendType         {"constant"};  // "none" | "constant" | "trend"
    int         maxLags           {-1};          // -1 = auto via AIC/BIC
    std::string lagSelection      {"aic"};       // "aic" | "bic" | "fixed"
    double      significanceLevel {0.05};
};

struct KpssConfig {
    std::string trendType         {"level"};     // "level" | "trend"
    int         lags              {-1};          // -1 = auto (Newey-West bandwidth)
    double      significanceLevel {0.05};
};

struct PpConfig {
    std::string trendType         {"constant"};  // "none" | "constant" | "trend"
    int         lags              {-1};          // -1 = auto (Newey-West bandwidth)
    double      significanceLevel {0.05};
};

struct StationarityDifferencingConfig {
    bool apply {false};
    int  order {1};
};

struct StationarityTestsConfig {
    bool       adfEnabled      {true};
    bool       kpssEnabled     {true};
    bool       ppEnabled       {true};
    bool       runsTestEnabled {true};
    AdfConfig  adf             {};
    KpssConfig kpss            {};
    PpConfig   pp              {};
};

/**
 * @brief Top-level configuration for the loki_stationarity pipeline.
 */
struct StationarityConfig {
    DeseasonalizationConfig        deseasonalization {};
    StationarityDifferencingConfig differencing      {};
    StationarityTestsConfig        tests             {};
    double                         significanceLevel {0.05};
};

// -----------------------------------------------------------------------------
//  ArimaConfig  (sub-configs defined outside to avoid GCC 13 aggregate-init bug)
// -----------------------------------------------------------------------------

struct ArimaFitterConfig {
    std::string method        {"css"};   ///< Fitting method: "css" (MLE reserved for future).
    int         maxIterations {200};     ///< Reserved for future MLE use.
    double      tol           {1.0e-8}; ///< Reserved for future MLE use.
};

struct ArimaSarimaConfig {
    int P{0};  ///< Seasonal AR order.
    int D{0};  ///< Seasonal differencing order.
    int Q{0};  ///< Seasonal MA order.
    int s{0};  ///< Seasonal period in samples (0 = no seasonal component).
};

/**
 * @brief Top-level configuration for the loki_arima pipeline.
 */
struct ArimaConfig {
    // Gap filling
    std::string  gapFillStrategy  {"linear"};
    int          gapFillMaxLength {0};

    // Deseasonalization (same options as stationarity)
    DeseasonalizationConfig deseasonalization {};

    // Order
    bool         autoOrder {true};   ///< Auto-select (p,q) via AIC/BIC grid search.
    int          p         {1};      ///< AR order (used if autoOrder=false).
    int          d         {-1};     ///< Differencing order (-1 = auto from StationarityAnalyzer).
    int          q         {0};      ///< MA order (used if autoOrder=false).
    int          maxP      {5};      ///< Grid search upper bound for p.
    int          maxQ      {5};      ///< Grid search upper bound for q.
    std::string  criterion {"aic"};  ///< Order selection criterion: "aic" or "bic".

    // Seasonal (SARIMA -- s=0 disables)
    ArimaSarimaConfig seasonal {};

    // Fitting
    ArimaFitterConfig fitter {};

    // Forecast
    bool         computeForecast   {false};
    double       forecastHorizon   {0.0};    ///< Forecast horizon in days.
    double       confidenceLevel   {0.95};
    double       significanceLevel {0.05};
};

// -----------------------------------------------------------------------------
//  AppConfig
// -----------------------------------------------------------------------------

struct AppConfig {
    std::filesystem::path workspace;

    InputConfig        input;
    OutputConfig       output;
    PlotConfig         plots;
    HomogeneityConfig  homogeneity;
    StatsConfig        stats;
    OutlierConfig      outlier;
    FilterConfig       filter;
    RegressionConfig   regression;
    StationarityConfig stationarity;
    ArimaConfig        arima;

    std::filesystem::path logDir;
    std::filesystem::path csvDir;
    std::filesystem::path imgDir;
    std::filesystem::path protocolsDir;
};

} // namespace loki