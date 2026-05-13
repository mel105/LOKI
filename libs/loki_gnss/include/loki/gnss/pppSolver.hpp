#pragma once

#include <loki/gnss/gnssTypes.hpp>
#include <loki/gnss/sp3Orbit.hpp>
#include <loki/gnss/pppFilter.hpp>
#include <loki/gnss/phaseWindup.hpp>
#include <loki/gnss/correctionModel.hpp>
#include <loki/core/config.hpp>

#include <memory>
#include <vector>

namespace loki::gnss {

// =============================================================================
//  PppResult
// =============================================================================

/**
 * @brief PPP result for one epoch.
 */
struct PppResult {
    GpsTime time;
    double  x{0.0};
    double  y{0.0};
    double  z{0.0};
    double  clkBiasM{0.0};   ///< Receiver clock [m].
    double  ztdWetM{0.0};    ///< Estimated ZTD wet [m].
    double  ztdTotalM{0.0};  ///< Total ZTD = Saastamoinen ZHD + estimated ZWD [m].
    double  pdop{0.0};
    int     nSats{0};
    bool    valid{false};
    bool    converged{false};

    std::vector<double> residualsCode;    ///< IF code residuals [m].
    std::vector<double> residualsPhase;   ///< IF phase residuals [m].
};

// =============================================================================
//  PppSolverConfig
// =============================================================================

/**
 * @brief Configuration for PppSolver.
 *
 * Defined outside the class to work around GCC 13 / Windows aggregate-init bug.
 */
struct PppSolverConfig {
    std::vector<std::string> constellations{"GPS"};
    double elevMaskDeg{10.0};
    bool   phaseWindup{true};
    bool   pcoPcv{false};      ///< PCO/PCV corrections (requires ANTEX).

    PppFilterConfig filter{};

    explicit PppSolverConfig() = default;
};

// =============================================================================
//  PppSolver
// =============================================================================

/**
 * @brief Precise Point Positioning solver (ionosphere-free combination).
 *
 * Algorithm:
 *   For each epoch:
 *     1. Select pseudorange and carrier-phase observations (same code priorities
 *        as SppSolver, plus carrier-phase selection).
 *     2. Form ionosphere-free (IF) combinations:
 *          P_IF = (f1^2*P1 - f2^2*P2) / (f1^2 - f2^2)   [code]
 *          L_IF = (f1^2*L1 - f2^2*L2) / (f1^2 - f2^2)   [phase, in metres]
 *     3. Compute satellite ECEF position from SP3 (Lagrange order 9).
 *     4. Compute satellite clock from CLK file (linear interpolation).
 *     5. Apply Sagnac rotation to satellite position.
 *     6. Compute phase windup correction [m].
 *     7. Apply Saastamoinen a priori ZHD (mapped); ZTD wet is filter parameter.
 *     8. Call PppFilter::measurementUpdate().
 *     9. Store PppResult.
 *
 * GPS L1/L2 frequencies are used.  Galileo E1/E5a frequencies used when
 * GALILEO is in the constellation list.
 *
 * Convergence: filter is typically not converged for the first 20-30 minutes
 * (~40-60 epochs at 30s).  PppResult::converged reflects filter convergence.
 *
 * PCO/PCV corrections: applied only when pcoPcv=true and AntexFile is loaded.
 * For the initial PPP implementation, PCO/PCV is left as a planned extension
 * (flag defaults to false).
 *
 * References:
 *   Kouba & Heroux (2001), GPS Solutions 5(2).
 *   IS-GPS-200 Table 20-I (GPS frequencies).
 *   Galileo OS-SIS-ICD Issue 2.0, Table 1 (Galileo frequencies).
 */
class PppSolver {
public:
    using Config = PppSolverConfig;

    /**
     * @brief Constructs the PPP solver.
     *
     * @param cfg          Solver configuration.
     * @param orbit        Sp3Orbit backed by SP3 + CLK files.
     * @param corrections  Additional correction models (Sagnac, troposphere a priori ...).
     *                     Note: troposphere ZHD portion is applied via corrections;
     *                     the ZWD is estimated by the Kalman filter.
     */
    explicit PppSolver(Config                                         cfg,
                       std::shared_ptr<Sp3Orbit>                      orbit,
                       std::vector<std::unique_ptr<CorrectionModel>>  corrections = {});

    /**
     * @brief Processes all epochs in an OBS file.
     *
     * @param obs  Parsed RINEX OBS file.
     * @param nav  Parsed NAV file (for broadcast iono fallback, ignored for PPP).
     * @param gnssCfg  Full GNSS config (reference position, station info).
     * @return     One PppResult per epoch.
     */
    [[nodiscard]] std::vector<PppResult> solveAll(
        const ObsFile&    obs,
        const NavFile&    nav,
        const GnssConfig& gnssCfg) const;

private:
    Config                                        m_cfg;
    std::shared_ptr<Sp3Orbit>                     m_orbit;
    std::vector<std::unique_ptr<CorrectionModel>> m_corrections;

    // GPS L1/L2 frequencies [Hz].
    static constexpr double GPS_F1 = 1575.42e6;
    static constexpr double GPS_F2 = 1227.60e6;
    // Galileo E1/E5a frequencies [Hz].
    static constexpr double GAL_F1 = 1575.42e6;
    static constexpr double GAL_F5 = 1176.45e6;

    static constexpr double SPEED_OF_LIGHT = 299792458.0;

    // IF combination coefficients (GPS).
    static constexpr double GPS_ALPHA = (GPS_F1*GPS_F1) / (GPS_F1*GPS_F1 - GPS_F2*GPS_F2);
    static constexpr double GPS_BETA  = (GPS_F2*GPS_F2) / (GPS_F1*GPS_F1 - GPS_F2*GPS_F2);

    // IF combination coefficients (Galileo E1/E5a).
    static constexpr double GAL_ALPHA = (GAL_F1*GAL_F1) / (GAL_F1*GAL_F1 - GAL_F5*GAL_F5);
    static constexpr double GAL_BETA  = (GAL_F5*GAL_F5) / (GAL_F1*GAL_F1 - GAL_F5*GAL_F5);

    bool isEnabled(GnssSystem system) const;

    /**
     * @brief Selects and forms IF-combined observations for one satellite.
     *
     * @param satObs   Raw RINEX observations for one satellite.
     * @param system   Constellation.
     * @return         Filled PppObservation, valid=false if codes unavailable.
     */
    static PppObservation formIfObs(const SatObs& satObs, GnssSystem system);

    /**
     * @brief Selects carrier-phase observations for IF combination.
     *
     * Priority (GPS): L1C > L1W, L2W > L2C.
     * Priority (GAL): L1X > L1C, L5X > L7X.
     *
     * @return  Phase in metres, 0.0 if not found.
     */
    static double selectPhase(const SatObs& satObs,
                               GnssSystem system,
                               int freqIdx);  // 1 = first frequency, 2 = second

    /**
     * @brief Computes Saastamoinen a priori ZHD contribution
     *        (the dry component, mapped to slant).
     */
    static double saastamoinenZhd(double lat_rad, double h_m,
                                   double elevation_rad);
};

} // namespace loki::gnss
