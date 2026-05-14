#pragma once

#include <loki/gnss/gnssTypes.hpp>

#include <map>
#include <string>
#include <utility>

namespace loki::gnss {

/**
 * @brief One Observable-Specific Bias (OSB) record from a BIAS-SINEX file.
 *
 * Units: nanoseconds [ns].  Convert to metres: bias_m = bias_ns * c / 1e9.
 */
struct OsbRecord {
    GnssSystem system{GnssSystem::UNKNOWN};
    int        prn{0};
    std::string signal;     ///< e.g. "C1C", "C1W", "C2W", "C1X"
    double      bias_ns{0.0};
    double      sigma_ns{0.0};
};

/**
 * @brief Loaded OSB table for one day.
 *
 * Key: (system, prn, signal) -> bias [ns].
 */
struct OsbFile {
    // Key: (system, prn, signal)
    using Key = std::tuple<GnssSystem, int, std::string>;
    std::map<Key, OsbRecord> records;

    /**
     * @brief Returns bias [ns] for a given satellite signal.
     * @return 0.0 if not found.
     */
    double getBiasNs(GnssSystem system, int prn,
                     const std::string& signal) const;
};

/**
 * @brief Parser for BIAS-SINEX OSB files (CODE MGEX format).
 *
 * Parses lines starting with " OSB" in the data section.
 * Signal codes follow RINEX 3 naming (C1C, C1W, C2W, C1X, C5X, ...).
 *
 * Usage:
 * @code
 *   ObsBiasParser parser;
 *   OsbFile osb = parser.parseGz("COD0MGXFIN_20240750000_01D_01D_OSB.BIA.gz");
 *   double bias = osb.getBiasNs(GnssSystem::GPS, 1, "C1C");
 * @endcode
 */
class ObsBiasParser {
public:
    ObsBiasParser() = default;

    [[nodiscard]] OsbFile parse(const std::string& filePath) const;
    [[nodiscard]] OsbFile parseGz(const std::string& filePath) const;

private:
    [[nodiscard]] OsbFile parseStream(std::istream& stream) const;

    static GnssSystem systemFromChar(char c);
};

} // namespace loki::gnss
