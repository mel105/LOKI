#include <loki/gnss/rinexNavParser.hpp>
#include <loki/core/exceptions.hpp>

#include <array>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace loki;
using namespace loki::gnss;

// =============================================================================
//  Anonymous namespace -- file-local helpers
// =============================================================================
namespace {

/// @brief Returns a substring of s, or "" if the range is out of bounds.
std::string safeSub(const std::string& s, std::size_t pos, std::size_t len) {
    if (pos >= s.size()) return "";
    return s.substr(pos, std::min(len, s.size() - pos));
}

/// @brief Trims leading and trailing whitespace from a string.
std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

/// @brief Deleter for FILE* opened via popen.
struct PipeCloser {
    void operator()(FILE* fp) const { if (fp) ::pclose(fp); }
};

using UniquePipe = std::unique_ptr<FILE, PipeCloser>;

} // namespace

// =============================================================================
//  RinexNavParser -- constructor
// =============================================================================

RinexNavParser::RinexNavParser(Config cfg)
    : m_cfg(cfg) {}

// =============================================================================
//  Public interface
// =============================================================================

NavFile RinexNavParser::parse(const std::string& filePath) const {
    std::ifstream ifs(filePath);
    if (!ifs.is_open()) {
        throw FileNotFoundException("RinexNavParser: cannot open file: " + filePath);
    }
    return parseStream(ifs, filePath);
}

NavFile RinexNavParser::parseGz(const std::string& filePath) const {
    // Decompress via gzip -d -c (writes to stdout, file unchanged)
    const std::string cmd = "gzip -d -c \"" + filePath + "\"";
    UniquePipe pipe(::popen(cmd.c_str(), "r"));
    if (!pipe) {
        throw FileNotFoundException(
            "RinexNavParser: cannot open gzip pipe for: " + filePath);
    }

    // Buffer the decompressed bytes into a string stream
    std::string buffer;
    buffer.reserve(1 << 20); // 1 MB initial reserve
    std::array<char, 4096> chunk{};
    while (std::fgets(chunk.data(), static_cast<int>(chunk.size()), pipe.get())) {
        buffer += chunk.data();
    }

    if (buffer.empty()) {
        throw ParseException(
            "RinexNavParser: gzip produced no output for: " + filePath);
    }

    std::istringstream iss(std::move(buffer));
    return parseStream(iss, filePath);
}

// =============================================================================
//  parseStream -- top-level dispatcher
// =============================================================================

NavFile RinexNavParser::parseStream(std::istream& stream,
                                     const std::string& sourceName) const {
    NavFile out;
    const int version = parseHeader(stream, out, sourceName);

    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        if (version >= 3) {
            // RINEX 3: first character is the GNSS system identifier
            const char sys = line.empty() ? ' ' : line[0];
            if (sys == 'G' || sys == 'R' || sys == 'E' ||
                sys == 'C' || sys == 'S' || sys == 'J') {
                try {
                    parseRecord3(sys, line, stream, out, sourceName);
                } catch (const ParseException& ex) {
                    if (m_cfg.verbose) {
                        std::cerr << "[RinexNavParser] skipping record: "
                                  << ex.what() << "\n";
                    }
                }
            }
            // Lines not starting with a system char are continuation lines
            // or blank -- already consumed by record parsers; skip here.

        } else {
            // RINEX 2: GPS or GLONASS single-constellation file
            // First character: digit or space = GPS PRN (satellite nav)
            //                  digit          = GLONASS slot (geo nav)
            // Distinguish by the constellation recorded during header parse.
            // We use a simple heuristic: if the NavFile already has gloEph
            // or the first char is a digit in columns 0-2, try both.
            // The header parse sets rinexVersion with a suffix for GLO ("2G").
            const bool isGlo = (out.rinexVersion.size() > 1 &&
                                 out.rinexVersion[1] == 'G');
            try {
                if (isGlo) {
                    parseGloRecord2(line, stream, out, sourceName);
                } else {
                    parseGpsRecord2(line, stream, out, sourceName);
                }
            } catch (const ParseException& ex) {
                if (m_cfg.verbose) {
                    std::cerr << "[RinexNavParser] skipping record: "
                              << ex.what() << "\n";
                }
            }
        }
    }

    return out;
}

