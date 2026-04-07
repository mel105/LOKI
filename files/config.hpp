#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <map>

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
    bool arimaOverlay            {true};   ///< Deseasonalized residuals + ARIMA fitted overlay.
    bool arimaForecast          {false};   ///< Forecast plot with 95% prediction interval.
    bool arimaDiagnostics        {true};   ///< Four-panel residual diagnostics (via loki::Plot).

    // -- SSA pipeline plots ---------------------------------------------------
    bool ssaScree         {true};   ///< Eigenvalue scree plot with cumulative variance.
    bool ssaWCorr         {true};   ///< W-correlation matrix heatmap.
    bool ssaComponents    {true};   ///< First N elementary components vs time.
    bool ssaReconstruction{true};   ///< Original + group reconstructions overlay.

    // -- Decomposition pipeline plots -----------------------------------------
    bool decompOverlay     {true};   ///< Original series with trend overlaid.
    bool decompPanels      {true};   ///< 3-panel stacked: trend / seasonal / residual.
    bool decompDiagnostics {false};  ///< Residual diagnostics (ACF, histogram, QQ).

    // -- Spectral pipeline plots ----------------------------------------------
    bool spectralPsd         {true};   ///< PSD / periodogram plot (log-log or log-linear).
    bool spectralPeaks       {true};   ///< Annotated dominant peaks on PSD plot.
    bool spectralAmplitude   {false};  ///< Amplitude spectrum |X[k]| (linear scale).
    bool spectralPhase       {false};  ///< Phase spectrum atan2(Im,Re) in radians (FFT only).
    bool spectralSpectrogram {false};  ///< 2-D time-frequency heatmap (STFT).

    // -- Kalman pipeline plots ------------------------------------------------
    bool kalmanOverlay     {true};   ///< Original + filtered + (smoothed) + confidence band.
    bool kalmanInnovations {true};   ///< Innovations vs time with +/-2sigma envelope.
    bool kalmanGain        {false};  ///< Kalman gain K[t][0] vs time.
    bool kalmanUncertainty {false};  ///< Filter/smoother std dev sqrt(P[t]) vs time.
    bool kalmanForecast    {true};   ///< Forecast with prediction interval (if steps > 0).
    bool kalmanDiagnostics {false};  ///< 4-panel residual diagnostics (ACF, QQ, histogram).

    // -- QC pipeline plots ----------------------------------------------------
    bool qcCoverage  {true};   ///< Time-axis coverage plot (valid/gap/outlier colour bands).
    bool qcHistogram {true};   ///< Value histogram with normal fit overlay.
    bool qcAcf       {false};  ///< ACF of valid observations.
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
    int          forecastTail      {1461};
    double       confidenceLevel   {0.95};
    double       significanceLevel {0.05};
};

// -----------------------------------------------------------------------------
//  SsaConfig  (sub-configs defined outside to avoid GCC 13 aggregate-init bug)
// -----------------------------------------------------------------------------

struct SsaWindowConfig {
    int windowLength    {0};      ///< Explicit window length in samples. 0 = auto.
    int period          {0};      ///< Dominant period in samples (0 = unknown).
    int periodMultiplier{2};      ///< L = period * multiplier when auto and period > 0.
    int maxWindowLength {20000};  ///< Safety cap on auto-selected L (critical for high-rate data).
};

struct SsaGroupingConfig {
    std::string method            {"wcorr"}; ///< "manual" | "wcorr" | "kmeans" | "variance"
    double      wcorrThreshold    {0.8};     ///< W-corr threshold for hierarchical cut (wcorr method).
    int         kmeansK           {0};       ///< Number of clusters (0 = auto via silhouette).
    double      varianceThreshold {0.95};    ///< Cumulative variance kept (variance method).
    std::map<std::string, std::vector<int>> manualGroups; ///< Name -> eigentriple indices (manual method).
};

struct SsaReconstructionConfig {
    std::string method {"diagonal_averaging"}; ///< "diagonal_averaging" | "simple"
};

/**
 * @brief Top-level configuration for the loki_ssa pipeline.
 */
struct SsaConfig {
    std::string             gapFillStrategy    {"linear"};
    int                     gapFillMaxLength   {0};
    DeseasonalizationConfig deseasonalization  {};      ///< Usually "none" -- SSA handles seasonality.
    SsaWindowConfig         window             {};
    SsaGroupingConfig       grouping           {};
    SsaReconstructionConfig reconstruction     {};
    bool                    computeWCorr       {true};
    int                     svdRank            {40};   ///< Randomized SVD rank. 0 = full eigendecomposition.
    int                     svdOversampling    {10};   ///< Extra columns for randomized SVD accuracy.
    int                     svdPowerIter       {2};    ///< Subspace power iterations for randomized SVD.
    int                     wcorrMaxComponents {30};   ///< Max components used in w-corr matrix. 0 = all.
    double                  significanceLevel  {0.05};
    int                     forecastTail       {1461}; ///< Samples shown before forecast in plot.
};

