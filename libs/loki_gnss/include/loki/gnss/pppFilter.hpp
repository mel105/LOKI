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
 * Single-phase startup: carrier phase is used from the first epoch.
 * Ambiguities are bootstrapped as N = ifPhase - ifCode on first appearance.
 * The bootstrap absorbs any code bias; the phase residual is near zero by
 * construction, so the filter drives ZWD and position from phase innovations
 * (weight ~360 000x larger than code).  Code observations remain in the
 * update to weakly constrain the clock but cannot corrupt ZWD.
 *
 * ZWD convergence: with ~15 satellites and sigma_phase = 5 mm, ZWD sigma
 * drops below 1 cm within ~10 minutes.  No code-only pre-filter is needed.
 */
struct PppFilterConfig {
    // Process noise [m^2/s].
    double qPos    {1.0e-8};   ///< Position (static).
    double qClk    {100.0};    ///< Receiver clock random walk.
    double qZtd    {1.0e-5};   ///< ZWD random walk [m^2/s] -- ~1 mm/sqrt(hr).
    double qZtdCode{1.0e-5};   ///< Unused (kept for ABI compatibility).
    double qAmb    {0.0};      ///< Ambiguity (0 = constant between slips).

    // Measurement noise.
    double sigmaCodeM {3.0};   ///< IF pseudorange sigma [m].
    double sigmaPhaseM{0.005}; ///< IF carrier-phase sigma [m].

    // Cycle slip.
    double gfSlipThreshM{0.35};

    // Convergence.
    double convergenceM{0.05};

    // Initial covariance.
    double p0Pos{100.0};
    double p0Clk{1.0e10};
    double p0Ztd{0.5};
    double p0Amb{1.0e6};
    double p0Isb{1.0e4};

    // Set to 0: use phase from the first epoch (recommended).
    // Code-only pre-filter is harmful when a systematic code bias exists
    // because the filter absorbs the bias into ZWD over many epochs.
    int codeOnlyEpochs{0};

    // ZWD soft constraint sigma [m].
    // Each epoch a pseudo-observation pulls ZWD toward the Saastamoinen prior.
    // sigma=0.5 m allows ~50cm deviation from prior before strong pull-back.
    double sigmaZtdConstraint{0.05};

    // epochs to keep ambiguity after satellite disappears
    int ambiguityHoldEpochs{5};  

    explicit PppFilterConfig() = default;
};

// =============================================================================
//  PppObservation
// =============================================================================

/**
 * @brief One IF-combined satellite observation for PppFilter.
 *
 * Troposphere split:
 *   tropoZhdM = ZHD_zenith * mf_h(el)   [m, fixed slant ZHD]
 *   mfWet     = mf_w(el)                [dimensionless]
 *
 * Filter model:
 *   tropo_slant = tropoZhdM + state(ZTD_wet) * mfWet
 */
struct PppObservation {
    GnssSystem system{GnssSystem::UNKNOWN};
    int        prn{0};

    double ifCode{0.0};
    double ifPhase{0.0};

    double satX{0.0}, satY{0.0}, satZ{0.0};
    double satClkM{0.0};

    double tropoZhdM{0.0};
    double phaseWindupM{0.0};
    double pcoM{0.0};
    double mfWet{1.0};

    double gfObs{0.0};
    double elevation{0.0};
    bool   valid{false};
};

// =============================================================================
//  PppFilter
// =============================================================================

/**
 * @brief Sequential Kalman filter for PPP.
 *
 * State vector:
 *   [0-2]  X, Y, Z     ECEF [m]
 *   [3]    clk_GPS      receiver clock [m]
 *   [4]    ZTD_wet      zenith wet delay [m]
 *   [5]    ISB_GAL      Galileo ISB [m]  (added on first Galileo obs)
 *   [6+]   N_i          float IF ambiguities [m]  (added on first appearance)
 *
 * Both code and phase are used from the first epoch.  Ambiguities are
 * bootstrapped as N = ifPhase - ifCode, which absorbs any code biases.
 * The phase weight is ~360 000x larger than code, so ZWD is driven by
 * phase innovations and cannot be corrupted by code biases.
 */
class PppFilter {
public:
    using Config = PppFilterConfig;

    explicit PppFilter(Config cfg = Config{});

    void init(double approxX, double approxY, double approxZ,
              double ztdPrior = 0.1);

    /// @brief Sets the receiver clock state after a code-WLS bootstrap.
    /// Call immediately after init(), before the first timeUpdate().
    void initClock(double clkM);

    void timeUpdate(double dt);

    bool measurementUpdate(const std::vector<PppObservation>& obs);

    void   position(double& x, double& y, double& z) const;
    double clockBiasM()  const;
    double ztdWetM()     const;
    double isbGalM()     const;
    void   positionVariance(double& vx, double& vy, double& vz) const;
    bool   converged() const { return m_converged; }

    void resetAmbiguity(GnssSystem system, int prn);
    void reset();

private:
    Config          m_cfg;
    Eigen::VectorXd m_x;
    Eigen::MatrixXd m_P;
    bool            m_initialised{false};
    bool            m_converged{false};
    int             m_epochCount{0};
    int             m_isbGalIdx{-1};

    using SatKey = std::pair<GnssSystem, int>;
    std::map<SatKey, int>    m_ambIdx;
    std::map<SatKey, double> m_prevGf;
    std::map<SatKey, int>    m_satAbsentCount;

    double m_prevX{0.0}, m_prevY{0.0}, m_prevZ{0.0};
    double m_ztdPrior{0.135};  // Saastamoinen prior [m], set in init()

    static constexpr int BASE_DIM = 5;

    // With codeOnlyEpochs=0 this always returns true after init.
    bool usePhase() const { return m_epochCount >= m_cfg.codeOnlyEpochs; }

    int  ensureIsbGal();
    static bool needsIsb(GnssSystem system);
    int  ensureAmbiguity(GnssSystem system, int prn, double bootstrapValue);
    void removeAmbiguity(GnssSystem system, int prn);
    void pruneAbsentSatellites(const std::vector<PppObservation>& obs);
    void josephUpdate(const Eigen::MatrixXd& K,
                      const Eigen::MatrixXd& H,
                      const Eigen::MatrixXd& R);
    bool detectCycleSlip(GnssSystem system, int prn, double gf_current, double elevRad);
    static double elevSigma(double sigmaBase, double elevRad);
};

} // namespace loki::gnss