// =============================================================================
//  parseHeader
// =============================================================================

int RinexNavParser::parseHeader(std::istream& stream, NavFile& out,
                                 const std::string& sourceName) const {
    int majorVersion = 3;
    std::string line;

    while (std::getline(stream, line)) {
        if (line.size() < 60) line.resize(80, ' ');

        const std::string label = trim(safeSub(line, 60, 20));

        if (label == "RINEX VERSION / TYPE") {
            const std::string verStr = trim(safeSub(line, 0, 9));
            out.rinexVersion = verStr;
            majorVersion = (verStr.empty() ? 3 : static_cast<int>(std::stod(verStr)));

            // For RINEX 2, record the constellation type for later dispatch:
            // column 40 = file system ('G'=GPS, 'R'=GLONASS, 'M'=mixed, etc.)
            const char sysChar = (line.size() > 40) ? line[40] : ' ';
            if (majorVersion < 3 && (sysChar == 'R' || sysChar == 'r')) {
                out.rinexVersion += "G"; // mark GLO in version string (internal flag)
            }

        } else if (label == "IONOSPHERIC CORR" || label == "ION ALPHA" ||
                   label == "ION BETA") {
            // RINEX 3 uses "IONOSPHERIC CORR" with a type tag in cols 0-3
            // RINEX 2 uses separate "ION ALPHA" / "ION BETA" labels
            const std::string corrType = trim(safeSub(line, 0, 4));

            if (corrType == "GPSA" || label == "ION ALPHA") {
                for (int i = 0; i < 4; ++i) {
                    out.ionoAlpha[static_cast<std::size_t>(i)] =
                        parseRinexDouble(safeSub(line, 5 + i * 12, 12));
                }
            } else if (corrType == "GPSB" || label == "ION BETA") {
                for (int i = 0; i < 4; ++i) {
                    out.ionoBeta[static_cast<std::size_t>(i)] =
                        parseRinexDouble(safeSub(line, 5 + i * 12, 12));
                }
            } else if (corrType == "GAL") {
                for (int i = 0; i < 3; ++i) {
                    out.nequickAi[static_cast<std::size_t>(i)] =
                        parseRinexDouble(safeSub(line, 5 + i * 12, 12));
                }
            }

        } else if (label == "END OF HEADER") {
            break;
        }
        // All other header records are silently skipped.
    }

    if (majorVersion < 2 || majorVersion > 4) {
        throw ParseException("RinexNavParser: unsupported RINEX version in: " +
                             sourceName);
    }
    return majorVersion;
}

// =============================================================================
//  parseRecord3 -- RINEX 3 dispatcher
// =============================================================================

void RinexNavParser::parseRecord3(char systemChar, const std::string& firstLine,
                                   std::istream& stream, NavFile& out,
                                   const std::string& sourceName) const {
    switch (systemChar) {
        case 'G': parseGpsRecord3 (firstLine, stream, out, sourceName); break;
        case 'J': parseGpsRecord3 (firstLine, stream, out, sourceName); break; // QZSS same struct
        case 'E': parseGalRecord3 (firstLine, stream, out, sourceName); break;
        case 'R': parseGloRecord3 (firstLine, stream, out, sourceName); break;
        case 'C': parseBdsRecord3 (firstLine, stream, out, sourceName); break;
        case 'S': parseSbasRecord3(firstLine, stream, out, sourceName); break;
        default:  break; // Unknown system -- skip gracefully
    }
}

// =============================================================================
//  RINEX 3 -- GPS record
//  Structure: 1 epoch line + 7 broadcast lines (af0/af1/af2 + 7x4 params)
// =============================================================================

