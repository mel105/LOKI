#include <loki/gnss/gnssUtils.hpp>

#include <cmath>
#include <vector>

namespace loki::gnss {

// =============================================================================
//  selectPseudorange
// =============================================================================

double selectPseudorange(const SatObs& sat, GnssSystem system)
{
    // Maximum physically plausible pseudorange [m].
    // GEO orbit radius ~42 164 km; 45 000 km provides a generous upper bound.
    constexpr double MAX_PR = 4.5e7;

    std::vector<std::string> priority;
    switch (system) {
        case GnssSystem::GPS:
        case GnssSystem::QZSS:
            priority = {"C1C", "C1W", "C2W", "C2C", "C1X", "C5X"};
            break;
        case GnssSystem::GALILEO:
            priority = {"C1X", "C5X", "C7X", "C8X", "C1C"};
            break;
        case GnssSystem::GLONASS:
            priority = {"C1C", "C1P", "C2C", "C2P"};
            break;
        case GnssSystem::BEIDOU:
            priority = {"C2I", "C6I", "C7I", "C1X", "C5X"};
            break;
        default:
            return 0.0;
    }

    for (const auto& code : priority) {
        const auto it = sat.obs.find(code);
        if (it != sat.obs.end() && it->second.valid &&
            it->second.value > 1.0e6 && it->second.value < MAX_PR)
            return it->second.value;
    }
    return 0.0;
}

// =============================================================================
//  elevationWeight
// =============================================================================

double elevationWeight(double elevRad)
{
    const double s = std::sin(elevRad);
    return s * s;
}

// =============================================================================
//  isbKey
// =============================================================================

std::string isbKey(GnssSystem system)
{
    switch (system) {
        case GnssSystem::GLONASS: return "GLONASS";
        case GnssSystem::GALILEO: return "GALILEO";
        case GnssSystem::BEIDOU:  return "BEIDOU";
        default:                  return "";   // GPS / QZSS: reference clock
    }
}

} // namespace loki::gnss
