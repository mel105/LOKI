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

    cfg.logDir = cfg.workspace / "OUTPUT" / "LOG";
    cfg.csvDir = cfg.workspace / "OUTPUT" / "CSV";
    cfg.imgDir = cfg.workspace / "OUTPUT" / "IMG";

    const std::filesystem::path inputDir = cfg.workspace / "INPUT";

    cfg.input       = _parseInput      (j.value("input",       json::object()), inputDir);
    cfg.output      = _parseOutput     (j.value("output",      json::object()));
    cfg.homogeneity = _parseHomogeneity(j.value("homogeneity", json::object()));
    cfg.outlier     = _parseOutlier    (j.value("outlier",     json::object()));
    cfg.filter      = _parseFilter     (j.value("filter",      json::object()));
    cfg.plots       = _parsePlots      (j.value("plots",       json::object()));
    cfg.stats       = _parseStats      (j);

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
    else if (levelStr == "info")    { cfg.logLevel = LogLevel::INFO;     }
    else if (levelStr == "warning") { cfg.logLevel = LogLevel::WARNING;  }
    else if (levelStr == "error")   { cfg.logLevel = LogLevel::ERROR;    }
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

    cfg.applyGapFilling = getOrDefault<bool>(j, "apply_gap_filling", true,  false);
    cfg.applyAdjustment = getOrDefault<bool>(j, "apply_adjustment",  true,  false);

    if (j.contains("gap_filling")) {
        const auto& gf = j.at("gap_filling");
        cfg.gapFilling.strategy           = getOrDefault<std::string>(gf, "strategy",            "linear", false);
        cfg.gapFilling.maxFillLength      = getOrDefault<int>        (gf, "max_fill_length",      0,        false);
        cfg.gapFilling.gapThresholdFactor = getOrDefault<double>     (gf, "gap_threshold_factor", 1.5,      false);
        cfg.gapFilling.minSeriesYears     = getOrDefault<int>        (gf, "min_series_years",     10,       false);
    }

    if (j.contains("pre_outlier")) {
        cfg.preOutlier = _parseOutlierFilter(j.at("pre_outlier"), 5.0);
        if (cfg.preOutlier.enabled)
            LOKI_INFO("Config: homogeneity.pre_outlier enabled (method=" + cfg.preOutlier.method + ").");
    }

    if (j.contains("deseasonalization")) {
        const auto& ds = j.at("deseasonalization");
        cfg.deseasonalization.strategy           = getOrDefault<std::string>(ds, "strategy",             "median_year", false);
        cfg.deseasonalization.maWindowSize       = getOrDefault<int>        (ds, "ma_window_size",        365,           false);
        cfg.deseasonalization.medianYearMinYears = getOrDefault<int>        (ds, "median_year_min_years", 5,             false);
    }

    if (j.contains("post_outlier")) {
        cfg.postOutlier = _parseOutlierFilter(j.at("post_outlier"), 3.0);
        if (cfg.postOutlier.enabled)
            LOKI_INFO("Config: homogeneity.post_outlier enabled (method=" + cfg.postOutlier.method + ").");
    }

    if (j.contains("detection")) {
        const auto& det = j.at("detection");
        cfg.detection.minSegmentPoints     = getOrDefault<int>        (det, "min_segment_points",    60,    false);
        cfg.detection.minSegmentSeconds    = getOrDefault<double>     (det, "min_segment_seconds",   0.0,   false);
        cfg.detection.minSegmentDuration   = getOrDefault<std::string>(det, "min_segment_duration",  "",    false);
        cfg.detection.significanceLevel    = getOrDefault<double>     (det, "significance_level",    0.05,  false);
        cfg.detection.acfDependenceLimit   = getOrDefault<double>     (det, "acf_dependence_limit",  0.2,   false);
        cfg.detection.correctForDependence = getOrDefault<bool>       (det, "correct_for_dependence", true, false);
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
        cfg.deseasonalization.strategy           = getOrDefault<std::string>(ds, "strategy",             "none", false);
        cfg.deseasonalization.maWindowSize       = getOrDefault<int>        (ds, "ma_window_size",        365,    false);
        cfg.deseasonalization.medianYearMinYears = getOrDefault<int>        (ds, "median_year_min_years", 5,      false);
    }

    if (j.contains("detection")) {
        const auto& det = j.at("detection");
        cfg.detection.method          = getOrDefault<std::string>(det, "method",           "mad", false);
        cfg.detection.iqrMultiplier   = getOrDefault<double>     (det, "iqr_multiplier",   1.5,   false);
        cfg.detection.madMultiplier   = getOrDefault<double>     (det, "mad_multiplier",   3.0,   false);
        cfg.detection.zscoreThreshold = getOrDefault<double>     (det, "zscore_threshold", 3.0,   false);
    }

    if (j.contains("replacement")) {
        const auto& rep = j.at("replacement");
        cfg.replacement.strategy      = getOrDefault<std::string>(rep, "strategy",        "linear", false);
        cfg.replacement.maxFillLength = getOrDefault<int>        (rep, "max_fill_length",  0,        false);
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
        cfg.gapFilling.strategy      = getOrDefault<std::string>(gf, "strategy",       "linear", false);
        cfg.gapFilling.maxFillLength = getOrDefault<int>        (gf, "max_fill_length", 0,        false);
    }

    // -- method ---------------------------------------------------------------
    const std::string methodStr = getOrDefault<std::string>(j, "method", "kernel", false);
    if      (methodStr == "moving_average") cfg.method = FilterMethodEnum::MOVING_AVERAGE;
    else if (methodStr == "ema")            cfg.method = FilterMethodEnum::EMA;
    else if (methodStr == "weighted_ma")    cfg.method = FilterMethodEnum::WEIGHTED_MA;
    else if (methodStr == "kernel")         cfg.method = FilterMethodEnum::KERNEL;
    else if (methodStr == "loess")          cfg.method = FilterMethodEnum::LOESS;
    else if (methodStr == "savitzky_golay") cfg.method = FilterMethodEnum::SAVITZKY_GOLAY;
    else {
        LOKI_WARNING("Config 'filter.method' unknown value '" + methodStr + "' -- using 'kernel'.");
        cfg.method = FilterMethodEnum::KERNEL;
    }

    cfg.autoWindowMethod = getOrDefault<std::string>(j, "auto_window_method", "silverman_mad", false);

    // -- moving_average -------------------------------------------------------
    if (j.contains("moving_average")) {
        const auto& ma = j.at("moving_average");
        cfg.movingAverage.window = getOrDefault<int>(ma, "window", 365, false);
    }

    // -- ema ------------------------------------------------------------------
    if (j.contains("ema")) {
        const auto& ema = j.at("ema");
        cfg.ema.alpha = getOrDefault<double>(ema, "alpha", 0.1, false);
        if (cfg.ema.alpha <= 0.0 || cfg.ema.alpha > 1.0) {
            throw ConfigException(
                "ConfigLoader: filter.ema.alpha must be in (0, 1], got "
                + std::to_string(cfg.ema.alpha) + ".");
        }
    }

    // -- weighted_ma ----------------------------------------------------------
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

    // -- kernel ---------------------------------------------------------------
    if (j.contains("kernel")) {
        const auto& k = j.at("kernel");
        cfg.kernel.bandwidth      = getOrDefault<double>     (k, "bandwidth",       0.1,            false);
        cfg.kernel.kernelType     = getOrDefault<std::string>(k, "kernel_type",     "epanechnikov", false);
        cfg.kernel.gaussianCutoff = getOrDefault<double>     (k, "gaussian_cutoff", 3.0,            false);
    }

    // -- loess ----------------------------------------------------------------
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

    // -- savitzky_golay -------------------------------------------------------
    if (j.contains("savitzky_golay")) {
        const auto& sg = j.at("savitzky_golay");
        cfg.savitzkyGolay.window = getOrDefault<int>(sg, "window", 11, false);
        cfg.savitzkyGolay.degree = getOrDefault<int>(sg, "degree",  2, false);
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
    }

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

} // namespace loki
