#include <loki/gnss/troposphere.hpp>

#include <algorithm>
#include <cmath>

namespace loki::gnss {

double SaastamoinenModel::delay(const CorrectionInput& in) const
{
    // Clamp height: model breaks down below -1000 m.
    const double h = std::max(in.h, -1000.0);

    // Standard atmosphere at height h.
    const double P = 1013.25 * std::pow(1.0 - 2.2557e-5 * h, 5.2568); // [hPa]
    const double T = 288.15 - 6.5e-3 * h;                               // [K]
    const double e = 6.108 * std::exp(17.15 * (T - 273.15) / (T - 38.25)); // [hPa]

    // Zenith hydrostatic delay [m].
    const double Zhd = 0.0022768 * P
        / (1.0 - 0.00266 * std::cos(2.0 * in.lat) - 2.8e-7 * h);

    // Zenith wet delay [m].
    const double Zwet = 0.002277 * (1255.0 / T + 0.05) * e;

    // Simple 1/sin(el) mapping, clamped to avoid blow-up near horizon.
    const double sinEl = std::max(std::sin(in.elevation), 0.05);

    return (Zhd + Zwet) / sinEl;
}

} // namespace loki::gnss
