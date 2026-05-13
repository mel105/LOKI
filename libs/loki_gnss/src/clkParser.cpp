#include <loki/gnss/clkParser.hpp>
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

ClkFile ClkParser::parse(const std::string& filePath) const
{
    std::ifstream f(filePath);
    if (!f.is_open())
        throw FileNotFoundException("ClkParser: cannot open '" + filePath + "'.");
    return parseStream(f, filePath);
}

ClkFile ClkParser::parseGz(const std::string& filePath) const
{
    const std::string cmd = "gzip -d -c \"" + filePath + "\"";
#if defined(_WIN32)
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe)
        throw FileNotFoundException(
            "ClkParser: cannot open gzip pipe for '" + filePath + "'.");

    std::string content;
    content.reserve(1 << 24);   // 16 MB -- CLK files are large
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
// =============================================================================

ClkFile ClkParser::parseStream(std::istream& stream,
                                const std::string& /*source*/) const
{
    ClkFile out;
    std::string line;
    bool inHeader = true;

    while (std::getline(stream, line)) {

        if (inHeader) {
            if (line.find("END OF HEADER") != std::string::npos) {
                inHeader = false;
            } else if (line.find("RINEX VERSION") != std::string::npos
                       && line.size() >= 9) {
                out.rinexVersion = line.substr(0, 9);
            }
            continue;
        }

        // Data records start with a 2-char type token.
        if (line.size() < 8) continue;
        const std::string typeStr = line.substr(0, 2);
        if (typeStr != "AS" && typeStr != "AR" &&
            typeStr != "CR" && typeStr != "DR" && typeStr != "MS")
            continue;

        ClkRecord rec;
        if (parseDataRecord(line, rec))
            out.records.push_back(std::move(rec));
    }

    return out;
}

// =============================================================================
//  parseDataRecord
//
//  RINEX CLK record format (space-separated columns):
//
//    <type> <name> <YYYY> <MM> <DD> <hh> <mm> <ss.ssssss> <nvals> <bias> [sigma] [drift] [driftRMS]
//
//  Examples:
//    AS G01  2024  3 15  0  0  0.000000  2-7.412345678901E-04  1.23456789012E-11
//    AR GOPE 2024  3 15  0  0  0.000000  2  1.234567890123E-04  1.23E-12
//
//  Note: there may be NO space between nvals and bias when bias is negative
//  (e.g. "  2-7.41...").  We handle this by reading nvals as int then
//  reading the rest of the field as the bias double -- sscanf handles the
//  "2-7.41" case correctly when scanning from that position.
// =============================================================================

bool ClkParser::parseDataRecord(const std::string& line, ClkRecord& rec)
{
    // Type (cols 0-1).
    const std::string typeStr = line.substr(0, 2);
    if      (typeStr == "AS") rec.type = ClkType::AS;
    else if (typeStr == "AR") rec.type = ClkType::AR;
    else if (typeStr == "CR") rec.type = ClkType::CR;
    else if (typeStr == "DR") rec.type = ClkType::DR;
    else                      rec.type = ClkType::MS;

    // Name field: cols 3-6 for satellites (3 chars), up to col 9 for receivers.
    // Most robust: take chars 3..10, trim whitespace.
    if (line.size() < 10) return false;
    {
        std::string raw = line.substr(3, 8);
        const auto f = raw.find_first_not_of(' ');
        const auto l = raw.find_last_not_of(' ');
        if (f == std::string::npos) return false;
        rec.name = raw.substr(f, l - f + 1);
    }

    // Epoch and data values: parse from col 8 onward.
    // We use sscanf for the fixed epoch columns then read floats.
    // Col 8 = start of YYYY (4-digit year).
    if (line.size() < 35) return false;

    int    yr = 0, mo = 0, dy = 0, hr = 0, mi = 0;
    double sc = 0.0;

    // sscanf from col 8: "%4d %2d %2d %2d %2d %9lf"
    // Use generous format -- columns may have extra spaces in some producers.
    if (std::sscanf(line.c_str() + 8, "%d %d %d %d %d %lf",
                    &yr, &mo, &dy, &hr, &mi, &sc) != 6)
        return false;

    rec.time = makeGpsTime(yr, mo, dy, hr, mi, sc);

    // Number of data values + bias: these follow the epoch fields.
    // The epoch string occupies approximately cols 8..38 (30 chars).
    // We locate "nvals" by scanning past the 6 epoch tokens.
    // Safer: find position after the 6th numeric token.
    const char* p = line.c_str() + 8;
    // Skip 6 tokens (yr, mo, dy, hr, mi, sc).
    for (int tok = 0; tok < 6; ++tok) {
        while (*p == ' ') ++p;            // skip spaces
        while (*p && *p != ' ') ++p;      // skip token chars
    }
    // p now points to the space(s) before nvals.
    int nvals = 0;
    double bias = 0.0;
    if (std::sscanf(p, "%d", &nvals) != 1) return false;

    // Bias: right after nvals, may be glued (no space when negative).
    // Skip the nvals integer then read the bias.
    while (*p == ' ') ++p;
    while (*p && *p != ' ' && *p != '-' && *p != '+') ++p;
    // p now at start of bias value (could start with '-').
    if (std::sscanf(p, "%lf", &bias) != 1) return false;
    rec.bias = bias;

    // Sigma (optional).
    double sigma = 0.0;
    const char* q = p;
    while (*q && *q != ' ') ++q;   // skip bias
    if (std::sscanf(q, "%lf", &sigma) == 1) {
        rec.biasRMS = sigma;
        // Drift (optional).
        while (*q && *q != ' ') ++q;
        double drift = 0.0;
        if (std::sscanf(q, "%lf", &drift) == 1) {
            rec.drift = drift;
            while (*q && *q != ' ') ++q;
            double driftRms = 0.0;
            if (std::sscanf(q, "%lf", &driftRms) == 1)
                rec.driftRMS = driftRms;
        }
    }

    return true;
}

// =============================================================================
//  makeGpsTime
// =============================================================================

GpsTime ClkParser::makeGpsTime(int yr, int mo, int dy,
                                int hr, int mi, double sc)
{
    if (yr < 80)  yr += 2000;
    if (yr < 100) yr += 1900;

    const int a   = (14 - mo) / 12;
    const int y   = yr + 4800 - a;
    const int m   = mo + 12 * a - 3;
    const int jdn = dy + (153*m + 2)/5 + 365*y + y/4 - y/100 + y/400 - 32045;

    constexpr int GPS_EPOCH_JDN = 2444245;
    const int daysSinceGps = jdn - GPS_EPOCH_JDN;
    const int week         = daysSinceGps / 7;
    const int dow          = daysSinceGps % 7;

    const double sow = static_cast<double>(dow)  * 86400.0
                     + static_cast<double>(hr)    * 3600.0
                     + static_cast<double>(mi)    * 60.0
                     + sc;
    return GpsTime{week, sow};
}
