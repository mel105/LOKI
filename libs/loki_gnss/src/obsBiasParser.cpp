#include <loki/gnss/obsBiasParser.hpp>
#include <loki/core/exceptions.hpp>

#include <cstdio>
#include <fstream>
#include <sstream>

using namespace loki;
using namespace loki::gnss;

// =============================================================================
//  OsbFile::getBiasNs
// =============================================================================

double OsbFile::getBiasNs(GnssSystem system, int prn,
                           const std::string& signal) const
{
    const auto it = records.find({system, prn, signal});
    return (it != records.end()) ? it->second.bias_ns : 0.0;
}

// =============================================================================
//  Public interface
// =============================================================================

OsbFile ObsBiasParser::parse(const std::string& filePath) const
{
    std::ifstream f(filePath);
    if (!f.is_open())
        throw FileNotFoundException(
            "ObsBiasParser: cannot open '" + filePath + "'.");
    return parseStream(f);
}

OsbFile ObsBiasParser::parseGz(const std::string& filePath) const
{
    const std::string cmd = "gzip -d -c \"" + filePath + "\"";
#if defined(_WIN32)
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe)
        throw FileNotFoundException(
            "ObsBiasParser: cannot open gzip pipe for '" + filePath + "'.");

    std::string content;
    content.reserve(1 << 21);
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe))
        content += buf;

#if defined(_WIN32)
    _pclose(pipe);
#else
    pclose(pipe);
#endif

    std::istringstream ss(content);
    return parseStream(ss);
}

// =============================================================================
//  parseStream
//
//  BIAS-SINEX OSB data record format (space-separated):
//   " OSB  <SVN> <PRN>   <signal>  <start> <end>  <unit>  <bias>  <sigma>"
//
//  Columns (0-based, after leading space):
//   0      : "OSB"
//   1      : SVN (satellite vehicle number, e.g. "G063") -- ignored
//   2      : PRN (e.g. "G01")
//   3      : signal code (e.g. "C1C")
//   4      : start epoch "YYYY:DOY:SOD"
//   5      : end epoch
//   6      : unit ("ns")
//   7      : bias value [ns]
//   8      : sigma [ns]
// =============================================================================

OsbFile ObsBiasParser::parseStream(std::istream& stream) const
{
    OsbFile out;
    std::string line;
    bool inData = false;

    while (std::getline(stream, line)) {
        // Data block starts after "+BIAS/SOLUTION" marker.
        if (line.find("+BIAS/SOLUTION") != std::string::npos) {
            inData = true;
            continue;
        }
        if (line.find("-BIAS/SOLUTION") != std::string::npos) {
            inData = false;
            continue;
        }
        if (!inData) continue;
        if (line.empty() || line[0] == '*') continue;  // comment

        // Data lines start with " OSB".
        if (line.size() < 5 || line.substr(1, 3) != "OSB") continue;

        std::istringstream iss(line.substr(1));  // skip leading space
        std::string tag, svn, prnStr, signal, startEp, endEp, unit;
        double bias = 0.0, sigma = 0.0;

        if (!(iss >> tag >> svn >> prnStr >> signal >> startEp >> endEp >> unit >> bias))
            continue;
        iss >> sigma;  // optional

        if (tag != "OSB") continue;
        if (unit != "ns") continue;   // we only handle nanosecond biases

        // Parse PRN: first char = system, rest = PRN number.
        if (prnStr.size() < 3) continue;
        const GnssSystem sys = systemFromChar(prnStr[0]);
        if (sys == GnssSystem::UNKNOWN) continue;

        int prn = 0;
        try { prn = std::stoi(prnStr.substr(1)); }
        catch (...) { continue; }

        OsbRecord rec;
        rec.system   = sys;
        rec.prn      = prn;
        rec.signal   = signal;
        rec.bias_ns  = bias;
        rec.sigma_ns = sigma;

        out.records[{sys, prn, signal}] = std::move(rec);
    }

    return out;
}

// =============================================================================
//  systemFromChar
// =============================================================================

GnssSystem ObsBiasParser::systemFromChar(char c)
{
    switch (c) {
        case 'G': return GnssSystem::GPS;
        case 'R': return GnssSystem::GLONASS;
        case 'E': return GnssSystem::GALILEO;
        case 'C': return GnssSystem::BEIDOU;
        case 'J': return GnssSystem::QZSS;
        default:  return GnssSystem::UNKNOWN;
    }
}