void RinexNavParser::parseGpsRecord3(const std::string& firstLine,
                                      std::istream& stream, NavFile& out,
                                      const std::string& sourceName) const {
    char sys{}; int prn{}, yr{}, mo{}, dy{}, hr{}, mi{};
    double sec{};
    parseSvEpochLine3(firstLine, sys, prn, yr, mo, dy, hr, mi, sec);

    // Clock corrections are on the epoch line (cols 23-60)
    const double af0 = parseRinexDouble(safeSub(firstLine, 23, 19));
    const double af1 = parseRinexDouble(safeSub(firstLine, 42, 19));
    const double af2 = parseRinexDouble(safeSub(firstLine, 61, 19));

    // 7 broadcast record lines, each with up to 4 fields of 19 chars
    const auto f = readBroadcastLines(stream, 7, sourceName);
    // f[0..3]  = line 1: IODE, Crs, deltaN, M0
    // f[4..7]  = line 2: Cuc, e, Cus, sqrtA
    // f[8..11] = line 3: toe_sow, Cic, Omega0, Cis
    // f[12..15]= line 4: i0, Crc, omega, OmegaDot
    // f[16..19]= line 5: IDOT, CodesOnL2, GPSweek, L2PFlag
    // f[20..23]= line 6: URA, SVhealth, TGD, IODC
    // f[24..27]= line 7: TransmTime, fitInterval, (spare), (spare)

    GpsBroadcastEph eph;
    eph.prn  = prn;
    eph.toc  = epochToGpsTime(yr, mo, dy, hr, mi, sec);
    eph.af0  = af0;
    eph.af1  = af1;
    eph.af2  = af2;

    eph.IODE     = f[0];
    eph.Crs      = f[1];
    eph.deltaN   = f[2];
    eph.M0       = f[3];

    eph.Cuc      = f[4];
    eph.e        = f[5];
    eph.Cus      = f[6];
    eph.sqrtA    = f[7];

    // toe: GPS week from line 5, sow from line 3
    const double toeSow  = f[8];
    eph.Cic      = f[9];
    eph.Omega0   = f[10];
    eph.Cis      = f[11];

    eph.i0       = f[12];
    eph.Crc      = f[13];
    eph.omega    = f[14];
    eph.OmegaDot = f[15];

    eph.IDOT     = f[16];
    eph.week     = f[18]; // GPS week of toe (from broadcast)

    eph.toe = GpsTime{ static_cast<int>(eph.week), toeSow };

    eph.URA      = f[20];
    eph.SVhealth = static_cast<int>(f[21]);
    eph.TGD      = f[22];
    eph.IODC     = f[23];

    eph.fitInterval = f[25];

    if (m_cfg.skipUnhealthy && eph.SVhealth != 0) return;

    out.gpsEph.push_back(eph);
}

// =============================================================================
//  RINEX 3 -- Galileo record
//  Structure: 1 epoch line + 7 broadcast lines
// =============================================================================

void RinexNavParser::parseGalRecord3(const std::string& firstLine,
                                      std::istream& stream, NavFile& out,
                                      const std::string& sourceName) const {
    char sys{}; int prn{}, yr{}, mo{}, dy{}, hr{}, mi{};
    double sec{};
    parseSvEpochLine3(firstLine, sys, prn, yr, mo, dy, hr, mi, sec);

    const double af0 = parseRinexDouble(safeSub(firstLine, 23, 19));
    const double af1 = parseRinexDouble(safeSub(firstLine, 42, 19));
    const double af2 = parseRinexDouble(safeSub(firstLine, 61, 19));

    const auto f = readBroadcastLines(stream, 7, sourceName);
    // Line 1: IODnav, Crs, deltaN, M0
    // Line 2: Cuc, e, Cus, sqrtA
    // Line 3: toe_sow, Cic, Omega0, Cis
    // Line 4: i0, Crc, omega, OmegaDot
    // Line 5: IDOT, DataSrc, GALweek, (spare)
    // Line 6: SISA, SVhealth, BGDe5a, BGDe5b
    // Line 7: TransmTime, (spare), (spare), (spare)

    GalBroadcastEph eph;
    eph.prn  = prn;
    eph.toc  = epochToGpsTime(yr, mo, dy, hr, mi, sec);
    eph.af0  = af0;
    eph.af1  = af1;
    eph.af2  = af2;

    eph.IODNAV   = f[0];
    eph.Crs      = f[1];
    eph.deltaN   = f[2];
    eph.M0       = f[3];

    eph.Cuc      = f[4];
    eph.e        = f[5];
    eph.Cus      = f[6];
    eph.sqrtA    = f[7];

    const double toeSow = f[8];
    eph.Cic      = f[9];
    eph.Omega0   = f[10];
    eph.Cis      = f[11];

    eph.i0       = f[12];
    eph.Crc      = f[13];
    eph.omega    = f[14];
    eph.OmegaDot = f[15];

    eph.IDOT     = f[16];
    eph.week     = f[18];
    eph.toe      = GpsTime{ static_cast<int>(eph.week), toeSow };

    eph.SISA     = f[20];
    eph.SVhealth = static_cast<int>(f[21]);
    eph.BGDe5a   = f[22];
    eph.BGDe5b   = f[23];

    // Determine message type from DataSrc bitmask (f[17])
    const int dataSrc = static_cast<int>(f[17]);
    if ((dataSrc & 0x0200) != 0) {
        eph.msgType = NavMsgType::FNAV; // bit 9: E5a FNAV
    } else {
        eph.msgType = NavMsgType::INAV; // default: E1/E5b INAV
    }

    if (m_cfg.skipUnhealthy && eph.SVhealth != 0) return;

    out.galEph.push_back(eph);
}

