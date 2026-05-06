#pragma once

#include <loki/gnss/gnssTypes.hpp>

namespace loki::gnss {

/**
 * @brief Satellite position and clock bias computed from broadcast ephemeris.
 *
 * Position is in WGS-84 ECEF [m]. Clock bias is in seconds.
 * valid = false if the ephemeris is unhealthy or computation failed.
 */
struct SatState {
    double x{0.0};        ///< ECEF X [m]
    double y{0.0};        ///< ECEF Y [m]
    double z{0.0};        ///< ECEF Z [m]
    double clkBias{0.0};  ///< Satellite clock bias [s]
    bool   valid{false};
};

/**
 * @brief Satellite orbit computation from broadcast navigation messages.
 *
 * Implements:
 *   - GPS / QZSS  : IS-GPS-200 Keplerian propagation
 *   - Galileo     : OS-SIS-ICD Keplerian propagation (same equations, GAL constants)
 *   - BeiDou      : BDS-SIS-ICD Keplerian propagation (same equations, BDS constants)
 *   - GLONASS     : ICD-GLONASS RK4 numerical integration of PZ-90 state vector
 *
 * Reference time convention: all GpsTime arguments are in GPS time scale.
 * GLONASS toe is stored in GPS time after parser conversion.
 */
class KeplerOrbit {
public:
    /**
     * @brief Computes GPS satellite ECEF position and clock bias.
     * @param eph  GPS broadcast ephemeris record.
     * @param t    Signal transmission time in GPS time.
     * @return     SatState in WGS-84 ECEF [m], clock bias [s].
     */
    static SatState computeGps(const GpsBroadcastEph& eph,
                                const GpsTime& t);

    /**
     * @brief Computes Galileo satellite ECEF position and clock bias.
     * @param eph  Galileo broadcast ephemeris record.
     * @param t    Signal transmission time in GPS time.
     */
    static SatState computeGal(const GalBroadcastEph& eph,
                                const GpsTime& t);

    /**
     * @brief Computes BeiDou satellite ECEF position and clock bias.
     * @param eph  BeiDou broadcast ephemeris record.
     * @param t    Signal transmission time in GPS time.
     */
    static SatState computeBds(const BdsBroadcastEph& eph,
                                const GpsTime& t);

    /**
     * @brief Computes GLONASS satellite ECEF position and clock bias.
     *
     * Uses 4th-order Runge-Kutta integration of the PZ-90.11 equations
     * of motion from toe to t. Integration step: 30 s.
     *
     * @param eph  GLONASS broadcast ephemeris (state vector at toe).
     * @param t    Signal transmission time in GPS time.
     */
    static SatState computeGlo(const GloBroadcastEph& eph,
                                const GpsTime& t);

    /**
     * @brief Selects the best ephemeris and computes satellite state.
     *
     * Searches NavFile for ephemerides matching (system, prn) and selects
     * the one whose reference time (toe) is closest to t. Falls back to
     * toc proximity if toe is not set.
     *
     * @param nav     Complete navigation file (all constellations).
     * @param system  GNSS constellation.
     * @param prn     Satellite PRN number.
     * @param t       Signal transmission time in GPS time.
     * @return        SatState, or invalid SatState if no ephemeris found.
     */
    static SatState compute(const NavFile&  nav,
                             GnssSystem      system,
                             int             prn,
                             const GpsTime&  t);

private:
    // -------------------------------------------------------------------------
    //  Physical and system constants
    // -------------------------------------------------------------------------

    // WGS-84 (GPS, Galileo)
    static constexpr double GM_WGS84    = 3.986004418e14;  ///< [m^3/s^2]
    static constexpr double OE_WGS84    = 7.2921151467e-5; ///< Earth rotation rate [rad/s]

    // PZ-90.11 (GLONASS)
    static constexpr double GM_PZ90     = 3.9860044e14;    ///< [m^3/s^2]
    static constexpr double OE_PZ90     = 7.292115e-5;     ///< [rad/s]
    static constexpr double AE_PZ90     = 6378136.0;       ///< Semi-major axis [m]
    static constexpr double J2_PZ90     = 1.08262575e-3;   ///< Second zonal harmonic

    // CGCS2000 (BeiDou)
    static constexpr double GM_BDS      = 3.986004418e14;  ///< [m^3/s^2]
    static constexpr double OE_BDS      = 7.2921150e-5;    ///< [rad/s]

    static constexpr double F_CLOCK     = -4.442807633e-10;///< Relativistic clock correction factor [s/m^0.5]
    static constexpr double SPEED_OF_LIGHT = 299792458.0;  ///< [m/s]

    // -------------------------------------------------------------------------
    //  Internal Keplerian propagation template
    // -------------------------------------------------------------------------

    /**
     * @brief Core Keplerian orbit propagation (GPS / GAL / BDS equations).
     *
     * All three constellations use identical IS-GPS-200 equations but with
     * different GM and OmegaE constants. Calling code passes these as
     * template parameters to avoid runtime branching.
     *
     * @tparam GM      Gravitational parameter [m^3/s^2]
     * @tparam OmegaE  Earth rotation rate [rad/s] (scaled by 1e10 for template)
     *
     * Fields passed as individual doubles to avoid template coupling to
     * specific ephemeris struct types.
     */
    static SatState _keplerPropagate(
        double sqrtA,    double e,       double i0,      double Omega0,
        double omega,    double M0,      double deltaN,  double IDOT,
        double OmegaDot, double Crc,     double Crs,     double Cuc,
        double Cus,      double Cic,     double Cis,
        double af0,      double af1,     double af2,     double TGD,
        double toc_sow,  double toe_sow, int    toe_week,
        double t_sow,    int    t_week,
        double gm,       double oe);

    // -------------------------------------------------------------------------
    //  GLONASS RK4 helpers
    // -------------------------------------------------------------------------

    /// @brief PZ-90.11 acceleration vector at position (x,y,z) [km, km/s].
    static void _gloAccel(double x,  double y,  double z,
                           double ax, double ay, double az,
                           double& ddx, double& ddy, double& ddz);

    /// @brief Single RK4 step for GLONASS state vector.
    static void _gloRk4Step(double& x,  double& y,  double& z,
                              double& vx, double& vy, double& vz,
                              double  ax, double  ay, double  az,
                              double  dt);

    // -------------------------------------------------------------------------
    //  Helper: find closest ephemeris by toe proximity
    // -------------------------------------------------------------------------

    /// @brief Returns index of ephemeris in vec whose toe is closest to t.
    template<typename EphVec>
    static int _findBestEph(const EphVec& vec, int prn, double t_sow, int t_week);
};

} // namespace loki::gnss
