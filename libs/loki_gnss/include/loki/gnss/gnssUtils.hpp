#pragma once

#include <loki/gnss/gnssTypes.hpp>

#include <string>

namespace loki::gnss {

/**
 * @brief Selects the best available pseudorange from a satellite observation.
 *
 * Priority order (per constellation):
 *   GPS / QZSS : C1C > C1W > C2W > C2C > C1X > C5X
 *   Galileo    : C1X > C5X > C7X > C8X > C1C
 *   GLONASS    : C1C > C1P > C2C > C2P
 *   BeiDou     : C2I > C6I > C7I > C1X > C5X
 *
 * @param sat     Satellite observation record.
 * @param system  GNSS constellation of the satellite.
 * @return        Best pseudorange [m], or 0.0 if no valid code found.
 */
double selectPseudorange(const SatObs& sat, GnssSystem system);

/**
 * @brief Elevation-dependent observation weight: w = sin^2(elevation).
 *
 * @param elevRad  Elevation angle [rad].
 * @return         Weight in (0, 1].
 */
double elevationWeight(double elevRad);

/**
 * @brief Returns the inter-system bias map key for a constellation.
 *
 * GPS and QZSS share the GPS reference clock column and return "".
 * All other constellations return their name string used as ISB map key.
 *
 * @param system  GNSS constellation.
 * @return        ISB key string, or "" for GPS/QZSS.
 */
std::string isbKey(GnssSystem system);

} // namespace loki::gnss