// =============================================================================
//  RINEX 3 -- GLONASS record
//  Structure: 1 epoch line + 3 broadcast lines (state vector)
// =============================================================================

void RinexNavParser::parseGloRecord3(const std::string& firstLine,
                                      std::istream& stream, NavFile& out,
                                      const std::string& sourceName) const {
    char sys{}; int prn{}, yr{}, mo{}, dy{}, hr{}, mi{};
    double sec{};
    parseSvEpochLine3(firstLine, sys, prn, yr, mo, dy, hr, mi, sec);

    // Clock: -tauN (col 23-41), +gammaN (col 42-60), MessageFrameTime (col 61-79)
    const double neg_tauN  = parseRinexDouble(safeSub(firstLine, 23, 19));
    const double gammaN    = parseRinexDouble(safeSub(firstLine, 42, 19));

    const auto f = readBroadcastLines(stream, 3, sourceName);
    // Line 1: X [km], Xdot [km/s], Xdotdot [km/s^2], health
    // Line 2: Y [km], Ydot,         Ydotdot,           freqChannel
    // Line 3: Z [km], Zdot,         Zdotdot,           age

    GloBroadcastEph eph;
    eph.prn    = prn;
    eph.toe    = epochToGpsTime(yr, mo, dy, hr, mi, sec);
    eph.tauN   = -neg_tauN;  // RINEX stores -tauN; we store tauN
    eph.gammaN = gammaN;

    eph.x  = f[0];  eph.vx = f[1];  eph.ax = f[2];
    eph.health = static_cast<int>(f[3]);

    eph.y  = f[4];  eph.vy = f[5];  eph.ay = f[6];
    eph.freqCh = static_cast<int>(f[7]);

    eph.z  = f[8];  eph.vz = f[9];  eph.az = f[10];
    eph.age = static_cast<int>(f[11]);

    if (m_cfg.skipUnhealthy && eph.health != 0) return;

    out.gloEph.push_back(eph);
}

// =============================================================================
//  RINEX 3 -- BeiDou record
//  Structure: 1 epoch line + 7 broadcast lines (same layout as GPS)
// =============================================================================

