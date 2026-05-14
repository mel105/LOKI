#pragma once

#include <loki/gnss/correctionModel.hpp>

namespace loki::gnss {

// =============================================================================
//  SaastamoinenModel
// =============================================================================

/**
 * @brief Saastamoinen troposphere delay model.
 *
 * The standard atmosphere is derived from station height h [m]:
 *   P = 1013.25 * (1 - 2.2557e-5 * h)^5.2568  [hPa]
 *   T = 288.15  - 6.5e-3 * h                   [K]
 *   e = 6.108   * exp(17.15*(T-273.15)/(T-38.25)) [hPa]
 *
 * Zenith hydrostatic delay (ZHD):
 *   ZHD = 0.0022768 * P / (1 - 0.00266*cos(2*lat) - 2.8e-7*h)  [m]
 *
 * Zenith wet delay (ZWD, standard atmosphere approximation):
 *   ZWD = 0.002277 * (1255/T + 0.05) * e  [m]
 *
 * The CorrectionModel::delay() interface returns the full slant delay
 * (ZHD + ZWD) / sin(elevation) and is used by SPP.
 *
 * For PPP, use the static methods zhd() and zwd() directly to separate
 * the two components: ZHD is applied as a fixed correction, ZWD is
 * estimated by the Kalman filter.
 *
 * For climatological applications (IWV retrieval), use IwvConverter.
 */
class SaastamoinenModel : public CorrectionModel {
public:
    SaastamoinenModel() = default;

    /**
     * @brief Full slant troposphere delay (ZHD + ZWD) / sin(el) [m].
     *
     * Used by SPP solver via the CorrectionModel interface.
     * @param in  Requires: in.lat [rad], in.h [m], in.elevation [rad].
     */
    double delay(const CorrectionInput& in) const override;

    // ------------------------------------------------------------------
    //  Static component methods -- use these for PPP and climatology
    // ------------------------------------------------------------------

    /**
     * @brief Zenith hydrostatic delay [m] from standard atmosphere.
     *
     * @param lat_rad  Geodetic latitude [rad].
     * @param h_m      Ellipsoidal height [m].
     * @return         ZHD [m], typically ~2.3 m at sea level.
     */
    static double zhd(double lat_rad, double h_m);

    /**
     * @brief Zenith hydrostatic delay [m] from measured surface pressure.
     *
     * More accurate than zhd() when actual pressure is available.
     * @param P_hPa    Surface pressure [hPa].
     * @param lat_rad  Geodetic latitude [rad].
     * @param h_m      Ellipsoidal height [m].
     * @return         ZHD [m].
     */
    static double zhdFromPressure(double P_hPa, double lat_rad, double h_m);

    /**
     * @brief Zenith wet delay [m] from standard atmosphere.
     *
     * This is a rough a priori estimate (~50-150 mm).
     * In PPP it is used only to initialise the filter; the Kalman filter
     * then estimates ZWD as a random-walk state parameter.
     *
     * @param h_m  Ellipsoidal height [m].
     * @return     ZWD [m].
     */
    static double zwd(double h_m);
};

// =============================================================================
//  NiellMappingFunction
// =============================================================================

/**
 * @brief Niell Mapping Functions (NMF, Niell 1996).
 *
 * Maps zenith delays to slant delays as a function of elevation angle,
 * latitude, and day of year.  More accurate than 1/sin(el) at low
 * elevations (below ~20 deg), where the difference can be several cm.
 *
 * Two separate mapping functions are provided:
 *   mf_h : hydrostatic (dry) component
 *   mf_w : wet component
 *
 * Both use the continued-fraction form:
 *   mf(el) = [1 + a/(1 + b/(1+c))] / [sin(el) + a/(sin(el) + b/(sin(el)+c))]
 *
 * The hydrostatic coefficients (a,b,c) depend on latitude and day of year.
 * The wet coefficients depend only on latitude.
 *
 * Accuracy: ~3-5 mm for el > 5 deg.
 * Sufficient for static PPP without NWM products.
 *
 * Reference: Niell, A.E. (1996), J. Geophys. Res. 101(B2), 3227-3246.
 */
class NiellMappingFunction {
public:
    /**
     * @brief Hydrostatic (dry) mapping function.
     *
     * @param lat_rad  Geodetic latitude [rad].
     * @param h_m      Ellipsoidal height [m] (height correction applied).
     * @param doy      Day of year (1-365).
     * @param el_rad   Elevation angle [rad].
     * @return         Dimensionless mapping factor mf_h >= 1.
     */
    static double hydrostatic(double lat_rad, double h_m,
                               int doy, double el_rad);

