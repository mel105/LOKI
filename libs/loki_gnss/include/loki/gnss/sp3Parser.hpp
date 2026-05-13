#pragma once

#include <loki/gnss/gnssTypes.hpp>

#include <istream>
#include <string>

namespace loki::gnss {

/**
 * @brief Parser for SP3-c and SP3-d precise satellite orbit files.
 *
 * SP3 files contain satellite ECEF positions [km] and optional velocities
 * [dm/s] at regular intervals (typically 5 or 15 minutes).
 * Clock values are stored in microseconds.
 *
 * Both SP3-c and SP3-d formats are supported.  The version character in
 * the first header line determines the version.
 *
 * Header parsing: all lines until the first '*' epoch line are treated as
 * header regardless of line count.  The rigid 22-line assumption is NOT
 * used -- SP3-d files (e.g. CODE MGEX) have 30+ header lines.
 *
 * Sentinel values:
 *   Clock = 999999.999999  -> Sp3SatState::clockMissing = true.
 *   Position all-zero      -> Sp3SatState::posMissing   = true.
 *
 * Usage:
 * @code
 *   Sp3Parser parser;
 *   Sp3File sp3 = parser.parseGz("COD0MGXFIN_20240750000_01D_05M_ORB.SP3.gz");
 * @endcode
 */
class Sp3Parser {
public:
    Sp3Parser() = default;

    /**
     * @brief Parses an uncompressed SP3-c or SP3-d file.
     * @param filePath  Path to the .SP3 file.
     * @throws FileNotFoundException  if the file cannot be opened.
     * @throws ParseException         if the content is malformed.
     */
    [[nodiscard]] Sp3File parse(const std::string& filePath) const;

    /**
     * @brief Parses a gzip-compressed SP3 file (.SP3.gz).
     * @param filePath  Path to the .SP3.gz file.
     */
    [[nodiscard]] Sp3File parseGz(const std::string& filePath) const;

private:
    /// @brief Core parser -- reads header then epochs from any istream.
    [[nodiscard]] Sp3File parseStream(std::istream& stream,
                                      const std::string& source) const;

    /// @brief Parses the SP3 header (all lines up to but NOT including the
    ///        first '*' epoch line).  Fills version, coordinateSystem,
    ///        orbitType, agency, interval.  Leaves the stream positioned
    ///        at the first '*' line (already consumed from stream and
    ///        returned via firstEpochLine).
    void parseHeader(std::istream& stream, Sp3File& out,
                     const std::string& source,
                     std::string& firstEpochLine) const;

    /// @brief Parses one 'P' position record line into state.
    /// @return false if the line is malformed or satellite system unknown.
    static bool parsePosRecord(const std::string& line, Sp3SatState& state);

    /// @brief Converts a '*' epoch header line to GpsTime.
    static GpsTime parseEpochLine(const std::string& line,
                                   const std::string& source);

    /// @brief Decodes a 3-char satellite ID (e.g. "G01") to system + PRN.
    static void parseSatId(const std::string& id,
                            GnssSystem& system, int& prn);
};

} // namespace loki::gnss
