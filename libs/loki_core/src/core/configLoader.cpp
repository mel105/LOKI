#include "loki/core/configLoader.hpp"

#include "loki/core/exceptions.hpp"
#include "loki/core/logger.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>

using namespace loki;

namespace loki {

using json = nlohmann::json;

// -----------------------------------------------------------------------------
//  Helpers
// -----------------------------------------------------------------------------

namespace {

template<typename T>
T getOrDefault(const json&        j,
               const std::string& key,
               const T&           defaultVal,
               bool               warnIfMissing = true)
{
    if (!j.contains(key)) {
        if (warnIfMissing) {
            LOKI_WARNING("Config key '" + key + "' not found -- using default.");
        }
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
    try {
        ifs >> j;
    } catch (const json::parse_error& e) {
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
    cfg.plots       = _parsePlots      (j.value("plots",       json::object()));
    cfg.stats       = _parseStats      (j);

    return cfg;
}

// -----------------------------------------------------------------------------
//  Private: section parsers
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
        for (const auto& f : j.at("files")) {
            cfg.files.push_back(_resolvePath(f.get<std::string>(), inputDir));
        }
    } else if (hasScan) {
        cfg.mode    = InputMode::SCAN_DIRECTORY;
        cfg.scanDir = inputDir;
    } else {
        LOKI_WARNING("Config 'input': no 'file', 'files', or 'scan_directory' found -- "
                     "defaulting to SCAN_DIRECTORY on INPUT/.");
        cfg.mode    = InputMode::SCAN_DIRECTORY;
        cfg.scanDir = inputDir;
    }

    const std::string tfStr = getOrDefault<std::string>(j, "time_format", "gpst_seconds");
    cfg.timeFormat = _parseTimeFormat(tfStr);

    const std::string delimStr = getOrDefault<std::string>(j, "delimiter", ";");
    cfg.delimiter = delimStr.empty() ? ';' : delimStr.front();

    const std::string commentStr = getOrDefault<std::string>(j, "comment_char", "%");
    cfg.commentChar = commentStr.empty() ? '%' : commentStr.front();

    // Value columns (1-based, values <= 1 are skipped in loader).
    if (j.contains("columns")) {
        cfg.columns = j.at("columns").get<std::vector<int>>();
        std::erase_if(cfg.columns, [](int c) { return c <= 1; });
    } else {
        LOKI_WARNING("Config 'input.columns' not specified -- all value columns will be loaded.");
    }

    // Time columns (0-based field indices that together form the time token).
    // Default: empty = field 0 only (single-column time).
    // Example: [0, 1] for "1990-01-01" "00:00:00" split across two fields.
    if (j.contains("time_columns")) {
        cfg.timeColumns = j.at("time_columns").get<std::vector<int>>();
        if (cfg.timeColumns.empty()) {
            LOKI_WARNING("Config 'input.time_columns' is empty -- defaulting to field 0.");
        } else {
            std::string msg = "Config 'input.time_columns': joining fields [";
            for (std::size_t i = 0; i < cfg.timeColumns.size(); ++i) {
                if (i > 0) msg += ", ";
                msg += std::to_string(cfg.timeColumns[i]);
            }
            msg += "] as time token.";
            LOKI_INFO(msg);
        }
    }
    // If time_columns is absent: empty vector, loader uses field 0 by default.

    const std::string mergeStr = getOrDefault<std::string>(j, "merge_strategy", "separate");
    cfg.mergeStrategy = _parseMergeStrategy(mergeStr);

    return cfg;
}

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

HomogeneityConfig ConfigLoader::_parseHomogeneity(const nlohmann::json& j)
{
    HomogeneityConfig cfg;

    cfg.applyGapFilling  = getOrDefault<bool>(j, "apply_gap_filling",  true,  false);
    cfg.applyAdjustment  = getOrDefault<bool>(j, "apply_adjustment",   true,  false);

    // -- Gap filling ----------------------------------------------------------
    if (j.contains("gap_filling")) {
        const auto& gf = j.at("gap_filling");
        cfg.gapFilling.strategy           = getOrDefault<std::string>(gf, "strategy",             "linear", false);
        cfg.gapFilling.maxFillLength      = getOrDefault<int>        (gf, "max_fill_length",       0,        false);
        cfg.gapFilling.gapThresholdFactor = getOrDefault<double>     (gf, "gap_threshold_factor",  1.5,      false);
        cfg.gapFilling.minSeriesYears     = getOrDefault<int>        (gf, "min_series_years",      10,       false);
    }

    // -- Pre outlier (future) -------------------------------------------------
    if (j.contains("pre_outlier")) {
        const auto& po = j.at("pre_outlier");
        cfg.preOutlier.enabled   = getOrDefault<bool>  (po, "enabled",   false, false);
        cfg.preOutlier.threshold = getOrDefault<double>(po, "threshold",  3.0,  false);
        if (cfg.preOutlier.enabled) {
            LOKI_WARNING("Config 'homogeneity.pre_outlier.enabled=true' -- "
                         "loki_outlier is not yet implemented. Skipping.");
            cfg.preOutlier.enabled = false;
        }
    }

    // -- Deseasonalization ----------------------------------------------------
    if (j.contains("deseasonalization")) {
        const auto& ds = j.at("deseasonalization");
        cfg.deseasonalization.strategy           = getOrDefault<std::string>(ds, "strategy",               "median_year", false);
        cfg.deseasonalization.maWindowSize        = getOrDefault<int>        (ds, "ma_window_size",          365,           false);
        cfg.deseasonalization.medianYearMinYears  = getOrDefault<int>        (ds, "median_year_min_years",   5,             false);
    }

    // -- Post outlier (future) ------------------------------------------------
    if (j.contains("post_outlier")) {
        const auto& po = j.at("post_outlier");
        cfg.postOutlier.enabled   = getOrDefault<bool>  (po, "enabled",   false, false);
        cfg.postOutlier.threshold = getOrDefault<double>(po, "threshold",  3.0,  false);
        if (cfg.postOutlier.enabled) {
            LOKI_WARNING("Config 'homogeneity.post_outlier.enabled=true' -- "
                         "loki_outlier is not yet implemented. Skipping.");
            cfg.postOutlier.enabled = false;
        }
    }

    // -- Detection ------------------------------------------------------------
    if (j.contains("detection")) {
        const auto& det = j.at("detection");
        cfg.detection.minSegmentPoints   = getOrDefault<int>   (det, "min_segment_points",   60,    false);
        cfg.detection.minSegmentSeconds  = getOrDefault<double>(det, "min_segment_seconds",  0.0,   false);
        cfg.detection.minSegmentDuration = getOrDefault<std::string>(det, "min_segment_duration", "", false);
        cfg.detection.significanceLevel  = getOrDefault<double>(det, "significance_level",   0.05,  false);
        cfg.detection.acfDependenceLimit = getOrDefault<double>(det, "acf_dependence_limit", 0.2,   false);

        // Parse min_segment_duration and convert to seconds (overrides min_segment_seconds).
        if (!cfg.detection.minSegmentDuration.empty()) {
            const double parsed = _parseDuration(cfg.detection.minSegmentDuration);
            if (parsed > 0.0) {
                cfg.detection.minSegmentSeconds = parsed;
                LOKI_INFO("Config 'detection.min_segment_duration': '"
                          + cfg.detection.minSegmentDuration + "' -> "
                          + std::to_string(static_cast<long long>(parsed)) + " s.");
            } else {
                LOKI_WARNING("Config 'detection.min_segment_duration': cannot parse '"
                             + cfg.detection.minSegmentDuration
                             + "' -- ignoring. Use format: 1y | 180d | 6h | 30m | 60s.");
            }
        }
    }

    return cfg;
}

PlotConfig ConfigLoader::_parsePlots(const nlohmann::json& j)
{
    PlotConfig cfg;

    if (j.contains("output_format")) {
        const std::string fmt = j["output_format"].get<std::string>();
        if (fmt != "png" && fmt != "eps" && fmt != "svg") {
            throw ConfigException(
                "Config 'plots.output_format' must be 'png', 'eps', or 'svg'. Got: '" + fmt + "'.");
        }
        cfg.outputFormat = fmt;
    }

    if (j.contains("time_format")) cfg.timeFormat = j["time_format"].get<std::string>();

    if (j.contains("enabled")) {
        const auto& e = j["enabled"];

        // Generic plots
        if (e.contains("time_series"))  cfg.timeSeries  = e["time_series"].get<bool>();
        if (e.contains("comparison"))   cfg.comparison  = e["comparison"].get<bool>();
        if (e.contains("histogram"))    cfg.histogram   = e["histogram"].get<bool>();
        if (e.contains("acf"))          cfg.acf         = e["acf"].get<bool>();
        if (e.contains("qq_plot"))      cfg.qqPlot      = e["qq_plot"].get<bool>();
        if (e.contains("boxplot"))      cfg.boxplot     = e["boxplot"].get<bool>();

        // Homogeneity plots
        if (e.contains("original_series"))  cfg.originalSeries  = e["original_series"].get<bool>();
        if (e.contains("seasonal_overlay")) cfg.seasonalOverlay = e["seasonal_overlay"].get<bool>();
        if (e.contains("deseasonalized"))   cfg.deseasonalized  = e["deseasonalized"].get<bool>();
        if (e.contains("change_points"))    cfg.changePoints    = e["change_points"].get<bool>();
        if (e.contains("adjusted_series"))  cfg.adjustedSeries  = e["adjusted_series"].get<bool>();
        if (e.contains("homog_comparison")) cfg.homogComparison = e["homog_comparison"].get<bool>();
        if (e.contains("shift_magnitudes")) cfg.shiftMagnitudes = e["shift_magnitudes"].get<bool>();
        if (e.contains("correction_curve")) cfg.correctionCurve = e["correction_curve"].get<bool>();
    }

    return cfg;
}

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

// -----------------------------------------------------------------------------
//  Private: path resolution
// -----------------------------------------------------------------------------

std::filesystem::path ConfigLoader::_resolvePath(const std::string&           raw,
                                                  const std::filesystem::path& baseDir)
{
    const std::filesystem::path p(raw);
    return p.is_absolute() ? p : baseDir / p;
}

// -----------------------------------------------------------------------------
//  Private: duration parser
// -----------------------------------------------------------------------------

double ConfigLoader::_parseDuration(const std::string& s)
{
    // Format: <number><unit> where unit is one of: y d h m s
    // Examples: "1y", "180d", "6h", "30m", "60s"
    if (s.empty()) return 0.0;

    static constexpr double SECONDS_PER_MINUTE = 60.0;
    static constexpr double SECONDS_PER_HOUR   = 3600.0;
    static constexpr double SECONDS_PER_DAY    = 86400.0;
    static constexpr double SECONDS_PER_YEAR   = 365.25 * SECONDS_PER_DAY;

    const char unit = s.back();
    const std::string numStr = s.substr(0, s.size() - 1);

    double value = 0.0;
    try {
        value = std::stod(numStr);
    } catch (...) {
        return 0.0;
    }

    if (value <= 0.0) return 0.0;

    switch (unit) {
        case 'y': return value * SECONDS_PER_YEAR;
        case 'd': return value * SECONDS_PER_DAY;
        case 'h': return value * SECONDS_PER_HOUR;
        case 'm': return value * SECONDS_PER_MINUTE;
        case 's': return value;
        default:  return 0.0;
    }
}

} // namespace loki
