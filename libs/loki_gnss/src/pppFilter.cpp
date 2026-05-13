#include <loki/gnss/pppFilter.hpp>
#include <loki/core/exceptions.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace loki;
using namespace loki::gnss;

// =============================================================================
//  Constructor
// =============================================================================

PppFilter::PppFilter(Config cfg)
    : m_cfg(cfg)
{}

// =============================================================================
//  init
// =============================================================================

void PppFilter::init(double approxX, double approxY, double approxZ,
                     double ztdPrior)
{
    // Initial state: BASE_DIM only (ambiguities added dynamically).
    m_x = Eigen::VectorXd::Zero(BASE_DIM);
    m_x(0) = approxX;
    m_x(1) = approxY;
    m_x(2) = approxZ;
    m_x(3) = 0.0;         // receiver clock
    m_x(4) = ztdPrior;    // ZTD wet

    m_P = Eigen::MatrixXd::Zero(BASE_DIM, BASE_DIM);
    m_P(0,0) = m_cfg.p0Pos;
    m_P(1,1) = m_cfg.p0Pos;
    m_P(2,2) = m_cfg.p0Pos;
    m_P(3,3) = m_cfg.p0Clk;
    m_P(4,4) = m_cfg.p0Ztd;

    m_ambIdx.clear();
    m_prevGf.clear();
    m_initialised = true;
    m_converged   = false;
    m_prevX = approxX; m_prevY = approxY; m_prevZ = approxZ;
}

// =============================================================================
//  reset
// =============================================================================

void PppFilter::reset()
{
    m_x            = Eigen::VectorXd{};
    m_P            = Eigen::MatrixXd{};
    m_ambIdx.clear();
    m_prevGf.clear();
    m_initialised  = false;
    m_converged    = false;
}

// =============================================================================
//  timeUpdate
// =============================================================================

void PppFilter::timeUpdate(double dt)
{
    if (!m_initialised) return;

    // State transition is identity for all parameters (static station).
    // Process noise:
    //   position : qPos * dt  (near zero for static)
    //   clock    : qClk * dt  (large -- modelled as white noise)
    //   ZTD wet  : qZtd * dt  (random walk)
    //   ambiguities: qAmb * dt (constant, zero noise)

    const int n = static_cast<int>(m_x.size());

    for (int i = 0; i < 3; ++i)
        m_P(i, i) += m_cfg.qPos * dt;
    m_P(3, 3) += m_cfg.qClk * dt;
    m_P(4, 4) += m_cfg.qZtd * dt;
    if (m_cfg.qAmb > 0.0) {
        for (int i = BASE_DIM; i < n; ++i)
            m_P(i, i) += m_cfg.qAmb * dt;
    }
}

// =============================================================================
//  measurementUpdate
// =============================================================================

