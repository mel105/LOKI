#include <loki/geodesy/geodesicLine.hpp>
#include <loki/core/logger.hpp>

#include <cmath>
#include <numbers>

using namespace loki::geodesy;
using namespace loki::math;

namespace {

constexpr double DEG2RAD     = std::numbers::pi / 180.0;
constexpr double RAD2DEG     = 180.0 / std::numbers::pi;
constexpr double EARTH_RADIUS = 6371008.8; // IUGG arithmetic mean radius [m]
constexpr double VINCENTY_TOL = 1e-12;
constexpr int    MAX_ITER     = 200;

inline double toRad(double d) noexcept { return d * DEG2RAD; }
inline double toDeg(double r) noexcept { return r * RAD2DEG; }

// Normalise azimuth to [0, 360)
inline double normAz(double azDeg) noexcept
{
    azDeg = std::fmod(azDeg, 360.0);
    if (azDeg < 0.0) azDeg += 360.0;
    return azDeg;
}

// Clamp latitude away from exact poles to avoid singularities
inline double clampLat(double latRad) noexcept
{
    constexpr double HALF_PI = std::numbers::pi / 2.0;
    constexpr double EPS     = 1e-10;
    if (latRad >  HALF_PI - EPS) return  HALF_PI - EPS;
    if (latRad < -HALF_PI + EPS) return -HALF_PI + EPS;
    return latRad;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Vincenty inverse
// ---------------------------------------------------------------------------

GeodesicResult loki::geodesy::vincentyInverse(double lat1Deg, double lon1Deg,
                                               double lat2Deg, double lon2Deg,
                                               const Ellipsoid& ell)
{
    double a = ell.a;
    double b = ell.b;
    double f = ell.f;

    double lat1 = clampLat(toRad(lat1Deg));
    double lat2 = clampLat(toRad(lat2Deg));

    // Reduced latitudes
    double U1 = std::atan((1.0 - f) * std::tan(lat1));
    double U2 = std::atan((1.0 - f) * std::tan(lat2));

    double sinU1 = std::sin(U1), cosU1 = std::cos(U1);
    double sinU2 = std::sin(U2), cosU2 = std::cos(U2);

    // Difference in longitude (mod 2pi)
    double lon1 = toRad(lon1Deg);
    double lon2 = toRad(lon2Deg);
    double L    = std::fmod(lon2 - lon1, 2.0 * std::numbers::pi);

    double lambda    = L;
    double lambdaPrev = 0.0;
    bool   converged = false;

    double sinSigma{}, cosSigma{}, sigma{}, sinAlpha{}, cosSqAlpha{}, cos2SigmaM{};

    for (int i = 0; i < MAX_ITER; ++i) {
        lambdaPrev = lambda;

        double sinLam = std::sin(lambda);
        double cosLam = std::cos(lambda);

        double t1 = cosU2 * sinLam;
        double t2 = cosU1 * sinU2 - sinU1 * cosU2 * cosLam;

        sinSigma = std::sqrt(t1 * t1 + t2 * t2);
        cosSigma = sinU1 * sinU2 + cosU1 * cosU2 * cosLam;
        sigma    = std::atan2(sinSigma, cosSigma);

        if (sinSigma == 0.0) {
            // Coincident points
            return { 0.0, 0.0, 180.0 };
        }

        sinAlpha   = cosU1 * cosU2 * sinLam / sinSigma;
        cosSqAlpha = 1.0 - sinAlpha * sinAlpha;

        cos2SigmaM = (cosSqAlpha > 0.0)
            ? cosSigma - 2.0 * sinU1 * sinU2 / cosSqAlpha
            : 0.0; // equatorial line

        double C = f / 16.0 * cosSqAlpha * (4.0 + f * (4.0 - 3.0 * cosSqAlpha));

        lambda = L + (1.0 - C) * f * sinAlpha
                 * (sigma + C * sinSigma * (cos2SigmaM + C * cosSigma
                    * (-1.0 + 2.0 * cos2SigmaM * cos2SigmaM)));

        if (std::fabs(lambda - lambdaPrev) < VINCENTY_TOL) {
            converged = true;
            break;
        }

        if (std::fabs(lambda) > std::numbers::pi) {
            LOKI_WARNING("vincentyInverse: near-antipodal points, precision reduced");
            lambda = std::numbers::pi;
            break;
        }
    }

    if (!converged) {
        LOKI_WARNING("vincentyInverse: failed to converge");
    }

    double u2 = cosSqAlpha * (a * a - b * b) / (b * b);
    double A  = 1.0 + u2 / 16384.0 * (4096.0 + u2 * (-768.0 + u2 * (320.0 - 175.0 * u2)));
    double B  = u2 / 1024.0 * (256.0 + u2 * (-128.0 + u2 * (74.0 - 47.0 * u2)));

    double deltaSigma = B * sinSigma * (cos2SigmaM + B / 4.0
        * (cosSigma * (-1.0 + 2.0 * cos2SigmaM * cos2SigmaM)
           - B / 6.0 * cos2SigmaM * (-3.0 + 4.0 * sinSigma * sinSigma)
                                   * (-3.0 + 4.0 * cos2SigmaM * cos2SigmaM)));

    double distance = b * A * (sigma - deltaSigma);

    // Forward azimuth at point 1
    double az1 = std::atan2(cosU2 * std::sin(lambda),
                            cosU1 * sinU2 - sinU1 * cosU2 * std::cos(lambda));

    // Reverse azimuth at point 2
    double az2 = std::atan2(cosU1 * std::sin(lambda),
                            -sinU1 * cosU2 + cosU1 * sinU2 * std::cos(lambda));

    return { distance, normAz(toDeg(az1)), normAz(toDeg(az2)) };
}

// ---------------------------------------------------------------------------
// Vincenty direct
// ---------------------------------------------------------------------------

DirectResult loki::geodesy::vincentyDirect(double lat1Deg, double lon1Deg,
                                            double azimuthDeg, double distance,
                                            const Ellipsoid& ell)
{
    double a = ell.a;
    double b = ell.b;
    double f = ell.f;

    double lat1 = clampLat(toRad(lat1Deg));
    double lon1 = toRad(lon1Deg);
    double az1  = toRad(azimuthDeg);

    double U1    = std::atan((1.0 - f) * std::tan(lat1));
    double sinU1 = std::sin(U1);
    double cosU1 = std::cos(U1);
    double sinAz = std::sin(az1);
    double cosAz = std::cos(az1);

    double sigma1    = std::atan2(std::tan(U1), cosAz);
    double sinAlpha  = cosU1 * sinAz;
    double cosSqAlpha = 1.0 - sinAlpha * sinAlpha;

    double u2 = cosSqAlpha * (a * a - b * b) / (b * b);
    double A  = 1.0 + u2 / 16384.0 * (4096.0 + u2 * (-768.0 + u2 * (320.0 - 175.0 * u2)));
    double B  = u2 / 1024.0 * (256.0 + u2 * (-128.0 + u2 * (74.0 - 47.0 * u2)));

    double sigma    = distance / (b * A);
    double sigmaPrev = 0.0;

    double cos2SigmaM{}, sinSigma{}, cosSigma{};

    for (int i = 0; i < MAX_ITER; ++i) {
        sigmaPrev  = sigma;
        cos2SigmaM = std::cos(2.0 * sigma1 + sigma);
        sinSigma   = std::sin(sigma);
        cosSigma   = std::cos(sigma);

        double deltaSigma = B * sinSigma * (cos2SigmaM + B / 4.0
            * (cosSigma * (-1.0 + 2.0 * cos2SigmaM * cos2SigmaM)
               - B / 6.0 * cos2SigmaM * (-3.0 + 4.0 * sinSigma * sinSigma)
                                       * (-3.0 + 4.0 * cos2SigmaM * cos2SigmaM)));

        sigma = distance / (b * A) + deltaSigma;

        if (std::fabs(sigma - sigmaPrev) < VINCENTY_TOL) break;
    }

    cos2SigmaM = std::cos(2.0 * sigma1 + sigma);
    sinSigma   = std::sin(sigma);
    cosSigma   = std::cos(sigma);

    double lat2 = std::atan2(
        sinU1 * cosSigma + cosU1 * sinSigma * cosAz,
        (1.0 - f) * std::sqrt(sinAlpha * sinAlpha
            + (sinU1 * sinSigma - cosU1 * cosSigma * cosAz)
            * (sinU1 * sinSigma - cosU1 * cosSigma * cosAz)));

    double lambda = std::atan2(
        sinSigma * sinAz,
        cosU1 * cosSigma - sinU1 * sinSigma * cosAz);

    double C   = f / 16.0 * cosSqAlpha * (4.0 + f * (4.0 - 3.0 * cosSqAlpha));
    double lon2 = lon1 + lambda - (1.0 - C) * f * sinAlpha
                  * (sigma + C * sinSigma * (cos2SigmaM + C * cosSigma
                     * (-1.0 + 2.0 * cos2SigmaM * cos2SigmaM)));

    double az2 = std::atan2(sinAlpha,
                            -sinU1 * sinSigma + cosU1 * cosSigma * cosAz);

    return { toDeg(lat2), toDeg(lon2), normAz(toDeg(az2)) };
}

// ---------------------------------------------------------------------------
// Haversine
// ---------------------------------------------------------------------------

GeodesicResult loki::geodesy::haversine(double lat1Deg, double lon1Deg,
                                         double lat2Deg, double lon2Deg)
{
    double lat1 = toRad(lat1Deg);
    double lon1 = toRad(lon1Deg);
    double lat2 = toRad(lat2Deg);
    double lon2 = toRad(lon2Deg);

    double dLat = lat2 - lat1;
    double dLon = lon2 - lon1;

    double sinHalfLat = std::sin(dLat / 2.0);
    double sinHalfLon = std::sin(dLon / 2.0);

    double a = sinHalfLat * sinHalfLat
             + std::cos(lat1) * std::cos(lat2) * sinHalfLon * sinHalfLon;

    double c        = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
    double distance = EARTH_RADIUS * c;

    double az1 = std::atan2(std::sin(dLon) * std::cos(lat2),
                            std::cos(lat1) * std::sin(lat2)
                            - std::sin(lat1) * std::cos(lat2) * std::cos(dLon));

    double az2 = std::atan2(std::sin(-dLon) * std::cos(lat1),
                            std::cos(lat2) * std::sin(lat1)
                            - std::sin(lat2) * std::cos(lat1) * std::cos(-dLon));

    return { distance, normAz(toDeg(az1)), normAz(toDeg(az2)) };
}
