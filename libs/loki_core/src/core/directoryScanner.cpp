#include "loki/core/directoryScanner.hpp"

#include <array>

namespace {
constexpr std::array<std::string_view, 3> RECOGNISED_EXTENSIONS{".csv", ".txt", ".log"};
} // anonymous namespace

#include "loki/core/exceptions.hpp"
#include "loki/core/logger.hpp"

#include <algorithm>
#include <system_error>

using namespace loki;

namespace loki {

std::vector<std::filesystem::path>
DirectoryScanner::scan(const std::filesystem::path& directory)
{
    if (!std::filesystem::exists(directory)) {
        throw FileNotFoundException(
            "DirectoryScanner: directory not found: '" + directory.string() + "'.");
    }
    if (!std::filesystem::is_directory(directory)) {
        throw IoException(
            "DirectoryScanner: path is not a directory: '" + directory.string() + "'.");
    }

    std::vector<std::filesystem::path> results;

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {

        if (ec) {
            LOKI_WARNING("DirectoryScanner: cannot read entry in '"
                         + directory.string() + "': " + ec.message());
            ec.clear();
            continue;
        }

        // Skip subdirectories.
        if (!entry.is_regular_file()) {
            continue;
        }

        const auto& path = entry.path();

        // Skip hidden files (name starts with '.').
        if (path.filename().string().front() == '.') {
            continue;
        }

        if (isRecognised(path)) {
            results.push_back(path);
        }
    }

    // Sort alphabetically for reproducibility across platforms.
    std::sort(results.begin(), results.end());

    LOKI_INFO("DirectoryScanner: found " + std::to_string(results.size())
              + " file(s) in '" + directory.string() + "'.");

    return results;
}

bool DirectoryScanner::isRecognised(const std::filesystem::path& path)
{
    const std::string ext = path.extension().string();

    // Files with no extension are accepted.
    if (ext.empty()) {
        return true;
    }

    // Convert extension to lowercase for case-insensitive comparison.
    std::string extLower = ext;
    std::transform(extLower.begin(), extLower.end(), extLower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    for (const auto& recognised : RECOGNISED_EXTENSIONS) {
        if (extLower == recognised) {
            return true;
        }
    }
    return false;
}

} // namespace loki
