#include <loki/gnss/sp3Parser.hpp>
#include <loki/core/exceptions.hpp>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

using namespace loki;
using namespace loki::gnss;

// =============================================================================
//  Public interface
// =============================================================================

Sp3File Sp3Parser::parse(const std::string& filePath) const
{
    std::ifstream f(filePath);
    if (!f.is_open())
        throw FileNotFoundException("Sp3Parser: cannot open '" + filePath + "'.");
    return parseStream(f, filePath);
}

Sp3File Sp3Parser::parseGz(const std::string& filePath) const
{
    const std::string cmd = "gzip -d -c \"" + filePath + "\"";
#if defined(_WIN32)
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe)
        throw FileNotFoundException(
            "Sp3Parser: cannot open gzip pipe for '" + filePath + "'.");

    std::string content;
    content.reserve(1 << 22);   // 4 MB initial
    char buf[8192];
    while (fgets(buf, sizeof(buf), pipe))
        content += buf;

#if defined(_WIN32)
    _pclose(pipe);
#else
    pclose(pipe);
#endif

    std::istringstream ss(content);
    return parseStream(ss, filePath);
}

// =============================================================================
//  parseStream
//
//  Strategy:
//    1. Call parseHeader() which reads lines until the first '*' epoch line
//       and returns that line in firstEpochLine.
//    2. Process firstEpochLine as the first epoch.
//    3. Continue reading: '*' starts a new epoch, 'P' adds a position record,
//       'EOF' / end-of-stream terminates.
// =============================================================================

Sp3File Sp3Parser::parseStream(std::istream& stream,
                                const std::string& source) const
{
    Sp3File out;

    std::string firstEpochLine;
    parseHeader(stream, out, source, firstEpochLine);

    // If the file has no epoch lines at all (header-only), return empty.
    if (firstEpochLine.empty())
        return out;

    Sp3Epoch current;
    current.time = parseEpochLine(firstEpochLine, source);
    bool inEpoch = true;

    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        if (line[0] == '*') {
            // Flush current epoch if it has satellites.
            if (inEpoch && !current.satellites.empty())
                out.epochs.push_back(std::move(current));
            current = Sp3Epoch{};
            current.time = parseEpochLine(line, source);
            inEpoch = true;

        } else if (line[0] == 'P' && inEpoch) {
            Sp3SatState state;
            if (parsePosRecord(line, state))
                current.satellites.push_back(state);

        } else if (line.size() >= 3 && line.substr(0, 3) == "EOF") {
            break;
        }
        // 'V' velocity records are ignored -- not needed for PPP orbit interp.
    }

    // Flush last epoch.
    if (inEpoch && !current.satellites.empty())
        out.epochs.push_back(std::move(current));

    if (out.epochs.empty())
        throw ParseException(
            "Sp3Parser: no epochs found in '" + source + "'.");

    return out;
}

// =============================================================================
//  parseHeader
//
//  Reads lines one by one until a line starting with '*' is found.
//  That line is returned in firstEpochLine WITHOUT being pushed into
//  the stream (getline already consumed it).
//  All other lines are parsed for version / coordinate system / interval.
// =============================================================================

void Sp3Parser::parseHeader(std::istream& stream, Sp3File& out,
                             const std::string& source,
                             std::string& firstEpochLine) const
{
    firstEpochLine.clear();
    std::string line;

    while (std::getline(stream, line)) {

        // First epoch line -- return it to the caller.
        if (!line.empty() && line[0] == '*') {
            firstEpochLine = line;
            return;
        }

        // EOF sentinel inside header (malformed file).
        if (line.size() >= 3 && line.substr(0, 3) == "EOF")
            return;

        // Line 1: version character at position 1.
        // SP3-d first line: "#dP2024 ..." or "#cP2024 ..."
        if (!line.empty() && line[0] == '#' && line.size() > 1) {
            if (line[1] == 'a' || line[1] == 'A') out.version = Sp3Version::SP3A;
            else if (line[1] == 'c' || line[1] == 'C') out.version = Sp3Version::SP3C;
            else if (line[1] == 'd' || line[1] == 'D') out.version = Sp3Version::SP3D;

            // Second header line "## week sow interval ...":
            // interval is at fixed position -- parse if it starts with "##".
            if (line.size() > 1 && line[1] == '#') {
                // ## <gpsWeek> <sow> <interval> <mjdInt> <mjdFrac>
                std::istringstream iss(line.substr(2));
                int    week = 0;
                double sow  = 0.0;
                double interval = 0.0;
                if (iss >> week >> sow >> interval)
                    out.interval = interval;
            }
            continue;
        }

        // %c line: coordinate system at fixed columns.
        // "%c M  cc GPS ccc cccc ..."  -- coord system token 4 in the line.
        if (!line.empty() && line[0] == '%' && line.size() > 1 && line[1] == 'c') {
            // Coordinate system: token starting at col 46 (0-based) in SP3-c/d.
            // More robustly: 4th whitespace-separated token.
            std::istringstream iss(line.substr(2));
            std::string t1, t2, t3, coordSys;
            if (iss >> t1 >> t2 >> t3 >> coordSys)
                out.coordinateSystem = coordSys;
            continue;
        }

        // '+' satellite list lines -- ignored (we parse sat ID from 'P' records).
        // '++' accuracy lines     -- ignored.
        // '%f' / '%i'             -- ignored.
        // '/*' comment lines      -- ignored.
    }
    // Reached end of stream without finding any '*' line.
    // firstEpochLine remains empty -> parseStream returns empty Sp3File.
}

