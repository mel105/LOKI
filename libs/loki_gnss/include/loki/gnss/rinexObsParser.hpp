#pragma once

#include <loki/gnss/gnssTypes.hpp>

#include <istream>
#include <string>
#include <vector>

namespace loki::gnss {

/**
 * @brief Configuration for RinexObsParser.
 *
 * Defined outside the class to work around a GCC 13 / Windows MINGW64 bug
 * where nested structs with default member initialisers cannot be used as
 * default arguments in the enclosing class constructor.
 */
struct RinexObsParserConfig {
    /// Path to the CRX2RNX binary (Hatanaka decompressor, GSI Japan).
    /// On Windows ".exe" is appended automatically if absent.
    std::string crx2rnxPath{"tools/hatanaka/CRX2RNX"};
    /// Print per-epoch diagnostics to stderr during parsing.
    bool verbose{false};

    explicit RinexObsParserConfig(
        std::string crx2rnxPath_ = "tools/hatanaka/CRX2RNX",
        bool        verbose_     = false)
        : crx2rnxPath(std::move(crx2rnxPath_))
        , verbose(verbose_)
    {}
};

/**
 * @brief Parser for RINEX 3.x observation files, including Hatanaka-compressed
 *        (.crx) and gzip-compressed (.crx.gz or .rnx.gz) variants.
 *
 * Decompression pipeline for .crx.gz input:
 *   1. gzip -d -c  : decompress to a temporary file
 *   2. CRX2RNX     : convert Hatanaka differential encoding to standard RINEX 3
 *   3. parseStream : parse the resulting RINEX 3 OBS stream
 *
 * Temporary files are written to the system temp directory and deleted
 * immediately after parsing.
 *
 * Usage:
 * @code
 *   RinexObsParser::Config cfg;
 *   cfg.crx2rnxPath = "tools/hatanaka/CRX2RNX";
 *   RinexObsParser parser(cfg);
 *   ObsFile obs = parser.parseGz("/path/to/GOPE00CZE_R_20240750000_01D_30S_MO.crx.gz");
 * @endcode
 */
class RinexObsParser {
public:
    using Config = RinexObsParserConfig;

    /// @brief Constructs the parser with the given configuration.
    explicit RinexObsParser(Config cfg = Config{});

    /**
     * @brief Parses an uncompressed RINEX 3.x OBS file (.rnx).
     *
     * @param filePath Absolute or relative path to the .rnx file.
     * @return Populated ObsFile.
     * @throws FileNotFoundException if the file cannot be opened.
     * @throws ParseException        if the header or any epoch is malformed.
     */
    [[nodiscard]] ObsFile parse(const std::string& filePath) const;

    /**
     * @brief Parses a gzip-compressed RINEX OBS or Hatanaka file.
     *
     * Accepted: .rnx.gz (gzip only) and .crx.gz (Hatanaka + gzip).
     * Hatanaka detection is automatic via the file's first header line.
     *
     * @param filePath Path to the compressed file.
     * @return Populated ObsFile.
     * @throws FileNotFoundException if the file or CRX2RNX binary is missing.
     * @throws ParseException        if decompression or parsing fails.
     */
    [[nodiscard]] ObsFile parseGz(const std::string& filePath) const;

    // -- Static helpers exposed for use in main / tests -----------------------

    /// @brief Parses the epoch flag line ("> YYYY MM DD hh mm ss flag nSat").
    static bool parseEpochLine(const std::string& line,
                               GpsTime& time, int& epochFlag, int& numSat);

    /// @brief Parses one satellite observation line given the obs code list.
    static SatObs parseSatLine(const std::string& satId,
                               const std::string& line,
                               const std::vector<std::string>& obsCodes);

    /// @brief Splits a 3-char satellite ID (e.g. "G01") into system + PRN.
    static void parseSatId(const std::string& satId,
                           GnssSystem& system, int& prn);

    /// @brief Extracts one 16-char obs field (14-char value + LLI + SNR).
    static ObsValue extractObsField(const std::string& line, std::size_t col);

    /// @brief Returns a unique path in the system temp directory.
    static std::string makeTempPath(const std::string& suffix);

private:
    Config m_cfg;

    // -- Core parsing ---------------------------------------------------------

    /// @brief Drives header + epoch parsing; owns the obs-code map.
    [[nodiscard]] ObsFile parseStream(std::istream& stream,
                                      const std::string& sourceName) const;

    // -- Decompression --------------------------------------------------------

    /// @brief Runs gzip -d -c and writes output to a temp file.
    std::string decompressGzip(const std::string& gzPath) const;

    /// @brief Runs CRX2RNX on a .crx file and writes output to a temp .rnx file.
    std::string convertHatanaka(const std::string& crxPath) const;

    /// @brief Resolves the CRX2RNX binary path; appends .exe on Windows.
    [[nodiscard]] std::string resolveCrx2rnx() const;

    // -- Declared stubs (logic lives in parseStream) --------------------------
    void parseHeader(std::istream& stream, ObsFile& out,
                     const std::string& sourceName) const;
    bool parseEpoch(std::istream& stream, ObsFile& out,
                    const std::string& sourceName) const;
};

} // namespace loki::gnss
