#pragma once

#include <loki/gnss/gnssTypes.hpp>

#include <array>
#include <map>
#include <utility>

namespace loki::gnss {

/**
 * @brief Phase windup correction for carrier-phase observations (Wu et al. 1993).
 *
 * During satellite motion the carrier-phase measurement accumulates an extra
 * fractional-cycle offset due to the rotation of the satellite transmit
 * antenna relative to the receiver.  This effect is called phase windup.
 *
 * The correction is continuous (accumulates across epochs) and must be
 * tracked per satellite arc.  A new arc (cycle slip or satellite rise)
 * resets the accumulated value.
 *
 * Algorithm (simplified Wu et al.):
 *   1. Compute satellite effective dipole unit vector D_s in ECEF:
 *        e_z  = -sat_pos / |sat_pos|                      (nadir vector)
 *        e_sun = sun_pos (unit)                            (toward Sun)
 *        e_y  = cross(e_z, e_sun) / |...|                 (orbital Y)
 *        D_s  = cross(e_y, e_z)                           (effective X dipole)
 *   2. Compute receiver effective dipole unit vector D_r in ECEF:
 *        e_k  = (sat_pos - sta_pos) / |...|               (unit LOS)
 *        east = cross(north_hat, e_k) / |...|             (receiver east)
 *        D_r  = east - cross(e_k, north_hat)              (effective dipole)
 *   3. delta_phi_raw = sign(e_k . cross(D_s, D_r)) *
 *                      acos(D_s . D_r / (|D_s||D_r|))    [radians]
 *   4. Unwrap: delta_phi_unwrapped adjusted to nearest integer-cycle
 *              continuation from previous epoch.
 *   5. Result in cycles = delta_phi_unwrapped / (2*pi).
 *
 * Sun position is computed from the analytical low-precision model
 * (same Meeus series used by SolidTidesModel -- ~1 arcmin, adequate for
 * phase windup at mm level).
 *
 * The PhaseWindup object is stateful: it must be reused across all epochs
 * for a given processing session.  One instance per PppSolver run.
 *
 * Usage:
 * @code
 *   PhaseWindup pw;
 *   // Per epoch, per satellite:
 *   double cycles = pw.compute(system, prn, sat_pos, sat_vel, sta_pos, gpsTime);
 *   double metres = cycles * lambda;   // lambda = c/freq [m]
 * @endcode
 */
class PhaseWindup {
public:
    PhaseWindup() = default;

    /**
     * @brief Computes the accumulated phase windup correction.
     *
     * @param system    GNSS constellation.
     * @param prn       Satellite PRN.
     * @param sat_pos   Satellite ECEF position [m].
     * @param sat_vel   Satellite ECEF velocity [m/s] (from SP3 finite diff).
     * @param sta_pos   Station ECEF position [m].
     * @param gpsWeek   GPS week (for Sun position).
     * @param gpsSow    GPS seconds of week (for Sun position).
     * @return          Accumulated phase windup [cycles].
     *                  Multiply by wavelength [m/cycle] to get path length.
     */
    double compute(GnssSystem                  system,
                   int                         prn,
                   const std::array<double,3>& sat_pos,
                   const std::array<double,3>& sat_vel,
                   const std::array<double,3>& sta_pos,
                   int                         gpsWeek,
                   double                      gpsSow);

    /**
     * @brief Resets the accumulated phase windup for one satellite arc.
     *
     * Call this when a cycle slip is detected or the satellite rises.
     */
    void resetArc(GnssSystem system, int prn);

    /**
     * @brief Resets all accumulated state (new session / new file).
     */
    void reset();

private:
    using SatKey = std::pair<GnssSystem, int>;

    // Accumulated phase windup per satellite [cycles].
    std::map<SatKey, double> m_accumulated;

    // Previous epoch effective dipole vectors (for continuity check).
    std::map<SatKey, std::array<double,3>> m_prevDs;
    std::map<SatKey, std::array<double,3>> m_prevDr;

    // Low-precision Sun ECEF [m] (same Meeus series as SolidTidesModel).
    static std::array<double,3> sunEcef(int gpsWeek, double gpsSow);

    // Vector math helpers.
    static std::array<double,3> cross(const std::array<double,3>& a,
                                       const std::array<double,3>& b);
    static double dot(const std::array<double,3>& a,
                      const std::array<double,3>& b);
    static std::array<double,3> normalize(const std::array<double,3>& v);
    static double norm(const std::array<double,3>& v);
};

} // namespace loki::gnss
