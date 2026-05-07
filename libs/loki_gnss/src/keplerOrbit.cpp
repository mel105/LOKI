#include <loki/gnss/keplerOrbit.hpp>
#include <loki/core/exceptions.hpp>

#include <cmath>
#include <limits>
#include <numbers>

using namespace loki;
using namespace loki::gnss;

// =============================================================================
//  _findBestEph
//  Finds the index of the ephemeris in vec with matching PRN and smallest
//  |toe - t| difference. Returns -1 if no matching PRN found.
// =============================================================================

template<typename EphVec>
int KeplerOrbit::_findBestEph(const EphVec& vec, int prn,
                                double t_sow, int t_week) {
    int    best  = -1;
    double bestDt = std::numeric_limits<double>::max();
    const double t_total = static_cast<double>(t_week) * 604800.0 + t_sow;

    for (int i = 0; i < static_cast<int>(vec.size()); ++i) {
        if (vec[i].prn != prn) continue;
        const double toe_total =
            static_cast<double>(vec[i].toe.week) * 604800.0 + vec[i].toe.sow;
        const double dt = std::fabs(toe_total - t_total);
        if (dt < bestDt) { bestDt = dt; best = i; }
    }
    return best;
}

// Explicit instantiations for all ephemeris vector types
template int KeplerOrbit::_findBestEph(
    const std::vector<GpsBroadcastEph>&, int, double, int);
template int KeplerOrbit::_findBestEph(
    const std::vector<GalBroadcastEph>&, int, double, int);
template int KeplerOrbit::_findBestEph(
    const std::vector<BdsBroadcastEph>&, int, double, int);
template int KeplerOrbit::_findBestEph(
    const std::vector<GloBroadcastEph>&, int, double, int);

// =============================================================================
//  _keplerPropagate  (GPS / Galileo / BeiDou -- identical equations)
//
//  Implements IS-GPS-200 Table 20-IV.
//  gm  = gravitational parameter [m^3/s^2]
//  oe  = Earth rotation rate [rad/s]
// =============================================================================

