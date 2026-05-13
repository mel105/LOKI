#pragma once

#include <loki/gnss/correctionModel.hpp>

#include <array>

namespace loki::gnss {

/**
 * @brief Solid Earth Tides correction (IERS 2010 Conventions, step-1).
 *
 * The solid Earth deforms under the gravitational pull of the Moon and Sun.
 * This displaces the station position by up to ~30 cm vertically and ~5 cm
 * horizontally, causing a corresponding change in the satellite-to-station
 * range.
 *
 * Implementation follows IERS 2010 Conventions, Section 7.1.1, step-1
 * (degree-2 Love numbers, no frequency-dependent corrections):
 *
 *   h2 = 0.6078  (vertical Love number)
 *   l2 = 0.0847  (horizontal Shida number)
 *
 * Displacement vector for each body b (Moon, Sun):
 *
 *   dX = (GM_b/GM_e) * (R_e/r_b)^3 * R_e *
 *        [ h2 * (3/2*(n_b . n_s)^2 - 1/2) * n_s
 *        + 3*l2*(n_b . n_s)*(n_b - (n_b . n_s)*n_s) ]
 *
 * where n_b = unit vector Earth->body, n_s = unit vector Earth->station,
 * R_e = equatorial radius, r_b = distance Earth->body.
 *
 * Body positions (Moon, Sun) are computed from analytical series
 * (low-precision ephemeris, ~1 arcmin accuracy, sufficient for ~1 cm tides).
 *
 * The range correction is the projection of the displacement onto the
 * satellite line-of-sight unit vector (station->satellite):
 *
 *   delta_rho = -(dX . u_sat)
 *
 * where u_sat = (sat - sta) / |sat - sta|.
 *
 * Accuracy: ~1 cm for step-1. Step-2 (frequency-dependent terms, polar
 * motion) adds ~1 mm -- not needed for SPP.
 *
 * Required inputs (from CorrectionInput):
 *   staX/Y/Z  -- station ECEF [m]
 *   satX/Y/Z  -- satellite ECEF [m]
 *   gpsWeek   -- GPS week
 *   gpsSow    -- GPS seconds of week
 */
class SolidTidesModel : public CorrectionModel {
public:
    SolidTidesModel() = default;

    /**
     * @brief Computes the solid-tide range correction.
     * @param in  Must have staX/Y/Z, satX/Y/Z, gpsWeek, gpsSow filled.
     * @return    Range correction [m]. Positive = pseudorange increases.
     */
    double delay(const CorrectionInput& in) const override;

private:
    // IERS 2010 Love/Shida numbers (degree-2, elastic Earth)
    static constexpr double H2 = 0.6078;
    static constexpr double L2 = 0.0847;

    // Physical constants
    static constexpr double GM_MOON  = 4.902800e12;   ///< [m^3/s^2]
    static constexpr double GM_SUN   = 1.327124e20;   ///< [m^3/s^2]
    static constexpr double GM_EARTH = 3.986004418e14;///< [m^3/s^2]
    static constexpr double R_EARTH  = 6378136.6;     ///< Mean equatorial radius [m]

    /**
     * @brief Low-precision Moon ECEF position [m] at GPS time.
     * Analytical series, accuracy ~1 arcmin (~1500 km absolute, ~1 cm tides).
     */
    static std::array<double,3> moonEcef(int gpsWeek, double gpsSow);

    /**
     * @brief Low-precision Sun ECEF position [m] at GPS time.
     * Analytical series, accuracy ~1 arcmin.
     */
    static std::array<double,3> sunEcef(int gpsWeek, double gpsSow);

    /**
     * @brief Computes station displacement [m] due to one body.
     * @param staEcef  Station ECEF [m].
     * @param bodyEcef Body ECEF [m].
     * @param gmRatio  GM_body / GM_earth.
     * @return         Displacement vector [m] (3 components, ECEF).
     */
    static std::array<double,3> _displacementFromBody(
        const std::array<double,3>& staEcef,
        const std::array<double,3>& bodyEcef,
        double gmRatio);
};

} // namespace loki::gnss
