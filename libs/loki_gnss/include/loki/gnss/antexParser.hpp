#pragma once

#include <loki/gnss/gnssTypes.hpp>

#include <string>

namespace loki::gnss {

/**
 * @brief Parser for IGS ANTEX antenna calibration files.
 *
 * ANTEX (ANTenna EXchange format) stores phase center offsets (PCO) and
 * phase center variations (PCV) for satellite and receiver antennas.
 *
 * Supported blocks:
 *   - Satellite antennas  (TYPE = BLOCK IIF, IIR, ..., IOV, FOC, ...)
 *   - Receiver antennas   (TYPE = individual or type-mean)
 *
 * Each AntennaCalib entry holds one or more AntennaFreq structs with:
 *   - PCO in E/N/U [mm] for receiver or X/Y/Z [mm] for satellite.
 *   - PCV grid: NOAZI row + optional azimuth-dependent rows.
 *
 * ANTEX files are large ASCII text files and are never gzip-compressed
 * in IGS distribution -- parseGz() is therefore NOT provided.
 *
 * Usage:
 * @code
 *   AntexParser parser;
 *   AntexFile antex = parser.parse("igs20.atx");
 *   // Lookup satellite antenna by SVN code.
 *   // Lookup receiver antenna by antenna type string from OBS header.
 * @endcode
 */
class AntexParser {
public:
    AntexParser() = default;

    /**
     * @brief Parses an ANTEX file.
     * @param filePath  Path to the .atx file.
     * @return          Populated AntexFile.
     * @throws FileNotFoundException  if the file cannot be opened.
     * @throws ParseException         on malformed content.
     */
    [[nodiscard]] AntexFile parse(const std::string& filePath) const;

private:
    [[nodiscard]] AntexFile parseStream(std::istream& stream,
                                        const std::string& source) const;

    /**
     * @brief Parses one antenna block (from START OF ANTENNA to END OF ANTENNA).
     *
     * @param stream  Input stream positioned just after "START OF ANTENNA".
     * @param source  File name for error messages.
     * @param calib   Output calibration entry.
     * @return        True if parsed successfully, false if END OF ANTENNA not found.
     */
    static bool parseAntennaBlock(std::istream& stream,
                                   const std::string& source,
                                   AntennaCalib& calib);

    /**
     * @brief Parses one frequency block (from START OF FREQUENCY to END OF FREQUENCY).
     * @return AntennaFreq with PCO and PCV filled.
     */
    static AntennaFreq parseFreqBlock(std::istream& stream,
                                       const std::string& source);

    /**
     * @brief Parses a NOAZI or azimuth PCV row.
     * @param line    The data line starting with "NOAZI" or an azimuth value.
     * @return        Vector of PCV values [mm].
     */
    static std::vector<double> parsePcvRow(const std::string& line);
};

} // namespace loki::gnss
