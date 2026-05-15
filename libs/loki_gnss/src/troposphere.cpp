#include <loki/gnss/troposphere.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <loki/core/logger.hpp>
#include <string>

namespace loki::gnss {

// =============================================================================
//  SaastamoinenModel::delay  (SPP interface -- full slant delay)
// =============================================================================

double SaastamoinenModel::delay(const CorrectionInput& in) const
{
    const double h      = std::max(in.h, -1000.0);
    const double sinEl  = std::max(std::sin(in.elevation), 0.05);
    return (zhd(in.lat, h) + zwd(h)) / sinEl;
}

// =============================================================================
//  SaastamoinenModel::zhd
// =============================================================================

double SaastamoinenModel::zhd(double lat_rad, double h_m)
{
    const double h = std::max(h_m, -1000.0);
    const double P = 1013.25 * std::pow(1.0 - 2.2557e-5 * h, 5.2568);
    return zhdFromPressure(P, lat_rad, h);
}

// =============================================================================
//  SaastamoinenModel::zhdFromPressure
// =============================================================================

double SaastamoinenModel::zhdFromPressure(double P_hPa,
                                           double lat_rad, double h_m)
{
    const double h = std::max(h_m, -1000.0);
    return 0.0022768 * P_hPa
           / (1.0 - 0.00266 * std::cos(2.0 * lat_rad) - 2.8e-7 * h);
}

// =============================================================================
//  SaastamoinenModel::zwd
// =============================================================================

double SaastamoinenModel::zwd(double h_m)
{
    const double h = std::max(h_m, -1000.0);
    const double T = 288.15 - 6.5e-3 * h;
    const double e = 6.108 * std::exp(17.15 * (T - 273.15) / (T - 38.25));
    return 0.002277 * (1255.0 / T + 0.05) * e;
}

// =============================================================================
//  NiellMappingFunction -- coefficient tables (Niell 1996, Table 1-3)
//
//  Five latitude entries: 15, 30, 45, 60, 75 deg.
//  Hydrostatic coefficients: mean and amplitude (seasonal variation).
//  Wet coefficients: mean only (no seasonal variation).
//
//  Coefficient layout:  [lat15, lat30, lat45, lat60, lat75]
// =============================================================================

namespace {

// Hydrostatic -- average (a, b, c)
constexpr double NH_AVG_A[5] = {1.2769934e-3, 1.2683230e-3, 1.2465397e-3,
                                  1.2196049e-3, 1.2045996e-3};
constexpr double NH_AVG_B[5] = {2.9153695e-3, 2.9152299e-3, 2.9288445e-3,
                                  2.9022565e-3, 2.9024912e-3};
constexpr double NH_AVG_C[5] = {62.610505e-3, 62.837393e-3, 63.721774e-3,
                                  63.824265e-3, 64.258455e-3};

// Hydrostatic -- amplitude (seasonal)
constexpr double NH_AMP_A[5] = { 0.0,          1.2709626e-5, 2.6523662e-5,
                                   3.4000452e-5,  4.1202191e-5};
constexpr double NH_AMP_B[5] = { 0.0,          2.1414979e-5, 3.0515550e-5,
                                   7.5460770e-5,  7.5869791e-5};
constexpr double NH_AMP_C[5] = { 0.0,          9.0128400e-5, 4.3497037e-5,
                                   8.4795348e-5,  1.7020961e-4};

// Hydrostatic -- height correction (a_ht, b_ht, c_ht) -- latitude-independent
constexpr double NH_HT_A = 2.53e-5;
constexpr double NH_HT_B = 5.49e-3;
constexpr double NH_HT_C = 1.14e-3;

// Wet -- average (a, b, c)
constexpr double NW_AVG_A[5] = {5.8021897e-4, 5.6794847e-4, 5.8118019e-4,
                                  5.9727542e-4, 6.1641693e-4};
constexpr double NW_AVG_B[5] = {1.4275268e-3, 1.5138625e-3, 1.4572752e-3,
                                  1.5007428e-3, 1.7599082e-3};
constexpr double NW_AVG_C[5] = {4.3472961e-2, 4.6729510e-2, 4.3908931e-2,
                                  4.4626982e-2, 5.4736038e-2};

} // namespace

// =============================================================================
//  NiellMappingFunction::interpCoeff
//
//  Linear interpolation between the 5 latitude nodes.
// =============================================================================

double NiellMappingFunction::interpCoeff(const double table[5],
                                          double lat_abs_deg)
{
    // Clamp to [15, 75] deg.
    lat_abs_deg = std::max(15.0, std::min(75.0, lat_abs_deg));

    // Node spacing = 15 deg.
    const double idx_d = (lat_abs_deg - 15.0) / 15.0;
    const int    idx0  = std::min(static_cast<int>(idx_d), 3);
    const int    idx1  = idx0 + 1;
    const double frac  = idx_d - static_cast<double>(idx0);

    return table[idx0] * (1.0 - frac) + table[idx1] * frac;
}

// =============================================================================
//  NiellMappingFunction::continuedFraction
// =============================================================================

double NiellMappingFunction::continuedFraction(double a, double b, double c,
                                                 double el_rad)
{
    const double s = std::sin(el_rad);
    const double num = 1.0 + a / (1.0 + b / (1.0 + c));
    const double den = s + a / (s + b / (s + c));
    return num / den;
}

// =============================================================================
//  NiellMappingFunction::hydrostatic
// =============================================================================

double NiellMappingFunction::hydrostatic(double lat_rad, double h_m,
                                          int doy, double el_rad)
{
    el_rad = std::max(el_rad, 0.05);   // guard against horizon

    const double lat_deg = std::fabs(lat_rad * (180.0 / std::numbers::pi));

    // Seasonal phase: Northern hemisphere peak at doy=28 (late Jan),
    // Southern hemisphere shifted by half year.
    const double doy_eff = (lat_rad >= 0.0)
        ? static_cast<double>(doy)
        : static_cast<double>(doy) + 182.5;
    const double phase = std::cos(2.0 * std::numbers::pi * (doy_eff - 28.0) / 365.25);

    // Interpolated average + seasonal amplitude.
    const double a = interpCoeff(NH_AVG_A, lat_deg)
                   - phase * interpCoeff(NH_AMP_A, lat_deg);
    const double b = interpCoeff(NH_AVG_B, lat_deg)
                   - phase * interpCoeff(NH_AMP_B, lat_deg);
    const double c = interpCoeff(NH_AVG_C, lat_deg)
                   - phase * interpCoeff(NH_AMP_C, lat_deg);

    double mf = continuedFraction(a, b, c, el_rad);

    // Height correction.
    const double mf_before = mf;

    const double ht_corr = 1.0/std::sin(el_rad)
                     - continuedFraction(NH_HT_A, NH_HT_B, NH_HT_C, el_rad);
    mf += (h_m / 1000.0) * ht_corr;

    static bool nmfLogged = false;

    // debug
    if (!nmfLogged) {
        LOKI_INFO("PPP_DEBUG NMF: el_deg=" + std::to_string(el_rad*180.0/std::numbers::pi)
                  + " mf_before_ht=" + std::to_string(mf_before)
                  + " ht_corr=" + std::to_string(ht_corr)
                  + " h_km=" + std::to_string(h_m/1000.0)
                  + " mf_final=" + std::to_string(mf));
        nmfLogged = true;
    }

    // end debug

    return mf;
}

// =============================================================================
//  NiellMappingFunction::wet
// =============================================================================

double NiellMappingFunction::wet(double lat_rad, double el_rad)
{
    el_rad = std::max(el_rad, 0.05);
    const double lat_deg = std::fabs(lat_rad * (180.0 / std::numbers::pi));

    const double a = interpCoeff(NW_AVG_A, lat_deg);
    const double b = interpCoeff(NW_AVG_B, lat_deg);
    const double c = interpCoeff(NW_AVG_C, lat_deg);

    return continuedFraction(a, b, c, el_rad);
}

// =============================================================================
//  IwvConverter::conversionFactor
// =============================================================================

double IwvConverter::conversionFactor(double T_mean_K)
{
    // pi = 10^6 / (rho_w * R_v * (k3/T_m + k2'))
    const double denom = RHO_W * R_V * (K3 / T_mean_K + K2_PRIME);
    return 1.0e6 / denom;
}

// =============================================================================
//  IwvConverter::zwdToIwv
// =============================================================================

double IwvConverter::zwdToIwv(double zwd_m, double T_mean_K)
{
    return conversionFactor(T_mean_K) * zwd_m;
}

// =============================================================================
//  IwvConverter::surfaceTempToMeanTemp
// =============================================================================

double IwvConverter::surfaceTempToMeanTemp(double T_surface_K)
{
    // Bevis (1992) mid-latitude regression.
    return 70.2 + 0.72 * T_surface_K;
}

} // namespace loki::gnss
