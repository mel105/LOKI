#include <loki/gnss/rinexObsParser.hpp>
#include <loki/core/exceptions.hpp>

#include <chrono>
#include <zlib.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace loki;
using namespace loki::gnss;

namespace fs = std::filesystem;

// =============================================================================
//  File-local helpers
// =============================================================================
namespace {

std::string safeSub(const std::string& s, std::size_t pos, std::size_t len) {
    if (pos >= s.size()) return "";
    return s.substr(pos, std::min(len, s.size() - pos));
}

std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

GnssSystem systemFromChar(char c) {
    switch (c) {
        case 'G': return GnssSystem::GPS;
        case 'R': return GnssSystem::GLONASS;
        case 'E': return GnssSystem::GALILEO;
        case 'C': return GnssSystem::BEIDOU;
        case 'J': return GnssSystem::QZSS;
        case 'S': return GnssSystem::SBAS;
        default:  return GnssSystem::UNKNOWN;
    }
}

} // namespace

// =============================================================================
//  Constructor
// =============================================================================

RinexObsParser::RinexObsParser(Config cfg)
    : m_cfg(std::move(cfg)) {}

// =============================================================================
//  Public: parse
// =============================================================================

ObsFile RinexObsParser::parse(const std::string& filePath) const {
    std::ifstream ifs(filePath);
    if (!ifs.is_open()) {
        throw FileNotFoundException(
            "RinexObsParser: cannot open file: " + filePath);
    }
    return parseStream(ifs, filePath);
}

// =============================================================================
//  Public: parseGz
// =============================================================================

ObsFile RinexObsParser::parseGz(const std::string& filePath) const {
    const std::string crx2rnx = resolveCrx2rnx();

    const std::string basePath = makeTempPath("");
    const std::string crxPath  = basePath + ".crx";
    const std::string rnxPath  = basePath + ".rnx";

    // Step 1: gunzip using zlib directly in C++ -- no external process needed.
    // This avoids all popen/cmd.exe/shell issues on Windows.
    {
        gzFile gz = ::gzopen(filePath.c_str(), "rb");
        if (!gz) throw FileNotFoundException(
            "RinexObsParser: cannot open gz file: " + filePath);
        FILE* out = std::fopen(crxPath.c_str(), "wb");
        if (!out) { ::gzclose(gz);
            throw ParseException("RinexObsParser: cannot create: " + crxPath); }
        char buf[65536]; int n = 0;
        while ((n = ::gzread(gz, buf, sizeof(buf))) > 0)
            std::fwrite(buf, 1, static_cast<std::size_t>(n), out);
        std::fclose(out);
        ::gzclose(gz);
    }

    // Step 2: CRX2RNX via _popen with cmd.exe (Windows PE32+ binary).
    // _popen on MinGW64 uses cmd.exe -- correct for Windows .exe programs.
    // CRX2RNX auto-creates basePath.rnx from basePath.crx.
    // Use backslash paths for cmd.exe.
    {
        auto toWin = [](std::string p) {
            for (char& c : p) { if (c == '/') c = '\\'; }
            return p;
        };
        const std::string crx2rnxW = toWin(crx2rnx);
        const std::string crxW     = toWin(crxPath);
        // cmd /c "exe" "input"   -- no output arg, CRX2RNX auto-derives .rnx
        const std::string cmd =
            "cmd /c \"\"" + crx2rnxW + "\" -f \"" + crxW + "\"\"";
        FILE* pipe = ::_popen(cmd.c_str(), "r");
        if (!pipe) { fs::remove(crxPath);
            throw ParseException("RinexObsParser: _popen(CRX2RNX) failed"); }
        char buf[4096];
        while (std::fread(buf, 1, sizeof(buf), pipe) > 0) {}
        const int ret = ::_pclose(pipe);
        fs::remove(crxPath);
        if (ret != 0 && ret != 2) { fs::remove(rnxPath);
            throw ParseException("RinexObsParser: CRX2RNX failed (exit="
                + std::to_string(ret) + ") for: " + filePath); }

        if (m_cfg.verbose) {
            std::cerr << "[RinexObsParser] cmd=" << cmd << "\n";
            std::cerr << "[RinexObsParser] rnxPath=" << rnxPath << "\n";
            std::ifstream tmp(rnxPath, std::ios::ate);
            std::cerr << "[RinexObsParser] size=" << tmp.tellg() << " bytes\n";
        }

        FILE* probe = std::fopen(rnxPath.c_str(), "rb");
        if (!probe) throw ParseException(
            "RinexObsParser: CRX2RNX produced no output for: " + filePath);
        std::fclose(probe);
    }

    // Step 3: parse
    ObsFile result;
    try {
        std::ifstream ifs(rnxPath);
        if (!ifs.is_open()) throw ParseException(
            "RinexObsParser: cannot open RNX: " + rnxPath);
        result = parseStream(ifs, filePath);
    } catch (...) { fs::remove(rnxPath); throw; }
    fs::remove(rnxPath);
    return result;
}


