#include <loki/gnss/relativity.hpp>

#include <cmath>

namespace loki::gnss::Relativity {

std::array<double, 3> rotateSatPosition(const std::array<double, 3>& satEcef,
                                         double tof)
{
    const double theta  = OMEGA_E * tof;
    const double cosT   = std::cos(theta);
    const double sinT   = std::sin(theta);
    return {
         satEcef[0] * cosT + satEcef[1] * sinT,
        -satEcef[0] * sinT + satEcef[1] * cosT,
         satEcef[2]
    };
}

double sagnacDelay(const std::array<double, 3>& satEcef,
                   const std::array<double, 3>& rcvEcef)
{
    return (OMEGA_E / SPEED_OF_LIGHT)
         * (satEcef[0] * rcvEcef[1] - satEcef[1] * rcvEcef[0]);
}

} // namespace loki::gnss::Relativity
