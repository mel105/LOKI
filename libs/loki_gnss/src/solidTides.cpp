#include <loki/gnss/solidTides.hpp>

#include <cmath>
#include <numbers>

namespace loki::gnss {

// =============================================================================
//  GPS time -> Julian Date
// =============================================================================

static double gpsToJd(int gpsWeek, double gpsSow)
{
    // GPS epoch = JD 2444244.5 (1980-01-06 00:00:00 UTC)
    constexpr double JD_GPS_EPOCH = 2444244.5;
    return JD_GPS_EPOCH
           + static_cast<double>(gpsWeek) * 7.0
           + gpsSow / 86400.0;
}

// Julian centuries from J2000.0
static double jd2T(double jd)
{
    return (jd - 2451545.0) / 36525.0;
}

// =============================================================================
//  Low-precision Sun ECEF [m]
//  Source: Astronomical Algorithms, Meeus (1998), simplified to 0.01 deg.
//  Accuracy adequate for ~1 cm tidal corrections.
// =============================================================================

std::array<double,3> SolidTidesModel::sunEcef(int gpsWeek, double gpsSow)
{
    const double jd = gpsToJd(gpsWeek, gpsSow);
    const double T  = jd2T(jd);

    // Mean longitude [deg]
    const double L0 = 280.46646 + 36000.76983 * T;
    // Mean anomaly [deg]
    const double M  = 357.52911 + 35999.05029 * T - 0.0001537 * T * T;
    const double M_rad = M * (std::numbers::pi / 180.0);

    // Equation of centre [deg]
    const double C = (1.914602 - 0.004817*T - 0.000014*T*T) * std::sin(M_rad)
                   + (0.019993 - 0.000101*T) * std::sin(2.0*M_rad)
                   + 0.000289 * std::sin(3.0*M_rad);

    // Sun true longitude [deg] and radius vector [AU]
    const double sunLon_deg = L0 + C;
    const double sunLon_rad = sunLon_deg * (std::numbers::pi / 180.0);

    const double R_AU = 1.000001018 * (1.0 - 0.016708634*0.016708634)
                      / (1.0 + 0.016708634 * std::cos(M_rad + C * (std::numbers::pi/180.0)));

    // Mean obliquity of ecliptic [rad]
    const double eps = (23.439291111 - 0.013004167*T) * (std::numbers::pi / 180.0);

    // Ecliptic -> equatorial
    const double x_eq = R_AU * std::cos(sunLon_rad);
    const double y_eq = R_AU * (std::cos(eps)*std::sin(sunLon_rad));
    const double z_eq = R_AU * (std::sin(eps)*std::sin(sunLon_rad));

    // Greenwich Mean Sidereal Time [rad]
    const double GMST_deg = 280.46061837
                          + 360.98564736629 * (jd - 2451545.0)
                          + 0.000387933 * T * T
                          - T * T * T / 38710000.0;
    const double GMST_rad = GMST_deg * (std::numbers::pi / 180.0);

    // Equatorial -> ECEF (rotate by -GMST around Z)
    const double AU_to_m = 1.495978707e11;
    const double cosG = std::cos(GMST_rad);
    const double sinG = std::sin(GMST_rad);

    return {
        ( cosG * x_eq + sinG * y_eq) * AU_to_m,
        (-sinG * x_eq + cosG * y_eq) * AU_to_m,
        z_eq * AU_to_m
    };
}

// =============================================================================
//  Low-precision Moon ECEF [m]
//  Source: Meeus (1998) chapter 47, truncated series.
//  Absolute position accuracy ~1 arcmin (~1000 km), tidal error ~1 cm.
// =============================================================================

std::array<double,3> SolidTidesModel::moonEcef(int gpsWeek, double gpsSow)
{
    const double jd = gpsToJd(gpsWeek, gpsSow);
    const double T  = jd2T(jd);

    // Fundamental arguments [deg]
    const double Lp = 218.3164477 + 481267.88123421 * T;  // Moon mean longitude
    const double D  = 297.8501921 + 445267.1114034  * T;  // Mean elongation
    const double M  = 357.5291092 +  35999.0502909  * T;  // Sun mean anomaly
    const double Mp = 134.9633964 + 477198.8675055  * T;  // Moon mean anomaly
    const double F  =  93.2720950 + 483202.0175233  * T;  // Arg of latitude

    auto toR = [](double deg) { return deg * (std::numbers::pi / 180.0); };

    // Longitude correction [deg] -- main terms only
    const double dL = 6.288774 * std::sin(toR(Mp))
                    + 1.274027 * std::sin(toR(2*D - Mp))
                    + 0.658314 * std::sin(toR(2*D))
                    + 0.213618 * std::sin(toR(2*Mp))
                    - 0.185116 * std::sin(toR(M))
                    - 0.114332 * std::sin(toR(2*F));

    // Latitude correction [deg]
    const double dB = 5.128122 * std::sin(toR(F))
                    + 0.280602 * std::sin(toR(Mp + F))
                    + 0.277693 * std::sin(toR(Mp - F))
                    + 0.173237 * std::sin(toR(2*D - F));

    // Distance [km]
    const double dist_km = 385000.56
                         - 20905.355 * std::cos(toR(Mp))
                         -  3699.111 * std::cos(toR(2*D - Mp))
                         -  2955.968 * std::cos(toR(2*D))
                         -   569.925 * std::cos(toR(2*Mp));

    const double lon_rad = (Lp + dL) * (std::numbers::pi / 180.0);
    const double lat_rad =          dB * (std::numbers::pi / 180.0);
    const double dist_m  = dist_km * 1000.0;

    // Ecliptic -> equatorial
    const double T2 = T * T;
    const double eps = (23.439291111 - 0.013004167*T - 1.64e-7*T2
                       + 5.04e-7*T2*T) * (std::numbers::pi / 180.0);

    const double cosLat = std::cos(lat_rad);
    const double x_eq = dist_m * cosLat * std::cos(lon_rad);
    const double y_eq = dist_m * (std::cos(eps)*cosLat*std::sin(lon_rad)
                                - std::sin(eps)*std::sin(lat_rad));
    const double z_eq = dist_m * (std::sin(eps)*cosLat*std::sin(lon_rad)
                                + std::cos(eps)*std::sin(lat_rad));

    // Equatorial -> ECEF
    const double GMST_deg = 280.46061837
                          + 360.98564736629 * (jd - 2451545.0)
                          + 0.000387933 * T * T
                          - T * T * T / 38710000.0;
    const double GMST_rad = GMST_deg * (std::numbers::pi / 180.0);
    const double cosG = std::cos(GMST_rad);
    const double sinG = std::sin(GMST_rad);

    return {
         cosG * x_eq + sinG * y_eq,
        -sinG * x_eq + cosG * y_eq,
        z_eq
    };
}

// =============================================================================
//  _displacementFromBody
//  IERS 2010 step-1 displacement formula for one body.
// =============================================================================

std::array<double,3> SolidTidesModel::_displacementFromBody(
    const std::array<double,3>& staEcef,
    const std::array<double,3>& bodyEcef,
    double gmRatio)
{
    // Unit vector Earth->station
    const double rSta = std::sqrt(staEcef[0]*staEcef[0]
                                + staEcef[1]*staEcef[1]
                                + staEcef[2]*staEcef[2]);
    if (rSta < 1.0) return {0.0, 0.0, 0.0};

    const double ns[3] = {staEcef[0]/rSta, staEcef[1]/rSta, staEcef[2]/rSta};

    // Unit vector Earth->body and distance
    const double rBody = std::sqrt(bodyEcef[0]*bodyEcef[0]
                                 + bodyEcef[1]*bodyEcef[1]
                                 + bodyEcef[2]*bodyEcef[2]);
    if (rBody < 1.0) return {0.0, 0.0, 0.0};

    const double nb[3] = {bodyEcef[0]/rBody, bodyEcef[1]/rBody, bodyEcef[2]/rBody};

    // Dot product n_b . n_s
    const double dot = nb[0]*ns[0] + nb[1]*ns[1] + nb[2]*ns[2];

    // Amplitude factor: GM_b/GM_e * (R_e/r_b)^3 * R_e
    const double amp = gmRatio * std::pow(R_EARTH / rBody, 3.0) * R_EARTH;

    // IERS 2010 eq. (7.5):
    // dX = amp * [ h2*(3/2*dot^2 - 1/2)*ns + 3*l2*dot*(nb - dot*ns) ]
    std::array<double,3> disp{};
    for (int i = 0; i < 3; ++i) {
        const double term_h = H2 * (1.5 * dot*dot - 0.5) * ns[i];
        const double term_l = 3.0 * L2 * dot * (nb[i] - dot * ns[i]);
        disp[i] = amp * (term_h + term_l);
    }
    return disp;
}

// =============================================================================
//  delay
// =============================================================================

double SolidTidesModel::delay(const CorrectionInput& in) const
{
    const std::array<double,3> staEcef{in.staX, in.staY, in.staZ};
    const std::array<double,3> satEcef{in.satX, in.satY, in.satZ};

    // Satellite line-of-sight unit vector (station -> satellite)
    double dx = satEcef[0] - staEcef[0];
    double dy = satEcef[1] - staEcef[1];
    double dz = satEcef[2] - staEcef[2];
    const double rng = std::sqrt(dx*dx + dy*dy + dz*dz);
    if (rng < 1.0) return 0.0;
    dx /= rng; dy /= rng; dz /= rng;

    // Compute body positions
    const auto moonPos = moonEcef(in.gpsWeek, in.gpsSow);
    const auto sunPos  = sunEcef (in.gpsWeek, in.gpsSow);

    // Station displacement from Moon and Sun
    const auto dispMoon = _displacementFromBody(staEcef, moonPos,
                                                 GM_MOON / GM_EARTH);
    const auto dispSun  = _displacementFromBody(staEcef, sunPos,
                                                 GM_SUN  / GM_EARTH);

    // Total displacement
    const double totalX = dispMoon[0] + dispSun[0];
    const double totalY = dispMoon[1] + dispSun[1];
    const double totalZ = dispMoon[2] + dispSun[2];

    // Range correction = projection of displacement onto LOS
    // Negative because station moves toward satellite -> range decreases
    return -(totalX*dx + totalY*dy + totalZ*dz);
}

} // namespace loki::gnss