// -----------------------------------------------------------------------------
//  DecompositionConfig  (sub-configs defined outside to avoid GCC 13 aggregate-init bug)
// -----------------------------------------------------------------------------

/// Decomposition method selection.
enum class DecompositionMethodEnum {
    CLASSICAL, ///< Moving-average trend + per-slot seasonal median/mean.
    STL        ///< Cleveland et al. (1990): iterative LOESS trend + seasonal.
};

/**
 * @brief Configuration for the classical decomposition path.
 */
struct ClassicalDecompositionConfig {
    std::string trendFilter  {"moving_average"}; ///< Trend estimator. "moving_average" only for now.
    std::string seasonalType {"median"};          ///< Slot aggregation: "median" | "mean".
};

/**
 * @brief Configuration for the STL decomposition path.
 */
struct StlDecompositionConfig {
    int    nInner     {2};    ///< Inner loop iterations (trend + seasonal passes per outer step).
    int    nOuter     {1};    ///< Outer (robustness) iterations. 0 = no robustness weighting.
    int    sDegree    {1};    ///< Local polynomial degree for seasonal LOESS smoother.
    int    tDegree    {1};    ///< Local polynomial degree for trend LOESS smoother.
    double sBandwidth {0.0};  ///< Seasonal LOESS bandwidth fraction. 0 = auto (1.5 / period).
    double tBandwidth {0.0};  ///< Trend LOESS bandwidth fraction. 0 = auto (1.5 * period / n).
};

/**
 * @brief Top-level configuration for the loki_decomposition pipeline.
 */
struct DecompositionConfig {
    std::string                  gapFillStrategy   {"linear"};
    int                          gapFillMaxLength  {0};
    DecompositionMethodEnum      method            {DecompositionMethodEnum::CLASSICAL};
    int                          period            {1461};  ///< Period in samples.
    ClassicalDecompositionConfig classical         {};
    StlDecompositionConfig       stl               {};
    double                       significanceLevel {0.05};
};

// -----------------------------------------------------------------------------
//  SpectralConfig  (sub-configs defined outside to avoid GCC 13 aggregate-init bug)
// -----------------------------------------------------------------------------

/**
 * @brief Configuration for the FFT / Welch PSD path.
 */
struct SpectralFftConfig {
    std::string windowFunction {"hann"};  ///< "hann" | "hamming" | "blackman" | "flattop" | "rectangular"
    bool        welch          {false};   ///< Enable Welch averaged PSD.
    int         welchSegments  {8};       ///< Number of overlapping segments for Welch method.
    double      welchOverlap   {0.5};     ///< Segment overlap fraction [0, 1).
};

/**
 * @brief Configuration for the Lomb-Scargle periodogram path.
 */
struct SpectralLombScargleConfig {
    double oversampling {4.0};    ///< Frequency grid oversampling factor.
    bool   fastNfft     {false};  ///< Use NFFT approximation for n >= 100k (Press & Rybicki 1989).
    double fapThreshold {0.01};   ///< FAP significance threshold for peak reporting.
};

/**
 * @brief Configuration for the STFT spectrogram.
 */
struct SpectralSpectrogramConfig {
    bool   enabled        {false};
    int    windowLength   {1461};  ///< STFT window in samples.
    double overlap        {0.5};   ///< Window overlap fraction [0, 1).
    double focusPeriodMin {0.0};   ///< Zoom lower bound in days (0 = no zoom).
    double focusPeriodMax {0.0};   ///< Zoom upper bound in days (0 = no zoom).
};

/**
 * @brief Configuration for peak detection and reporting.
 */
struct SpectralPeakConfig {
    int    topN          {10};   ///< Number of top peaks to report.
    double minPeriodDays {0.0};  ///< Ignore peaks below this period in days (0 = no limit).
    double maxPeriodDays {0.0};  ///< Ignore peaks above this period in days (0 = no limit).
};

/**
 * @brief Top-level configuration for the loki_spectral pipeline.
 */
struct SpectralConfig {
    std::string                gapFillStrategy  {"linear"};
    int                        gapFillMaxLength {0};
    std::string                method           {"auto"};  ///< "auto" | "fft" | "lomb_scargle"
    SpectralFftConfig          fft              {};
    SpectralLombScargleConfig  lombScargle      {};
    SpectralSpectrogramConfig  spectrogram      {};
    SpectralPeakConfig         peaks            {};
    double                     significanceLevel{0.05};
};

// -----------------------------------------------------------------------------
//  KalmanConfig  (sub-configs defined outside to avoid GCC 13 aggregate-init bug)
// -----------------------------------------------------------------------------

