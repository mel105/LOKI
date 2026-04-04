#pragma once
#include <nlohmann/json.hpp>

#include "loki/core/config.hpp"

#include <filesystem>

namespace loki {

/**
 * @brief Loads and validates application configuration from a JSON file.
 *
 * All parsing is done via nlohmann::json. Missing optional keys fall back to
 * default values with a LOKI_WARNING. Missing required keys throw ConfigException.
 *
 * ### Required keys
 * - `workspace` -- absolute path to the workspace root directory.
 *
 * ### Optional keys (all others)
 * Every other key has a documented default value. A warning is logged when
 * a key is absent and its default is applied.
 *
 * ### Path resolution
 * Paths inside `input` (file, files, scan_directory) are interpreted as
 * relative to `<workspace>/INPUT/` unless they are already absolute.
 *
 * Usage example:
 * @code
 *   AppConfig cfg = ConfigLoader::load("config/outlier.json");
 *   Logger::initDefault(cfg.logDir, "loki_outlier", cfg.output.logLevel);
 * @endcode
 */
class ConfigLoader {
public:

    /**
     * @brief Parses a JSON config file and returns a fully populated AppConfig.
     * @param jsonPath Path to the JSON configuration file.
     * @return Populated AppConfig with all paths resolved to absolute form.
     * @throws FileNotFoundException if jsonPath does not exist.
     * @throws ParseException if the file is not valid JSON.
     * @throws ConfigException if a required key is missing.
     */
    static AppConfig load(const std::filesystem::path& jsonPath);

private:

    static InputConfig        _parseInput         (const nlohmann::json& j,
                                                  const std::filesystem::path& inputDir);
    static OutputConfig       _parseOutput        (const nlohmann::json& j);
    static HomogeneityConfig  _parseHomogeneity   (const nlohmann::json& j);
    static OutlierConfig      _parseOutlier       (const nlohmann::json& j);
    static FilterConfig       _parseFilter        (const nlohmann::json& j);
    static RegressionConfig   _parseRegression    (const nlohmann::json& j);
    static PlotConfig         _parsePlots         (const nlohmann::json& j);
    static StatsConfig        _parseStats         (const nlohmann::json& j);
    static StationarityConfig _parseStationarity  (const nlohmann::json& j);
    static TimeFormat         _parseTimeFormat    (const std::string& s);
    static MergeStrategy      _parseMergeStrategy (const std::string& s);
    static ArimaConfig        _parseArima         (const nlohmann::json& j);
    static SsaConfig          _parseSsa           (const nlohmann::json& j);

    /// Parses a shared OutlierFilterConfig block (used for pre/post outlier in homogeneity).
    static OutlierFilterConfig _parseOutlierFilter(const nlohmann::json& j,
                                                   double defaultMadMultiplier = 5.0);

    /// Resolves a path against baseDir if not already absolute.
    static std::filesystem::path _resolvePath(const std::string& raw,
                                              const std::filesystem::path& baseDir);
};

} // namespace loki