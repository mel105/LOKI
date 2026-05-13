#pragma once

#include <loki/gnss/gnssTypes.hpp>

#include <Eigen/Dense>
#include <map>
#include <utility>
#include <vector>

namespace loki::gnss {

// =============================================================================
//  PppFilterConfig
// =============================================================================

/**
 * @brief Configuration for PppFilter.
 *
 * Defined outside the class to work around GCC 13 / Windows aggregate-init bug.
 */
struct PppFilterConfig {
    // Process noise densities [m^2/s].
    double qPos{1.0e-8};       ///< Position white noise [m^2/s] (static: near zero).
    double qClk{10000.0};      ///< Receiver clock walk [m^2/s] (large: modelled as white).
    double qZtd{1.0e-8};       ///< ZTD wet random walk [m^2/s] (~1 mm per ~30 min).
    double qAmb{0.0};          ///< Ambiguity noise [m^2/s] (0 = constant, reset on slip).

    // Measurement noise.
    double sigmaCodeM{1.0};    ///< IF pseudorange sigma [m].
    double sigmaPhaseM{0.005}; ///< IF carrier-phase sigma [m] (5 mm).

    // Cycle slip detection.
    double gfSlipThreshM{0.05};///< Geometry-free slip threshold [m].

    // Convergence criterion: 3D position change below this threshold.
    double convergenceM{0.05}; ///< [m], filter considered converged.

    // Initial state covariance.
    double p0Pos{100.0};       ///< Initial position variance [m^2].
    double p0Clk{1.0e6};       ///< Initial clock variance [m^2].
    double p0Ztd{0.25};        ///< Initial ZTD variance [m^2] (~0.5 m).
    double p0Amb{1.0e6};       ///< Initial ambiguity variance [m^2].

    explicit PppFilterConfig() = default;
};

// =============================================================================
//  PppObservation
// =============================================================================

/**
 * @brief One IF-combined satellite observation for the PPP Kalman filter.
 *
 * Formed by PppSolver from raw pseudorange and carrier-phase observations.
 */
struct PppObservation {
    GnssSystem system{GnssSystem::UNKNOWN};
    int        prn{0};

    // Ionosphere-free combined observables [m].
    double ifCode{0.0};    ///< IF pseudorange [m].
    double ifPhase{0.0};   ///< IF carrier phase [m].

    // Geometry and corrections (all in metres).
    double satX{0.0}, satY{0.0}, satZ{0.0};  ///< Satellite ECEF [m].
    double satClkM{0.0};                      ///< Satellite clock correction [m].
    double tropoM{0.0};                       ///< A priori troposphere [m] (Saastamoinen ZHD * mf).
    double phaseWindupM{0.0};                 ///< Phase windup [m].
    double pcoM{0.0};                         ///< PCO/PCV correction [m] (applied externally).

    // Geometry-free combination (for cycle slip detection).
    double gfObs{0.0};   ///< L1 - L2 carrier phase [m].

    double elevation{0.0};  ///< [rad], used for elevation-dependent weighting.
    bool   valid{false};
};

// =============================================================================
//  PppFilter
// =============================================================================

/**
 * @brief Sequential Kalman filter for PPP with dynamic state management.
 *
 * State vector: [X, Y, Z, clk_GPS, ZTD_wet, N_1, ..., N_n]
 *
 *   Indices 0-2   : receiver ECEF position [m].
 *   Index   3     : receiver clock bias for GPS [m].
 *   Index   4     : ZTD wet component [m] (estimated).
 *   Indices 5+    : float IF ambiguities per satellite arc [m].
 *                   Added when satellite rises, removed when it sets.
 *
 * Multi-constellation: Galileo gets its own ISB parameter at index 5
 * when enabled (added as the first ambiguity slot, never removed).
 *
 * Design:
 *   - Time update: propagate state + process noise (static: position frozen,
 *     clock random walk, ZTD random walk, ambiguities constant).
 *   - Measurement update: simultaneous multi-observation update with
 *     H matrix (m x n), R diagonal (elevation-dependent).
 *   - Covariance: Joseph-form P = (I-KH)P(I-KH)^T + KRK^T (numerically stable).
 *   - Cycle slip: geometry-free L1-L2 jump detection; resets ambiguity
 *     parameter (old removed, new added with large variance).
 *
 * This class is NOT thread-safe -- one instance per processing session.
 *
 * References:
 *   Zumberge et al. (1997), Precise point positioning for the efficient
 *     and robust analysis of GPS data from large networks.
 *   Kouba & Heroux (2001), Precise Point Positioning Using IGS Orbit
 *     and Clock Products.
 */
class PppFilter {
public:
    using Config = PppFilterConfig;