// =============================================================================
//  parsePosRecord
//
//  SP3 'P' record format (SP3-d, fixed-width):
//    P<SysChar><PRN2> <X14.6> <Y14.6> <Z14.6> <CLK14.6> [sigmas...]
//
//  Columns (0-based):
//    0     : 'P'
//    1     : system char (G/R/E/C/J/S/I)
//    2-3   : PRN zero-padded (2 digits)
//    4     : space
//    5-18  : X [km]  (14 chars, right-justified)
//    19    : space
//    20-33 : Y [km]
//    34    : space
//    35-48 : Z [km]
//    49    : space
//    50-63 : CLK [microseconds]
// =============================================================================

bool Sp3Parser::parsePosRecord(const std::string& line, Sp3SatState& state)
{
    if (line.size() < 4) return false;

    // Satellite ID: chars 1-3 (system char + 2-digit PRN).
    const std::string satId = line.substr(1, 3);
    parseSatId(satId, state.system, state.prn);
    if (state.system == GnssSystem::UNKNOWN || state.prn <= 0) return false;

    // Numerical fields: parse from col 4 onward as free-format doubles
    // (whitespace-separated -- works for both SP3-c and SP3-d).
    std::istringstream iss(line.substr(4));
    double x = 0.0, y = 0.0, z = 0.0, clk = 0.0;
    if (!(iss >> x >> y >> z >> clk)) return false;

    // Missing position sentinel: 0.000000 exactly.
    if (x == 0.0 && y == 0.0 && z == 0.0) {
        state.posMissing = true;
    } else {
        state.x = x;   // [km]
        state.y = y;
        state.z = z;
    }

    // Missing clock sentinel: >= 999999.0.
    if (clk >= 999990.0) {
        state.clockMissing = true;
    } else {
        state.clk = clk;   // [microseconds]
    }

    // Optional sigma fields.
    double sx = 0.0, sy = 0.0, sz = 0.0, sc = 0.0;
    if (iss >> sx >> sy >> sz >> sc) {
        state.sigX   = sx;
        state.sigY   = sy;
        state.sigZ   = sz;
        state.sigClk = sc;
    }

    return true;
}

// =============================================================================
//  parseEpochLine
//
//  Format: * YYYY MM DD hh mm ss.sssssss
// =============================================================================

GpsTime Sp3Parser::parseEpochLine(const std::string& line,
                                   const std::string& source)
{
    int    yr = 0, mo = 0, dy = 0, hr = 0, mi = 0;
    double sc = 0.0;

    if (std::sscanf(line.c_str(), "* %d %d %d %d %d %lf",
                    &yr, &mo, &dy, &hr, &mi, &sc) != 6)
        throw ParseException(
            "Sp3Parser: malformed epoch line: '" + line
            + "' in '" + source + "'.");

    // Julian Day Number (Gregorian calendar formula).
    const int a   = (14 - mo) / 12;
    const int y   = yr + 4800 - a;
    const int m   = mo + 12 * a - 3;
    const int jdn = dy + (153*m + 2)/5 + 365*y + y/4 - y/100 + y/400 - 32045;

    // GPS epoch: 1980-01-06, JDN = 2444245.
    constexpr int GPS_EPOCH_JDN = 2444245;
    const int daysSinceGps = jdn - GPS_EPOCH_JDN;
    const int week         = daysSinceGps / 7;
    const int dow          = daysSinceGps % 7;

    const double sow = static_cast<double>(dow) * 86400.0
                     + static_cast<double>(hr)  * 3600.0
                     + static_cast<double>(mi)  * 60.0
                     + sc;

    return GpsTime{week, sow};
}

// =============================================================================
//  parseSatId
// =============================================================================

void Sp3Parser::parseSatId(const std::string& id,
                            GnssSystem& system, int& prn)
{
    if (id.size() < 3) { system = GnssSystem::UNKNOWN; prn = 0; return; }

    switch (id[0]) {
        case 'G': system = GnssSystem::GPS;     break;
        case 'R': system = GnssSystem::GLONASS; break;
        case 'E': system = GnssSystem::GALILEO; break;
        case 'C': system = GnssSystem::BEIDOU;  break;
        case 'J': system = GnssSystem::QZSS;    break;
        case 'S': system = GnssSystem::SBAS;    break;
        default:  system = GnssSystem::UNKNOWN; prn = 0; return;
    }

    try {
        prn = std::stoi(id.substr(1));
    } catch (...) {
        system = GnssSystem::UNKNOWN;
        prn = 0;
    }
}