// =============================================================================
//  parseStream
//  Owns the obs-code map; drives header + epoch phases in one method to avoid
//  sharing the map across methods via awkward out-parameters or thread-locals.
// =============================================================================

ObsFile RinexObsParser::parseStream(std::istream& stream,
                                     const std::string& sourceName) const {
    ObsFile out;
    std::map<char, std::vector<std::string>> sysObsCodes;
    char lastSysChar = 'G';
    std::string line;

    // ── Phase 1: header ───────────────────────────────────────────────────────
    while (std::getline(stream, line)) {
        if (line.size() < 60) line.resize(80, ' ');
        const std::string label = trim(safeSub(line, 60, 20));

        if (label == "RINEX VERSION / TYPE") {
            out.rinexVersion = trim(safeSub(line, 0, 9));

        } else if (label == "MARKER NAME") {
            out.receiver.markerName = trim(safeSub(line, 0, 60));

        } else if (label == "MARKER NUMBER") {
            out.receiver.markerNumber = trim(safeSub(line, 0, 20));

        } else if (label == "REC # / TYPE / VERS") {
            out.receiver.receiverSerial   = trim(safeSub(line,  0, 20));
            out.receiver.receiverType     = trim(safeSub(line, 20, 20));
            out.receiver.receiverFirmware = trim(safeSub(line, 40, 20));

        } else if (label == "ANT # / TYPE") {
            out.receiver.antennaSerial = trim(safeSub(line,  0, 20));
            out.receiver.antennaType   = trim(safeSub(line, 20, 20));

        } else if (label == "APPROX POSITION XYZ") {
            try {
                out.receiver.approxX = std::stod(safeSub(line,  0, 14));
                out.receiver.approxY = std::stod(safeSub(line, 14, 14));
                out.receiver.approxZ = std::stod(safeSub(line, 28, 14));
            } catch (...) {}

        } else if (label == "ANTENNA: DELTA H/E/N") {
            try {
                out.receiver.antDeltaH = std::stod(safeSub(line,  0, 14));
                out.receiver.antDeltaE = std::stod(safeSub(line, 14, 14));
                out.receiver.antDeltaN = std::stod(safeSub(line, 28, 14));
            } catch (...) {}

        } else if (label == "INTERVAL") {
            try { out.interval = std::stod(trim(safeSub(line, 0, 10))); }
            catch (...) {}

        } else if (label == "SYS / # / OBS TYPES") {
            // col 0  : system char (' ' = continuation)
            // col 3  : total count (first line of each system only)
            // col 7+ : obs codes, 4-char each (3 chars + space), max 13 per line
            const char sysChar = line[0];
            if (sysChar != ' ' && sysChar != '\t') {
                lastSysChar = sysChar;
                int nObs = 0;
                try { nObs = std::stoi(safeSub(line, 3, 3)); } catch (...) {}
                sysObsCodes[lastSysChar].reserve(
                    static_cast<std::size_t>(nObs));
            }
            auto& codes = sysObsCodes[lastSysChar];
            for (int i = 0; i < 13; ++i) {
                const std::string code = trim(safeSub(
                    line, 7 + static_cast<std::size_t>(i) * 4, 3));
                if (!code.empty()) codes.push_back(code);
            }

        } else if (label == "END OF HEADER") {
            break;
        }
    }

    if (m_cfg.verbose) {
        std::cerr << "[RinexObsParser] Header: station="
                  << out.receiver.markerName
                  << "  ver=" << out.rinexVersion
                  << "  interval=" << out.interval << "s\n";
        for (const auto& [sys, codes] : sysObsCodes) {
            std::cerr << "  SYS=" << sys
                      << "  nObs=" << codes.size() << "\n";
        }
    }

    // ── Phase 2: epochs ───────────────────────────────────────────────────────
    int epochCount = 0;

    while (std::getline(stream, line)) {
        if (line.empty() || line[0] != '>') continue;

        GpsTime epochTime;
        int epochFlag = 0;
        int numSat    = 0;
        if (!parseEpochLine(line, epochTime, epochFlag, numSat)) continue;

        // Special event block: skip the following numSat header lines
        if (epochFlag > 1) {
            for (int i = 0; i < numSat; ++i) std::getline(stream, line);
            continue;
        }

        ObsEpoch epoch;
        epoch.time      = epochTime;
        epoch.epochFlag = epochFlag;
        epoch.numSat    = numSat;
        epoch.satellites.reserve(static_cast<std::size_t>(numSat));

        for (int s = 0; s < numSat; ++s) {
            if (!std::getline(stream, line)) break;
            if (line.size() < 3) continue;

            // RINEX 3: satellite ID is the first 3 chars of the obs line
            const std::string satId   = safeSub(line, 0, 3);
            const char        sysChar = satId.empty() ? ' ' : satId[0];

            const auto it = sysObsCodes.find(sysChar);
            if (it == sysObsCodes.end()) continue;

            const std::string obsLine =
                line.size() > 3 ? line.substr(3) : "";
            epoch.satellites.push_back(
                parseSatLine(satId, obsLine, it->second));
        }

        out.epochs.push_back(std::move(epoch));
        ++epochCount;

        if (m_cfg.verbose && (epochCount % 500 == 0)) {
            std::cerr << "[RinexObsParser] " << epochCount
                      << " epochs...\n";
        }
    }

    if (m_cfg.verbose) {
        std::cerr << "[RinexObsParser] Done: " << epochCount
                  << " epochs  src=" << sourceName << "\n";
    }
    return out;
}

