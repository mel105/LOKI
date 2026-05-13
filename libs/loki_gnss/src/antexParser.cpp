#include <loki/gnss/antexParser.hpp>
#include <loki/core/exceptions.hpp>

#include <fstream>
#include <sstream>
#include <string>

using namespace loki;
using namespace loki::gnss;

// =============================================================================
//  Public interface
// =============================================================================

AntexFile AntexParser::parse(const std::string& filePath) const
{
    std::ifstream f(filePath);
    if (!f.is_open())
        throw FileNotFoundException("AntexParser: cannot open '" + filePath + "'.");
    return parseStream(f, filePath);
}

// =============================================================================
//  parseStream
// =============================================================================

AntexFile AntexParser::parseStream(std::istream& stream,
                                    const std::string& source) const
{
    AntexFile out;
    std::string line;
    bool inHeader = true;

    while (std::getline(stream, line)) {
        if (inHeader) {
            if (line.find("END OF HEADER") != std::string::npos) {
                inHeader = false;
            } else if (line.find("ANTEX VERSION") != std::string::npos) {
                out.version = line.substr(0, 8);
            }
            continue;
        }

        if (line.find("START OF ANTENNA") != std::string::npos) {
            AntennaCalib calib;
            if (parseAntennaBlock(stream, source, calib))
                out.antennas.push_back(std::move(calib));
        }
    }

    return out;
}

// =============================================================================
//  parseAntennaBlock
// =============================================================================

bool AntexParser::parseAntennaBlock(std::istream& stream,
                                     const std::string& source,
                                     AntennaCalib& calib)
{
    std::string line;
    bool sawType = false;

    while (std::getline(stream, line)) {
        if (line.find("END OF ANTENNA") != std::string::npos)
            return sawType;

        // TYPE / SERIAL NO -- columns 1-20 type, 21-40 serial.
        if (line.find("TYPE / SERIAL NO") != std::string::npos) {
            sawType = true;
            if (line.size() >= 20)
                calib.antennaType = line.substr(0, 20);
            // Trim trailing spaces.
            auto pos = calib.antennaType.find_last_not_of(' ');
            if (pos != std::string::npos)
                calib.antennaType = calib.antennaType.substr(0, pos + 1);

            // Serial number (or satellite SVN code): columns 21-40.
            if (line.size() >= 40) {
                const std::string sn = line.substr(20, 20);
                const auto f = sn.find_first_not_of(' ');
                const auto l = sn.find_last_not_of(' ');
                if (f != std::string::npos)
                    calib.serialNo = sn.substr(f, l - f + 1);
            }

            // Satellite code: columns 41-60 (SVN + PRN for satellite antennas).
            if (line.size() >= 60) {
                const std::string sc = line.substr(40, 20);
                const auto f = sc.find_first_not_of(' ');
                const auto l = sc.find_last_not_of(' ');
                if (f != std::string::npos) {
                    calib.svCode   = sc.substr(f, l - f + 1);
                    calib.satellite = true;
                }
            }
            continue;
        }

        // DAZI -- azimuth increment (not stored, PCV rows carry their own azimuth).

        // START OF FREQUENCY.
        if (line.find("START OF FREQUENCY") != std::string::npos) {
            try {
                AntennaFreq freq = parseFreqBlock(stream, source);
                // Frequency label: rest of "START OF FREQUENCY" line.
                if (line.size() >= 10) {
                    std::string lbl = line.substr(8);
                    const auto f = lbl.find_first_not_of(' ');
                    const auto l = lbl.find_last_not_of(' ');
                    if (f != std::string::npos)
                        freq.obsCode = lbl.substr(f, l - f + 1);
                }
                calib.freqs.push_back(std::move(freq));
            } catch (const ParseException&) {
                // Skip malformed frequency block.
            }
        }
    }
    return sawType;
}

// =============================================================================
//  parseFreqBlock
// =============================================================================

AntennaFreq AntexParser::parseFreqBlock(std::istream& stream,
                                         const std::string& source)
{
    AntennaFreq freq;
    std::string line;

    while (std::getline(stream, line)) {
        if (line.find("END OF FREQUENCY") != std::string::npos)
            return freq;

        // NORTH / EAST / UP (receiver) or X / Y / Z (satellite).
        if (line.find("NORTH / EAST UP") != std::string::npos ||
            line.find("NORTH / EAST / UP") != std::string::npos) {
            std::sscanf(line.c_str(), " %lf %lf %lf",
                        &freq.pcoN, &freq.pcoE, &freq.pcoU);
            continue;
        }
        // Some ANTEX files label satellite PCO as "X / Y / Z".
        if (line.find("X / Y / Z") != std::string::npos) {
            std::sscanf(line.c_str(), " %lf %lf %lf",
                        &freq.pcoE, &freq.pcoN, &freq.pcoU);
            continue;
        }

        // NOAZI row (non-azimuth-dependent PCV).
        if (line.substr(0, 5) == "NOAZI") {
            freq.pcv.pcv.push_back(parsePcvRow(line.substr(5)));
            continue;
        }

        // Azimuth-dependent row: starts with numeric azimuth.
        if (!line.empty() && (line[0] == ' ' || std::isdigit(static_cast<unsigned char>(line[0])))) {
            // Check if it looks like a numeric azimuth (not a label).
            std::istringstream probe(line);
            double az;
            if (probe >> az) {
                freq.pcv.azimuths.push_back(az);
                std::string rest;
                std::getline(probe, rest);
                freq.pcv.pcv.push_back(parsePcvRow(rest));
            }
        }
    }
    throw ParseException("AntexParser: END OF FREQUENCY not found in '" + source + "'.");
}

// =============================================================================
//  parsePcvRow
// =============================================================================

std::vector<double> AntexParser::parsePcvRow(const std::string& line)
{
    std::vector<double> vals;
    std::istringstream iss(line);
    double v;
    while (iss >> v)
        vals.push_back(v);
    return vals;
}
