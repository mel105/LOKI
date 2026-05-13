#pragma once

#include <loki/gnss/gnssTypes.hpp>

#include <istream>
#include <string>

namespace loki::gnss {

/**
 * @brief Parser for RINEX CLK files (satellite and receiver clock corrections).
 *
 * Parses RINEX Clock Format versions 2 and 3.
 * Relevant record types:
 *   AS  -- analysis-center satellite clock (primary for PPP).
 *   AR  -- analysis-center receiver clock.
 *   CR, DR, MS -- calibration / monitor (parsed but rarely used).
 *
 * Record format (space-separated after the type and name fields):
 *   <type> <name> <YYYY> <MM> <DD> <hh> <mm> <ss.sss> <nvals> <bias> [sigma] [drift] ...
 *
 * The name field is right-trimmed of whitespace.  For satellite records
 * the name is a 3-char ID (e.g. "G01"); for receiver records it may be
 * up to 8 characters.
 *
 * Parsing is token-based (istringstream) to handle variable-width fields
 * robustly across different RINEX CLK producers.
 */
class ClkParser {
public:
    ClkParser() = default;

    /**
     * @brief Parses an uncompressed RINEX CLK file.
     * @throws FileNotFoundException  if the file cannot be opened.
     * @throws ParseException         on malformed content.
     */
    [[nodiscard]] ClkFile parse(const std::string& filePath) const;

    /**
     * @brief Parses a gzip-compressed RINEX CLK file.
     */
    [[nodiscard]] ClkFile parseGz(const std::string& filePath) const;

private:
    [[nodiscard]] ClkFile parseStream(std::istream& stream,
                                      const std::string& source) const;

    /// @brief Parses one data record line using token-based parsing.
    /// @return false if the line cannot be parsed (skip it).
    static bool parseDataRecord(const std::string& line, ClkRecord& rec);

    /// @brief Converts calendar fields to GpsTime (handles 2-digit years).
    static GpsTime makeGpsTime(int yr, int mo, int dy,
                                int hr, int mi, double sc);
};

} // namespace loki::gnss
