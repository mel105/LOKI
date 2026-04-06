#include "loki/core/configLoader.hpp"

#include "loki/core/exceptions.hpp"
#include "loki/core/logger.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>

using namespace loki;

namespace loki {

using json = nlohmann::json;

namespace {

template<typename T>
T getOrDefault(const json& j, const std::string& key, const T& defaultVal,
               bool warnIfMissing = true)
{
    if (!j.contains(key)) {
        if (warnIfMissing) LOKI_WARNING("Config key '" + key + "' not found -- using default.");
        return defaultVal;
    }
    return j.at(key).get<T>();
}

} // anonymous namespace

// -----------------------------------------------------------------------------
//  Public: load
// -----------------------------------------------------------------------------

AppConfig ConfigLoader::load(const std::filesystem::path& jsonPath)
{
    if (!std::filesystem::exists(jsonPath)) {
        throw FileNotFoundException(
            "ConfigLoader: config file not found: '" + jsonPath.string() + "'.");
    }

    std::ifstream ifs(jsonPath);
    if (!ifs.is_open()) {
        throw IoException(
            "ConfigLoader: cannot open config file: '" + jsonPath.string() + "'.");
    }

    json j;
    try { ifs >> j; }
    catch (const json::parse_error& e) {
        throw ParseException(
            std::string("ConfigLoader: JSON parse error in '")
            + jsonPath.string() + "': " + e.what());
    }

    LOKI_INFO("Loaded config: " + jsonPath.string());

    if (!j.contains("workspace")) {
        throw ConfigException(
            "ConfigLoader: required key 'workspace' is missing in '"
            + jsonPath.string() + "'.");
    }

    AppConfig cfg;
    cfg.workspace = std::filesystem::path(j.at("workspace").get<std::string>());

    if (!std::filesystem::exists(cfg.workspace)) {
        LOKI_WARNING("Workspace directory does not exist: '"
                     + cfg.workspace.string() + "' -- it will be created when needed.");
    }

    cfg.logDir       = cfg.workspace / "OUTPUT" / "LOG";
    cfg.csvDir       = cfg.workspace / "OUTPUT" / "CSV";
    cfg.imgDir       = cfg.workspace / "OUTPUT" / "IMG";
    cfg.protocolsDir = cfg.workspace / "OUTPUT" / "PROTOCOLS";

    const std::filesystem::path inputDir = cfg.workspace / "INPUT";

    cfg.input         = _parseInput        (j.value("input",        json::object()), inputDir);
    cfg.output        = _parseOutput       (j.value("output",       json::object()));
    cfg.homogeneity   = _parseHomogeneity  (j.value("homogeneity",  json::object()));
    cfg.outlier       = _parseOutlier      (j.value("outlier",      json::object()));
    cfg.filter        = _parseFilter       (j.value("filter",       json::object()));
    cfg.regression    = _parseRegression   (j.value("regression",   json::object()));
    cfg.plots         = _parsePlots        (j.value("plots",        json::object()));
    cfg.stats         = _parseStats        (j);
    cfg.stationarity  = _parseStationarity (j.value("stationarity", json::object()));
    cfg.arima         = _parseArima        (j.value("arima",        json::object()));
    cfg.ssa           = _parseSsa          (j.value("ssa",          json::object()));
    cfg.decomposition = _parseDecomposition(j.value("decomposition",json::object()));
    cfg.spectral      = _parseSpectral     (j.value("spectral",     json::object()));
    cfg.kalman        = _parseKalman       (j.value("kalman",       json::object()));

    return cfg;
}

// -----------------------------------------------------------------------------
//  Private: _parseInput
// -----------------------------------------------------------------------------

InputConfig ConfigLoader::_parseInput(const nlohmann::json& j,
                                      const std::filesystem::path& inputDir)
{
    InputConfig cfg;

    const bool hasSingle = j.contains("file");
    const bool hasList   = j.contains("files");
    const bool hasScan   = j.contains("scan_directory") &&
                           j.at("scan_directory").get<bool>();

    if (hasSingle) {
        cfg.mode = InputMode::SINGLE_FILE;
        cfg.file = _resolvePath(j.at("file").get<std::string>(), inputDir);
    } else if (hasList) {
        cfg.mode = InputMode::FILE_LIST;
        for (const auto& f : j.at("files"))
            cfg.files.push_back(_resolvePath(f.get<std::string>(), inputDir));
    } else if (hasScan) {
        cfg.mode    = InputMode::SCAN_DIRECTORY;
        cfg.scanDir = inputDir;
    } else {
        LOKI_WARNING("Config 'input': no 'file', 'files', or 'scan_directory' found -- "
                     "defaulting to SCAN_DIRECTORY on INPUT/.");
        cfg.mode    = InputMode::SCAN_DIRECTORY;
        cfg.scanDir = inputDir;
    }

    cfg.timeFormat  = _parseTimeFormat(getOrDefault<std::string>(j, "time_format", "gpst_seconds"));
    const std::string delimStr   = getOrDefault<std::string>(j, "delimiter",    ";");
    const std::string commentStr = getOrDefault<std::string>(j, "comment_char", "%");
    cfg.delimiter   = delimStr.empty()   ? ';' : delimStr.front();
    cfg.commentChar = commentStr.empty() ? '%' : commentStr.front();

    if (j.contains("columns")) {
        cfg.columns = j.at("columns").get<std::vector<int>>();
        cfg.columns.erase(
            std::remove_if(cfg.columns.begin(), cfg.columns.end(),
                           [](int c) { return c <= 1; }),
            cfg.columns.end());
    } else {
        LOKI_WARNING("Config 'input.columns' not specified -- all value columns will be loaded.");
    }

    if (j.contains("time_columns"))
        cfg.timeColumns = j.at("time_columns").get<std::vector<int>>();

    cfg.mergeStrategy = _parseMergeStrategy(getOrDefault<std::string>(j, "merge_strategy", "separate"));

    return cfg;
}

// -----------------------------------------------------------------------------
//  Private: _parseOutput
// -----------------------------------------------------------------------------

OutputConfig ConfigLoader::_parseOutput(const nlohmann::json& j)
{
    OutputConfig cfg;
    const std::string levelStr = getOrDefault<std::string>(j, "log_level", "info", false);
    if      (levelStr == "debug")   { cfg.logLevel = LogLevel::DEBUG;   }
    else if (levelStr == "info")    { cfg.logLevel = LogLevel::INFO;    }
    else if (levelStr == "warning") { cfg.logLevel = LogLevel::WARNING; }
    else if (levelStr == "error")   { cfg.logLevel = LogLevel::ERROR;   }
    else {
        LOKI_WARNING("Config 'output.log_level' unknown value '" + levelStr + "' -- using 'info'.");
        cfg.logLevel = LogLevel::INFO;
    }
    return cfg;
}

// -----------------------------------------------------------------------------
//  Private: _parseOutlierFilter
// -----------------------------------------------------------------------------

OutlierFilterConfig ConfigLoader::_parseOutlierFilter(const nlohmann::json& j,
                                                      double defaultMadMultiplier)
{
    OutlierFilterConfig cfg;
    cfg.enabled             = getOrDefault<bool>       (j, "enabled",              false,                false);
    cfg.method              = getOrDefault<std::string>(j, "method",               "mad_bounds",         false);
    cfg.madMultiplier       = getOrDefault<double>     (j, "mad_multiplier",       defaultMadMultiplier, false);
    cfg.iqrMultiplier       = getOrDefault<double>     (j, "iqr_multiplier",       1.5,                  false);
    cfg.zscoreThreshold     = getOrDefault<double>     (j, "zscore_threshold",     3.0,                  false);
    cfg.replacementStrategy = getOrDefault<std::string>(j, "replacement_strategy", "linear",             false);
    cfg.maxFillLength       = getOrDefault<int>        (j, "max_fill_length",      0,                    false);
    return cfg;
}

// -----------------------------------------------------------------------------
//  Private: _parseHomogeneity
// -----------------------------------------------------------------------------

HomogeneityConfig ConfigLoader::_parseHomogeneity(const nlohmann::json& j)
{
    HomogeneityConfig cfg;

    cfg.applyGapFilling  = getOrDefault<bool>(j, "apply_gap_filling",  true,  false);
    cfg.applyAdjustment  = getOrDefault<bool>(j, "apply_adjustment",   true,  false);

    if (j.contains("gap_filling")) {
        const auto& gf = j.at("gap_filling");
        cfg.gapFilling.strategy           = getOrDefault<std::string>(gf, "strategy",             "linear", false);
        cfg.gapFilling.maxFillLength      = getOrDefault<int>        (gf, "max_fill_length",      0,        false);
        cfg.gapFilling.gapThresholdFactor = getOrDefault<double>     (gf, "gap_threshold_factor", 1.5,      false);
        cfg.gapFilling.minSeriesYears     = getOrDefault<int>        (gf, "min_series_years",     10,       false);
    }

    if (j.contains("pre_outlier"))
        cfg.preOutlier = _parseOutlierFilter(j.at("pre_outlier"), 5.0);

    if (j.contains("deseasonalization")) {
        const auto& ds = j.at("deseasonalization");
        cfg.deseasonalization.strategy           = getOrDefault<std::string>(ds, "strategy",              "median_year", false);
        cfg.deseasonalization.maWindowSize        = getOrDefault<int>        (ds, "ma_window_size",        365,           false);
        cfg.deseasonalization.medianYearMinYears  = getOrDefault<int>        (ds, "median_year_min_years", 5,             false);
    }

    if (j.contains("post_outlier"))
        cfg.postOutlier = _parseOutlierFilter(j.at("post_outlier"), 3.0);

    if (j.contains("detection")) {
        const auto& d = j.at("detection");
        cfg.detection.significanceLevel    = getOrDefault<double>     (d, "significance_level",     0.05,  false);
        cfg.detection.acfDependenceLimit   = getOrDefault<double>     (d, "acf_dependence_limit",   0.2,   false);
        cfg.detection.correctForDependence = getOrDefault<bool>       (d, "correct_for_dependence", true,  false);
        cfg.detection.minSegmentPoints     = getOrDefault<int>        (d, "min_segment_points",     60,    false);
        cfg.detection.minSegmentDuration   = getOrDefault<std::string>(d, "min_segment_duration",   "",    false);
    }

    return cfg;
}

// -----------------------------------------------------------------------------
//  Private: _parseOutlier
// -----------------------------------------------------------------------------

OutlierConfig ConfigLoader::_parseOutlier(const nlohmann::json& j)
{
    OutlierConfig cfg;

    if (j.contains("deseasonalization")) {
        const auto& ds = j.at("deseasonalization");
        cfg.deseasonalization.strategy          = getOrDefault<std::string>(ds, "strategy",              "none", false);
        cfg.deseasonalization.maWindowSize       = getOrDefault<int>        (ds, "ma_window_size",        365,    false);
        cfg.deseasonalization.medianYearMinYears = getOrDefault<int>        (ds, "median_year_min_years", 5,      false);
    }

    if (j.contains("detection")) {
        const auto& d = j.at("detection");
        cfg.detection.method          = getOrDefault<std::string>(d, "method",           "mad", false);
        cfg.detection.iqrMultiplier   = getOrDefault<double>     (d, "iqr_multiplier",   1.5,   false);
        cfg.detection.madMultiplier   = getOrDefault<double>     (d, "mad_multiplier",   3.0,   false);
        cfg.detection.zscoreThreshold = getOrDefault<double>     (d, "zscore_threshold", 3.0,   false);
    }

    if (j.contains("replacement")) {
        const auto& r = j.at("replacement");
        cfg.replacement.strategy      = getOrDefault<std::string>(r, "strategy",        "linear", false);
        cfg.replacement.maxFillLength = getOrDefault<int>        (r, "max_fill_length", 0,        false);
    }

    if (j.contains("hat_matrix")) {
        const auto& hm = j.at("hat_matrix");
        cfg.hatMatrix.arOrder           = getOrDefault<int>   (hm, "ar_order",           5,    false);
        cfg.hatMatrix.significanceLevel = getOrDefault<double>(hm, "significance_level",  0.05, false);
        cfg.hatMatrix.enabled           = getOrDefault<bool>  (hm, "enabled",             true, false);

        if (cfg.hatMatrix.arOrder < 1) {
            throw ConfigException(
                "ConfigLoader: outlier.hat_matrix.ar_order must be >= 1, got "
                + std::to_string(cfg.hatMatrix.arOrder) + ".");
        }
        if (cfg.hatMatrix.significanceLevel <= 0.0 || cfg.hatMatrix.significanceLevel >= 1.0) {
            throw ConfigException(
                "ConfigLoader: outlier.hat_matrix.significance_level must be in (0, 1), got "
                + std::to_string(cfg.hatMatrix.significanceLevel) + ".");
        }
    }

    return cfg;
}

// -----------------------------------------------------------------------------
//  Private: _parseFilter
// -----------------------------------------------------------------------------

FilterConfig ConfigLoader::_parseFilter(const nlohmann::json& j)
{
    FilterConfig cfg;

    if (j.contains("gap_filling")) {
        const auto& gf = j.at("gap_filling");
        cfg.gapFilling.strategy      = getOrDefault<std::string>(gf, "strategy",        "linear", false);
        cfg.gapFilling.maxFillLength = getOrDefault<int>        (gf, "max_fill_length", 0,        false);
    }

    const std::string method = getOrDefault<std::string>(j, "method", "kernel", false);
    if      (method == "moving_average")  cfg.method = FilterMethodEnum::MOVING_AVERAGE;
    else if (method == "ema")             cfg.method = FilterMethodEnum::EMA;
    else if (method == "weighted_ma")     cfg.method = FilterMethodEnum::WEIGHTED_MA;
    else if (method == "kernel")          cfg.method = FilterMethodEnum::KERNEL;
    else if (method == "loess")           cfg.method = FilterMethodEnum::LOESS;
    else if (method == "savitzky_golay")  cfg.method = FilterMethodEnum::SAVITZKY_GOLAY;
    else {
        LOKI_WARNING("Config 'filter.method' unknown value '" + method + "' -- using 'kernel'.");
        cfg.method = FilterMethodEnum::KERNEL;
    }

    cfg.autoWindowMethod = getOrDefault<std::string>(j, "auto_window_method", "silverman_mad", false);

    if (j.contains("moving_average")) {
        const auto& ma = j.at("moving_average");
        cfg.movingAverage.window = getOrDefault<int>(ma, "window", 365, false);
    }

    if (j.contains("ema")) {
        const auto& ema = j.at("ema");
        cfg.ema.alpha = getOrDefault<double>(ema, "alpha", 0.1, false);
        if (cfg.ema.alpha <= 0.0 || cfg.ema.alpha > 1.0) {
            throw ConfigException(
                "ConfigLoader: filter.ema.alpha must be in (0, 1], got "
                + std::to_string(cfg.ema.alpha) + ".");
        }
    }

    if (j.contains("weighted_ma")) {
        const auto& wma = j.at("weighted_ma");
        if (wma.contains("weights")) {
            cfg.weightedMa.weights = wma.at("weights").get<std::vector<double>>();
            if (cfg.weightedMa.weights.empty()) {
                throw ConfigException(
                    "ConfigLoader: filter.weighted_ma.weights must not be empty.");
            }
        }
    }

    if (j.contains("kernel")) {
        const auto& k = j.at("kernel");
        cfg.kernel.bandwidth      = getOrDefault<double>     (k, "bandwidth",       0.1,            false);
        cfg.kernel.kernelType     = getOrDefault<std::string>(k, "kernel_type",     "epanechnikov", false);
        cfg.kernel.gaussianCutoff = getOrDefault<double>     (k, "gaussian_cutoff", 3.0,            false);
    }

    if (j.contains("loess")) {
        const auto& lo = j.at("loess");
        cfg.loess.bandwidth        = getOrDefault<double>     (lo, "bandwidth",         0.25,      false);
        cfg.loess.degree           = getOrDefault<int>        (lo, "degree",            1,         false);
        cfg.loess.kernelType       = getOrDefault<std::string>(lo, "kernel_type",       "tricube", false);
        cfg.loess.robust           = getOrDefault<bool>       (lo, "robust",            false,     false);
        cfg.loess.robustIterations = getOrDefault<int>        (lo, "robust_iterations", 3,         false);
        if (cfg.loess.degree != 1 && cfg.loess.degree != 2) {
            throw ConfigException(
                "ConfigLoader: filter.loess.degree must be 1 or 2, got "
                + std::to_string(cfg.loess.degree) + ".");
        }
    }

    if (j.contains("savitzky_golay")) {
        const auto& sg = j.at("savitzky_golay");
        cfg.savitzkyGolay.window = getOrDefault<int>(sg, "window", 11, false);
        cfg.savitzkyGolay.degree = getOrDefault<int>(sg, "degree",  2, false);
    }

    return cfg;
}

// -----------------------------------------------------------------------------
//  Private: _parseRegression
// -----------------------------------------------------------------------------

RegressionConfig ConfigLoader::_parseRegression(const nlohmann::json& j)
{
    RegressionConfig cfg;

    const std::string method = getOrDefault<std::string>(j, "method", "linear", false);
    if      (method == "linear")      cfg.method = RegressionMethodEnum::LINEAR;
    else if (method == "polynomial")  cfg.method = RegressionMethodEnum::POLYNOMIAL;
    else if (method == "harmonic")    cfg.method = RegressionMethodEnum::HARMONIC;
    else if (method == "trend")       cfg.method = RegressionMethodEnum::TREND;
    else if (method == "robust")      cfg.method = RegressionMethodEnum::ROBUST;
    else if (method == "calibration") cfg.method = RegressionMethodEnum::CALIBRATION;
    else if (method == "nonlinear")   cfg.method = RegressionMethodEnum::NONLINEAR;
    else {
        LOKI_WARNING("Config 'regression.method' unknown value '" + method + "' -- using 'linear'.");
        cfg.method = RegressionMethodEnum::LINEAR;
    }

    cfg.polynomialDegree  = getOrDefault<int>        (j, "polynomial_degree",  1,          false);
    cfg.harmonicTerms     = getOrDefault<int>        (j, "harmonic_terms",     2,          false);
    cfg.period            = getOrDefault<double>     (j, "period",             365.25,     false);
    cfg.robust            = getOrDefault<bool>       (j, "robust",             false,      false);
    cfg.robustIterations  = getOrDefault<int>        (j, "robust_iterations",  10,         false);
    cfg.robustWeightFn    = getOrDefault<std::string>(j, "robust_weight_fn",   "bisquare", false);
    cfg.computePrediction = getOrDefault<bool>       (j, "compute_prediction", false,      false);
    cfg.predictionHorizon = getOrDefault<double>     (j, "prediction_horizon", 0.0,        false);
    cfg.confidenceLevel   = getOrDefault<double>     (j, "confidence_level",   0.95,       false);
    cfg.significanceLevel = getOrDefault<double>     (j, "significance_level", 0.05,       false);
    cfg.cvFolds           = getOrDefault<int>        (j, "cv_folds",           10,         false);

    if (j.contains("gap_filling")) {
        const auto& gf = j.at("gap_filling");
        cfg.gapFilling.strategy      = getOrDefault<std::string>(gf, "strategy",        "linear", false);
        cfg.gapFilling.maxFillLength = getOrDefault<int>        (gf, "max_fill_length", 0,        false);
    }

    if (cfg.polynomialDegree < 1) {
        throw ConfigException(
            "ConfigLoader: regression.polynomial_degree must be >= 1, got "
            + std::to_string(cfg.polynomialDegree) + ".");
    }
    if (cfg.harmonicTerms < 1) {
        throw ConfigException(
            "ConfigLoader: regression.harmonic_terms must be >= 1, got "
            + std::to_string(cfg.harmonicTerms) + ".");
    }
    if (cfg.confidenceLevel <= 0.0 || cfg.confidenceLevel >= 1.0) {
        throw ConfigException(
            "ConfigLoader: regression.confidence_level must be in (0, 1), got "
            + std::to_string(cfg.confidenceLevel) + ".");
    }

    if (j.contains("nonlinear")) {
        const auto& nl = j.at("nonlinear");
        const std::string nlModel = getOrDefault<std::string>(nl, "model", "exponential", false);
        if      (nlModel == "exponential") cfg.nonlinear.model = NonlinearModelEnum::EXPONENTIAL;
        else if (nlModel == "logistic")    cfg.nonlinear.model = NonlinearModelEnum::LOGISTIC;
        else if (nlModel == "gaussian")    cfg.nonlinear.model = NonlinearModelEnum::GAUSSIAN;
        else {
            LOKI_WARNING("Config 'regression.nonlinear.model' unknown value '"
                         + nlModel + "' -- using 'exponential'.");
            cfg.nonlinear.model = NonlinearModelEnum::EXPONENTIAL;
        }

        if (nl.contains("initial_params"))
            cfg.nonlinear.initialParams = nl.at("initial_params").get<std::vector<double>>();

        cfg.nonlinear.maxIterations   = getOrDefault<int>   (nl, "max_iterations",   100,    false);
        cfg.nonlinear.gradTol         = getOrDefault<double>(nl, "grad_tol",         1.0e-8, false);
        cfg.nonlinear.stepTol         = getOrDefault<double>(nl, "step_tol",         1.0e-8, false);
        cfg.nonlinear.lambdaInit      = getOrDefault<double>(nl, "lambda_init",      1.0e-3, false);
        cfg.nonlinear.lambdaFactor    = getOrDefault<double>(nl, "lambda_factor",    10.0,   false);
        cfg.nonlinear.confidenceLevel = getOrDefault<double>(nl, "confidence_level", 0.95,   false);
    }

    return cfg;
}

// -----------------------------------------------------------------------------
//  Private: _parsePlots
// -----------------------------------------------------------------------------

PlotConfig ConfigLoader::_parsePlots(const nlohmann::json& j)
{
    PlotConfig cfg;

    if (j.contains("output_format")) cfg.outputFormat = j["output_format"].get<std::string>();
    if (j.contains("time_format"))   cfg.timeFormat   = j["time_format"].get<std::string>();

    if (j.contains("plot_options")) {
        const auto& po = j["plot_options"];
        if (po.contains("acf_max_lag"))    cfg.options.acfMaxLag     = po["acf_max_lag"].get<int>();
        if (po.contains("histogram_bins")) cfg.options.histogramBins = po["histogram_bins"].get<int>();
    }

    if (j.contains("enabled")) {
        const auto& e = j["enabled"];

        // Generic
        if (e.contains("time_series"))  cfg.timeSeries  = e["time_series"].get<bool>();
        if (e.contains("comparison"))   cfg.comparison  = e["comparison"].get<bool>();
        if (e.contains("histogram"))    cfg.histogram   = e["histogram"].get<bool>();
        if (e.contains("acf"))          cfg.acf         = e["acf"].get<bool>();
        if (e.contains("pacf_plot"))    cfg.pacfPlot    = e["pacf_plot"].get<bool>();
        if (e.contains("qq_plot"))      cfg.qqPlot      = e["qq_plot"].get<bool>();
        if (e.contains("boxplot"))      cfg.boxplot     = e["boxplot"].get<bool>();

        // Outlier pipeline
        if (e.contains("original_series"))       cfg.originalSeries      = e["original_series"].get<bool>();
        if (e.contains("adjusted_series"))       cfg.adjustedSeries      = e["adjusted_series"].get<bool>();
        if (e.contains("homog_comparison"))      cfg.homogComparison     = e["homog_comparison"].get<bool>();
        if (e.contains("deseasonalized"))        cfg.deseasonalized      = e["deseasonalized"].get<bool>();
        if (e.contains("seasonal_overlay"))      cfg.seasonalOverlay     = e["seasonal_overlay"].get<bool>();
        if (e.contains("residuals_with_bounds")) cfg.residualsWithBounds = e["residuals_with_bounds"].get<bool>();
        if (e.contains("outlier_overlay"))       cfg.outlierOverlay      = e["outlier_overlay"].get<bool>();
        if (e.contains("leverage_plot"))         cfg.leveragePlot        = e["leverage_plot"].get<bool>();

        // Homogeneity pipeline
        if (e.contains("change_points"))    cfg.changePoints    = e["change_points"].get<bool>();
        if (e.contains("shift_magnitudes")) cfg.shiftMagnitudes = e["shift_magnitudes"].get<bool>();
        if (e.contains("correction_curve")) cfg.correctionCurve = e["correction_curve"].get<bool>();

        // Filter pipeline
        if (e.contains("filter_overlay"))            cfg.filterOverlay          = e["filter_overlay"].get<bool>();
        if (e.contains("filter_overlay_residuals"))  cfg.filterOverlayResiduals = e["filter_overlay_residuals"].get<bool>();
        if (e.contains("filter_residuals"))          cfg.filterResiduals        = e["filter_residuals"].get<bool>();
        if (e.contains("filter_residuals_acf"))      cfg.filterResidualsAcf     = e["filter_residuals_acf"].get<bool>();
        if (e.contains("residuals_histogram"))       cfg.residualsHistogram     = e["residuals_histogram"].get<bool>();
        if (e.contains("residuals_qq"))              cfg.residualsQq            = e["residuals_qq"].get<bool>();

        // Regression pipeline
        if (e.contains("regression_overlay"))       cfg.regressionOverlay      = e["regression_overlay"].get<bool>();
        if (e.contains("regression_residuals"))     cfg.regressionResiduals    = e["regression_residuals"].get<bool>();
        if (e.contains("regression_cdf_plot"))      cfg.regressionCdfPlot      = e["regression_cdf_plot"].get<bool>();
        if (e.contains("regression_qq_bands"))      cfg.regressionQqBands      = e["regression_qq_bands"].get<bool>();
        if (e.contains("regression_residual_acf"))  cfg.regressionResidualAcf  = e["regression_residual_acf"].get<bool>();
        if (e.contains("regression_residual_hist")) cfg.regressionResidualHist = e["regression_residual_hist"].get<bool>();
        if (e.contains("regression_influence"))     cfg.regressionInfluence    = e["regression_influence"].get<bool>();
        if (e.contains("regression_leverage"))      cfg.regressionLeverage     = e["regression_leverage"].get<bool>();

        // Arima pipeline
        if (e.contains("arima_overlay"))     cfg.arimaOverlay     = e["arima_overlay"].get<bool>();
        if (e.contains("arima_forecast"))    cfg.arimaForecast    = e["arima_forecast"].get<bool>();
        if (e.contains("arima_diagnostics")) cfg.arimaDiagnostics = e["arima_diagnostics"].get<bool>();

        // SSA pipeline
        if (e.contains("ssa_scree"))          cfg.ssaScree          = e["ssa_scree"].get<bool>();
        if (e.contains("ssa_wcorr"))          cfg.ssaWCorr          = e["ssa_wcorr"].get<bool>();
        if (e.contains("ssa_components"))     cfg.ssaComponents     = e["ssa_components"].get<bool>();
        if (e.contains("ssa_reconstruction")) cfg.ssaReconstruction = e["ssa_reconstruction"].get<bool>();

        // Decomposition pipeline
        if (e.contains("decomp_overlay"))     cfg.decompOverlay     = e["decomp_overlay"].get<bool>();
        if (e.contains("decomp_panels"))      cfg.decompPanels      = e["decomp_panels"].get<bool>();
        if (e.contains("decomp_diagnostics")) cfg.decompDiagnostics = e["decomp_diagnostics"].get<bool>();

        // Spectral pipeline (readable from "enabled" block)
        if (e.contains("spectral_psd"))         cfg.spectralPsd         = e["spectral_psd"].get<bool>();
        if (e.contains("spectral_peaks"))       cfg.spectralPeaks       = e["spectral_peaks"].get<bool>();
        if (e.contains("spectral_spectrogram")) cfg.spectralSpectrogram = e["spectral_spectrogram"].get<bool>();
    }

    // Spectral flags at top level of "plots" block (used by spectral.json)
    if (j.contains("spectral_psd"))         cfg.spectralPsd         = j["spectral_psd"].get<bool>();
    if (j.contains("spectral_peaks"))       cfg.spectralPeaks       = j["spectral_peaks"].get<bool>();
    if (j.contains("spectral_amplitude"))   cfg.spectralAmplitude   = j["spectral_amplitude"].get<bool>();
    if (j.contains("spectral_phase"))       cfg.spectralPhase       = j["spectral_phase"].get<bool>();
    if (j.contains("spectral_spectrogram")) cfg.spectralSpectrogram = j["spectral_spectrogram"].get<bool>();

    // Kalman flags at top level of "plots" block
    if (j.contains("kalman_overlay"))      cfg.kalmanOverlay      = j["kalman_overlay"].get<bool>();
    if (j.contains("kalman_innovations"))  cfg.kalmanInnovations  = j["kalman_innovations"].get<bool>();
    if (j.contains("kalman_gain"))         cfg.kalmanGain         = j["kalman_gain"].get<bool>();
    if (j.contains("kalman_uncertainty"))  cfg.kalmanUncertainty  = j["kalman_uncertainty"].get<bool>();
    if (j.contains("kalman_forecast"))     cfg.kalmanForecast     = j["kalman_forecast"].get<bool>();
    if (j.contains("kalman_diagnostics"))  cfg.kalmanDiagnostics  = j["kalman_diagnostics"].get<bool>();

    return cfg;
}

// -----------------------------------------------------------------------------
//  Private: _parseStats
// -----------------------------------------------------------------------------

StatsConfig ConfigLoader::_parseStats(const nlohmann::json& j)
{
    StatsConfig cfg;
    if (!j.contains("stats")) return cfg;

    const auto& s = j.at("stats");
    if (s.contains("enabled")) cfg.enabled = s.at("enabled").get<bool>();
    if (s.contains("hurst"))   cfg.hurst   = s.at("hurst").get<bool>();

    if (s.contains("nan_policy")) {
        const std::string p = s.at("nan_policy").get<std::string>();
        if      (p == "skip")      { cfg.nanPolicy = loki::NanPolicy::SKIP;      }
        else if (p == "throw")     { cfg.nanPolicy = loki::NanPolicy::THROW;     }
        else if (p == "propagate") { cfg.nanPolicy = loki::NanPolicy::PROPAGATE; }
        else {
            throw ConfigException(
                "configLoader: unknown nan_policy value '" + p +
                "'. Expected: skip | throw | propagate.");
        }
    }
    return cfg;
}

// -----------------------------------------------------------------------------
//  Private: enum parsers
// -----------------------------------------------------------------------------

TimeFormat ConfigLoader::_parseTimeFormat(const std::string& s)
{
    if (s == "index")         return TimeFormat::INDEX;
    if (s == "gpst_seconds")  return TimeFormat::GPS_TOTAL_SECONDS;
    if (s == "gpst_week_sow") return TimeFormat::GPS_WEEK_SOW;
    if (s == "utc")           return TimeFormat::UTC;
    if (s == "mjd")           return TimeFormat::MJD;
    if (s == "unix")          return TimeFormat::UNIX;
    LOKI_WARNING("Config 'input.time_format' unknown value '" + s + "' -- using 'gpst_seconds'.");
    return TimeFormat::GPS_TOTAL_SECONDS;
}

MergeStrategy ConfigLoader::_parseMergeStrategy(const std::string& s)
{
    if (s == "separate") return MergeStrategy::SEPARATE;
    if (s == "merge")    return MergeStrategy::MERGE;
    LOKI_WARNING("Config 'input.merge_strategy' unknown value '" + s + "' -- using 'separate'.");
    return MergeStrategy::SEPARATE;
}

std::filesystem::path ConfigLoader::_resolvePath(const std::string& raw,
                                                  const std::filesystem::path& baseDir)
{
    const std::filesystem::path p(raw);
    return p.is_absolute() ? p : baseDir / p;
}

// -----------------------------------------------------------------------------
//  Private: _parseStationarity
// -----------------------------------------------------------------------------

StationarityConfig ConfigLoader::_parseStationarity(const nlohmann::json& j)
{
    StationarityConfig cfg;

    if (j.contains("deseasonalization")) {
        const auto& ds = j.at("deseasonalization");
        cfg.deseasonalization.strategy          = getOrDefault<std::string>(ds, "strategy",              "median_year", false);
        cfg.deseasonalization.maWindowSize       = getOrDefault<int>        (ds, "ma_window_size",        1461,          false);
        cfg.deseasonalization.medianYearMinYears = getOrDefault<int>        (ds, "median_year_min_years", 5,             false);
    }

    if (j.contains("differencing")) {
        const auto& d = j.at("differencing");
        cfg.differencing.apply = getOrDefault<bool>(d, "apply", false, false);
        cfg.differencing.order = getOrDefault<int> (d, "order", 1,     false);
        if (cfg.differencing.order < 1 || cfg.differencing.order > 2) {
            throw ConfigException(
                "ConfigLoader: stationarity.differencing.order must be 1 or 2, got "
                + std::to_string(cfg.differencing.order) + ".");
        }
    }

    if (j.contains("tests")) {
        const auto& t = j.at("tests");

        if (t.contains("adf")) {
            const auto& a = t.at("adf");
            cfg.tests.adfEnabled             = getOrDefault<bool>        (a, "enabled",            true,       false);
            cfg.tests.adf.trendType          = getOrDefault<std::string> (a, "trend_type",          "constant", false);
            cfg.tests.adf.lagSelection       = getOrDefault<std::string> (a, "lag_selection",       "aic",      false);
            cfg.tests.adf.maxLags            = getOrDefault<int>         (a, "max_lags",            -1,         false);
            cfg.tests.adf.significanceLevel  = getOrDefault<double>      (a, "significance_level",  0.05,       false);

            const std::string& tt = cfg.tests.adf.trendType;
            if (tt != "none" && tt != "constant" && tt != "trend") {
                throw ConfigException(
                    "ConfigLoader: stationarity.tests.adf.trend_type must be "
                    "'none', 'constant', or 'trend', got '" + tt + "'.");
            }
            const std::string& ls = cfg.tests.adf.lagSelection;
            if (ls != "aic" && ls != "bic" && ls != "fixed") {
                throw ConfigException(
                    "ConfigLoader: stationarity.tests.adf.lag_selection must be "
                    "'aic', 'bic', or 'fixed', got '" + ls + "'.");
            }
        }

        if (t.contains("kpss")) {
            const auto& k = t.at("kpss");
            cfg.tests.kpssEnabled            = getOrDefault<bool>        (k, "enabled",            true,    false);
            cfg.tests.kpss.trendType         = getOrDefault<std::string> (k, "trend_type",          "level", false);
            cfg.tests.kpss.lags              = getOrDefault<int>         (k, "lags",               -1,      false);
            cfg.tests.kpss.significanceLevel = getOrDefault<double>      (k, "significance_level",  0.05,   false);

            const std::string& tt = cfg.tests.kpss.trendType;
            if (tt != "level" && tt != "trend") {
                throw ConfigException(
                    "ConfigLoader: stationarity.tests.kpss.trend_type must be "
                    "'level' or 'trend', got '" + tt + "'.");
            }
        }

        if (t.contains("pp")) {
            const auto& p = t.at("pp");
            cfg.tests.ppEnabled              = getOrDefault<bool>        (p, "enabled",            true,       false);
            cfg.tests.pp.trendType           = getOrDefault<std::string> (p, "trend_type",          "constant", false);
            cfg.tests.pp.lags                = getOrDefault<int>         (p, "lags",               -1,         false);
            cfg.tests.pp.significanceLevel   = getOrDefault<double>      (p, "significance_level",  0.05,       false);

            const std::string& tt = cfg.tests.pp.trendType;
            if (tt != "none" && tt != "constant" && tt != "trend") {
                throw ConfigException(
                    "ConfigLoader: stationarity.tests.pp.trend_type must be "
                    "'none', 'constant', or 'trend', got '" + tt + "'.");
            }
        }

        if (t.contains("runs_test")) {
            const auto& r = t.at("runs_test");
            cfg.tests.runsTestEnabled = getOrDefault<bool>(r, "enabled", true, false);
        }
    }

    cfg.significanceLevel = getOrDefault<double>(j, "significance_level", 0.05, false);

    if (cfg.significanceLevel <= 0.0 || cfg.significanceLevel >= 1.0) {
        throw ConfigException(
            "ConfigLoader: stationarity.significance_level must be in (0, 1), got "
            + std::to_string(cfg.significanceLevel) + ".");
    }

    return cfg;
}

// -----------------------------------------------------------------------------
//  Private: _parseArima
// -----------------------------------------------------------------------------

ArimaConfig ConfigLoader::_parseArima(const nlohmann::json& j)
{
    ArimaConfig cfg;

    cfg.gapFillStrategy  = getOrDefault<std::string>(j, "gap_fill_strategy",   "linear", false);
    cfg.gapFillMaxLength = getOrDefault<int>        (j, "gap_fill_max_length", 0,        false);

    if (j.contains("deseasonalization")) {
        const auto& ds = j.at("deseasonalization");
        cfg.deseasonalization.strategy          = getOrDefault<std::string>(ds, "strategy",              "median_year", false);
        cfg.deseasonalization.maWindowSize       = getOrDefault<int>        (ds, "ma_window_size",        1461,          false);
        cfg.deseasonalization.medianYearMinYears = getOrDefault<int>        (ds, "median_year_min_years", 5,             false);
    }

    cfg.autoOrder = getOrDefault<bool>       (j, "auto_order", true,  false);
    cfg.p         = getOrDefault<int>        (j, "p",          1,     false);
    cfg.d         = getOrDefault<int>        (j, "d",          -1,    false);
    cfg.q         = getOrDefault<int>        (j, "q",          0,     false);
    cfg.maxP      = getOrDefault<int>        (j, "max_p",      5,     false);
    cfg.maxQ      = getOrDefault<int>        (j, "max_q",      5,     false);
    cfg.criterion = getOrDefault<std::string>(j, "criterion",  "aic", false);

    if (cfg.criterion != "aic" && cfg.criterion != "bic") {
        throw ConfigException(
            "ConfigLoader: arima.criterion must be 'aic' or 'bic', got '"
            + cfg.criterion + "'.");
    }
    if (cfg.d < -1 || cfg.d > 2) {
        throw ConfigException(
            "ConfigLoader: arima.d must be -1, 0, 1, or 2, got "
            + std::to_string(cfg.d) + ".");
    }
    if (cfg.maxP < 0 || cfg.maxQ < 0) {
        throw ConfigException(
            "ConfigLoader: arima.max_p and max_q must be >= 0.");
    }

    if (j.contains("seasonal")) {
        const auto& s = j.at("seasonal");
        cfg.seasonal.P = getOrDefault<int>(s, "P", 0, false);
        cfg.seasonal.D = getOrDefault<int>(s, "D", 0, false);
        cfg.seasonal.Q = getOrDefault<int>(s, "Q", 0, false);
        cfg.seasonal.s = getOrDefault<int>(s, "s", 0, false);
        if (cfg.seasonal.s < 0) {
            throw ConfigException(
                "ConfigLoader: arima.seasonal.s must be >= 0, got "
                + std::to_string(cfg.seasonal.s) + ".");
        }
    }

    if (j.contains("fitter")) {
        const auto& f = j.at("fitter");
        cfg.fitter.method        = getOrDefault<std::string>(f, "method",         "css",    false);
        cfg.fitter.maxIterations = getOrDefault<int>        (f, "max_iterations", 200,      false);
        cfg.fitter.tol           = getOrDefault<double>     (f, "tol",            1.0e-8,   false);
        if (cfg.fitter.method != "css") {
            LOKI_WARNING("ConfigLoader: arima.fitter.method '" + cfg.fitter.method
                         + "' not yet implemented -- will fall back to 'css'.");
        }
    }

    cfg.computeForecast   = getOrDefault<bool>  (j, "compute_forecast",  false, false);
    cfg.forecastTail      = getOrDefault<int>   (j, "forecast_tail",      1461, false);
    cfg.forecastHorizon   = getOrDefault<double>(j, "forecast_horizon",    0.0, false);
    cfg.confidenceLevel   = getOrDefault<double>(j, "confidence_level",   0.95, false);
    cfg.significanceLevel = getOrDefault<double>(j, "significance_level", 0.05, false);

    if (cfg.confidenceLevel <= 0.0 || cfg.confidenceLevel >= 1.0) {
        throw ConfigException(
            "ConfigLoader: arima.confidence_level must be in (0, 1), got "
            + std::to_string(cfg.confidenceLevel) + ".");
    }
    if (cfg.significanceLevel <= 0.0 || cfg.significanceLevel >= 1.0) {
        throw ConfigException(
            "ConfigLoader: arima.significance_level must be in (0, 1), got "
            + std::to_string(cfg.significanceLevel) + ".");
    }

    return cfg;
}

// -----------------------------------------------------------------------------
//  Private: _parseSsa
// -----------------------------------------------------------------------------

SsaConfig ConfigLoader::_parseSsa(const nlohmann::json& j)
{
    SsaConfig cfg;

    cfg.gapFillStrategy  = getOrDefault<std::string>(j, "gap_fill_strategy",  "linear", false);
    cfg.gapFillMaxLength = getOrDefault<int>        (j, "gap_fill_max_length", 0,        false);

    if (j.contains("deseasonalization")) {
        const auto& ds = j.at("deseasonalization");
        cfg.deseasonalization.strategy          = getOrDefault<std::string>(ds, "strategy",              "none", false);
        cfg.deseasonalization.maWindowSize       = getOrDefault<int>        (ds, "ma_window_size",        1461,   false);
        cfg.deseasonalization.medianYearMinYears = getOrDefault<int>        (ds, "median_year_min_years", 5,      false);
    }

    if (j.contains("window")) {
        const auto& w = j.at("window");
        cfg.window.windowLength     = getOrDefault<int>(w, "window_length",     0,     false);
        cfg.window.period           = getOrDefault<int>(w, "period",            0,     false);
        cfg.window.periodMultiplier = getOrDefault<int>(w, "period_multiplier", 2,     false);
        cfg.window.maxWindowLength  = getOrDefault<int>(w, "max_window_length", 20000, false);

        if (cfg.window.windowLength < 0) {
            throw ConfigException(
                "ConfigLoader: ssa.window.window_length must be >= 0, got "
                + std::to_string(cfg.window.windowLength) + ".");
        }
        if (cfg.window.period < 0) {
            throw ConfigException(
                "ConfigLoader: ssa.window.period must be >= 0, got "
                + std::to_string(cfg.window.period) + ".");
        }
        if (cfg.window.periodMultiplier < 1) {
            throw ConfigException(
                "ConfigLoader: ssa.window.period_multiplier must be >= 1, got "
                + std::to_string(cfg.window.periodMultiplier) + ".");
        }
        if (cfg.window.maxWindowLength < 2) {
            throw ConfigException(
                "ConfigLoader: ssa.window.max_window_length must be >= 2, got "
                + std::to_string(cfg.window.maxWindowLength) + ".");
        }
    }

    if (j.contains("grouping")) {
        const auto& g = j.at("grouping");
        cfg.grouping.method            = getOrDefault<std::string>(g, "method",             "wcorr", false);
        cfg.grouping.wcorrThreshold    = getOrDefault<double>     (g, "wcorr_threshold",    0.8,     false);
        cfg.grouping.kmeansK           = getOrDefault<int>        (g, "kmeans_k",           0,       false);
        cfg.grouping.varianceThreshold = getOrDefault<double>     (g, "variance_threshold", 0.95,    false);

        const std::string& method = cfg.grouping.method;
        if (method != "manual" && method != "wcorr" &&
            method != "kmeans" && method != "variance") {
            throw ConfigException(
                "ConfigLoader: ssa.grouping.method must be one of "
                "'manual', 'wcorr', 'kmeans', 'variance', got '" + method + "'.");
        }

        if (cfg.grouping.wcorrThreshold <= 0.0 || cfg.grouping.wcorrThreshold > 1.0) {
            throw ConfigException(
                "ConfigLoader: ssa.grouping.wcorr_threshold must be in (0, 1], got "
                + std::to_string(cfg.grouping.wcorrThreshold) + ".");
        }
        if (cfg.grouping.kmeansK < 0) {
            throw ConfigException(
                "ConfigLoader: ssa.grouping.kmeans_k must be >= 0, got "
                + std::to_string(cfg.grouping.kmeansK) + ".");
        }
        if (cfg.grouping.varianceThreshold <= 0.0 || cfg.grouping.varianceThreshold > 1.0) {
            throw ConfigException(
                "ConfigLoader: ssa.grouping.variance_threshold must be in (0, 1], got "
                + std::to_string(cfg.grouping.varianceThreshold) + ".");
        }

        if (g.contains("manual_groups") && g.at("manual_groups").is_object()) {
            for (const auto& [name, indices] : g.at("manual_groups").items()) {
                std::vector<int> idx;
                for (const auto& v : indices) {
                    idx.push_back(v.get<int>());
                }
                cfg.grouping.manualGroups[name] = idx;
            }
        }
    }

    if (j.contains("reconstruction")) {
        const auto& r = j.at("reconstruction");
        cfg.reconstruction.method = getOrDefault<std::string>(r, "method", "diagonal_averaging", false);
        const std::string& rm = cfg.reconstruction.method;
        if (rm != "diagonal_averaging" && rm != "simple") {
            throw ConfigException(
                "ConfigLoader: ssa.reconstruction.method must be "
                "'diagonal_averaging' or 'simple', got '" + rm + "'.");
        }
    }

    cfg.computeWCorr       = getOrDefault<bool>  (j, "corr",              true,  false);
    cfg.svdRank            = getOrDefault<int>   (j, "svd_rank",             40, false);
    cfg.svdOversampling    = getOrDefault<int>   (j, "svd_oversampling",     10, false);
    cfg.svdPowerIter       = getOrDefault<int>   (j, "svd_power_iter",        2, false);
    cfg.wcorrMaxComponents = getOrDefault<int>   (j, "wcorr_max_components", 30, false);
    cfg.significanceLevel  = getOrDefault<double>(j, "significance_level", 0.05, false);
    cfg.forecastTail       = getOrDefault<int>   (j, "forecast_tail",      1461, false);

    if (cfg.significanceLevel <= 0.0 || cfg.significanceLevel >= 1.0) {
        throw ConfigException(
            "ConfigLoader: ssa.significance_level must be in (0, 1), got "
            + std::to_string(cfg.significanceLevel) + ".");
    }
    if (cfg.svdRank < 0) {
        throw ConfigException(
            "ConfigLoader: ssa.svd_rank must be >= 0, got "
            + std::to_string(cfg.svdRank) + ".");
    }
    if (cfg.svdOversampling < 0) {
        throw ConfigException(
            "ConfigLoader: ssa.svd_oversampling must be >= 0, got "
            + std::to_string(cfg.svdOversampling) + ".");
    }
    if (cfg.svdPowerIter < 0) {
        throw ConfigException(
            "ConfigLoader: ssa.svd_power_iter must be >= 0, got "
            + std::to_string(cfg.svdPowerIter) + ".");
    }
    if (cfg.wcorrMaxComponents < 0) {
        throw ConfigException(
            "ConfigLoader: ssa.wcorr_max_components must be >= 0, got "
            + std::to_string(cfg.wcorrMaxComponents) + ".");
    }

    return cfg;
}

// -----------------------------------------------------------------------------
//  Private: _parseDecomposition
// -----------------------------------------------------------------------------

DecompositionConfig ConfigLoader::_parseDecomposition(const nlohmann::json& j)
{
    DecompositionConfig cfg;

    cfg.gapFillStrategy  = getOrDefault<std::string>(j, "gap_fill_strategy",   "linear", false);
    cfg.gapFillMaxLength = getOrDefault<int>        (j, "gap_fill_max_length", 0,        false);

    const std::string method = getOrDefault<std::string>(j, "method", "classical", false);
    if      (method == "classical") cfg.method = DecompositionMethodEnum::CLASSICAL;
    else if (method == "stl")       cfg.method = DecompositionMethodEnum::STL;
    else {
        LOKI_WARNING("ConfigLoader: decomposition.method unknown value '"
                     + method + "' -- using 'classical'.");
        cfg.method = DecompositionMethodEnum::CLASSICAL;
    }

    cfg.period = getOrDefault<int>(j, "period", 1461, false);
    if (cfg.period < 2) {
        throw ConfigException(
            "ConfigLoader: decomposition.period must be >= 2, got "
            + std::to_string(cfg.period) + ".");
    }

    if (j.contains("classical")) {
        const auto& c = j.at("classical");
        cfg.classical.trendFilter  = getOrDefault<std::string>(c, "trend_filter",  "moving_average", false);
        cfg.classical.seasonalType = getOrDefault<std::string>(c, "seasonal_type", "median",         false);

        if (cfg.classical.trendFilter != "moving_average") {
            LOKI_WARNING("ConfigLoader: decomposition.classical.trend_filter '"
                         + cfg.classical.trendFilter
                         + "' not recognised -- using 'moving_average'.");
            cfg.classical.trendFilter = "moving_average";
        }
        if (cfg.classical.seasonalType != "median" && cfg.classical.seasonalType != "mean") {
            throw ConfigException(
                "ConfigLoader: decomposition.classical.seasonal_type must be "
                "'median' or 'mean', got '" + cfg.classical.seasonalType + "'.");
        }
    }

    if (j.contains("stl")) {
        const auto& s = j.at("stl");
        cfg.stl.nInner     = getOrDefault<int>   (s, "n_inner",      2,   false);
        cfg.stl.nOuter     = getOrDefault<int>   (s, "n_outer",      1,   false);
        cfg.stl.sDegree    = getOrDefault<int>   (s, "s_degree",     1,   false);
        cfg.stl.tDegree    = getOrDefault<int>   (s, "t_degree",     1,   false);
        cfg.stl.sBandwidth = getOrDefault<double>(s, "s_bandwidth",  0.0, false);
        cfg.stl.tBandwidth = getOrDefault<double>(s, "t_bandwidth",  0.0, false);

        if (cfg.stl.nInner < 1) {
            throw ConfigException(
                "ConfigLoader: decomposition.stl.n_inner must be >= 1, got "
                + std::to_string(cfg.stl.nInner) + ".");
        }
        if (cfg.stl.nOuter < 0) {
            throw ConfigException(
                "ConfigLoader: decomposition.stl.n_outer must be >= 0, got "
                + std::to_string(cfg.stl.nOuter) + ".");
        }
        if (cfg.stl.sDegree != 1 && cfg.stl.sDegree != 2) {
            throw ConfigException(
                "ConfigLoader: decomposition.stl.s_degree must be 1 or 2, got "
                + std::to_string(cfg.stl.sDegree) + ".");
        }
        if (cfg.stl.tDegree != 1 && cfg.stl.tDegree != 2) {
            throw ConfigException(
                "ConfigLoader: decomposition.stl.t_degree must be 1 or 2, got "
                + std::to_string(cfg.stl.tDegree) + ".");
        }
        if (cfg.stl.sBandwidth < 0.0 || cfg.stl.sBandwidth > 1.0) {
            throw ConfigException(
                "ConfigLoader: decomposition.stl.s_bandwidth must be in [0, 1], got "
                + std::to_string(cfg.stl.sBandwidth) + ".");
        }
        if (cfg.stl.tBandwidth < 0.0 || cfg.stl.tBandwidth > 1.0) {
            throw ConfigException(
                "ConfigLoader: decomposition.stl.t_bandwidth must be in [0, 1], got "
                + std::to_string(cfg.stl.tBandwidth) + ".");
        }
    }

    cfg.significanceLevel = getOrDefault<double>(j, "significance_level", 0.05, false);
    if (cfg.significanceLevel <= 0.0 || cfg.significanceLevel >= 1.0) {
        throw ConfigException(
            "ConfigLoader: decomposition.significance_level must be in (0, 1), got "
            + std::to_string(cfg.significanceLevel) + ".");
    }

    return cfg;
}

// -----------------------------------------------------------------------------
//  Private: _parseSpectral
// -----------------------------------------------------------------------------

SpectralConfig ConfigLoader::_parseSpectral(const nlohmann::json& j)
{
    SpectralConfig cfg;

    cfg.gapFillStrategy  = getOrDefault<std::string>(j, "gap_fill_strategy",  "linear", false);
    cfg.gapFillMaxLength = getOrDefault<int>        (j, "gap_fill_max_length", 0,        false);

    const std::string method = getOrDefault<std::string>(j, "method", "auto", false);
    if (method != "auto" && method != "fft" && method != "lomb_scargle") {
        LOKI_WARNING("ConfigLoader: spectral.method unknown value '"
                     + method + "' -- using 'auto'.");
        cfg.method = "auto";
    } else {
        cfg.method = method;
    }

    if (j.contains("fft")) {
        const auto& f = j.at("fft");
        cfg.fft.windowFunction = getOrDefault<std::string>(f, "window_function", "hann", false);
        const std::vector<std::string> validWindows =
            {"hann", "hamming", "blackman", "flattop", "rectangular"};
        bool windowValid = false;
        for (const auto& w : validWindows) {
            if (cfg.fft.windowFunction == w) { windowValid = true; break; }
        }
        if (!windowValid) {
            LOKI_WARNING("ConfigLoader: spectral.fft.window_function '"
                         + cfg.fft.windowFunction + "' not recognised -- using 'hann'.");
            cfg.fft.windowFunction = "hann";
        }
        cfg.fft.welch         = getOrDefault<bool>  (f, "welch",          false, false);
        cfg.fft.welchSegments = getOrDefault<int>   (f, "welch_segments", 8,     false);
        cfg.fft.welchOverlap  = getOrDefault<double>(f, "welch_overlap",  0.5,   false);
        if (cfg.fft.welchSegments < 2) {
            throw ConfigException(
                "ConfigLoader: spectral.fft.welch_segments must be >= 2, got "
                + std::to_string(cfg.fft.welchSegments) + ".");
        }
        if (cfg.fft.welchOverlap < 0.0 || cfg.fft.welchOverlap >= 1.0) {
            throw ConfigException(
                "ConfigLoader: spectral.fft.welch_overlap must be in [0, 1), got "
                + std::to_string(cfg.fft.welchOverlap) + ".");
        }
    }

    if (j.contains("lomb_scargle")) {
        const auto& ls = j.at("lomb_scargle");
        cfg.lombScargle.oversampling = getOrDefault<double>(ls, "oversampling",  4.0,   false);
        cfg.lombScargle.fastNfft     = getOrDefault<bool>  (ls, "fast_nfft",     false, false);
        cfg.lombScargle.fapThreshold = getOrDefault<double>(ls, "fap_threshold", 0.01,  false);
        if (cfg.lombScargle.oversampling < 1.0) {
            throw ConfigException(
                "ConfigLoader: spectral.lomb_scargle.oversampling must be >= 1.0, got "
                + std::to_string(cfg.lombScargle.oversampling) + ".");
        }
        if (cfg.lombScargle.fapThreshold <= 0.0 || cfg.lombScargle.fapThreshold >= 1.0) {
            throw ConfigException(
                "ConfigLoader: spectral.lomb_scargle.fap_threshold must be in (0, 1), got "
                + std::to_string(cfg.lombScargle.fapThreshold) + ".");
        }
    }

    if (j.contains("spectrogram")) {
        const auto& sg = j.at("spectrogram");
        cfg.spectrogram.enabled        = getOrDefault<bool>  (sg, "enabled",          false, false);
        cfg.spectrogram.windowLength   = getOrDefault<int>   (sg, "window_length",    1461,  false);
        cfg.spectrogram.overlap        = getOrDefault<double>(sg, "overlap",          0.5,   false);
        cfg.spectrogram.focusPeriodMin = getOrDefault<double>(sg, "focus_period_min", 0.0,   false);
        cfg.spectrogram.focusPeriodMax = getOrDefault<double>(sg, "focus_period_max", 0.0,   false);
        if (cfg.spectrogram.windowLength < 4) {
            throw ConfigException(
                "ConfigLoader: spectral.spectrogram.window_length must be >= 4, got "
                + std::to_string(cfg.spectrogram.windowLength) + ".");
        }
        if (cfg.spectrogram.overlap < 0.0 || cfg.spectrogram.overlap >= 1.0) {
            throw ConfigException(
                "ConfigLoader: spectral.spectrogram.overlap must be in [0, 1), got "
                + std::to_string(cfg.spectrogram.overlap) + ".");
        }
        if (cfg.spectrogram.focusPeriodMax > 0.0 &&
            cfg.spectrogram.focusPeriodMax <= cfg.spectrogram.focusPeriodMin) {
            throw ConfigException(
                "ConfigLoader: spectral.spectrogram.focus_period_max must be > "
                "focus_period_min when both are non-zero.");
        }
    }

    if (j.contains("peaks")) {
        const auto& pk = j.at("peaks");
        cfg.peaks.topN          = getOrDefault<int>   (pk, "top_n",           10,  false);
        cfg.peaks.minPeriodDays = getOrDefault<double>(pk, "min_period_days",  0.0, false);
        cfg.peaks.maxPeriodDays = getOrDefault<double>(pk, "max_period_days",  0.0, false);
        if (cfg.peaks.topN < 1) {
            throw ConfigException(
                "ConfigLoader: spectral.peaks.top_n must be >= 1, got "
                + std::to_string(cfg.peaks.topN) + ".");
        }
        if (cfg.peaks.maxPeriodDays > 0.0 &&
            cfg.peaks.maxPeriodDays <= cfg.peaks.minPeriodDays) {
            throw ConfigException(
                "ConfigLoader: spectral.peaks.max_period_days must be > min_period_days "
                "when both are non-zero.");
        }
    }

    cfg.significanceLevel = getOrDefault<double>(j, "significance_level", 0.05, false);
    if (cfg.significanceLevel <= 0.0 || cfg.significanceLevel >= 1.0) {
        throw ConfigException(
            "ConfigLoader: spectral.significance_level must be in (0, 1), got "
            + std::to_string(cfg.significanceLevel) + ".");
    }

    return cfg;
}

// -----------------------------------------------------------------------------
//  Private: _parseKalman
// -----------------------------------------------------------------------------

KalmanConfig ConfigLoader::_parseKalman(const nlohmann::json& j)
{
    KalmanConfig cfg;

    cfg.gapFillStrategy  = getOrDefault<std::string>(j, "gap_fill_strategy",  "auto", false);
    cfg.gapFillMaxLength = getOrDefault<int>        (j, "gap_fill_max_length", 0,     false);
    cfg.model            = getOrDefault<std::string>(j, "model",    "local_level",    false);
    cfg.smoother         = getOrDefault<std::string>(j, "smoother", "rts",            false);
    cfg.significanceLevel= getOrDefault<double>     (j, "significance_level", 0.05,  false);

    if (cfg.model != "local_level"
        && cfg.model != "local_trend"
        && cfg.model != "constant_velocity") {
        throw ConfigException(
            "ConfigLoader: kalman.model '" + cfg.model + "' not recognised. "
            "Valid: local_level, local_trend, constant_velocity.");
    }

    if (cfg.smoother != "none" && cfg.smoother != "rts") {
        throw ConfigException(
            "ConfigLoader: kalman.smoother '" + cfg.smoother + "' not recognised. "
            "Valid: none, rts.");
    }

    if (cfg.gapFillStrategy != "auto"
        && cfg.gapFillStrategy != "linear"
        && cfg.gapFillStrategy != "median_year"
        && cfg.gapFillStrategy != "none") {
        throw ConfigException(
            "ConfigLoader: kalman.gap_fill_strategy '" + cfg.gapFillStrategy
            + "' not recognised. Valid: auto, linear, median_year, none.");
    }

    if (cfg.gapFillMaxLength < 0) {
        throw ConfigException(
            "ConfigLoader: kalman.gap_fill_max_length must be >= 0, got "
            + std::to_string(cfg.gapFillMaxLength) + ".");
    }

    if (cfg.significanceLevel <= 0.0 || cfg.significanceLevel >= 1.0) {
        throw ConfigException(
            "ConfigLoader: kalman.significance_level must be in (0, 1), got "
            + std::to_string(cfg.significanceLevel) + ".");
    }

    // ---- noise sub-section --------------------------------------------------
    if (j.contains("noise")) {
        const auto& n = j.at("noise");

        cfg.noise.estimation = getOrDefault<std::string>(n, "estimation", "manual", false);
        if (cfg.noise.estimation != "manual"
            && cfg.noise.estimation != "heuristic"
            && cfg.noise.estimation != "em") {
            throw ConfigException(
                "ConfigLoader: kalman.noise.estimation '" + cfg.noise.estimation
                + "' not recognised. Valid: manual, heuristic, em.");
        }

        cfg.noise.Q               = getOrDefault<double>(n, "Q",               1.0,    false);
        cfg.noise.R               = getOrDefault<double>(n, "R",               1.0,    false);
        cfg.noise.smoothingFactor = getOrDefault<double>(n, "smoothing_factor", 10.0,  false);
        cfg.noise.QInit           = getOrDefault<double>(n, "Q_init",           1.0,   false);
        cfg.noise.RInit           = getOrDefault<double>(n, "R_init",           1.0,   false);
        cfg.noise.emMaxIter       = getOrDefault<int>   (n, "em_max_iter",      100,   false);
        cfg.noise.emTol           = getOrDefault<double>(n, "em_tol",           1.0e-6,false);

        if (cfg.noise.Q <= 0.0) {
            throw ConfigException(
                "ConfigLoader: kalman.noise.Q must be > 0, got "
                + std::to_string(cfg.noise.Q) + ".");
        }
        if (cfg.noise.R <= 0.0) {
            throw ConfigException(
                "ConfigLoader: kalman.noise.R must be > 0, got "
                + std::to_string(cfg.noise.R) + ".");
        }
        if (cfg.noise.QInit <= 0.0) {
            throw ConfigException(
                "ConfigLoader: kalman.noise.Q_init must be > 0, got "
                + std::to_string(cfg.noise.QInit) + ".");
        }
        if (cfg.noise.RInit <= 0.0) {
            throw ConfigException(
                "ConfigLoader: kalman.noise.R_init must be > 0, got "
                + std::to_string(cfg.noise.RInit) + ".");
        }
        if (cfg.noise.smoothingFactor <= 0.0) {
            throw ConfigException(
                "ConfigLoader: kalman.noise.smoothing_factor must be > 0, got "
                + std::to_string(cfg.noise.smoothingFactor) + ".");
        }
        if (cfg.noise.emMaxIter < 1) {
            throw ConfigException(
                "ConfigLoader: kalman.noise.em_max_iter must be >= 1, got "
                + std::to_string(cfg.noise.emMaxIter) + ".");
        }
        if (cfg.noise.emTol <= 0.0) {
            throw ConfigException(
                "ConfigLoader: kalman.noise.em_tol must be > 0, got "
                + std::to_string(cfg.noise.emTol) + ".");
        }
    }

    // ---- forecast sub-section -----------------------------------------------
    if (j.contains("forecast")) {
        const auto& f = j.at("forecast");
        cfg.forecast.steps           = getOrDefault<int>   (f, "steps",            0,    false);
        cfg.forecast.confidenceLevel = getOrDefault<double>(f, "confidence_level", 0.95, false);

        if (cfg.forecast.steps < 0) {
            throw ConfigException(
                "ConfigLoader: kalman.forecast.steps must be >= 0, got "
                + std::to_string(cfg.forecast.steps) + ".");
        }
        if (cfg.forecast.confidenceLevel <= 0.0 || cfg.forecast.confidenceLevel >= 1.0) {
            throw ConfigException(
                "ConfigLoader: kalman.forecast.confidence_level must be in (0, 1), got "
                + std::to_string(cfg.forecast.confidenceLevel) + ".");
        }
    }

    return cfg;
}

} // namespace loki