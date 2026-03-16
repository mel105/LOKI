#pragma once

#include "loki/core/config.hpp"
#include "loki/io/loader.hpp"

#include <vector>

namespace loki {

/**
 * @brief Orchestrates file discovery and loading based on AppConfig.
 *
 * DataManager is the single entry point for loading data in an application.
 * It reads InputConfig, decides which files to load (single / list / scan),
 * delegates to Loader, and applies the merge strategy.
 *
 * ### Merge strategies
 * - SEPARATE: each file is loaded independently — one LoadResult per file.
 * - MERGE: all files are concatenated into one LoadResult per column,
 *   then sorted chronologically.
 *
 * ### Error handling
 * If one file in a list or scan fails to load, a LOKI_ERROR is logged and
 * loading continues with the remaining files. If no file loads successfully,
 * a DataException is thrown.
 *
 * Usage example:
 * @code
 *   AppConfig cfg = ConfigLoader::load("config/loki_homogeneity.json");
 *   Logger::initDefault(cfg.logDir, "loki_homogeneity", cfg.output.logLevel);
 *
 *   DataManager dm(cfg);
 *   auto results = dm.load();
 *
 *   for (const auto& r : results) {
 *       LOKI_INFO("File: " + r.filePath.string()
 *                 + "  records: " + std::to_string(r.series[0].size()));
 *   }
 * @endcode
 */
class DataManager {
public:

    /**
     * @brief Constructs a DataManager with the given application configuration.
     * @param config Full application config (workspace + input settings used).
     */
    explicit DataManager(const AppConfig& config);

    /**
     * @brief Loads all configured input files and returns the results.
     *
     * @return Vector of LoadResult — one per file (SEPARATE) or one total (MERGE).
     * @throws DataException if no files were loaded successfully.
     */
    [[nodiscard]] std::vector<LoadResult> load() const;

private:

    AppConfig m_config;

    /// Collects file paths according to InputMode.
    [[nodiscard]] std::vector<std::filesystem::path> _collectFiles() const;

    /// Loads a list of files, skipping failures with LOKI_ERROR.
    [[nodiscard]] std::vector<LoadResult> _loadFiles(
        const std::vector<std::filesystem::path>& files) const;

    /// Merges a vector of LoadResults into a single chronologically sorted result.
    [[nodiscard]] static LoadResult _merge(std::vector<LoadResult> results);
};

} // namespace loki
