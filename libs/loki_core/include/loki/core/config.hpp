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

struct SnhtSection {
    int      nPermutations{999};
    uint64_t seed{0};
};

struct PeltSection {
    std::string penaltyType{"bic"};   ///< "bic" | "aic" | "mbic" | "fixed"
    double      fixedPenalty{0.0};
    int         minSegmentLength{2};
};

struct BocpdSection {
    double hazardLambda{250.0};
    double priorMean{0.0};
    double priorVar{1.0};
    double priorAlpha{1.0};
    double priorBeta{1.0};
    double threshold{0.5};
    int    minSegmentLength{30};
};

struct DetectionConfig {
    std::string  method{"yao_davis"};  ///< "yao_davis" | "snht" | "pelt" | "bocpd"
    int          minSegmentPoints{60};
    double       minSegmentSeconds{0.0};
    std::string  minSegmentDuration{""};
    double       significanceLevel{0.05};
    double       acfDependenceLimit{0.2};
    bool         correctForDependence{true};
    SnhtSection  snht{};
    PeltSection  pelt{};
    BocpdSection bocpd{};
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
    SAVITZKY_GOLAY, 
    SPLINE
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

struct SplineFilterConfig {
    int         subsampleStep{0};
    std::string bc{"not_a_knot"};
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
    SplineFilterConfig        spline{};
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

    // -- Clustering pipeline plots --------------------------------------------
    bool clusteringLabels     {true};  ///< Time axis coloured by cluster label.
    bool clusteringScatter    {true};  ///< 2-D feature scatter coloured by label (>= 2 features).
    bool clusteringSilhouette {true};  ///< Silhouette plot (k-means only).

    // -- Simulate pipeline plots ------------------------------------------------
    bool simulateOverlay       {true};
    bool simulateEnvelope      {true};
    bool simulateBootstrapDist {true};
    bool simulateAcfComparison {true};

    // -- EVT pipeline plots ---------------------------------------------------
    bool evtMeanExcess   {true};   ///< Mean excess plot with selected threshold.
    bool evtStability    {true};   ///< xi and sigma stability vs threshold.
    bool evtReturnLevels {true};   ///< Return level plot with CI band.
    bool evtExceedances  {true};   ///< Empirical CDF of exceedances vs GPD fit.
    bool evtGpdFit       {true};   ///< GPD Q-Q plot.

