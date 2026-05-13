#include <loki/gnss/phaseWindup.hpp>

#include <cmath>
#include <numbers>

using namespace loki::gnss;

// =============================================================================
//  Public interface
// =============================================================================

double PhaseWindup::compute(GnssSystem                  system,
                             int                         prn,
                             const std::array<double,3>& sat_pos,
                             const std::array<double,3>& /*sat_vel*/,
                             const std::array<double,3>& sta_pos,
                             int                         gpsWeek,
                             double                      gpsSow)
{
    const SatKey key{system, prn};

    // ------------------------------------------------------------------
    //  Step 1: satellite effective dipole D_s
    // ------------------------------------------------------------------
    // Nadir unit vector (Earth -> satellite is +z direction of body frame).
    const auto neg_r_s = std::array<double,3>{
        -sat_pos[0], -sat_pos[1], -sat_pos[2]};
    const auto e_z = normalize(neg_r_s);

    // Unit vector toward Sun.
    const auto sun  = sunEcef(gpsWeek, gpsSow);
    const auto e_sun = normalize(sun);

    // Orbital y-axis (perpendicular to nadir and Sun direction).
    auto e_y = normalize(cross(e_z, e_sun));

    // Effective satellite dipole (x-axis of body frame projected to sky).
    auto D_s = cross(e_y, e_z);  // not normalised yet -- magnitude may vary
    {
        // Subtract projection onto LOS to get component perpendicular to LOS.
        const auto e_k = normalize(std::array<double,3>{
            sat_pos[0] - sta_pos[0],
            sat_pos[1] - sta_pos[1],
            sat_pos[2] - sta_pos[2]});
        const double proj = dot(D_s, e_k);
        D_s[0] -= proj * e_k[0];
        D_s[1] -= proj * e_k[1];
        D_s[2] -= proj * e_k[2];
    }

    // ------------------------------------------------------------------
    //  Step 2: receiver effective dipole D_r
    // ------------------------------------------------------------------
    // Line-of-sight unit vector (receiver -> satellite).
    const auto e_k = normalize(std::array<double,3>{
        sat_pos[0] - sta_pos[0],
        sat_pos[1] - sta_pos[1],
        sat_pos[2] - sta_pos[2]});

    // Local North unit vector at station.
    // Approximate: project global Z on to tangent plane, normalise.
    const std::array<double,3> global_z{0.0, 0.0, 1.0};
    const double z_proj = dot(global_z, e_k);
    std::array<double,3> north_hat{
        global_z[0] - z_proj * e_k[0],
        global_z[1] - z_proj * e_k[1],
        global_z[2] - z_proj * e_k[2]};
    const double nh_norm = norm(north_hat);
    if (nh_norm < 1.0e-9) {
        // Satellite is at zenith -- windup undefined, return accumulated.
        return m_accumulated.count(key) ? m_accumulated[key] : 0.0;
    }
    north_hat[0] /= nh_norm;
    north_hat[1] /= nh_norm;
    north_hat[2] /= nh_norm;

    // Receiver east = cross(north, e_k).
    const auto east_hat = normalize(cross(north_hat, e_k));

    // Receiver effective dipole: east - cross(e_k, north).
    const auto cross_k_n = cross(e_k, north_hat);
    std::array<double,3> D_r{
        east_hat[0] - cross_k_n[0],
        east_hat[1] - cross_k_n[1],
        east_hat[2] - cross_k_n[2]};

    // Project out LOS component.
    {
        const double proj = dot(D_r, e_k);
        D_r[0] -= proj * e_k[0];
        D_r[1] -= proj * e_k[1];
        D_r[2] -= proj * e_k[2];
    }

    // ------------------------------------------------------------------
    //  Step 3: raw phase difference [radians]
    // ------------------------------------------------------------------
    const double ds_norm = norm(D_s);
    const double dr_norm = norm(D_r);
    if (ds_norm < 1.0e-9 || dr_norm < 1.0e-9) {
        return m_accumulated.count(key) ? m_accumulated[key] : 0.0;
    }

    const double cos_phi = dot(D_s, D_r) / (ds_norm * dr_norm);
    const double clamped = std::max(-1.0, std::min(1.0, cos_phi));
    const auto   cross_sd = cross(D_s, D_r);
    const double sign_phi = (dot(e_k, cross_sd) >= 0.0) ? 1.0 : -1.0;

    const double delta_phi_rad = sign_phi * std::acos(clamped);

    // ------------------------------------------------------------------
    //  Step 4: phase unwrapping (keep continuity)
    // ------------------------------------------------------------------
    double phi_cycles = delta_phi_rad / (2.0 * std::numbers::pi);

    if (m_accumulated.count(key)) {
        const double prev = m_accumulated[key];
        // Round difference to nearest integer to unwrap.
        const double diff = phi_cycles - prev;
        const double rounded = std::round(diff);
        phi_cycles = prev + (diff - rounded);
    }

    m_accumulated[key] = phi_cycles;
    return phi_cycles;
}

