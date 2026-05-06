#pragma once

#include <loki/gnss/gnssTypes.hpp>

#include <istream>
#include <string>

namespace loki::gnss {

/**
 * @brief Configuration for RinexNavParser.
 *
 * Defined outside the class to work around a GCC 13 / Windows MINGW64 bug
 * where nested structs with default member initialisers cannot be used as
 * default arguments in the enclosing class constructor.
 */
struct RinexNavParserConfig {
    bool skipUnhealthy{false}; ///< Skip ephemerides with SVhealth != 0.
    bool verbose{false};       ///< Print per-record diagnostics to stderr.

    explicit RinexNavParserConfig(bool skipUnhealthy_ = false,
                                  bool verbose_       = false)
        : skipUnhealthy(skipUnhealthy_)
        , verbose(verbose_)
    {}
};

/**
 * @brief Parser for RINEX 2.x and 3.x broadcast navigation files.
 *
 * Supports all constellations present in RINEX 3 mixed navigation files:
 * GPS, GLONASS, Galileo, BeiDou, SBAS.
 *
 * RINEX version is detected automatically from the file header -- the caller
 * does not need to know or specify the version.
 *
 * Usage:
 * @code
 *   RinexNavParser parser;
 *   NavFile nav = parser.parse("/path/to/BRDC00IGS_R_20240750000_01D_MN.rnx");
 *   NavFile nav = parser.parseGz("/path/to/BRDC00IGS_R_20240750000_01D_MN.rnx.gz");
 * @endcode
 */
class RinexNavParser {
public:
    using Config = RinexNavParserConfig;

    /// @brief Constructs the parser with the given configuration.
    explicit RinexNavParser(Config cfg = Config{});

    /**
     * @brief Parses an uncompressed RINEX 2.x or 3.x navigation file.
     *
     * @param filePath Absolute or relative path to the .rnx file.
     * @return Populated NavFile containing all parsed ephemerides.
     * @throws FileNotFoundException if the file cannot be opened.
     * @throws ParseException        if the header or any record is malformed.
     */
    [[nodiscard]] NavFile parse(const std::string& filePath) const;

    /**
     * @brief Parses a gzip-compressed RINEX navigation file (.rnx.gz).
     *
     * Decompresses via "gzip -d -c <file>" piped to the parser.
     * Requires gzip to be available on PATH.
     *
     * @param filePath Absolute or relative path to the .rnx.gz file.
     * @return Populated NavFile.
     * @throws FileNotFoundException if the file cannot be opened or gzip fails.
     * @throws ParseException        if the decompressed content is malformed.
     */
    [[nodiscard]] NavFile parseGz(const std::string& filePath) const;

private:
    Config m_cfg;

    // -------------------------------------------------------------------------
    //  Internal parsing stages
    // -------------------------------------------------------------------------

    /// @brief Parses the nav file from an already-open stream.
    [[nodiscard]] NavFile parseStream(std::istream& stream,
                                      const std::string& sourceName) const;

    /// @brief Reads and processes the RINEX header section.
    /// Returns the detected RINEX major version (2 or 3).
    int parseHeader(std::istream& stream, NavFile& out,
                    const std::string& sourceName) const;

    // -- RINEX 3 record parsers -----------------------------------------------

    /// @brief Dispatches one RINEX 3 nav record by system character.
    void parseRecord3(char systemChar, const std::string& firstLine,
                      std::istream& stream, NavFile& out,
                      const std::string& sourceName) const;

    void parseGpsRecord3(const std::string& firstLine, std::istream& stream,
                         NavFile& out, const std::string& sourceName) const;

    void parseGalRecord3(const std::string& firstLine, std::istream& stream,
                         NavFile& out, const std::string& sourceName) const;

    void parseGloRecord3(const std::string& firstLine, std::istream& stream,
                         NavFile& out, const std::string& sourceName) const;

    void parseBdsRecord3(const std::string& firstLine, std::istream& stream,
                         NavFile& out, const std::string& sourceName) const;

    void parseSbasRecord3(const std::string& firstLine, std::istream& stream,
                          NavFile& out, const std::string& sourceName) const;

    // -- RINEX 2 record parsers -----------------------------------------------

    void parseGpsRecord2(const std::string& firstLine, std::istream& stream,
                         NavFile& out, const std::string& sourceName) const;

    void parseGloRecord2(const std::string& firstLine, std::istream& stream,
                         NavFile& out, const std::string& sourceName) const;

    // -- Low-level helpers ----------------------------------------------------

    /// @brief Reads nLines broadcast record lines; returns up to nLines*4 doubles.
    std::vector<double> readBroadcastLines(std::istream& stream, int nLines,
                                           const std::string& sourceName) const;

    static void parseSvEpochLine3(const std::string& line,
                                  char& systemChar, int& prn,
                                  int& yr, int& mo, int& dy,
                                  int& hr, int& mi, double& sec);

    static void parsePrnEpochLine2(const std::string& line,
                                   int& prn,
                                   int& yr, int& mo, int& dy,
                                   int& hr, int& mi, double& sec);

    /// @brief Parses one 19-char RINEX D-format field (handles Fortran D exponent).
    static double parseRinexDouble(const std::string& field);

    /// @brief Converts RINEX epoch fields to GpsTime (handles 2-digit years).
    static GpsTime epochToGpsTime(int yr, int mo, int dy,
                                  int hr, int mi, double sec);
};

} // namespace loki::gnss