bool PppFilter::measurementUpdate(const std::vector<PppObservation>& obs)
{
    if (!m_initialised)
        throw AlgorithmException("PppFilter: call init() before measurementUpdate().");

    // Filter out invalid observations.
    std::vector<const PppObservation*> valid;
    valid.reserve(obs.size());
    for (const auto& o : obs)
        if (o.valid) valid.push_back(&o);

    if (valid.empty()) return false;

    // ------------------------------------------------------------------
    //  Cycle slip detection and ambiguity management
    // ------------------------------------------------------------------
    for (const auto* o : valid) {
        if (detectCycleSlip(o->system, o->prn, o->gfObs))
            resetAmbiguity(o->system, o->prn);
    }

    // Remove ambiguity slots for satellites that have set.
    pruneAbsentSatellites(obs);

    // Ensure ambiguity slots exist for all visible satellites.
    for (const auto* o : valid)
        ensureAmbiguity(o->system, o->prn);

    const int n = static_cast<int>(m_x.size());

    // ------------------------------------------------------------------
    //  Build measurement vector and design matrix
    //  Two observations per satellite: code (IF) and phase (IF).
    // ------------------------------------------------------------------
    const int m = static_cast<int>(valid.size()) * 2;

    Eigen::VectorXd y(m);   // innovation vector
    Eigen::MatrixXd H = Eigen::MatrixXd::Zero(m, n);
    Eigen::MatrixXd R = Eigen::MatrixXd::Zero(m, m);

    const double rx = m_x(0);
    const double ry = m_x(1);
    const double rz = m_x(2);

    for (int i = 0; i < static_cast<int>(valid.size()); ++i) {
        const auto* o = valid[static_cast<std::size_t>(i)];

        // Geometric range.
        const double ddx = o->satX - rx;
        const double ddy = o->satY - ry;
        const double ddz = o->satZ - rz;
        const double rho = std::sqrt(ddx*ddx + ddy*ddy + ddz*ddz);

        // Unit vector (receiver -> satellite).
        const double ux = -ddx / rho;
        const double uy = -ddy / rho;
        const double uz = -ddz / rho;

        // Mapping function for ZTD (1/sin(elevation)).
        const double mf = 1.0 / std::sin(std::max(o->elevation, 0.05));

        // Ambiguity index for this satellite.
        const int iAmb = m_ambIdx.at({o->system, o->prn});

        // Computed observables (using current state).
        const double modeled_base = rho + m_x(3) - o->satClkM
                                  + m_x(4) * mf + o->tropoM;

        // ---- Code row (index 2*i) ----
        const int ic = 2 * i;
        H(ic, 0) = ux;
        H(ic, 1) = uy;
        H(ic, 2) = uz;
        H(ic, 3) = 1.0;    // receiver clock
        H(ic, 4) = mf;     // ZTD wet
        // No ambiguity in code observable.

        y(ic) = o->ifCode - modeled_base;

        const double sigCode = elevSigma(m_cfg.sigmaCodeM, o->elevation);
        R(ic, ic) = sigCode * sigCode;

        // ---- Phase row (index 2*i+1) ----
        const int ip = 2 * i + 1;
        H(ip, 0) = ux;
        H(ip, 1) = uy;
        H(ip, 2) = uz;
        H(ip, 3) = 1.0;
        H(ip, 4) = mf;
        H(ip, iAmb) = 1.0;  // float ambiguity

        y(ip) = o->ifPhase - modeled_base - m_x(iAmb) - o->phaseWindupM;

        const double sigPhase = elevSigma(m_cfg.sigmaPhaseM, o->elevation);
        R(ip, ip) = sigPhase * sigPhase;
    }

    // ------------------------------------------------------------------
    //  Kalman gain and state update
    // ------------------------------------------------------------------
    // S = H*P*H^T + R
    const Eigen::MatrixXd PHt = m_P * H.transpose();
    const Eigen::MatrixXd S   = H * PHt + R;

    // Solve for K via LDLT (S is symmetric positive definite).
    const Eigen::MatrixXd K = PHt * S.ldlt().solve(
        Eigen::MatrixXd::Identity(m, m));

    m_x += K * y;

    // Joseph-form covariance update.
    josephUpdate(K, H, R);

    // ------------------------------------------------------------------
    //  Convergence check
    // ------------------------------------------------------------------
    const double dx = m_x(0) - m_prevX;
    const double dy = m_x(1) - m_prevY;
    const double dz = m_x(2) - m_prevZ;
    const double d3d = std::sqrt(dx*dx + dy*dy + dz*dz);

    m_prevX = m_x(0); m_prevY = m_x(1); m_prevZ = m_x(2);

    if (d3d < m_cfg.convergenceM) m_converged = true;

    return m_converged;
}

// =============================================================================
//  State accessors
// =============================================================================

void PppFilter::position(double& x, double& y, double& z) const
{
    x = m_x.size() > 0 ? m_x(0) : 0.0;
    y = m_x.size() > 1 ? m_x(1) : 0.0;
    z = m_x.size() > 2 ? m_x(2) : 0.0;
}

double PppFilter::clockBiasM() const
{
    return m_x.size() > 3 ? m_x(3) : 0.0;
}

double PppFilter::ztdWetM() const
{
    return m_x.size() > 4 ? m_x(4) : 0.0;
}

void PppFilter::positionVariance(double& vx, double& vy, double& vz) const
{
    vx = m_P.rows() > 0 ? m_P(0,0) : 0.0;
    vy = m_P.rows() > 1 ? m_P(1,1) : 0.0;
    vz = m_P.rows() > 2 ? m_P(2,2) : 0.0;
}

// =============================================================================
//  Ambiguity management
// =============================================================================

int PppFilter::ensureAmbiguity(GnssSystem system, int prn)
{
    const SatKey key{system, prn};
    const auto it = m_ambIdx.find(key);
    if (it != m_ambIdx.end()) return it->second;

    // New satellite: append one slot to state vector and covariance.
    const int idx = static_cast<int>(m_x.size());

    Eigen::VectorXd xNew(idx + 1);
    xNew.head(idx) = m_x;
    xNew(idx) = 0.0;  // initial ambiguity = 0 (large variance below)
    m_x = std::move(xNew);

    Eigen::MatrixXd pNew = Eigen::MatrixXd::Zero(idx + 1, idx + 1);
    pNew.topLeftCorner(idx, idx) = m_P;
    pNew(idx, idx) = m_cfg.p0Amb;
    m_P = std::move(pNew);

    m_ambIdx[key] = idx;
    return idx;
}

