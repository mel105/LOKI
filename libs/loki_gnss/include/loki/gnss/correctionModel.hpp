#pragma once

namespace loki::gnss {

/**
 * @brief Input data passed to every correction model.
 *
 * All angular values are in radians. Heights and positions in metres.
 * gpsSow is GPS seconds of week of the observation epoch.
 *
 * satX/Y/Z and staX/Y/Z are ECEF positions [m] needed by corrections
 * that depend on the satellite-station geometry (solid tides, PCO/PCV).
 * They are filled by SppSolver before calling each model's delay().
 */
struct CorrectionInput {
    // Station geodetic (from current position estimate)
    double lat{0.0};        ///< Geodetic latitude [rad].
    double lon{0.0};        ///< Geodetic longitude [rad].
    double h{0.0};          ///< Ellipsoidal height [m].

    // Satellite geometry
    double elevation{0.0};  ///< Satellite elevation [rad].
    double azimuth{0.0};    ///< Satellite azimuth [rad].
    double gpsSow{0.0};     ///< GPS seconds of week of the observation epoch.

    // ECEF positions [m] -- filled by solver, used by geometry-dependent models
    double satX{0.0};       ///< Satellite ECEF X [m].
    double satY{0.0};       ///< Satellite ECEF Y [m].
    double satZ{0.0};       ///< Satellite ECEF Z [m].
    double staX{0.0};       ///< Station ECEF X [m] (current estimate).
    double staY{0.0};       ///< Station ECEF Y [m].
    double staZ{0.0};       ///< Station ECEF Z [m].

    // Full GPS time (needed by time-dependent models like solid tides)
    int    gpsWeek{0};      ///< GPS week number.
};

/**
 * @brief Abstract base class for all GNSS signal correction models.
 *
 * Each concrete model implements delay() to return the correction
 * in metres that is SUBTRACTED from the raw pseudorange:
 *
 *   prCorrected = prRaw + satClkM - sum(model->delay())
 *
 * Polymorphism allows SPP, PPP, and future solvers to inject any
 * combination of models without modifying solver code.
 */
class CorrectionModel {
public:
    virtual ~CorrectionModel() = default;

    /**
     * @brief Computes the signal delay in metres.
     *
     * @param in  Geometry, timing, and ECEF positions at the observation epoch.
     * @return    Delay [m]. Positive value increases the apparent pseudorange.
     */
    virtual double delay(const CorrectionInput& in) const = 0;
};

} // namespace loki::gnss
