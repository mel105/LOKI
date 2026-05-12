#pragma once

#include <loki/gnss/correctionModel.hpp>

namespace loki::gnss {

/**
 * @brief Saastamoinen troposphere delay model.
 *
 * Computes the zenith troposphere delay (hydrostatic + wet components) using
 * the standard atmosphere and maps it to slant delay via 1/sin(elevation).
 *
 * The standard atmosphere pressure and temperature are derived from the
 * station ellipsoidal height h [m]:
 *   P = 1013.25 * (1 - 2.2557e-5 * h)^5.2568  [hPa]
 *   T = 288.15 - 6.5e-3 * h                    [K]
 *   e = 6.108 * exp(17.15 * (T - 273.15) / (T - 38.25))  [hPa]
 *
 * Zenith hydrostatic delay (Saastamoinen 1973):
 *   Zhd = 0.0022768 * P / (1 - 0.00266*cos(2*lat) - 2.8e-7*h)
 *
 * Zenith wet delay (simplified):
 *   Zwet = 0.002277 * (1255/T + 0.05) * e
 *
 * Accuracy: ~1 cm in zenith, degrades toward the horizon.
 * PPP will use VMF3 mapping functions (planned).
 *
 * Note: in.h and in.lat are used; in.elevation is the mapping angle.
 */
class SaastamoinenModel : public CorrectionModel {
public:
    SaastamoinenModel() = default;

    /**
     * @brief Computes the troposphere slant delay.
     * @param in  Geometry: lat [rad], h [m], elevation [rad].
     * @return    Delay [m].
     */
    double delay(const CorrectionInput& in) const override;
};

} // namespace loki::gnss