    explicit PppFilter(Config cfg = Config{});

    /**
     * @brief Initialises the filter state at the first epoch.
     *
     * Must be called once before the first timeUpdate/measurementUpdate cycle.
     *
     * @param approxX  Approximate ECEF X [m] (from OBS header or SPP result).
     * @param approxY  Approximate ECEF Y [m].
     * @param approxZ  Approximate ECEF Z [m].
     * @param ztdPrior  A priori ZTD wet [m] (from Saastamoinen).
     */
    void init(double approxX, double approxY, double approxZ,
              double ztdPrior = 0.1);

    /**
     * @brief Time update: propagates state and adds process noise.
     *
     * @param dt  Epoch interval [s].
     */
    void timeUpdate(double dt);

    /**
     * @brief Measurement update for one epoch.
     *
     * Adds/removes ambiguity state slots based on which satellites are visible.
     * Detects cycle slips via geometry-free combination.
     *
     * @param obs   Vector of IF-combined observations for this epoch.
     * @return      True if the update converged (3D position change < threshold).
     */
    bool measurementUpdate(const std::vector<PppObservation>& obs);

    // ------------------------------------------------------------------
    //  State access
    // ------------------------------------------------------------------

    /// @brief Current receiver ECEF position [m].
    void position(double& x, double& y, double& z) const;

    /// @brief Current receiver clock bias [m].
    double clockBiasM() const;

    /// @brief Current estimated ZTD wet [m].
    double ztdWetM() const;

    /// @brief Position variance [m^2] (diagonal of P for X,Y,Z).
    void positionVariance(double& vx, double& vy, double& vz) const;

    /// @brief Returns true once measurementUpdate() has returned true.
    bool converged() const { return m_converged; }

    /// @brief Resets ambiguity for one satellite (cycle slip).
    void resetAmbiguity(GnssSystem system, int prn);

    /// @brief Full reset (new session).
    void reset();

private:
    Config          m_cfg;
    Eigen::VectorXd m_x;   ///< State vector.
    Eigen::MatrixXd m_P;   ///< Covariance matrix.
    bool            m_initialised{false};
    bool            m_converged{false};

    // Ambiguity slot management: (system,prn) -> index in m_x.
    using SatKey = std::pair<GnssSystem, int>;
    std::map<SatKey, int> m_ambIdx;

    // Geometry-free history for cycle slip detection.
    std::map<SatKey, double> m_prevGf;

    // Previous position for convergence check.
    double m_prevX{0.0}, m_prevY{0.0}, m_prevZ{0.0};

    // Fixed state dimension (before ambiguities).
    static constexpr int BASE_DIM = 5;  // X,Y,Z,clk,ZTD

    // ------------------------------------------------------------------
    //  Internal helpers
    // ------------------------------------------------------------------

    /// @brief Ensures an ambiguity slot exists for (system,prn); returns index.
    int ensureAmbiguity(GnssSystem system, int prn);

    /// @brief Removes ambiguity slot for (system,prn); re-indexes subsequent slots.
    void removeAmbiguity(GnssSystem system, int prn);

    /// @brief Removes ambiguity slots for satellites not in current obs set.
    void pruneAbsentSatellites(const std::vector<PppObservation>& obs);

    /**
     * @brief Joseph-form covariance update.
     *
     * P_new = (I - K*H) * P * (I - K*H)^T + K * R * K^T
     *
     * More numerically stable than the standard form for poorly conditioned
     * systems (relevant when ambiguity variance spans many orders of magnitude).
     */
    void josephUpdate(const Eigen::MatrixXd& K,
                      const Eigen::MatrixXd& H,
                      const Eigen::MatrixXd& R);

    /// @brief Elevation-dependent sigma scaling: sigma / sin(el).
    static double elevSigma(double sigmaBase, double elevRad);

    /// @brief Detects cycle slip for one satellite.
    bool detectCycleSlip(GnssSystem system, int prn, double gf_current);
};

} // namespace loki::gnss
