#include "loki/io/dataManager.hpp"

#include "loki/core/directoryScanner.hpp"
#include "loki/core/exceptions.hpp"
#include "loki/core/logger.hpp"

using namespace loki;

namespace loki {

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor
// ─────────────────────────────────────────────────────────────────────────────

DataManager::DataManager(const AppConfig& config)
    : m_config(config)
{}

// ─────────────────────────────────────────────────────────────────────────────
//  load
// ─────────────────────────────────────────────────────────────────────────────

std::vector<LoadResult> DataManager::load() const
{
    const auto files = _collectFiles();

    if (files.empty()) {
        throw DataException("DataManager: no input files found.");
    }

    auto results = _loadFiles(files);

    if (results.empty()) {
        throw DataException("DataManager: all input files failed to load.");
    }

    if (m_config.input.mergeStrategy == MergeStrategy::MERGE && results.size() > 1) {
        LOKI_INFO("DataManager: merging " + std::to_string(results.size())
                  + " LoadResult(s) into one.");
        results = { _merge(std::move(results)) };
    }

    LOKI_INFO("DataManager: load complete — "
              + std::to_string(results.size()) + " result(s).");

    return results;
}

// ─────────────────────────────────────────────────────────────────────────────
//  _collectFiles
// ─────────────────────────────────────────────────────────────────────────────

std::vector<std::filesystem::path> DataManager::_collectFiles() const
{
    switch (m_config.input.mode) {

        case InputMode::SINGLE_FILE:
            LOKI_INFO("DataManager: single file mode — '"
                      + m_config.input.file.string() + "'.");
            return { m_config.input.file };

        case InputMode::FILE_LIST:
            LOKI_INFO("DataManager: file list mode — "
                      + std::to_string(m_config.input.files.size()) + " file(s).");
            return m_config.input.files;

        case InputMode::SCAN_DIRECTORY:
            LOKI_INFO("DataManager: scan directory mode — '"
                      + m_config.input.scanDir.string() + "'.");
            return DirectoryScanner::scan(m_config.input.scanDir);
    }

    // Unreachable.
    return {};
}

// ─────────────────────────────────────────────────────────────────────────────
//  _loadFiles
// ─────────────────────────────────────────────────────────────────────────────

std::vector<LoadResult> DataManager::_loadFiles(
    const std::vector<std::filesystem::path>& files) const
{
    Loader loader(m_config.input);
    std::vector<LoadResult> results;
    results.reserve(files.size());

    for (const auto& file : files) {
        try {
            results.push_back(loader.load(file));
        } catch (const LOKIException& e) {
            LOKI_ERROR("DataManager: failed to load '"
                       + file.string() + "': " + e.what());
        }
    }

    return results;
}

// ─────────────────────────────────────────────────────────────────────────────
//  _merge
// ─────────────────────────────────────────────────────────────────────────────

LoadResult DataManager::_merge(std::vector<LoadResult> results)
{
    // Use the first result as the base — inherit its column names and metadata.
    LoadResult merged = std::move(results.front());

    const std::size_t numCols = merged.series.size();

    for (std::size_t ri = 1; ri < results.size(); ++ri) {
        auto& r = results[ri];

        if (r.series.size() != numCols) {
            LOKI_WARNING("DataManager::_merge: file '"
                         + r.filePath.string()
                         + "' has " + std::to_string(r.series.size())
                         + " column(s) but expected " + std::to_string(numCols)
                         + " — skipping this file in merge.");
            continue;
        }

        for (std::size_t ci = 0; ci < numCols; ++ci) {
            // Append all observations from this file into the merged series.
            for (std::size_t oi = 0; oi < r.series[ci].size(); ++oi) {
                const auto& obs = r.series[ci][oi];
                merged.series[ci].append(obs.time, obs.value, obs.flag);
            }
        }

        merged.linesRead    += r.linesRead;
        merged.linesSkipped += r.linesSkipped;
    }

    // Sort all merged series chronologically.
    for (auto& s : merged.series) {
        if (!s.isSorted()) {
            s.sortByTime();
        }
    }

    LOKI_INFO("DataManager: merged series has "
              + std::to_string(merged.series.empty() ? 0 : merged.series[0].size())
              + " records.");

    return merged;
}

} // namespace loki
