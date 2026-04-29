#pragma once

#include <cmath>
#include <numbers>
#include <stdexcept>
#include <string>

namespace loki::math {

// ---------------------------------------------------------------------------
// Ellipsoid model selector
// ---------------------------------------------------------------------------

/// @brief Supported reference ellipsoid models.
enum class EllipsoidModel {
    WGS84,       ///< World Geodetic System 1984
    GRS80,       ///< Geodetic Reference System 1980
    BESSEL,      ///< Bessel 1841
    KRASOVSKY,   ///< Krasovsky 1940
    CLARKE1866   ///< Clarke 1866
};

/// @brief Parse an ellipsoid model from a string name.
/// Accepted strings (case-sensitive): "WGS84", "WGS-84", "GRS80", "GRS-80",
/// "Bessel", "Krasovsky", "Clarke1866", "Clarke-1866".
/// @throws std::invalid_argument for unknown names.
inline EllipsoidModel ellipsoidFromString(const std::string& name)
{
    if (name == "WGS84"  || name == "WGS-84")          return EllipsoidModel::WGS84;
    if (name == "GRS80"  || name == "GRS-80")          return EllipsoidModel::GRS80;
    if (name == "Bessel")                              return EllipsoidModel::BESSEL;
    if (name == "Krasovsky")                           return EllipsoidModel::KRASOVSKY;
    if (name == "Clarke1866" || name == "Clarke-1866") return EllipsoidModel::CLARKE1866;
    throw std::invalid_argument("Unknown ellipsoid model: " + name);
}

// ---------------------------------------------------------------------------
// Ellipsoid struct
// ---------------------------------------------------------------------------

/**
 * @brief Reference ellipsoid defined by semi-major axis and inverse flattening.
 *
 * All derived quantities (b, e, e', N, M, W) are computed analytically from
 * the two defining constants (a, 1/f).  Angular arguments are always in
 * radians internally; callers are responsible for deg->rad conversion.
 *
 * Notation follows the standard geodetic convention used throughout loki_geodesy:
 *   a  - semi-major axis [m]
 *   b  - semi-minor axis [m]
 *   f  - first flattening  f = (a-b)/a
 *   e  - first eccentricity  e = sqrt(a^2 - b^2) / a
 *   e' - second eccentricity e' = sqrt(a^2 - b^2) / b
 *   W  - first geodetic function  W(lat) = sqrt(1 - e^2 sin^2(lat))
 *   N  - radius of curvature in prime vertical  N = a / W
 *   M  - meridian radius of curvature  M = a(1-e^2) / W^3
 *   R  - mean radius of curvature  R = sqrt(M*N)
 */
struct Ellipsoid {

    // ------------------------------------------------------------------
    // Defining constants
    // ------------------------------------------------------------------
    double a;            ///< Semi-major axis [m]
    double invF;         ///< Inverse flattening 1/f [-]

    // ------------------------------------------------------------------
    // Derived constants (computed once at construction)
    // ------------------------------------------------------------------
    double b;            ///< Semi-minor axis [m]
    double f;            ///< First flattening [-]
    double eSq;          ///< First eccentricity squared e^2 [-]
    double e;            ///< First eccentricity [-]
    double ePrimeSq;     ///< Second eccentricity squared e'^2 [-]
    double ePrime;       ///< Second eccentricity [-]

    // ------------------------------------------------------------------
    // Constructor
    // ------------------------------------------------------------------

    /**
     * @brief Construct from semi-major axis and inverse flattening.
     * @param semiMajorAxis  Semi-major axis [m], must be > 0.
     * @param inverseFlattening  1/f, must be > 0.
     */
    explicit Ellipsoid(double semiMajorAxis, double inverseFlattening)
        : a(semiMajorAxis)
        , invF(inverseFlattening)
    {
        f       = 1.0 / invF;
        b       = a * (1.0 - f);
        eSq     = f * (2.0 - f);                    // e^2 = 1 - (b/a)^2
        e       = std::sqrt(eSq);
        ePrimeSq = (a * a - b * b) / (b * b);       // e'^2 = (a^2-b^2)/b^2
        ePrime  = std::sqrt(ePrimeSq);
    }

