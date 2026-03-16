#pragma once

#include "loki/core/config.hpp"
#include "loki/timeseries/timeSeries.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace loki {

// ─────────────────────────────────────────────────────────────────────────────
//  LoadResult
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Result returned by Loader::load() for a single input file.
 */
struct LoadResult {

    /// One TimeSeries per loaded value column, in column order.
    std::vector<TimeSeries> series;

    /**
     * @brief Column names extracted from the file header or generated automatically.
     *
     * Parsed from a "% Columns: ..." comment line if present.
     * Falls back to "col_2", "col_3", ... when no header is found.
     * Index matches series: columnNames[i] describes series[i].
     */
    std::vector<std::string> columnNames;

    /// Source file path.
    std::filesystem::path filePath;

    /// Total lines read from the file (including comments and blank lines).
    std::size_t linesRead{0};

    /// Lines skipped: comments, blank lines, and malformed data lines.
    std::size_t linesSkipped{0};
};

// ─────────────────────────────────────────────────────────────────────────────
//  Loader
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Parses a single delimited text file into one or more TimeSeries.
 *
 * ### File format
 * - Lines beginning with the configured comment character are skipped.
 * - Blank lines are skipped.
 * - The first non-comment column on each data line is the time value.
 * - Remaining columns are numeric values.
 *
 * ### Column header detection
 * If a comment line matches the pattern "% Columns: NAME1, NAME2, ..."
 * (case-insensitive), column names are extracted from it.
 * Names in square brackets (units) are preserved: "WIG_SPEED[m/s]" stays as-is.
 * Config-supplied names take priority over detected names.
 *
 * ### Error handling
 * Malformed data lines (wrong column count, non-numeric values) are skipped
 * with a LOKI_WARNING. Parsing continues to the end of the file.
 *
 * Usage example:
 * @code
 *   Loader loader(cfg.input);
 *   LoadResult result = loader.load("/workspace/INPUT/sensor.csv");
 *   for (std::size_t i = 0; i < result.series.size(); ++i) {
 *       LOKI_INFO("Series '" + result.columnNames[i]
 *                 + "' has " + std::to_string(result.series[i].size()) + " points.");
 *   }
 * @endcode
 */
class Loader {
public:

    /**
     * @brief Constructs a Loader with the given input configuration.
     * @param config Input configuration (delimiter, comment char, time format, columns).
     */
    explicit Loader(const InputConfig& config);

    /**
     * @brief Parses the given file and returns a LoadResult.
     * @param filePath Absolute path to the input file.
     * @return LoadResult containing one TimeSeries per loaded value column.
     * @throws FileNotFoundException if filePath does not exist.
     * @throws IoException if the file cannot be opened.
     * @throws ParseException if no valid data lines are found.
     */
    [[nodiscard]] LoadResult load(const std::filesystem::path& filePath) const;

private:

    InputConfig m_config;

    // ── Internal helpers ──────────────────────────────────────────────────────

    /**
     * @brief Attempts to extract column names from a comment line.
     *
     * Looks for the pattern (case-insensitive):
     *   "% Columns: NAME1[unit], NAME2, NAME3"
     *
     * @param line  The raw comment line (including the comment character).
     * @param names Output vector populated with extracted names.
     * @return True if the pattern was matched and names were populated.
     */
    static bool _parseColumnHeader(const std::string&        line,
                                   char                      commentChar,
                                   std::vector<std::string>& names);

    /**
     * @brief Converts a raw time token to a TimeStamp using the configured TimeFormat.
     * @param token     The time field string from the data line.
     * @param nextToken Second token (only used for GPS_WEEK_SOW format).
     * @return Parsed TimeStamp.
     * @throws ParseException if the token cannot be converted.
     */
    [[nodiscard]] TimeStamp _parseTime(const std::string& token,
                                       const std::string& nextToken) const;

    /**
     * @brief Splits a line by the configured delimiter.
     * @param line Input line string.
     * @return Vector of field strings (may be empty strings for consecutive delimiters).
     */
    [[nodiscard]] std::vector<std::string> _splitLine(const std::string& line) const;
};

} // namespace loki
