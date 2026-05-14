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
 * Two-phase startup:
 *
 *   Phase 1 -- code-only (first codeOnlyEpochs epochs):
 *     Only pseudorange is used.  Clock (unknown ~-107 km), position,
 *     and ZWD are estimated from code alone.
 *
 *     Key insight: ZWD must converge to the correct value (~0.13 m)
 *     BEFORE phase observations are introduced.  If ZWD is wrong when
 *     phase starts, the ambiguities absorb the ZWD error and the filter
 *     locks into an incorrect state because phase weight >> code weight.
 *
 *     codeOnlyEpochs = 30 (15 min at 30 s):
 *       Sufficient for ZWD to converge from prior (~0.135 m) to a
 *       value within ~0.1 m of truth with 15 satellites and
 *       sigmaCodeM = 3 m.
 *
 *     qZtdCode = 1e-6 m^2/s:
 *       Larger ZWD process noise during code phase to allow the filter
 *       to adjust ZWD faster from the a priori value.  After phase
 *       observations are added, qZtd (smaller) takes over.
 *
 *   Phase 2 -- code + phase (remaining epochs):
 *     Ambiguities bootstrapped as N = ifPhase - ifCode.
 *     ZWD estimated as slow random walk (qZtd = 1e-8 m^2/s).
 */
struct PppFilterConfig {
    // Process noise [m^2/s].
    double qPos    {1.0e-8};   ///< Position (static).
    double qClk    {100.0};    ///< Receiver clock random walk.
    double qZtd    {1.0e-8};   ///< ZWD random walk (phase phase).
    double qZtdCode{1.0e-5};   ///< ZWD random walk (code-only phase, larger).
    double qAmb    {0.0};      ///< Ambiguity (0 = constant between slips).

    // Measurement noise.
    double sigmaCodeM {3.0};   ///< IF pseudorange sigma [m].
    double sigmaPhaseM{0.005}; ///< IF carrier-phase sigma [m].

    // Cycle slip.
    double gfSlipThreshM{0.05};

    // Convergence.
    double convergenceM{0.05};

    // Initial covariance.
    double p0Pos{100.0};
    double p0Clk{1.0e10};
    double p0Ztd{0.25};
    double p0Amb{1.0e6};
    double p0Isb{1.0e4};

    // Code-only pre-filter duration.
    int codeOnlyEpochs{120};    ///< ~15 min at 30 s interval.

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
 *   [5]    ISB_GAL      Galileo ISB [m]
 *   [6+]   N_i          float IF ambiguities [m]  (phase 2 only)
 *
 * Critical design decision:
 *   ZWD must be estimated from code alone first.  Carrier-phase measurements
 *   have ~360 000x larger weight than code and will lock in whatever ZWD
 *   value exists when they are first introduced.  30 code-only epochs give
 *   ZWD time to converge before this lock-in occurs.
 */
class PppFilter {
public:
    using Config = PppFilterConfig;

    explicit PppFilter(Config cfg = Config{});

    void init(double approxX, double approxY, double approxZ,
              double ztdPrior = 0.1);

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

    double m_prevX{0.0}, m_prevY{0.0}, m_prevZ{0.0};

    static constexpr int BASE_DIM = 5;

    bool usePhase() const { return m_epochCount >= m_cfg.codeOnlyEpochs; }

    int  ensureIsbGal();
    static bool needsIsb(GnssSystem system);
    int  ensureAmbiguity(GnssSystem system, int prn, double bootstrapValue);
    void removeAmbiguity(GnssSystem system, int prn);
    void pruneAbsentSatellites(const std::vector<PppObservation>& obs);
    void josephUpdate(const Eigen::MatrixXd& K,
                      const Eigen::MatrixXd& H,
                      const Eigen::MatrixXd& R);
    bool   detectCycleSlip(GnssSystem system, int prn, double gf_current);
    static double elevSigma(double sigmaBase, double elevRad);
};

} // namespace loki::gnss