    // ------------------------------------------------------------------
    // Latitude-dependent quantities (lat in radians)
    // ------------------------------------------------------------------

    /**
     * @brief First geodetic function W(lat) = sqrt(1 - e^2 * sin^2(lat)).
     * @param latRad  Geodetic latitude [rad].
     */
    [[nodiscard]] double W(double latRad) const noexcept
    {
        double sinLat = std::sin(latRad);
        return std::sqrt(1.0 - eSq * sinLat * sinLat);
    }

    /**
     * @brief Radius of curvature in prime vertical N(lat) = a / W(lat).
     * @param latRad  Geodetic latitude [rad].
     */
    [[nodiscard]] double N(double latRad) const noexcept
    {
        return a / W(latRad);
    }

    /**
     * @brief Meridian radius of curvature M(lat) = a(1-e^2) / W(lat)^3.
     * @param latRad  Geodetic latitude [rad].
     */
    [[nodiscard]] double M(double latRad) const noexcept
    {
        double w = W(latRad);
        return a * (1.0 - eSq) / (w * w * w);
    }

    /**
     * @brief Mean radius of curvature R(lat) = sqrt(M(lat) * N(lat)).
     * @param latRad  Geodetic latitude [rad].
     */
    [[nodiscard]] double R(double latRad) const noexcept
    {
        return std::sqrt(M(latRad) * N(latRad));
    }

    /**
     * @brief Derivative dN/dlat used in Jacobian computation.
     *
     * dN/dlat = a * e^2 * sin(lat) * cos(lat) / (1 - e^2 * sin^2(lat))^(3/2)
     *
     * @param latRad  Geodetic latitude [rad].
     */
    [[nodiscard]] double dNdLat(double latRad) const noexcept
    {
        double sinLat = std::sin(latRad);
        double cosLat = std::cos(latRad);
        double w      = W(latRad);
        return a * eSq * sinLat * cosLat / (w * w * w);
    }

    /**
     * @brief Radius of the parallel circle r(lat) = N(lat) * cos(lat).
     * @param latRad  Geodetic latitude [rad].
     */
    [[nodiscard]] double parallelRadius(double latRad) const noexcept
    {
        return N(latRad) * std::cos(latRad);
    }
};

// ---------------------------------------------------------------------------
// Factory: create a named ellipsoid
// ---------------------------------------------------------------------------

/**
 * @brief Create a reference ellipsoid by model.
 *
 * Defining constants (a [m], 1/f):
 *   WGS84      : 6378137.0,     298.257223563   (NIMA TR8350.2)
 *   GRS80      : 6378137.0,     298.257222101   (IUGG 1980)
 *   Bessel     : 6377397.155,   299.1528128     (Bessel 1841)
 *   Krasovsky  : 6378245.0,     298.3           (Krasovsky 1940)
 *   Clarke1866 : 6378206.4,     294.9786982     (Clarke 1866)
 *
 * @param model  Ellipsoid model selector.
 * @return Fully initialised Ellipsoid struct.
 */
[[nodiscard]] inline Ellipsoid makeEllipsoid(EllipsoidModel model)
{
    switch (model) {
        case EllipsoidModel::WGS84:
            return Ellipsoid{ 6378137.0,    298.257223563 };
        case EllipsoidModel::GRS80:
            return Ellipsoid{ 6378137.0,    298.257222101 };
        case EllipsoidModel::BESSEL:
            return Ellipsoid{ 6377397.155,  299.1528128   };
        case EllipsoidModel::KRASOVSKY:
            return Ellipsoid{ 6378245.0,    298.3         };
        case EllipsoidModel::CLARKE1866:
            return Ellipsoid{ 6378206.4,    294.9786982   };
    }
    // Unreachable but silences -Wreturn-type
    return Ellipsoid{ 6378137.0, 298.257223563 };
}

/// @brief Convenience overload: create from string name.
[[nodiscard]] inline Ellipsoid makeEllipsoid(const std::string& name)
{
    return makeEllipsoid(ellipsoidFromString(name));
}

} // namespace loki::math
