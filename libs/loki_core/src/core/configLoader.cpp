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

/**
 * @brief Reads a value from JSON with a fallback default and optional warning.
 *
 * If the key is absent, the default is returned and a warning is logged
 * only when warnIfMissing is true.
 */
template<typename T>
T getOrDefault(const json&        j,
               const std::string& key,
               const T&           defaultVal,
               bool               warnIfMissing = true)
{
    if (!j.contains(key)) {
        if (warnIfMissing) {
            LOKI_WARNING("Config key '" + key + "' not found — using default.");
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

    // ── workspace (required) ──────────────────────────────────────────────────

    if (!j.contains("workspace")) {
        throw ConfigException(
            "ConfigLoader: required key 'workspace' is missing in '"
            + jsonPath.string() + "'.");
    }

    AppConfig cfg;
    cfg.workspace = std::filesystem::path(j.at("workspace").get<std::string>());

    if (!std::filesystem::exists(cfg.workspace)) {
        LOKI_WARNING("Workspace directory does not exist: '"
                     + cfg.workspace.string() + "' — it will be created when needed.");
    }

    // ── Derived output directories ────────────────────────────────────────────

    cfg.logDir = cfg.workspace / "OUTPUT" / "LOG";
    cfg.csvDir = cfg.workspace / "OUTPUT" / "CSV";
    cfg.imgDir = cfg.workspace / "OUTPUT" / "IMG";

    const std::filesystem::path inputDir = cfg.workspace / "INPUT";

    // ── Sub-sections ──────────────────────────────────────────────────────────

    cfg.input       = _parseInput      (j.value("input",       json::object()), inputDir);
    cfg.output      = _parseOutput     (j.value("output",      json::object()));
    cfg.homogeneity = _parseHomogeneity(j.value("homogeneity", json::object()));
    cfg.plots       = _parsePlots(j.value("plots", json::object()));

    return cfg;
}

// -----------------------------------------------------------------------------
//  Private: section parsers
// -----------------------------------------------------------------------------

InputConfig ConfigLoader::_parseInput(const nlohmann::json& j,
                                      const std::filesystem::path& inputDir)
{
    InputConfig cfg;

    // ── Determine input mode ──────────────────────────────────────────────────

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
        LOKI_WARNING("Config 'input': no 'file', 'files', or 'scan_directory' found — "
                     "defaulting to SCAN_DIRECTORY on INPUT/.");
        cfg.mode    = InputMode::SCAN_DIRECTORY;
        cfg.scanDir = inputDir;
    }

    // ── Time format ───────────────────────────────────────────────────────────

    const std::string tfStr = getOrDefault<std::string>(
        j, "time_format", "gpst_seconds");
    cfg.timeFormat = _parseTimeFormat(tfStr);

    // ── Delimiter ─────────────────────────────────────────────────────────────

    const std::string delimStr = getOrDefault<std::string>(j, "delimiter", ";");
    if (delimStr.empty()) {
        LOKI_WARNING("Config 'input.delimiter' is empty — using ';'.");
        cfg.delimiter = ';';
    } else {
        cfg.delimiter = delimStr.front();
    }

    // ── Comment character ─────────────────────────────────────────────────────

    const std::string commentStr = getOrDefault<std::string>(j, "comment_char", "%");
    if (commentStr.empty()) {
        LOKI_WARNING("Config 'input.comment_char' is empty — using '%'.");
        cfg.commentChar = '%';
    } else {
        cfg.commentChar = commentStr.front();
    }

    // ── Columns ───────────────────────────────────────────────────────────────

    if (j.contains("columns")) {
        cfg.columns = j.at("columns").get<std::vector<int>>();

        // Validate: column 1 is reserved for time.
        for (const int col : cfg.columns) {
            if (col == 1) {
                LOKI_WARNING("Config 'input.columns' contains column 1 "
                             "(reserved for time) — ignoring it.");
            }
            if (col < 1) {
                LOKI_WARNING("Config 'input.columns' contains invalid column index "
                             + std::to_string(col) + " — ignoring it.");
            }
        }
        // Remove reserved / invalid entries.
        std::erase_if(cfg.columns, [](int c) { return c <= 1; });

    } else {
        LOKI_WARNING("Config 'input.columns' not specified — all value columns will be loaded.");
    }

    // ── Merge strategy ────────────────────────────────────────────────────────

    const std::string mergeStr = getOrDefault<std::string>(
        j, "merge_strategy", "separate");
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
        LOKI_WARNING("Config 'output.log_level' unknown value '"
                     + levelStr + "' — using 'info'.");
        cfg.logLevel = LogLevel::INFO;
    }

    return cfg;
}