    /**
     * @brief Wet mapping function.
     *
     * @param lat_rad  Geodetic latitude [rad].
     * @param el_rad   Elevation angle [rad].
     * @return         Dimensionless mapping factor mf_w >= 1.
     */
    static double wet(double lat_rad, double el_rad);

private:
    /// @brief Continued-fraction mapping function evaluation.
    static double continuedFraction(double a, double b, double c,
                                     double el_rad);

    /// @brief Interpolates NMF coefficient table at latitude lat_rad.
    static double interpCoeff(const double table[5], double lat_abs_deg);
};

// =============================================================================
//  IwvConverter
// =============================================================================

/**
 * @brief Converts GPS-derived Zenith Wet Delay to Integrated Water Vapour.
 *
 * The conversion follows Bevis et al. (1992, J. Geophys. Res. 97, 15787):
 *
 *   IWV [kg/m^2] = pi * ZWD [m]
 *
 *   pi = 10^6 / (rho_w * R_v * (k3/T_m + k2'))
 *
 *   k2'  = 22.1 K/hPa   (refractivity constant)
 *   k3   = 3.776e5 K^2/hPa
 *   R_v  = 461.522 J/(kg*K)  (specific gas constant for water vapour)
 *   rho_w = 1000 kg/m^3
 *
 *   T_m : mean atmospheric temperature [K], estimated from surface temperature:
 *     T_m = 70.2 + 0.72 * T_surface [K]   (Bevis 1992 regression, mid-latitudes)
 *
 * IWV [kg/m^2] = precipitable water vapour [mm] (numerically equal for water
 * density = 1000 kg/m^3).
 *
 * Typical values: 5-10 mm (dry winter) to 40-60 mm (moist summer).
 *
 * Used by:
 *   loki_klimatológia: GNSS meteorology, IWV time series analysis.
 *   loki_gnss:         Diagnostic output, ZWD validation.
 */
class IwvConverter {
public:
    /**
     * @brief Converts ZWD to IWV.
     *
     * @param zwd_m       Zenith wet delay [m].
     * @param T_mean_K    Mean atmospheric temperature [K].
     *                    If unknown, use surfaceTempToMeanTemp().
     * @return            IWV [kg/m^2] = precipitable water [mm].
     */
    static double zwdToIwv(double zwd_m, double T_mean_K);

    /**
     * @brief Estimates mean atmospheric temperature from surface temperature.
     *
     * Bevis (1992) mid-latitude regression:
     *   T_m = 70.2 + 0.72 * T_surface [K]
     *
     * Valid for mid-latitude stations (30-60 deg).
     * Use regional regression coefficients for tropical/polar stations.
     *
     * @param T_surface_K  Surface temperature [K].
     * @return             Mean atmospheric temperature T_m [K].
     */
    static double surfaceTempToMeanTemp(double T_surface_K);

    /**
     * @brief Conversion factor pi at a given mean temperature.
     *
     * Useful when the caller wants to apply a different T_m model.
     *
     * @param T_mean_K  Mean atmospheric temperature [K].
     * @return          pi [dimensionless, ~6.5].
     */
    static double conversionFactor(double T_mean_K);

private:
    static constexpr double K2_PRIME = 22.1;          // [K/hPa]
    static constexpr double K3       = 3.776e5;        // [K^2/hPa]
    static constexpr double R_V      = 461.522;        // [J/(kg*K)]
    static constexpr double RHO_W    = 1000.0;         // [kg/m^3]
};

} // namespace loki::gnss