SatState KeplerOrbit::_keplerPropagate(
    double sqrtA,    double e,        double i0,      double Omega0,
    double omega,    double M0,       double deltaN,  double IDOT,
    double OmegaDot, double Crc,      double Crs,     double Cuc,
    double Cus,      double Cic,      double Cis,
    double af0,      double af1,      double af2,     double TGD,
    double toc_sow,  double toe_sow,  int    toe_week,
    double t_sow,    int    t_week,
    double gm,       double oe)
{
    SatState state;

    // -- Time from clock reference epoch (toc) --------------------------------
    // toc and toe may be in different GPS weeks (week rollover case).
    // Estimate toc_week from toe_week and toc_sow proximity.
    int toc_week = toe_week;
    if (toc_sow - toe_sow > 302400.0)  toc_week = toe_week - 1;
    if (toc_sow - toe_sow < -302400.0) toc_week = toe_week + 1;
    const double toc_total = static_cast<double>(toc_week) * 604800.0 + toc_sow;
    const double t_total   = static_cast<double>(t_week)   * 604800.0 + t_sow;
    double dt_clk = t_total - toc_total;

    // Correct for GPS week crossover
    if (dt_clk >  302400.0) dt_clk -= 604800.0;
    if (dt_clk < -302400.0) dt_clk += 604800.0;

    // -- Satellite clock bias (IS-GPS-200 eq. 20.4.7) ------------------------
    // Relativistic correction term added after Eccentric Anomaly iteration.
    const double clk_raw = af0 + af1 * dt_clk + af2 * dt_clk * dt_clk - TGD;

    // -- Time from ephemeris reference epoch (toe) ----------------------------
    double tk = t_total - (static_cast<double>(toe_week) * 604800.0 + toe_sow);
    if (tk >  302400.0) tk -= 604800.0;
    if (tk < -302400.0) tk += 604800.0;

    // -- Semi-major axis ------------------------------------------------------
    const double A  = sqrtA * sqrtA;
    const double n0 = std::sqrt(gm / (A * A * A));   // Mean motion [rad/s]
    const double n  = n0 + deltaN;                    // Corrected mean motion

    // -- Mean anomaly ---------------------------------------------------------
    const double M = M0 + n * tk;

    // -- Eccentric Anomaly (Kepler equation, iterative) -----------------------
    double E = M;
    for (int iter = 0; iter < 12; ++iter) {
        const double dE = (M - E + e * std::sin(E)) / (1.0 - e * std::cos(E));
        E += dE;
        if (std::fabs(dE) < 1.0e-12) break;
    }

    // -- Relativistic clock correction ----------------------------------------
    const double dtr = F_CLOCK * e * sqrtA * std::sin(E);
    state.clkBias = clk_raw + dtr;

    // -- True anomaly ---------------------------------------------------------
    const double sinE = std::sin(E);
    const double cosE = std::cos(E);
    const double nu   = std::atan2(std::sqrt(1.0 - e * e) * sinE,
                                    cosE - e);

    // -- Argument of latitude -------------------------------------------------
    const double Phi = nu + omega;

    // -- Second-order harmonic corrections ------------------------------------
    const double sin2Phi = std::sin(2.0 * Phi);
    const double cos2Phi = std::cos(2.0 * Phi);

    const double du = Cus * sin2Phi + Cuc * cos2Phi;  // Argument of latitude
    const double dr = Crs * sin2Phi + Crc * cos2Phi;  // Radius
    const double di = Cis * sin2Phi + Cic * cos2Phi;  // Inclination

    // -- Corrected orbital elements -------------------------------------------
    const double u = Phi + du;
    const double r = A * (1.0 - e * cosE) + dr;
    const double i = i0 + di + IDOT * tk;

    // -- Position in orbital plane --------------------------------------------
    const double xp = r * std::cos(u);
    const double yp = r * std::sin(u);

    // -- Corrected longitude of ascending node --------------------------------
    // For BeiDou GEO (PRN 1-5, 59-63): different formula -- not handled here,
    // caller should use the standard formula; GEO correction is done in SPP.
    const double Omega = Omega0 + (OmegaDot - oe) * tk - oe * toe_sow;

    // -- ECEF coordinates -----------------------------------------------------
    const double sinO = std::sin(Omega);
    const double cosO = std::cos(Omega);
    const double sinI = std::sin(i);
    const double cosI = std::cos(i);

    state.x = xp * cosO - yp * cosI * sinO;
    state.y = xp * sinO + yp * cosI * cosO;
    state.z = yp * sinI;
    state.valid = true;

    return state;
}

// =============================================================================
//  computeGps
// =============================================================================

SatState KeplerOrbit::computeGps(const GpsBroadcastEph& e,
                                   const GpsTime& t) {
    if (e.SVhealth != 0) return SatState{};

    return _keplerPropagate(
        e.sqrtA,    e.e,        e.i0,       e.Omega0,
        e.omega,    e.M0,       e.deltaN,   e.IDOT,
        e.OmegaDot, e.Crc,      e.Crs,      e.Cuc,
        e.Cus,      e.Cic,      e.Cis,
        e.af0,      e.af1,      e.af2,      e.TGD,
        e.toc.sow,  e.toe.sow,  e.toe.week,
        t.sow,      t.week,
        GM_WGS84,   OE_WGS84);
}

// =============================================================================
//  computeGal
// =============================================================================

SatState KeplerOrbit::computeGal(const GalBroadcastEph& e,
                                   const GpsTime& t) {
    if (e.SVhealth != 0) return SatState{};

    // Galileo uses BGDe5a as the primary group delay for E1-only users.
    // For IF combination, TGD cancels -- we pass BGDe5a as TGD equivalent.
    return _keplerPropagate(
        e.sqrtA,    e.e,        e.i0,       e.Omega0,
        e.omega,    e.M0,       e.deltaN,   e.IDOT,
        e.OmegaDot, e.Crc,      e.Crs,      e.Cuc,
        e.Cus,      e.Cic,      e.Cis,
        e.af0,      e.af1,      e.af2,      e.BGDe5a,
        e.toc.sow,  e.toe.sow,  e.toe.week,
        t.sow,      t.week,
        GM_WGS84,   OE_WGS84);
}