// =============================================================================
//  Static helpers
// =============================================================================

bool RinexObsParser::parseEpochLine(const std::string& line,
                                     GpsTime& time, int& epochFlag,
                                     int& numSat) {
    if (line.empty() || line[0] != '>') return false;
    try {
        const int    yr  = std::stoi(safeSub(line,  2, 4));
        const int    mo  = std::stoi(safeSub(line,  7, 2));
        const int    dy  = std::stoi(safeSub(line, 10, 2));
        const int    hr  = std::stoi(safeSub(line, 13, 2));
        const int    mi  = std::stoi(safeSub(line, 16, 2));
        const double sec = std::stod(safeSub(line, 19, 10));
        epochFlag        = std::stoi(safeSub(line, 29, 3));
        numSat           = std::stoi(safeSub(line, 32, 3));
        const ::TimeStamp ts(yr, mo, dy, hr, mi, sec);
        time = GpsTime::fromTimeStamp(ts);
        return true;
    } catch (...) { return false; }
}

SatObs RinexObsParser::parseSatLine(const std::string& satId,
                                     const std::string& line,
                                     const std::vector<std::string>& obsCodes) {
    SatObs sat;
    parseSatId(satId, sat.system, sat.prn);
    for (std::size_t i = 0; i < obsCodes.size(); ++i) {
        const ObsValue ov = extractObsField(line, i * 16);
        if (ov.valid) sat.obs[obsCodes[i]] = ov;
    }
    return sat;
}

void RinexObsParser::parseSatId(const std::string& satId,
                                 GnssSystem& system, int& prn) {
    if (satId.size() < 3) { system = GnssSystem::UNKNOWN; prn = 0; return; }
    system = systemFromChar(satId[0]);
    try { prn = std::stoi(satId.substr(1, 2)); }
    catch (...) { prn = 0; }
}