    // -- Kriging pipeline plots ---------------------------------------------------
    bool krigingVariogram   {true};   ///< Empirical variogram bins + fitted model curve.
    bool krigingPredictions {true};   ///< Original series + Kriging estimates + CI band.
    bool krigingCrossval    {true};   ///< LOO cross-validation errors + std error histogram.
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
    std::string                    gapFillStrategy   {"linear"};
    int                            gapFillMaxLength  {0};
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
//  ClusteringConfig  (sub-configs defined outside to avoid GCC 13 aggregate-init bug)
// -----------------------------------------------------------------------------

/**
 * @brief Feature vector selection for clustering.
 */
struct ClusteringFeatureConfig {
    bool value           {true};   ///< Use the series value as a feature.
    bool derivative      {false};  ///< Use signed first difference v[t] - v[t-1].
    bool absDerivative   {false};  ///< Use |v[t] - v[t-1]|.
    bool secondDerivative{false};  ///< Use second difference v[t] - 2*v[t-1] + v[t-2].
    bool slope           {false};  ///< Use OLS slope over a sliding window (slope_window samples).
    int  slopeWindow     {15};     ///< Window length in samples for OLS slope estimation.
};

/**
 * @brief k-means configuration for the clustering pipeline.
 */
struct KMeansClusteringConfig {
    int                      k     {0};          ///< Number of clusters. 0 = auto via silhouette.
    int                      kMin  {2};          ///< Lower bound for auto k selection.
    int                      kMax  {10};         ///< Upper bound for auto k selection.
    int                      maxIter{300};       ///< Maximum iterations per run.
    int                      nInit  {10};        ///< Number of random restarts.
    double                   tol    {1.0e-4};    ///< Centroid shift convergence criterion.
    std::string              init   {"kmeans++"}; ///< Initialisation: "kmeans++" | "random"
    std::vector<std::string> labels {};          ///< User-defined label names (empty = auto).
};

/**
 * @brief DBSCAN configuration for the clustering pipeline.
 */
struct DbscanClusteringConfig {
    double      eps    {0.0};         ///< Neighbourhood radius in z-score space. 0 = auto via k-NN elbow.
    int         minPts {5};           ///< Minimum points for a core point.
    std::string metric {"euclidean"}; ///< Distance metric: "euclidean" | "manhattan"
};

/**
 * @brief Outlier reporting settings for the clustering pipeline.
 *
 * When enabled, DBSCAN noise points (label = -1) are flagged as outliers
 * in the flags CSV and reported in the protocol.
 */
struct ClusteringOutlierConfig {
    bool enabled {false}; ///< Flag DBSCAN noise points as outliers in CSV output.
};

// ClusteringFeatureConfig, KMeansClusteringConfig, DbscanClusteringConfig,
// ClusteringOutlierConfig defined above (GCC 13 pattern).

/**
 * @brief Top-level configuration for the loki_clustering pipeline.
 */
struct ClusteringConfig {
    std::string              method          {"kmeans"}; ///< "kmeans" | "dbscan"
    std::string              gapFillStrategy {"linear"}; ///< Gap fill before feature extraction.
    int                      gapFillMaxLength{0};
    ClusteringFeatureConfig  features        {};
    KMeansClusteringConfig   kmeans          {};
    DbscanClusteringConfig   dbscan          {};
    ClusteringOutlierConfig  outlier         {};
    double                   significanceLevel{0.05};
};

// -----------------------------------------------------------------------------
//  SimulateConfig  (sub-configs defined outside to avoid GCC 13 aggregate-init bug)
// -----------------------------------------------------------------------------

/**
 * @brief Outlier injection settings for synthetic simulation.
 */
struct SimulateInjectOutliersConfig {
    bool   enabled   = false;
    double fraction  = 0.01;   ///< Fraction of samples to perturb.
    double magnitude = 5.0;    ///< Shift magnitude in units of sigma.
};

/**
 * @brief Gap injection settings for synthetic simulation.
 */
struct SimulateInjectGapsConfig {
    bool enabled   = false;
    int  nGaps     = 5;    ///< Number of gap regions to inject.
    int  maxLength = 10;   ///< Maximum gap length in samples.
};

/**
 * @brief Mean shift injection settings for synthetic simulation.
 */
struct SimulateInjectShiftsConfig {
    bool   enabled   = false;
    int    nShifts   = 2;    ///< Number of step changes to inject.
    double magnitude = 1.0;  ///< Shift magnitude (applied with random sign).
};

/**
 * @brief ARIMA/AR model parameters for synthetic generation.
 */
struct SimulateArimaConfig {
    int    p     = 1;    ///< AR order.
    int    d     = 0;    ///< Differencing order.
    int    q     = 0;    ///< MA order.
    double sigma = 1.0;  ///< Innovation standard deviation.
};

/**
 * @brief Kalman state-space model parameters for synthetic generation.
 */
struct SimulateKalmanConfig {
    std::string model = "local_level";  ///< "local_level"|"local_trend"|"constant_velocity"
    double      Q     = 0.001;          ///< Process noise variance.
    double      R     = 0.01;           ///< Observation noise variance.
};

/**
 * @brief Top-level configuration for the loki_simulate pipeline.
 */
struct SimulateConfig {
    std::string mode             = "synthetic";  ///< "synthetic" | "bootstrap"
    std::string model            = "arima";      ///< "arima" | "ar" | "kalman"
    int         n                = 1000;         ///< Series length to generate.
    uint64_t    seed             = 42;           ///< RNG seed (0 = random).
    int         nSimulations     = 100;          ///< Number of simulations / bootstrap replicas.
    std::string gapFillStrategy  = "linear";     ///< Gap fill before bootstrap fitting.
    int         gapFillMaxLength = 0;            ///< Max gap length to fill (0 = unlimited).
    std::string bootstrapMethod  = "block";      ///< "percentile" | "bca" | "block"
    double      confidenceLevel  = 0.95;         ///< CI confidence level.
    double      significanceLevel = 0.05;        ///< Significance level for diagnostics.

