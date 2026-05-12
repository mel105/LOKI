#include <loki/gnss/ionosphere.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace loki::gnss {

KlobucharModel::KlobucharModel(const std::array<double, 4>& alpha,
                                const std::array<double, 4>& beta)
    : m_alpha(alpha)
    , m_beta(beta)
{}

// =============================================================================
//  delay  (IS-GPS-200 section 20.3.3.5.2.5)
//
//  Input angles are in radians. The algorithm uses semi-circles internally.
//  Returns L1 ionosphere path delay [m].
// =============================================================================

double KlobucharModel::delay(const CorrectionInput& in) const
{
    // Convert elevation to semi-circles for the IS-GPS-200 formula.
    const double elSC = in.elevation / std::numbers::pi;

    // Earth central angle between user and ionosphere pierce point [semi-circles].
    const double psi = 0.0137 / (elSC + 0.11) - 0.022;

    // Geodetic latitude of pierce point [semi-circles].
    double phi_i = in.lat / std::numbers::pi + psi * std::cos(in.azimuth);
    phi_i = std::clamp(phi_i, -0.416, 0.416);

    // Geodetic longitude of pierce point [semi-circles].
    const double lambda_i = in.lon / std::numbers::pi
        + psi * std::sin(in.azimuth) / std::cos(phi_i * std::numbers::pi);

    // Geomagnetic latitude of pierce point [semi-circles].
    const double phi_m = phi_i
        + 0.064 * std::cos((lambda_i - 1.617) * std::numbers::pi);

    // Local time at pierce point [s].
    double t = 4.32e4 * lambda_i + in.gpsSow;
    t = std::fmod(t, 86400.0);
    if (t < 0.0) t += 86400.0;

    // Amplitude of cosine term [s] -- must be >= 0.
    double Amp = m_alpha[0]
               + m_alpha[1] * phi_m
               + m_alpha[2] * phi_m * phi_m
               + m_alpha[3] * phi_m * phi_m * phi_m;
    Amp = std::max(Amp, 0.0);

    // Period of cosine term [s] -- must be >= 72000 s.
    double Per = m_beta[0]
               + m_beta[1] * phi_m
               + m_beta[2] * phi_m * phi_m
               + m_beta[3] * phi_m * phi_m * phi_m;
    Per = std::max(Per, 72000.0);

    // Phase argument [rad].
    const double x = 2.0 * std::numbers::pi * (t - 50400.0) / Per;

    // Obliquity factor (slant vs. zenith).
    const double F = 1.0 + 16.0 * std::pow(0.53 - elSC, 3.0);

    // Vertical delay [s], using cosine approximation when |x| < pi/2.
    const double T_iono = (std::fabs(x) < 1.57)
        ? (5.0e-9 + Amp * (1.0 - x*x / 2.0 + x*x*x*x / 24.0)) * F
        : 5.0e-9 * F;

    const double delayM = T_iono * SPEED_OF_LIGHT;

    // Sanity check: reject non-finite or physically implausible results.
    if (!std::isfinite(delayM) || delayM < 0.0 || delayM > 50.0)
        return 0.0;

    return delayM;
}

} // namespace loki::gnss
