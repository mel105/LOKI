#pragma once

#include <loki/gnss/correctionModel.hpp>

#include <array>

namespace loki::gnss {

/**
 * @brief Klobuchar single-frequency ionosphere delay model (IS-GPS-200 20.3.3.5.2.5).
 *
 * Returns the L1 ionosphere path delay in metres.
 * The model coefficients (alpha, beta) are broadcast in the GPS NAV header.
 *
 * Accuracy: ~50 % of the actual ionosphere delay is removed on average.
 * Suitable for SPP. PPP will use IONEX interpolation (IonexModel, planned).
 */
class KlobucharModel : public CorrectionModel {
public:
    /**
     * @brief Constructs the model with Klobuchar coefficients from the NAV header.
     * @param alpha  Four alpha coefficients [s, s/semi-circle, s/semi-circle^2, s/semi-circle^3].
     * @param beta   Four beta  coefficients [s, s/semi-circle, s/semi-circle^2, s/semi-circle^3].
     */
    explicit KlobucharModel(const std::array<double, 4>& alpha,
                             const std::array<double, 4>& beta);

    /**
     * @brief Computes the L1 ionosphere path delay.
     * @param in  Geometry: lat/lon [rad], elevation [rad], azimuth [rad], gpsSow [s].
     * @return    Delay [m]. Returns 0.0 if the result is non-finite or out of range.
     */
    double delay(const CorrectionInput& in) const override;

private:
    std::array<double, 4> m_alpha;
    std::array<double, 4> m_beta;

    static constexpr double SPEED_OF_LIGHT = 299792458.0;
};

} // namespace loki::gnss