    SimulateArimaConfig          arima          {};
    SimulateKalmanConfig         kalman         {};
    SimulateInjectOutliersConfig injectOutliers {};
    SimulateInjectGapsConfig     injectGaps     {};
    SimulateInjectShiftsConfig   injectShifts   {};
};

// -----------------------------------------------------------------------------
//  EvtConfig  (sub-configs defined outside to avoid GCC 13 aggregate-init bug)
// -----------------------------------------------------------------------------

/**
 * @brief Deseasonalization settings for the EVT pipeline.
 *
 * When enabled, the seasonal component is subtracted before EVT analysis.
 * Return levels are then in units of residuals (deviations from seasonal mean).
 * strategy: "none" | "moving_average" | "median_year"
 */
struct EvtDeseasonalizationConfig {
    bool        enabled            = false;
    std::string strategy           = "median_year"; ///< "none" | "moving_average" | "median_year"
    int         maWindowSize       = 1461;          ///< MA window in samples (moving_average only).
    int         medianYearMinYears = 5;             ///< Min years for MedianYearSeries slot.
};

/**
 * @brief Threshold selection settings for the EVT pipeline.
 */
struct EvtThresholdConfig {
    bool        autoSelect     = true;
    std::string method         = "mean_excess"; ///< "mean_excess" | "manual"
    double      value          = 0.0;           ///< Manual threshold value.
    int         minExceedances = 30;            ///< Min exceedances per candidate.
    int         nCandidates    = 50;            ///< Grid size for auto selection.
};
 
/**
 * @brief Confidence interval settings for the EVT pipeline.
 */
struct EvtCiConfig {
    bool        enabled                 = true;
    std::string method                  = "profile_likelihood";
    ///< "profile_likelihood" | "delta" | "bootstrap"
    int         nBootstrap              = 1000;
    int         maxExceedancesBootstrap = 10000; ///< Subsample limit for bootstrap CI.
};
 
/**
 * @brief Block maxima settings for the EVT pipeline.
 */
struct EvtBlockMaximaConfig {
    int blockSize = 1461; ///< Samples per block (1461 = 1 year at 6-hour resolution).
};
 
// EvtThresholdConfig, EvtCiConfig, EvtBlockMaximaConfig defined above (GCC 13 pattern).
 
/**
 * @brief Top-level configuration for the loki_evt pipeline.
 */
struct EvtConfig {
    std::string          method            = "pot";
    ///< "pot" | "block_maxima" | "both"
    std::string          timeUnit          = "hours";
    ///< "seconds" | "minutes" | "hours" | "days" | "years"
    std::vector<double>  returnPeriods     = {10.0, 100.0, 1000.0, 1.0e6, 1.0e8};
    double               confidenceLevel   = 0.95;
    double               significanceLevel = 0.05;
    std::string          gapFillStrategy   = "linear";
    int                  gapFillMaxLength  = 0;
    EvtThresholdConfig   threshold         {};
    EvtCiConfig          ci                {};
    EvtBlockMaximaConfig blockMaxima       {};
    EvtDeseasonalizationConfig deseasonalization {};
};

// -----------------------------------------------------------------------------
//  KrigingConfig  (sub-configs defined outside to avoid GCC 13 aggregate-init bug)
// -----------------------------------------------------------------------------
 
/**
 * @brief Describes a single station entry for spatial Kriging.
 *
 * Used only when KrigingConfig::mode = "spatial" or "space_time".
 * Not used in temporal Kriging.
 *
 * PLACEHOLDER -- spatial Kriging is not yet implemented.
 * Spatial coordinates are expected in the same unit system as the data
 * (e.g. decimal degrees for GNSS networks, metres for local surveys).
 */
struct KrigingSpatialStation {
    std::string file = "";   ///< Input file path (relative to INPUT/).
    double      x    = 0.0; ///< Spatial coordinate X (e.g. longitude / easting).
    double      y    = 0.0; ///< Spatial coordinate Y (e.g. latitude  / northing).
};
 
/**
 * @brief Variogram model fitting configuration.
 *
 * The empirical semi-variance is computed from observation pairs binned
 * by lag distance (temporal: days; spatial: coordinate units).
 * A theoretical model is then fitted via weighted least squares (WLS),
 * with weights proportional to the number of pairs per bin.
 *
 * Supported theoretical models:
 *   "spherical"    -- bounded, most commonly used in practice
 *   "exponential"  -- unbounded sill approach, lighter tail than Gaussian
 *   "gaussian"     -- smooth parabolic behaviour near origin (differentiable)
 *   "power"        -- unbounded, suitable for intrinsic processes (no sill)
 *   "nugget"       -- pure nugget (no spatial correlation); rarely useful alone
 *
 * FUTURE: anisotropic variogram (directional gamma(h, theta)) for space-time.
 */
struct KrigingVariogramConfig {
    std::string model    = "spherical"; ///< Theoretical model name (see above).
    int         nLagBins = 20;          ///< Number of equally-spaced lag bins.
    double      maxLag   = 0.0;         ///< Max lag for binning. 0 = auto (half range).
    double      nugget   = 0.0;         ///< Initial nugget for WLS (0 = estimate from data).
    double      sill     = 0.0;         ///< Initial sill   for WLS (0 = estimate from data).
    double      range    = 0.0;         ///< Initial range  for WLS (0 = estimate from data).
};
 
/**
 * @brief Configuration for Kriging prediction at arbitrary target points.
 *
 * In temporal mode, targets are specified as MJD values or as a uniform
 * forecast horizon beyond the last observation.
 *
 * When prediction.enabled = false, only the observation grid is interpolated
 * (useful as a gap-fill alternative).
 *
 * FUTURE: in spatial mode, targets will be (x, y) coordinate pairs.
 */
struct KrigingPredictionConfig {
    bool                enabled     = false; ///< Enable prediction beyond observation grid.
    std::vector<double> targetMjd   = {};    ///< Explicit MJD targets (optional).
    double              horizonDays = 0.0;   ///< Forecast horizon in days (0 = disabled).
    int                 nSteps      = 10;    ///< Uniform steps within horizon.
};
 
/**
 * @brief Top-level configuration for the loki_kriging pipeline.
 *
 * Kriging mode:
 *   "temporal"   -- 1-D Kriging in time (h = |t_i - t_j| in MJD days).
 *                   Implemented in v1. Suitable for gap filling, interpolation,
 *                   and short-range prediction on a single station time series.
 *   "spatial"    -- 2-D Kriging over a network of stations (h = Euclidean dist).
 *                   PLACEHOLDER: structure prepared, not yet implemented.
 *   "space_time" -- Combined spatio-temporal Kriging.
 *                   PLACEHOLDER: design reserved for future implementation.
 *
 * Kriging method:
 *   "simple"    -- known constant mean (mu = knownMean).
 *   "ordinary"  -- unknown local mean, sum-of-weights = 1 constraint. DEFAULT.
 *   "universal" -- mean is a polynomial trend of degree trendDegree.
 *                  Drift functions are powers of time: {1, t, t^2, ...}.
 */
struct KrigingConfig {
    std::string mode              = "temporal";  ///< "temporal"|"spatial"|"space_time"
    std::string method            = "ordinary";  ///< "simple"|"ordinary"|"universal"
    std::string gapFillStrategy   = "linear";    ///< Pre-processing gap fill strategy.
    int         gapFillMaxLength  = 0;           ///< Max gap length (0 = unlimited).
    double      knownMean         = 0.0;         ///< Simple Kriging: assumed global mean.
    int         trendDegree       = 1;           ///< Universal Kriging: polynomial degree.
    bool        crossValidate     = true;        ///< Run leave-one-out cross-validation.
    double      confidenceLevel   = 0.95;        ///< CI confidence level.
    double      significanceLevel = 0.05;        ///< Significance level for diagnostics.
 
    KrigingVariogramConfig    variogram  {};
    KrigingPredictionConfig   prediction {};
 
    // -- Spatial mode (PLACEHOLDER -- not yet implemented) --------------------
    // These fields are parsed and stored but ignored in temporal mode.
    // Future spatial implementation will use them to load and register stations.
    std::vector<KrigingSpatialStation> stations {}; ///< Station list (spatial mode only).
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
    ClusteringConfig    clustering;
    SimulateConfig      simulate;
    EvtConfig           evt;
    KrigingConfig       kriging;

    std::filesystem::path logDir;
    std::filesystem::path csvDir;
    std::filesystem::path imgDir;
    std::filesystem::path protocolsDir;
};

} // namespace loki