/**
 * @brief Noise covariance estimation settings for the Kalman filter.
 */
struct KalmanNoiseConfig {
    std::string estimation    {"manual"}; ///< "manual" | "heuristic" | "em"
    double      Q             {1.0};      ///< Process noise variance (manual mode).
    double      R             {1.0};      ///< Measurement noise variance (manual mode).
    double      smoothingFactor{10.0};   ///< Q = R / smoothingFactor (heuristic mode).
    double      QInit         {1.0};      ///< Initial Q for EM.
    double      RInit         {1.0};      ///< Initial R for EM.
    int         emMaxIter     {100};      ///< Maximum EM iterations.
    double      emTol         {1.0e-6};   ///< EM convergence tolerance (relative log-likelihood).
};

/**
 * @brief Forecast settings for the Kalman pipeline.
 */
struct KalmanForecastConfig {
    int    steps          {0};    ///< Prediction steps beyond last observation (0 = no forecast).
    double confidenceLevel{0.95}; ///< Confidence level for prediction interval.
};

// KalmanNoiseConfig and KalmanForecastConfig defined above (GCC 13 pattern).

/**
 * @brief Top-level configuration for the loki_kalman pipeline.
 */
struct KalmanConfig {
    std::string          gapFillStrategy  {"auto"};  ///< "auto"|"linear"|"median_year"|"none"
    int                  gapFillMaxLength {0};        ///< Max gap length to fill (0 = unlimited).
    std::string          model            {"local_level"}; ///< "local_level"|"local_trend"|"constant_velocity"
    KalmanNoiseConfig    noise            {};
    std::string          smoother         {"rts"};    ///< "none" | "rts"
    KalmanForecastConfig forecast         {};
    double               significanceLevel{0.05};
};

// -----------------------------------------------------------------------------
//  QcConfig  (sub-configs defined outside to avoid GCC 13 aggregate-init bug)
// -----------------------------------------------------------------------------

/**
 * @brief Per-method outlier detection settings for the QC pipeline.
 */
struct QcOutlierConfig {
    bool   iqrEnabled      {true};
    double iqrMultiplier   {1.5};
    bool   madEnabled      {true};
    double madMultiplier   {3.0};
    bool   zscoreEnabled   {true};
    double zscoreThreshold {3.0};
};

/**
 * @brief Seasonal feasibility settings for the QC pipeline.
 *
 * Controls whether MEDIAN_YEAR gap filling is recommended and whether
 * the seasonal consistency section runs. Auto-disabled for sub-hourly data.
 */
struct QcSeasonalConfig {
    bool   enabled           {true};  ///< Auto-disabled when median step > 3600 s.
    int    minYearsPerSlot   {5};     ///< Min valid years per MedianYearSeries slot.
    double minMonthCoverage  {0.5};   ///< Min fraction of months with data per year.
};

// QcOutlierConfig and QcSeasonalConfig defined above (GCC 13 pattern).

/**
 * @brief Top-level configuration for the loki_qc pipeline.
 */
struct QcConfig {
    bool             temporalEnabled     {true};   ///< Section 1: coverage analysis.
    bool             statsEnabled        {true};   ///< Section 2: descriptive statistics.
    bool             outlierEnabled      {true};   ///< Section 3: outlier detection.
    bool             samplingEnabled     {true};   ///< Section 4: sampling rate analysis.
    bool             seasonalEnabled     {true};   ///< Section 5: seasonal consistency (auto-disabled).
    bool             hurstEnabled        {true};   ///< Include Hurst exponent (slow for large n).
    int              maWindowSize        {365};    ///< MA window for deseasonalization before outlier detection.
    QcOutlierConfig  outlier             {};
    QcSeasonalConfig seasonal            {};
    double           significanceLevel   {0.05};
    double           uniformityThreshold {0.05};  ///< Fraction of non-uniform steps -> Lomb-Scargle recommended.
    double           minSpanYears        {10.0};  ///< Min span for MEDIAN_YEAR gap fill recommendation.
};

// -----------------------------------------------------------------------------
//  AppConfig
// -----------------------------------------------------------------------------

struct AppConfig {
    std::filesystem::path workspace;

    InputConfig         input;
    OutputConfig        output;
    PlotConfig          plots;
    HomogeneityConfig   homogeneity;
    StatsConfig         stats;
    OutlierConfig       outlier;
    FilterConfig        filter;
    RegressionConfig    regression;
    StationarityConfig  stationarity;
    ArimaConfig         arima;
    SsaConfig           ssa;
    DecompositionConfig decomposition;
    SpectralConfig      spectral;
    KalmanConfig        kalman;
    QcConfig            qc;

    std::filesystem::path logDir;
    std::filesystem::path csvDir;
    std::filesystem::path imgDir;
    std::filesystem::path protocolsDir;
};

} // namespace loki