void RinexNavParser::parseBdsRecord3(const std::string& firstLine,
                                      std::istream& stream, NavFile& out,
                                      const std::string& sourceName) const {
    char sys{}; int prn{}, yr{}, mo{}, dy{}, hr{}, mi{};
    double sec{};
    parseSvEpochLine3(firstLine, sys, prn, yr, mo, dy, hr, mi, sec);

    const double af0 = parseRinexDouble(safeSub(firstLine, 23, 19));
    const double af1 = parseRinexDouble(safeSub(firstLine, 42, 19));
    const double af2 = parseRinexDouble(safeSub(firstLine, 61, 19));

    const auto f = readBroadcastLines(stream, 7, sourceName);
    // Line 1: AODE, Crs, deltaN, M0
    // Line 2: Cuc, e, Cus, sqrtA
    // Line 3: toe_sow, Cic, Omega0, Cis
    // Line 4: i0, Crc, omega, OmegaDot
    // Line 5: IDOT, (spare), BDSweek, (spare)
    // Line 6: URA, SVhealth, TGD1(B1/B3), TGD2(B2/B3)
    // Line 7: TransmTime, AODC, (spare), (spare)

    BdsBroadcastEph eph;
    eph.prn  = prn;
    eph.toc  = epochToGpsTime(yr, mo, dy, hr, mi, sec);
    eph.af0  = af0;
    eph.af1  = af1;
    eph.af2  = af2;

    eph.AODE     = f[0];
    eph.Crs      = f[1];
    eph.deltaN   = f[2];
    eph.M0       = f[3];

    eph.Cuc      = f[4];
    eph.e        = f[5];
    eph.Cus      = f[6];
    eph.sqrtA    = f[7];

    const double toeSow = f[8];
    eph.Cic      = f[9];
    eph.Omega0   = f[10];
    eph.Cis      = f[11];

    eph.i0       = f[12];
    eph.Crc      = f[13];
    eph.omega    = f[14];
    eph.OmegaDot = f[15];

    eph.IDOT     = f[16];
    eph.week     = f[18];
    eph.toe      = GpsTime{ static_cast<int>(eph.week), toeSow };

    eph.URA      = f[20];
    eph.SVhealth = static_cast<int>(f[21]);
    eph.TGD1     = f[22];
    eph.TGD2     = f[23];

    eph.AODC     = f[25];

    // GEO PRNs: 1-5 and 59-63 (BDS convention) use D2 message
    eph.msgType = ((prn >= 1 && prn <= 5) || (prn >= 59 && prn <= 63))
                  ? NavMsgType::D2 : NavMsgType::D1;

    if (m_cfg.skipUnhealthy && eph.SVhealth != 0) return;

    out.bdsEph.push_back(eph);
}

// =============================================================================
//  RINEX 3 -- SBAS record
//  Structure: 1 epoch line + 3 broadcast lines (state vector)
// =============================================================================

void RinexNavParser::parseSbasRecord3(const std::string& firstLine,
                                       std::istream& stream, NavFile& out,
                                       const std::string& sourceName) const {
    char sys{}; int prn{}, yr{}, mo{}, dy{}, hr{}, mi{};
    double sec{};
    parseSvEpochLine3(firstLine, sys, prn, yr, mo, dy, hr, mi, sec);

    const double agf0 = parseRinexDouble(safeSub(firstLine, 23, 19));
    const double agf1 = parseRinexDouble(safeSub(firstLine, 42, 19));

    const auto f = readBroadcastLines(stream, 3, sourceName);
    // Line 1: X [km], Xdot, Xdotdot, URA
    // Line 2: Y [km], Ydot, Ydotdot, health
    // Line 3: Z [km], Zdot, Zdotdot, (spare/IODN)

    SbasBroadcastEph eph;
    eph.prn  = prn;
    eph.t0   = epochToGpsTime(yr, mo, dy, hr, mi, sec);
    eph.agf0 = agf0;
    eph.agf1 = agf1;

    eph.x  = f[0];  eph.vx = f[1];  eph.ax = f[2];  eph.URA    = f[3];
    eph.y  = f[4];  eph.vy = f[5];  eph.ay = f[6];  eph.health = static_cast<int>(f[7]);
    eph.z  = f[8];  eph.vz = f[9];  eph.az = f[10];

    if (m_cfg.skipUnhealthy && eph.health != 0) return;

    out.sbasEph.push_back(eph);
}

// =============================================================================
//  RINEX 2 -- GPS record
//  Structure: 1 epoch line (PRN + toc + af0/af1/af2) + 7 broadcast lines
// =============================================================================

