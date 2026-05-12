#pragma once

namespace loki::gnss {

/**
 * @brief Input data passed to every correction model.
 *
 * All angular values are in radians. Height is in metres.
 * gpsSow is GPS seconds of week of the observation epoch.
 */
struct CorrectionInput {
    double lat{0.0};        ///< Geodetic latitude [rad].
    double lon{0.0};        ///< Geodetic longitude [rad].
    double h{0.0};          ///< Ellipsoidal height [m].
    double elevation{0.0};  ///< Satellite elevation [rad].
    double azimuth{0.0};    ///< Satellite azimuth [rad].
    double gpsSow{0.0};     ///< GPS seconds of week of the observation epoch.
};

/**
 * @brief Abstract base class for all GNSS signal correction models.
 *
 * Each concrete model implements delay() to return the correction
 * in metres that must be SUBTRACTED from the raw pseudorange:
 *
 *   prCorrected = prRaw + satClkM - ionoDelay - tropoDelay - sagnacDelay
 *
 * Polymorphism allows SPP, PPP, and future solvers to inject any
 * combination of models (Klobuchar, IONEX, Saastamoinen, VMF3, ...)
 * without modifying solver code.
 */
class CorrectionModel {
public:
    virtual ~CorrectionModel() = default;

    /**
     * @brief Computes the signal delay in metres.
     *
     * @param in  Geometry and timing at the observation epoch.
     * @return    Delay [m]. Positive value increases the apparent pseudorange.
     */
    virtual double delay(const CorrectionInput& in) const = 0;
};

} // namespace loki::gnss
