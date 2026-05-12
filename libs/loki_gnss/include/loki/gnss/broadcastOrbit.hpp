#pragma once

#include <loki/gnss/orbitModel.hpp>

namespace loki::gnss {

/**
 * @brief Orbit model backed by broadcast navigation ephemerides.
 *
 * Delegates computation to KeplerOrbit::compute(), which selects the
 * best available ephemeris and propagates using IS-GPS-200 / ICD equations.
 *
 * This is the standard orbit model for SPP. Future PPP will use Sp3Orbit.
 */
class BroadcastOrbit : public OrbitModel {
public:
    BroadcastOrbit() = default;

    /**
     * @brief Computes satellite state from broadcast ephemeris.
     * @param nav     Navigation file.
     * @param system  GNSS constellation.
     * @param prn     Satellite PRN.
     * @param t       Signal transmission time in GPS time.
     * @return        SatState (ECEF [m], clock bias [s]), valid=false if no ephemeris.
     */
    SatState compute(const NavFile&  nav,
                     GnssSystem      system,
                     int             prn,
                     const GpsTime&  t) const override;
};

} // namespace loki::gnss
