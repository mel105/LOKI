#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "loki/core/logger.hpp"

namespace loki {

// -----------------------------------------------------------------------------
//  TimeFormat
// -----------------------------------------------------------------------------

/**
 * @brief Specifies how the time column in an input file is encoded.
 */
enum class TimeFormat {
    INDEX,              ///< Sequential integer index (no absolute time).
    GPS_TOTAL_SECONDS,  ///< Seconds since GPS epoch 1980-01-06 (e.g. 1424771514).
    GPS_WEEK_SOW,       ///< Two columns: GPS week number + seconds of week.
    UTC,                ///< ISO string: "YYYY-MM-DD hh:mm:ss[.sss]".
    MJD,                ///< Modified Julian Date as a floating-point number.
    UNIX                ///< Seconds since Unix epoch 1970-01-01.
};

// -----------------------------------------------------------------------------
//  InputMode
// -----------------------------------------------------------------------------

/**
 * @brief Controls how the Loader discovers input files.
 */
enum class InputMode {
    SINGLE_FILE,    ///< Load exactly one file specified by InputConfig::file.
    FILE_LIST,      ///< Load an explicit list of files (InputConfig::files).
    SCAN_DIRECTORY  ///< Scan InputConfig::scanDir for all recognised extensions.
};

// -----------------------------------------------------------------------------
//  MergeStrategy
// -----------------------------------------------------------------------------

/**
 * @brief Controls how multiple input files are combined.
 *
 * Only relevant when InputMode is FILE_LIST or SCAN_DIRECTORY.
 */
enum class MergeStrategy {
    SEPARATE,  ///< Each file produces an independent TimeSeries.
    MERGE      ///< All files are concatenated and sorted chronologically.
};

// -----------------------------------------------------------------------------
//  InputConfig
// -----------------------------------------------------------------------------

/**
 * @brief Configuration for data loading.
 *
 * Paths are stored as absolute paths. ConfigLoader resolves relative paths
 * against AppConfig::workspace before populating this struct.
 */
struct InputConfig {

    InputMode   mode{InputMode::SINGLE_FILE};

    /// Path to a single input file (used when mode == SINGLE_FILE).
    std::filesystem::path file;

    /// List of input files (used when mode == FILE_LIST).
    std::vector<std::filesystem::path> files;

    /// Directory to scan for input files (used when mode == SCAN_DIRECTORY).
    std::filesystem::path scanDir;

    TimeFormat    timeFormat{TimeFormat::GPS_TOTAL_SECONDS};

    /// Column separator character.
    char delimiter{';'};

    /// Lines starting with this character are treated as comments.
    char commentChar{'%'};

    /**
     * @brief 1-based indices of the value columns to load.
     *
     * Column 1 is always the time column and must not appear here.
     * An empty vector means "load all value columns".
     *
     * Example: for a file with columns [TIME, SPEED_A, SPEED_B, TEMP],
     * setting columns = {2, 4} loads SPEED_A and TEMP.
     */
    std::vector<int> columns;

    MergeStrategy mergeStrategy{MergeStrategy::SEPARATE};
};

// -----------------------------------------------------------------------------
//  OutputConfig
// -----------------------------------------------------------------------------

/**
 * @brief Configuration for program output.
 *
 * Extended in future modules (gnuplot, CSV export).
 */
struct OutputConfig {
    LogLevel logLevel{LogLevel::INFO};
};

// -----------------------------------------------------------------------------
//  HomogeneityConfig
// -----------------------------------------------------------------------------

/**
 * @brief Algorithm parameters for the homogeneity analysis module.
 */
struct HomogeneityConfig {
    std::string method{"snht"};         ///< Detection method: "snht", "pettitt", "buishand".
    double significanceLevel{0.05};     ///< Statistical significance threshold (0.0–1.0).
};

// -----------------------------------------------------------------------------
//  PlotConfig
// -----------------------------------------------------------------------------

/**
 * @brief Configuration for plot output.
 *
 * Controls which plots are generated and in what format.
 * timeFormat overrides InputConfig::timeFormat for the x-axis when non-empty.
 */
struct PlotConfig {
    std::string outputFormat{"png"};  ///< Output format: "png" | "eps" | "svg".
    std::string timeFormat{""};       ///< X-axis time format. Empty = inherit from input.

    bool timeSeries {true};   ///< Enable time series line plot.
    bool comparison {false};  ///< Enable two-panel comparison plot.
    bool histogram  {true};   ///< Enable histogram with normal overlay.
    bool acf        {false};  ///< Enable ACF correlogram.
    bool qqPlot     {false};  ///< Enable normal Q-Q plot.
    bool boxplot    {false};  ///< Enable box-and-whisker plot.
};

// -----------------------------------------------------------------------------
//  AppConfig
// -----------------------------------------------------------------------------

/**
 * @brief Top-level application configuration.
 *
 * Populated by ConfigLoader::load() from a JSON file.
 * All file paths inside sub-configs are absolute.
 *
 * Expected directory layout under workspace:
 * @code
 *   <workspace>/
 *   ├── INPUT/     ← input time series files
 *   └── OUTPUT/
 *       ├── LOG/   ← log files
 *       ├── CSV/   ← exported results
 *       └── IMG/   ← gnuplot images
 * @endcode
 */
struct AppConfig {

    /// Root workspace directory. All relative paths are resolved against this.
    std::filesystem::path workspace;

    InputConfig       input;
    OutputConfig      output;
    PlotConfig        plots;
    HomogeneityConfig homogeneity;

    // ── Derived paths (computed by ConfigLoader) ──────────────────────────────

    /// Absolute path to the log output directory (<workspace>/OUTPUT/LOG).
    std::filesystem::path logDir;

    /// Absolute path to the CSV output directory (<workspace>/OUTPUT/CSV).
    std::filesystem::path csvDir;

    /// Absolute path to the image output directory (<workspace>/OUTPUT/IMG).
    std::filesystem::path imgDir;
};

} // namespace loki