void RinexNavParser::parseGpsRecord2(const std::string& firstLine,
                                      std::istream& stream, NavFile& out,
                                      const std::string& sourceName) const {
    int prn{}, yr{}, mo{}, dy{}, hr{}, mi{};
    double sec{};
    parsePrnEpochLine2(firstLine, prn, yr, mo, dy, hr, mi, sec);

    // In RINEX 2, af0/af1/af2 are cols 22-60 on the epoch line
    const double af0 = parseRinexDouble(safeSub(firstLine, 22, 19));
    const double af1 = parseRinexDouble(safeSub(firstLine, 41, 19));
    const double af2 = parseRinexDouble(safeSub(firstLine, 60, 19));

    const auto f = readBroadcastLines(stream, 7, sourceName);

    GpsBroadcastEph eph;
    eph.prn  = prn;
    eph.toc  = epochToGpsTime(yr, mo, dy, hr, mi, sec);
    eph.af0  = af0;
    eph.af1  = af1;
    eph.af2  = af2;

    eph.IODE     = f[0];  eph.Crs   = f[1];  eph.deltaN = f[2];  eph.M0     = f[3];
    eph.Cuc      = f[4];  eph.e     = f[5];  eph.Cus    = f[6];  eph.sqrtA  = f[7];
    const double toeSow = f[8];
    eph.Cic      = f[9];  eph.Omega0 = f[10]; eph.Cis    = f[11];
    eph.i0       = f[12]; eph.Crc    = f[13]; eph.omega  = f[14]; eph.OmegaDot = f[15];
    eph.IDOT     = f[16];
    eph.week     = f[18];
    eph.toe      = GpsTime{ static_cast<int>(eph.week), toeSow };
    eph.URA      = f[20]; eph.SVhealth = static_cast<int>(f[21]);
    eph.TGD      = f[22]; eph.IODC     = f[23];
    eph.fitInterval = f[25];

    eph.msgType = NavMsgType::LNAV;

    if (m_cfg.skipUnhealthy && eph.SVhealth != 0) return;

    out.gpsEph.push_back(eph);
}

// =============================================================================
//  RINEX 2 -- GLONASS record
//  Structure: 1 epoch line (slot + toc + -tauN/gammaN) + 3 broadcast lines
// =============================================================================

void RinexNavParser::parseGloRecord2(const std::string& firstLine,
                                      std::istream& stream, NavFile& out,
                                      const std::string& sourceName) const {
    int prn{}, yr{}, mo{}, dy{}, hr{}, mi{};
    double sec{};
    parsePrnEpochLine2(firstLine, prn, yr, mo, dy, hr, mi, sec);

    const double neg_tauN = parseRinexDouble(safeSub(firstLine, 22, 19));
    const double gammaN   = parseRinexDouble(safeSub(firstLine, 41, 19));

    const auto f = readBroadcastLines(stream, 3, sourceName);

    GloBroadcastEph eph;
    eph.prn    = prn;
    eph.toe    = epochToGpsTime(yr, mo, dy, hr, mi, sec);
    eph.tauN   = -neg_tauN;
    eph.gammaN = gammaN;

    eph.x  = f[0];  eph.vx = f[1];  eph.ax = f[2];  eph.health = static_cast<int>(f[3]);
    eph.y  = f[4];  eph.vy = f[5];  eph.ay = f[6];  eph.freqCh = static_cast<int>(f[7]);
    eph.z  = f[8];  eph.vz = f[9];  eph.az = f[10]; eph.age    = static_cast<int>(f[11]);

    if (m_cfg.skipUnhealthy && eph.health != 0) return;

    out.gloEph.push_back(eph);
}

// =============================================================================
//  readBroadcastLines
//  Reads nLines continuation lines; each line has 4 fixed 19-char fields
//  starting at column 4 (RINEX 3) or 3 (RINEX 2, same for broadcast body).
//  We use col 4 for both -- the leading spaces in RINEX 2 body lines are 3.
//  In practice both use 4 spaces indent for broadcast record body lines.
// =============================================================================