void PppFilter::removeAmbiguity(GnssSystem system, int prn)
{
    const SatKey key{system, prn};
    const auto it = m_ambIdx.find(key);
    if (it == m_ambIdx.end()) return;

    const int rmIdx = it->second;
    const int n     = static_cast<int>(m_x.size());

    // Remove row/col rmIdx from P and element rmIdx from x.
    Eigen::VectorXd xNew(n - 1);
    xNew.head(rmIdx)           = m_x.head(rmIdx);
    xNew.tail(n - 1 - rmIdx)   = m_x.tail(n - 1 - rmIdx);
    m_x = std::move(xNew);

    Eigen::MatrixXd pNew(n - 1, n - 1);
    pNew.topLeftCorner(rmIdx, rmIdx)                          = m_P.topLeftCorner(rmIdx, rmIdx);
    pNew.topRightCorner(rmIdx, n-1-rmIdx)                     = m_P.topRightCorner(rmIdx, n-1-rmIdx);
    pNew.bottomLeftCorner(n-1-rmIdx, rmIdx)                   = m_P.bottomLeftCorner(n-1-rmIdx, rmIdx);
    pNew.bottomRightCorner(n-1-rmIdx, n-1-rmIdx)              = m_P.bottomRightCorner(n-1-rmIdx, n-1-rmIdx);
    m_P = std::move(pNew);

    // Update indices for slots that moved down.
    m_ambIdx.erase(it);
    for (auto& kv : m_ambIdx)
        if (kv.second > rmIdx) --kv.second;
}

void PppFilter::resetAmbiguity(GnssSystem system, int prn)
{
    // Remove existing slot so ensureAmbiguity will create a fresh one
    // with large initial variance at the next measurement update.
    removeAmbiguity(system, prn);
    m_prevGf.erase({system, prn});
}

void PppFilter::pruneAbsentSatellites(const std::vector<PppObservation>& obs)
{
    // Build set of currently visible satellites.
    std::vector<SatKey> visible;
    for (const auto& o : obs)
        if (o.valid) visible.emplace_back(o.system, o.prn);

    // Collect keys to remove.
    std::vector<SatKey> toRemove;
    for (const auto& kv : m_ambIdx) {
        const bool found = std::any_of(visible.begin(), visible.end(),
            [&](const SatKey& k){ return k == kv.first; });
        if (!found) toRemove.push_back(kv.first);
    }

    // Remove in reverse index order to preserve validity of other indices.
    std::sort(toRemove.begin(), toRemove.end(),
        [this](const SatKey& a, const SatKey& b){
            return m_ambIdx.at(a) > m_ambIdx.at(b);
        });
    for (const auto& k : toRemove)
        removeAmbiguity(k.first, k.second);
}

// =============================================================================
//  josephUpdate
// =============================================================================

void PppFilter::josephUpdate(const Eigen::MatrixXd& K,
                              const Eigen::MatrixXd& H,
                              const Eigen::MatrixXd& R)
{
    const int n = static_cast<int>(m_x.size());
    const Eigen::MatrixXd I = Eigen::MatrixXd::Identity(n, n);
    const Eigen::MatrixXd A = I - K * H;
    m_P = A * m_P * A.transpose() + K * R * K.transpose();

    // Enforce symmetry (numerical cleanup).
    m_P = 0.5 * (m_P + m_P.transpose());
}

// =============================================================================
//  detectCycleSlip
// =============================================================================

bool PppFilter::detectCycleSlip(GnssSystem system, int prn, double gf_current)
{
    const SatKey key{system, prn};
    const auto it = m_prevGf.find(key);

    if (it == m_prevGf.end()) {
        // First epoch for this satellite -- not a slip.
        m_prevGf[key] = gf_current;
        return false;
    }

    const double delta = std::abs(gf_current - it->second);
    it->second = gf_current;

    return delta > m_cfg.gfSlipThreshM;
}

// =============================================================================
//  elevSigma
// =============================================================================

double PppFilter::elevSigma(double sigmaBase, double elevRad)
{
    const double sinEl = std::sin(std::max(elevRad, 0.05));
    return sigmaBase / sinEl;
}