ObsValue RinexObsParser::extractObsField(const std::string& line,
                                          std::size_t col) {
    ObsValue ov;
    if (col >= line.size()) return ov;

    // 14-char value field (right-justified, blank = missing)
    const std::string valStr = trim(safeSub(line, col, 14));
    if (valStr.empty()) return ov;
    try { ov.value = std::stod(valStr); ov.valid = true; }
    catch (...) { return ov; }

    // 1-char LLI at col+14, 1-char SNR at col+15
    if (col + 14 < line.size()) {
        const char c = line[col + 14];
        if (c >= '0' && c <= '9') ov.lli = c - '0';
    }
    if (col + 15 < line.size()) {
        const char c = line[col + 15];
        if (c >= '0' && c <= '9') ov.snr = c - '0';
    }
    return ov;
}

std::string RinexObsParser::makeTempPath(const std::string& suffix) {
    const auto ns = std::chrono::high_resolution_clock::now()
                        .time_since_epoch().count();
    const std::string name = "loki_gnss_" + std::to_string(ns) + suffix;
#ifdef _WIN32
    // On Windows, use the TEMP environment variable to get a cmd.exe-compatible
    // path. fs::temp_directory_path() returns a POSIX /tmp/ path in MSYS2
    // which cmd.exe cannot resolve for output redirection.
    const char* winTemp = std::getenv("TEMP");
    if (!winTemp) winTemp = std::getenv("TMP");
    if (winTemp) {
        return std::string(winTemp) + "\\" + name;
    }
#endif
    return (fs::temp_directory_path() / name).string();
}

// =============================================================================
//  Decompression
// =============================================================================

std::string RinexObsParser::decompressGzip(const std::string& gzPath) const {
    const std::string tmpPath = makeTempPath(".tmp");
    const std::string cmd =
        "gzip -d -c \"" + gzPath + "\" > \"" + tmpPath + "\"";
    if (std::system(cmd.c_str()) != 0) { // NOLINT(cert-env33-c)
        throw FileNotFoundException(
            "RinexObsParser: gzip failed for: " + gzPath);
    }
    if (!fs::exists(tmpPath)) {
        throw ParseException(
            "RinexObsParser: gzip produced no output for: " + gzPath);
    }
    return tmpPath;
}

std::string RinexObsParser::convertHatanaka(const std::string& crxPath) const {
    const std::string crx2rnx = resolveCrx2rnx();
    const std::string rnxPath = makeTempPath(".rnx");
    const std::string cmd =
        "\"" + crx2rnx + "\" \"" + crxPath + "\" \"" + rnxPath + "\"";
    if (std::system(cmd.c_str()) != 0) { // NOLINT(cert-env33-c)
        throw ParseException(
            "RinexObsParser: CRX2RNX failed (binary=" + crx2rnx + ")");
    }
    if (!fs::exists(rnxPath)) {
        throw ParseException(
            "RinexObsParser: CRX2RNX produced no output for: " + crxPath);
    }
    return rnxPath;
}

std::string RinexObsParser::resolveCrx2rnx() const {
    const std::string base = m_cfg.crx2rnxPath;
#ifdef _WIN32
    // cmd.exe cannot run extensionless binaries -- always use .exe on Windows.
    const std::string withExe = base + ".exe";
    FILE* p = std::fopen(withExe.c_str(), "rb");
    if (p) { std::fclose(p); return withExe; }
#endif
    FILE* p2 = std::fopen(base.c_str(), "rb");
    if (p2) { std::fclose(p2); return base; }
    throw FileNotFoundException(
        "RinexObsParser: CRX2RNX not found at: " + base);
}

// =============================================================================
//  Declared-but-delegated stubs
//  parseHeader and parseEpoch are documented in the header but the actual logic
//  lives in parseStream. These stubs satisfy the linker.
// =============================================================================

void RinexObsParser::parseHeader(std::istream& /*s*/, ObsFile& /*out*/,
                                  const std::string& /*src*/) const {}

bool RinexObsParser::parseEpoch(std::istream& /*s*/, ObsFile& /*out*/,
                                 const std::string& /*src*/) const {
    return false;
}