std::vector<double> RinexNavParser::readBroadcastLines(
    std::istream& stream, int nLines,
    const std::string& /*sourceName*/) const {

    std::vector<double> fields;
    fields.reserve(static_cast<std::size_t>(nLines) * 4);

    std::string line;
    for (int i = 0; i < nLines; ++i) {
        if (!std::getline(stream, line)) {
            // Pad missing lines with zeros rather than throwing -- some
            // producers omit trailing lines when fields are zero.
            for (int j = 0; j < 4; ++j) fields.push_back(0.0);
            continue;
        }
        // RINEX broadcast record body: 4 spaces indent + 4 x 19-char fields
        for (int j = 0; j < 4; ++j) {
            const std::size_t col = 4 + static_cast<std::size_t>(j) * 19;
            fields.push_back(parseRinexDouble(safeSub(line, col, 19)));
        }
    }
    return fields;
}

// =============================================================================
//  parseSvEpochLine3
//  Format: [sys][prn2d] [yyyy] [mm] [dd] [HH] [MM] [SS.S] af0 af1 af2
//  cols:   0     1-2     4-7   9-10 12-13 15-16 18-19 21-22 23-61...
// =============================================================================

void RinexNavParser::parseSvEpochLine3(const std::string& line,
                                        char& systemChar, int& prn,
                                        int& yr, int& mo, int& dy,
                                        int& hr, int& mi, double& sec) {
    if (line.size() < 23) {
        throw ParseException("RinexNavParser: epoch line too short: " + line);
    }
    systemChar = line[0];
    prn = std::stoi(safeSub(line, 1, 2));
    yr  = std::stoi(safeSub(line, 4, 4));
    mo  = std::stoi(safeSub(line, 9, 2));
    dy  = std::stoi(safeSub(line, 12, 2));
    hr  = std::stoi(safeSub(line, 15, 2));
    mi  = std::stoi(safeSub(line, 18, 2));
    sec = std::stod(safeSub(line, 21, 2));
}

// =============================================================================
//  parsePrnEpochLine2
//  RINEX 2 format: [prn2d] [yy] [mm] [dd] [HH] [MM] [SS.S] af0 af1 af2
//  cols:           0-1     3-4  6-7  9-10 12-13 15-16 17-21
// =============================================================================

void RinexNavParser::parsePrnEpochLine2(const std::string& line,
                                         int& prn,
                                         int& yr, int& mo, int& dy,
                                         int& hr, int& mi, double& sec) {
    if (line.size() < 22) {
        throw ParseException("RinexNavParser: RINEX 2 epoch line too short: " + line);
    }
    prn = std::stoi(safeSub(line, 0, 2));
    yr  = std::stoi(safeSub(line, 3, 2));
    mo  = std::stoi(safeSub(line, 6, 2));
    dy  = std::stoi(safeSub(line, 9, 2));
    hr  = std::stoi(safeSub(line, 12, 2));
    mi  = std::stoi(safeSub(line, 15, 2));
    sec = std::stod(safeSub(line, 17, 5));
}

// =============================================================================
//  parseRinexDouble
//  Handles Fortran D-exponent notation (e.g. "0.1234D+05") and plain floats.
// =============================================================================

double RinexNavParser::parseRinexDouble(const std::string& raw) {
    std::string s = trim(raw);
    if (s.empty()) return 0.0;

    // Replace Fortran D/d exponent with E
    for (char& c : s) {
        if (c == 'D' || c == 'd') { c = 'E'; break; }
    }

    try {
        return std::stod(s);
    } catch (...) {
        return 0.0;
    }
}

// =============================================================================
//  epochToGpsTime
//  Converts RINEX epoch fields to GpsTime via TimeStamp.
//  RINEX 2 uses 2-digit years: 0-79 -> 2000-2079, 80-99 -> 1980-1999.
// =============================================================================

GpsTime RinexNavParser::epochToGpsTime(int yr, int mo, int dy,
                                        int hr, int mi, double sec) {
    // Handle 2-digit year (RINEX 2)
    if (yr < 100) {
        yr += (yr < 80) ? 2000 : 1900;
    }
    const int secInt  = static_cast<int>(sec);
    const double frac = sec - static_cast<double>(secInt);
    const ::TimeStamp ts(yr, mo, dy, hr, mi,
                         static_cast<double>(secInt) + frac);
    return GpsTime::fromTimeStamp(ts);
}
