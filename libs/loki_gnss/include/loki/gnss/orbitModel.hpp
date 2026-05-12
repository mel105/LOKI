#pragma once

#include <loki/gnss/gnssTypes.hpp>
#include <loki/gnss/keplerOrbit.hpp>

namespace loki::gnss {

/**
 * @brief Abstract base class for satellite orbit models.
 *
 * Concrete implementations include:
 *   - BroadcastOrbit : Keplerian propagation from RINEX NAV (SPP).
 *   - Sp3Orbit       : Lagrange interpolation from SP3 precise orbits (PPP, planned).
 *
 * All solvers depend on this interface. Swapping broadcast for precise
 * orbits requires no solver code changes.
 */
class OrbitModel {
public:
    virtual ~OrbitModel() = default;

    /**
     * @brief Computes satellite ECEF position and clock bias at signal transmission time.
     *
     * @param nav     Navigation file (broadcast ephemerides).
     * @param system  GNSS constellation.
     * @param prn     Satellite PRN number.
     * @param t       Signal transmission time in GPS time.
     * @return        SatState in ECEF [m], clock bias [s]. valid=false if no ephemeris found.
     */
    virtual SatState compute(const NavFile&  nav,
                              GnssSystem      system,
                              int             prn,
                              const GpsTime&  t) const = 0;
};

} // namespace loki::gnss