// =============================================================================
//  computeBds
// =============================================================================

SatState KeplerOrbit::computeBds(const BdsBroadcastEph& e,
                                   const GpsTime& t) {
    if (e.SVhealth != 0) return SatState{};

    // BeiDou reference time: BDT epoch = 2006-01-01 00:00:00 UTC.
    // Our GpsTime stores BDS time converted to GPST by the parser (14s offset).
    // TGD1 is the B1/B3 group delay used for single-frequency B1 users.
    return _keplerPropagate(
        e.sqrtA,    e.e,        e.i0,       e.Omega0,
        e.omega,    e.M0,       e.deltaN,   e.IDOT,
        e.OmegaDot, e.Crc,      e.Crs,      e.Cuc,
        e.Cus,      e.Cic,      e.Cis,
        e.af0,      e.af1,      e.af2,      e.TGD1,
        e.toc.sow,  e.toe.sow,  e.toe.week,
        t.sow,      t.week,
        GM_BDS,     OE_BDS);
}

// =============================================================================
//  computeGlo  -- RK4 integration of PZ-90.11 equations of motion
// =============================================================================

void KeplerOrbit::_gloAccel(double x,  double y,  double z,
                              double ax, double ay, double az,
                              double& ddx, double& ddy, double& ddz) {
    // PZ-90.11 equations of motion (ICD-GLONASS 5.1, section 3.3.3.3)
    // All units: km, km/s, km/s^2
    const double r2   = x*x + y*y + z*z;
    const double r    = std::sqrt(r2);
    const double r3   = r2 * r;
    const double r5   = r3 * r2;

    // Gravitational acceleration components
    const double mu_r3  = GM_PZ90 * 1.0e-9 / r3;  // convert GM from m^3/s^2 to km^3/s^2
    const double ae2_r2 = (AE_PZ90 * 1.0e-3) * (AE_PZ90 * 1.0e-3) / r2; // (ae/r)^2 in km

    const double j2_fac = 1.5 * J2_PZ90 * mu_r3 * ae2_r2 / r2;

    // z-component factor for J2
    const double z2_r2  = z * z / r2;

    ddx = -mu_r3 * x - j2_fac * x * (1.0 - 5.0 * z2_r2)
          + OE_PZ90 * OE_PZ90 * x + 2.0 * OE_PZ90 * ay + ax;
    ddy = -mu_r3 * y - j2_fac * y * (1.0 - 5.0 * z2_r2)
          + OE_PZ90 * OE_PZ90 * y - 2.0 * OE_PZ90 * ax + ay;
    ddz = -mu_r3 * z - j2_fac * z * (3.0 - 5.0 * z2_r2) + az;
}

void KeplerOrbit::_gloRk4Step(double& x,  double& y,  double& z,
                                double& vx, double& vy, double& vz,
                                double  ax, double  ay, double  az,
                                double  dt) {
    // k1
    double ddx1, ddy1, ddz1;
    _gloAccel(x, y, z, ax, ay, az, ddx1, ddy1, ddz1);
    double k1x = vx, k1y = vy, k1z = vz;
    double k1vx = ddx1, k1vy = ddy1, k1vz = ddz1;

    // k2
    double ddx2, ddy2, ddz2;
    _gloAccel(x + 0.5*dt*k1x,  y + 0.5*dt*k1y,  z + 0.5*dt*k1z,
              ax, ay, az, ddx2, ddy2, ddz2);
    double k2x = vx + 0.5*dt*k1vx, k2y = vy + 0.5*dt*k1vy, k2z = vz + 0.5*dt*k1vz;
    double k2vx = ddx2, k2vy = ddy2, k2vz = ddz2;

    // k3
    double ddx3, ddy3, ddz3;
    _gloAccel(x + 0.5*dt*k2x,  y + 0.5*dt*k2y,  z + 0.5*dt*k2z,
              ax, ay, az, ddx3, ddy3, ddz3);
    double k3x = vx + 0.5*dt*k2vx, k3y = vy + 0.5*dt*k2vy, k3z = vz + 0.5*dt*k2vz;
    double k3vx = ddx3, k3vy = ddy3, k3vz = ddz3;

    // k4
    double ddx4, ddy4, ddz4;
    _gloAccel(x + dt*k3x,  y + dt*k3y,  z + dt*k3z,
              ax, ay, az, ddx4, ddy4, ddz4);
    double k4vx = ddx4, k4vy = ddy4, k4vz = ddz4;
    double k4x = vx + dt*k3vx, k4y = vy + dt*k3vy, k4z = vz + dt*k3vz;

    // Update state
    const double sixth = 1.0 / 6.0;
    x  += dt * sixth * (k1x  + 2.0*k2x  + 2.0*k3x  + k4x);
    y  += dt * sixth * (k1y  + 2.0*k2y  + 2.0*k3y  + k4y);
    z  += dt * sixth * (k1z  + 2.0*k2z  + 2.0*k3z  + k4z);
    vx += dt * sixth * (k1vx + 2.0*k2vx + 2.0*k3vx + k4vx);
    vy += dt * sixth * (k1vy + 2.0*k2vy + 2.0*k3vy + k4vy);
    vz += dt * sixth * (k1vz + 2.0*k2vz + 2.0*k3vz + k4vz);
}

