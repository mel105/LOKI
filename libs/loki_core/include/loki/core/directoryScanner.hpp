#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace loki {

/**
 * @brief Scans a directory for time series input files.
 *
 * Recognised extensions: .csv  .txt  .log  and files with no extension.
 * Scanning is non-recursive (top-level directory only).
 * Results are returned sorted alphabetically for reproducibility.
 */
class DirectoryScanner {
public:

    /**
     * @brief Scans a directory and returns paths of all recognised input files.
     *
     * Files with no extension are also included.
     * Hidden files (name starting with '.') are skipped.
     * Subdirectories are not traversed.
     *
     * @param directory Path to the directory to scan.
     * @return Alphabetically sorted list of absolute file paths.
     * @throws FileNotFoundException if the directory does not exist.
     * @throws IoException if the directory cannot be read.
     */
    [[nodiscard]] static std::vector<std::filesystem::path>
    scan(const std::filesystem::path& directory);

    /**
     * @brief Returns true if the given path has a recognised extension.
     *
     * Recognised: .csv, .txt, .log, or no extension.
     * Comparison is case-insensitive.
     *
     * @param path File path to test.
     */
    [[nodiscard]] static bool isRecognised(const std::filesystem::path& path);
};

} // namespace loki