void PhaseWindup::resetArc(GnssSystem system, int prn)
{
    const SatKey key{system, prn};
    m_accumulated.erase(key);
    m_prevDs.erase(key);
    m_prevDr.erase(key);
}

void PhaseWindup::reset()
{
    m_accumulated.clear();
    m_prevDs.clear();
    m_prevDr.clear();
}

// =============================================================================
//  Sun position (Meeus analytical, ~1 arcmin)
// =============================================================================

std::array<double,3> PhaseWindup::sunEcef(int gpsWeek, double gpsSow)
{
    // GPS total seconds since 1980-01-06.
    const double gpsTotalSec = static_cast<double>(gpsWeek) * 604800.0 + gpsSow;
    // Julian date: GPS epoch = JD 2444244.5.
    const double jd  = 2444244.5 + gpsTotalSec / 86400.0;
    const double T   = (jd - 2451545.0) / 36525.0;  // Julian centuries from J2000

    // Mean longitude and mean anomaly of the Sun [deg].
    const double L0  = 280.46646 + 36000.76983 * T;
    const double M   = 357.52911 + 35999.05029 * T - 0.0001537 * T * T;
    const double Mrad = M * (std::numbers::pi / 180.0);

    // Equation of centre [deg].
    const double C   = (1.914602 - 0.004817 * T - 0.000014 * T * T) * std::sin(Mrad)
                     + (0.019993 - 0.000101 * T)                     * std::sin(2.0 * Mrad)
                     +  0.000289                                       * std::sin(3.0 * Mrad);

    // Sun true longitude and apparent longitude [deg].
    const double sunLon = L0 + C;
    const double omega  = 125.04 - 1934.136 * T;
    const double appLon = sunLon - 0.00569 - 0.00478 * std::sin(omega * (std::numbers::pi/180.0));

    // Obliquity of the ecliptic [deg].
    const double eps0   = 23.439291111 - 0.013004167 * T;
    const double eps    = eps0 + 0.00256 * std::cos(omega * (std::numbers::pi/180.0));
    const double epsRad = eps  * (std::numbers::pi / 180.0);
    const double lonRad = appLon * (std::numbers::pi / 180.0);

    // Sun unit vector in geocentric celestial reference frame.
    const double sunX_cel = std::cos(lonRad);
    const double sunY_cel = std::cos(epsRad) * std::sin(lonRad);
    const double sunZ_cel = std::sin(epsRad) * std::sin(lonRad);

    // Approximate Earth rotation angle (GMST in radians).
    const double GMST_deg = 280.46061837
                          + 360.98564736629 * (jd - 2451545.0)
                          + 0.000387933 * T * T;
    const double GMST_rad = GMST_deg * (std::numbers::pi / 180.0);

    // Rotate from celestial to ECEF.
    const double sunX = std::cos(GMST_rad) * sunX_cel + std::sin(GMST_rad) * sunY_cel;
    const double sunY =-std::sin(GMST_rad) * sunX_cel + std::cos(GMST_rad) * sunY_cel;
    const double sunZ = sunZ_cel;

    // Scale to approximate Sun distance [m] (~1 AU).
    constexpr double AU = 1.496e11;
    return {sunX * AU, sunY * AU, sunZ * AU};
}

// =============================================================================
//  Vector helpers
// =============================================================================

std::array<double,3> PhaseWindup::cross(const std::array<double,3>& a,
                                         const std::array<double,3>& b)
{
    return {
        a[1]*b[2] - a[2]*b[1],
        a[2]*b[0] - a[0]*b[2],
        a[0]*b[1] - a[1]*b[0]
    };
}

double PhaseWindup::dot(const std::array<double,3>& a,
                         const std::array<double,3>& b)
{
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

double PhaseWindup::norm(const std::array<double,3>& v)
{
    return std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

std::array<double,3> PhaseWindup::normalize(const std::array<double,3>& v)
{
    const double n = norm(v);
    if (n < 1.0e-30) return {0.0, 0.0, 0.0};
    return {v[0]/n, v[1]/n, v[2]/n};
}