HomogeneityConfig ConfigLoader::_parseHomogeneity(const nlohmann::json& j)
{
    HomogeneityConfig cfg;

    cfg.method = getOrDefault<std::string>(j, "method", "snht");

    if (cfg.method != "snht" && cfg.method != "pettitt" && cfg.method != "buishand") {
        LOKI_WARNING("Config 'homogeneity.method' unknown value '"
                     + cfg.method + "' — using 'snht'.");
        cfg.method = "snht";
    }

    cfg.significanceLevel = getOrDefault<double>(j, "significance_level", 0.05);

    if (cfg.significanceLevel <= 0.0 || cfg.significanceLevel >= 1.0) {
        LOKI_WARNING("Config 'homogeneity.significance_level' must be in (0, 1) — using 0.05.");
        cfg.significanceLevel = 0.05;
    }

    return cfg;
}

PlotConfig ConfigLoader::_parsePlots(const nlohmann::json& j)
{
    PlotConfig cfg;

    if (j.contains("output_format"))
        cfg.outputFormat = j["output_format"].get<std::string>();

    if (j.contains("time_format"))
        cfg.timeFormat = j["time_format"].get<std::string>();

    if (j.contains("enabled")) {
        const auto& e = j["enabled"];
        if (e.contains("time_series")) cfg.timeSeries = e["time_series"].get<bool>();
        if (e.contains("comparison"))  cfg.comparison = e["comparison"].get<bool>();
        if (e.contains("histogram"))   cfg.histogram  = e["histogram"].get<bool>();
        if (e.contains("acf"))         cfg.acf        = e["acf"].get<bool>();
        if (e.contains("qq_plot"))     cfg.qqPlot     = e["qq_plot"].get<bool>();
        if (e.contains("boxplot"))     cfg.boxplot    = e["boxplot"].get<bool>();
    }

    return cfg;
}

// -----------------------------------------------------------------------------
//  Private: enum parsers
// -----------------------------------------------------------------------------

TimeFormat ConfigLoader::_parseTimeFormat(const std::string& s)
{
    if (s == "index")          return TimeFormat::INDEX;
    if (s == "gpst_seconds")   return TimeFormat::GPS_TOTAL_SECONDS;
    if (s == "gpst_week_sow")  return TimeFormat::GPS_WEEK_SOW;
    if (s == "utc")            return TimeFormat::UTC;
    if (s == "mjd")            return TimeFormat::MJD;
    if (s == "unix")           return TimeFormat::UNIX;

    LOKI_WARNING("Config 'input.time_format' unknown value '"
                 + s + "' — using 'gpst_seconds'.");
    return TimeFormat::GPS_TOTAL_SECONDS;
}

MergeStrategy ConfigLoader::_parseMergeStrategy(const std::string& s)
{
    if (s == "separate") return MergeStrategy::SEPARATE;
    if (s == "merge")    return MergeStrategy::MERGE;

    LOKI_WARNING("Config 'input.merge_strategy' unknown value '"
                 + s + "' — using 'separate'.");
    return MergeStrategy::SEPARATE;
}

// -----------------------------------------------------------------------------
//  Private: path resolution
// -----------------------------------------------------------------------------

std::filesystem::path ConfigLoader::_resolvePath(const std::string&           raw,
                                                  const std::filesystem::path& baseDir)
{
    const std::filesystem::path p(raw);
    if (p.is_absolute()) {
        return p;
    }
    return baseDir / p;
}

} // namespace loki