SatState KeplerOrbit::computeGlo(const GloBroadcastEph& e,
                                   const GpsTime& t) {
    if (e.health != 0) return SatState{};

    // Time difference from toe [s]
    const double t_total   = static_cast<double>(t.week)     * 604800.0 + t.sow;
    const double toe_total = static_cast<double>(e.toe.week) * 604800.0 + e.toe.sow;
    double dt = t_total - toe_total;

    // Initial state in km and km/s (RINEX stores km)
    double x = e.x,  y = e.y,  z = e.z;
    double vx= e.vx, vy= e.vy, vz= e.vz;
    const double ax = e.ax, ay = e.ay, az = e.az;

    // RK4 integration with 30 s steps
    constexpr double STEP = 30.0;
    const int nSteps = static_cast<int>(std::fabs(dt) / STEP);
    const double sign = (dt >= 0.0) ? 1.0 : -1.0;

    for (int i = 0; i < nSteps; ++i)
        _gloRk4Step(x, y, z, vx, vy, vz, ax, ay, az, sign * STEP);

    // Final fractional step
    const double dtRem = dt - sign * static_cast<double>(nSteps) * STEP;
    if (std::fabs(dtRem) > 1.0e-9)
        _gloRk4Step(x, y, z, vx, vy, vz, ax, ay, az, dtRem);

    // Convert km -> m
    SatState state;
    state.x    = x  * 1000.0;
    state.y    = y  * 1000.0;
    state.z    = z  * 1000.0;

    // GLONASS clock: dt = -tauN + gammaN * (t - toe)
    state.clkBias = -e.tauN + e.gammaN * dt;
    state.valid   = true;
    return state;
}

// =============================================================================
//  compute -- dispatcher
// =============================================================================

SatState KeplerOrbit::compute(const NavFile&  nav,
                               GnssSystem      system,
                               int             prn,
                               const GpsTime&  t) {
    switch (system) {
        case GnssSystem::GPS:
        case GnssSystem::QZSS: {
            const int idx = _findBestEph(nav.gpsEph, prn, t.sow, t.week);
            if (idx < 0) return SatState{};
            return computeGps(nav.gpsEph[static_cast<std::size_t>(idx)], t);
        }
        case GnssSystem::GALILEO: {
            const int idx = _findBestEph(nav.galEph, prn, t.sow, t.week);
            if (idx < 0) return SatState{};
            return computeGal(nav.galEph[static_cast<std::size_t>(idx)], t);
        }
        case GnssSystem::GLONASS: {
            const int idx = _findBestEph(nav.gloEph, prn, t.sow, t.week);
            if (idx < 0) return SatState{};
            return computeGlo(nav.gloEph[static_cast<std::size_t>(idx)], t);
        }
        case GnssSystem::BEIDOU: {
            const int idx = _findBestEph(nav.bdsEph, prn, t.sow, t.week);
            if (idx < 0) return SatState{};
            return computeBds(nav.bdsEph[static_cast<std::size_t>(idx)], t);
        }
        default:
            return SatState{};
    }
}
