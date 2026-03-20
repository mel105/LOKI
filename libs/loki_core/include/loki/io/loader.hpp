#pragma once

#include "loki/core/config.hpp"
#include "loki/timeseries/timeSeries.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace loki {

// -----------------------------------------------------------------------------
//  LoadResult
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
//  Loader
// -----------------------------------------------------------------------------

/**
 * @brief Parses a single delimited text file into one or more TimeSeries.
 *
 * ### File format
 * - Lines beginning with the configured comment character are skipped.
 * - Blank lines are skipped.
 * - The first non-comment column(s) on each data line form the time token.
 *   The number of time fields is determined by InputConfig::timeColumns.
 * - Remaining columns are numeric values.
 *
 * ### Time columns
 * By default (timeColumns empty), field 0 is the time token.
 * Set timeColumns = [0, 1] to join two fields before parsing, e.g. when
 * date and time are in separate columns: "1990-01-01" "00:00:00".
 *
 * ### Column header detection
 * If a comment line matches the pattern "% Columns: NAME1, NAME2, ..."
 * (case-insensitive), column names are extracted from it.
 *
 * ### Error handling
 * Malformed data lines are skipped with a LOKI_WARNING.
 *
 * Usage example:
 * @code
 *   Loader loader(cfg.input);
 *   LoadResult result = loader.load("/workspace/INPUT/data.txt");
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

    /**
     * @brief Attempts to extract column names from a comment line.
     * @param line        Raw comment line (including comment character).
     * @param commentChar The configured comment character.
     * @param names       Output vector populated with extracted names.
     * @return True if pattern was matched and names were populated.
     */
    static bool _parseColumnHeader(const std::string&        line,
                                   char                      commentChar,
                                   std::vector<std::string>& names);

    /**
     * @brief Converts raw field tokens to a TimeStamp using the configured TimeFormat.
     *
     * For UTC with timeColumns = [0, 1], joins fields[0] and fields[1] with a space.
     * For GPS_WEEK_SOW, uses fields[0] (week) and fields[1] (sow).
     * For all other formats, uses fields[0] only.
     *
     * @param fields All split fields from the current data line.
     * @return Parsed TimeStamp.
     * @throws ParseException if the token cannot be converted.
     */
    [[nodiscard]] TimeStamp _parseTime(const std::vector<std::string>& fields) const;

    /**
     * @brief Splits a line by the configured delimiter.
     * @param line Input line string.
     * @return Vector of field strings.
     */
    [[nodiscard]] std::vector<std::string> _splitLine(const std::string& line) const;
};

} // namespace loki