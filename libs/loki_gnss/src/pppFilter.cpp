#include <loki/gnss/pppFilter.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <numbers>
#include <set>

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
    m_x = Eigen::VectorXd::Zero(BASE_DIM);
    m_x(0) = approxX;
    m_x(1) = approxY;
    m_x(2) = approxZ;
    m_x(3) = 0.0;
    m_x(4) = ztdPrior;
    m_ztdPrior = ztdPrior;

    m_P = Eigen::MatrixXd::Zero(BASE_DIM, BASE_DIM);
    m_P(0,0) = m_cfg.p0Pos;
    m_P(1,1) = m_cfg.p0Pos;
    m_P(2,2) = m_cfg.p0Pos;
    m_P(3,3) = m_cfg.p0Clk;
    m_P(4,4) = 1.0e-4;   // ZWD starts tightly constrained to prior; grows via qZtd

    m_isbGalIdx  = -1;
    m_epochCount = 0;
    m_ambIdx.clear();
    m_prevGf.clear();
    m_initialised = true;
    m_converged   = false;
    m_prevX = approxX;
    m_prevY = approxY;
    m_prevZ = approxZ;
}

// =============================================================================
//  initClock
// =============================================================================

void PppFilter::initClock(double clkM)
{
    if (!m_initialised) return;
    m_x(3)    = clkM;
    // Tighten clock covariance: bootstrap gives ~1 m accuracy on clock,
    // so 100 m^2 is a conservative but reasonable initial variance.
    m_P(3, 3) = 100.0 * 100.0;
}

// =============================================================================
//  reset
// =============================================================================

void PppFilter::reset()
{
    m_x = Eigen::VectorXd{};
    m_P = Eigen::MatrixXd{};
    m_isbGalIdx  = -1;
    m_epochCount = 0;
    m_ambIdx.clear();
    m_prevGf.clear();
    m_satAbsentCount.clear();
    m_initialised = false;
    m_converged   = false;
}

// =============================================================================
//  timeUpdate
// =============================================================================

void PppFilter::timeUpdate(double dt)
{
    if (!m_initialised) return;
    const int n = static_cast<int>(m_x.size());

    for (int i = 0; i < 3; ++i)
        m_P(i,i) += m_cfg.qPos * dt;

    m_P(3,3) += m_cfg.qClk * dt;
    m_P(4,4) += m_cfg.qZtd * dt;

    if (m_isbGalIdx >= 0)
        m_P(m_isbGalIdx, m_isbGalIdx) += 1.0e-8 * dt;

    if (m_cfg.qAmb > 0.0) {
        for (int i = BASE_DIM; i < n; ++i) {
            if (i == m_isbGalIdx) continue;
            m_P(i,i) += m_cfg.qAmb * dt;
        }
    }
}

// =============================================================================
//  measurementUpdate
// =============================================================================

bool PppFilter::measurementUpdate(const std::vector<PppObservation>& obs)
{

    // NEW DEBUG
    static int dbgFilt = 0;
    if (dbgFilt < 5) {
        LOKI_INFO("PPP_DEBUG filter ep=" + std::to_string(dbgFilt)
                + " x(0)=" + std::to_string(m_x(0))
                + " x(3)_clk=" + std::to_string(m_x(3))
                + " x(4)_zwd=" + std::to_string(m_x(4))
                + " P(4,4)=" + std::to_string(m_P(4,4))
                + " nObs=" + std::to_string(obs.size()));
        ++dbgFilt;
    }
    // END DEBUG
    if (!m_initialised)
        throw AlgorithmException(
            "PppFilter: call init() before measurementUpdate().");

    std::vector<const PppObservation*> valid;
    valid.reserve(obs.size());
    for (const auto& o : obs)
        if (o.valid) valid.push_back(&o);

    if (valid.empty()) return false;

    ++m_epochCount;
    const bool withPhase = usePhase();

    // Cycle slip detection (phase only).
    if (withPhase) {
        for (const auto* o : valid) {
            if (detectCycleSlip(o->system, o->prn, o->gfObs, o->elevation))
                resetAmbiguity(o->system, o->prn);
        }
        pruneAbsentSatellites(obs);
    }

    // Galileo ISB.
    for (const auto* o : valid) {
        if (needsIsb(o->system)) { ensureIsbGal(); break; }
    }

    // Ambiguity bootstrap (phase only).
    // N = ifPhase - modeled_range, where modeled_range uses current filter
    // state. This gives a near-zero initial phase residual regardless of
    // position error, preventing position/clock errors from corrupting ZWD.
    if (withPhase) {
        const double rx0 = m_x(0);
        const double ry0 = m_x(1);
        const double rz0 = m_x(2);
        for (const auto* o : valid) {
            const double ddx = o->satX - rx0;
            const double ddy = o->satY - ry0;
            const double ddz = o->satZ - rz0;
            const double rho = std::sqrt(ddx*ddx + ddy*ddy + ddz*ddz);
            const int    iIsb   = needsIsb(o->system) ? m_isbGalIdx : -1;
            const double isbVal = (iIsb >= 0) ? m_x(iIsb) : 0.0;
            const double modeled = rho + m_x(3) + isbVal
                                 - o->satClkM
                                 + o->tropoZhdM
                                 + m_x(4) * o->mfWet;
            const double bootstrapN = o->ifPhase - modeled - o->phaseWindupM;
            ensureAmbiguity(o->system, o->prn, bootstrapN);
        }
    }

    const int n          = static_cast<int>(m_x.size());
    const int rowsPerSat = withPhase ? 2 : 1;
    const int mSat       = static_cast<int>(valid.size()) * rowsPerSat;
    const int m          = mSat + 1;  // +1 for ZWD soft constraint

    Eigen::VectorXd y(m);
    Eigen::MatrixXd H = Eigen::MatrixXd::Zero(m, n);
    Eigen::MatrixXd R = Eigen::MatrixXd::Zero(m, m);

    const double rx = m_x(0);
    const double ry = m_x(1);
    const double rz = m_x(2);

    for (int i = 0; i < static_cast<int>(valid.size()); ++i) {
        const auto* o = valid[static_cast<std::size_t>(i)];

        const double ddx = o->satX - rx;
        const double ddy = o->satY - ry;
        const double ddz = o->satZ - rz;
        const double rho = std::sqrt(ddx*ddx + ddy*ddy + ddz*ddz);
        const double ux  = -ddx / rho;
        const double uy  = -ddy / rho;
        const double uz  = -ddz / rho;

        const int    iIsb   = needsIsb(o->system) ? m_isbGalIdx : -1;
        const double isbVal = (iIsb >= 0) ? m_x(iIsb) : 0.0;

        // Modeled pseudorange (same for code and phase rows; ambiguity added
        // for phase row below).
        const double modeled = rho
                             + m_x(3)
                             + isbVal
                             - o->satClkM
                             + o->tropoZhdM
                             + m_x(4) * o->mfWet;

        // ---- Code row ----
        const int ic = i * rowsPerSat;
        H(ic, 0) = ux;
        H(ic, 1) = uy;
        H(ic, 2) = uz;
        H(ic, 3) = 1.0;
        H(ic, 4) = o->mfWet;
        if (iIsb >= 0) H(ic, iIsb) = 1.0;

        y(ic) = o->ifCode - modeled;

        const double sc = elevSigma(m_cfg.sigmaCodeM, o->elevation);
        R(ic, ic) = sc * sc;

        // ---- Phase row (when withPhase == true) ----
        if (withPhase) {
            const int ip   = i * rowsPerSat + 1;
            const int iAmb = m_ambIdx.at({o->system, o->prn});

            H(ip, 0) = ux;
            H(ip, 1) = uy;
            H(ip, 2) = uz;
            H(ip, 3) = 1.0;
            H(ip, 4) = o->mfWet;
            if (iIsb >= 0) H(ip, iIsb) = 1.0;
            H(ip, iAmb) = 1.0;

            // Phase observation minus modeled range minus ambiguity minus windup.
            y(ip) = o->ifPhase - modeled - m_x(iAmb) - o->phaseWindupM;

            const double sp = elevSigma(m_cfg.sigmaPhaseM, o->elevation);
            R(ip, ip) = sp * sp;
        }
    }

    // ---- ZWD soft constraint row ----
    // Pulls ZWD toward Saastamoinen prior with sigma = sigmaZtdConstraint.
    // Prevents unbounded drift when phase innovations are dominated by
    // multipath or when geometry is poor.
    H(mSat, 4) = 1.0;
    y(mSat)    = m_ztdPrior - m_x(4);
    R(mSat, mSat) = m_cfg.sigmaZtdConstraint * m_cfg.sigmaZtdConstraint;

    // Kalman update (Joseph-form for numerical stability).
    const Eigen::MatrixXd PHt = m_P * H.transpose();
    const Eigen::MatrixXd S   = H * PHt + R;
    const Eigen::MatrixXd K   = PHt * S.ldlt().solve(
        Eigen::MatrixXd::Identity(m, m));

    m_x += K * y;

    // DEBUG
    static int dbgUpd = 0;
    if (dbgUpd < 5 || (dbgUpd % 100 == 0 && dbgUpd < 1000)) {
        LOKI_INFO("PPP_DEBUG update ep=" + std::to_string(dbgUpd)
                + " x(4)_zwd_after=" + std::to_string(m_x(4))
                + " x(3)_clk=" + std::to_string(m_x(3))
                + " y_zwd_constraint=" + std::to_string(y(mSat))
                + " K_zwd_norm=" + std::to_string(K.col(4).norm())
                + " stateSize=" + std::to_string(m_x.size()));
    }
    ++dbgUpd;
    
    // END DEBUG

    josephUpdate(K, H, R);

    // Convergence check (only meaningful once phase is active).
    if (withPhase) {
        const double dx  = m_x(0) - m_prevX;
        const double dy  = m_x(1) - m_prevY;
        const double dz  = m_x(2) - m_prevZ;
        if (std::sqrt(dx*dx + dy*dy + dz*dz) < m_cfg.convergenceM)
            m_converged = true;
    }

    m_prevX = m_x(0);
    m_prevY = m_x(1);
    m_prevZ = m_x(2);

    // DEBUG
    static int dbgRes = 0;
    if (dbgRes == 100) {
        for (int i = 0; i < static_cast<int>(valid.size()); ++i) {
            const int ic = i * rowsPerSat;
            LOKI_INFO("PPP_DEBUG residual ep=100 prn=" + std::to_string(valid[i]->prn)
                    + " sys=" + std::to_string(static_cast<int>(valid[i]->system))
                    + " y_code=" + std::to_string(y(ic))
                    + (withPhase ? " y_phase=" + std::to_string(y(ic+1)) : ""));
        }
    }
    ++dbgRes;
    // END DEBUG

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

double PppFilter::clockBiasM() const { return m_x.size() > 3 ? m_x(3) : 0.0; }
double PppFilter::ztdWetM()    const { return m_x.size() > 4 ? m_x(4) : 0.0; }

double PppFilter::isbGalM() const
{
    return (m_isbGalIdx >= 0 && m_isbGalIdx < static_cast<int>(m_x.size()))
         ? m_x(m_isbGalIdx) : 0.0;
}

void PppFilter::positionVariance(double& vx, double& vy, double& vz) const
{
    vx = m_P.rows() > 0 ? m_P(0,0) : 0.0;
    vy = m_P.rows() > 1 ? m_P(1,1) : 0.0;
    vz = m_P.rows() > 2 ? m_P(2,2) : 0.0;
}

// =============================================================================
//  ISB management
// =============================================================================

bool PppFilter::needsIsb(GnssSystem system)
{
    return system == GnssSystem::GALILEO;
}

int PppFilter::ensureIsbGal()
{
    if (m_isbGalIdx >= 0) return m_isbGalIdx;

    const int insertAt = BASE_DIM;
    const int oldSize  = static_cast<int>(m_x.size());
    const int newSize  = oldSize + 1;

    Eigen::VectorXd xNew(newSize);
    xNew.head(insertAt)           = m_x.head(insertAt);
    xNew(insertAt)                = 0.0;
    xNew.tail(oldSize - insertAt) = m_x.tail(oldSize - insertAt);
    m_x = std::move(xNew);

    Eigen::MatrixXd pNew = Eigen::MatrixXd::Zero(newSize, newSize);
    pNew.topLeftCorner    (insertAt,           insertAt)           = m_P.topLeftCorner    (insertAt,           insertAt);
    pNew.topRightCorner   (insertAt,           oldSize - insertAt) = m_P.topRightCorner   (insertAt,           oldSize - insertAt);
    pNew.bottomLeftCorner (oldSize - insertAt, insertAt)           = m_P.bottomLeftCorner (oldSize - insertAt, insertAt);
    pNew.bottomRightCorner(oldSize - insertAt, oldSize - insertAt) = m_P.bottomRightCorner(oldSize - insertAt, oldSize - insertAt);
    pNew(insertAt, insertAt) = m_cfg.p0Isb;
    m_P = std::move(pNew);

    for (auto& kv : m_ambIdx)
        if (kv.second >= insertAt) ++kv.second;

    m_isbGalIdx = insertAt;
    return m_isbGalIdx;
}

// =============================================================================
//  Ambiguity management
// =============================================================================

int PppFilter::ensureAmbiguity(GnssSystem system, int prn,
                                double bootstrapValue)
{
    const SatKey key{system, prn};
    const auto it = m_ambIdx.find(key);
    if (it != m_ambIdx.end()) return it->second;

    const int idx = static_cast<int>(m_x.size());

    Eigen::VectorXd xNew(idx + 1);
    xNew.head(idx) = m_x;
    xNew(idx)      = bootstrapValue;

    /* // DEBUG
    LOKI_INFO("PPP_DEBUG ambiguity bootstrap: prn="
          + std::to_string(prn)
          + " sys=" + std::to_string(static_cast<int>(system))
          + " bootstrapN=" + std::to_string(bootstrapValue)
          + " idx=" + std::to_string(idx));
    //END DEBUG
    */
   
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

    Eigen::VectorXd xNew(n - 1);
    xNew.head(rmIdx)     = m_x.head(rmIdx);
    xNew.tail(n-1-rmIdx) = m_x.tail(n-1-rmIdx);
    m_x = std::move(xNew);

    Eigen::MatrixXd pNew(n-1, n-1);
    pNew.topLeftCorner    (rmIdx,     rmIdx)     = m_P.topLeftCorner    (rmIdx,     rmIdx);
    pNew.topRightCorner   (rmIdx,     n-1-rmIdx) = m_P.topRightCorner   (rmIdx,     n-1-rmIdx);
    pNew.bottomLeftCorner (n-1-rmIdx, rmIdx)     = m_P.bottomLeftCorner (n-1-rmIdx, rmIdx);
    pNew.bottomRightCorner(n-1-rmIdx, n-1-rmIdx) = m_P.bottomRightCorner(n-1-rmIdx, n-1-rmIdx);
    m_P = std::move(pNew);

    m_ambIdx.erase(it);
    for (auto& kv : m_ambIdx)
        if (kv.second > rmIdx) --kv.second;

    if (m_isbGalIdx > rmIdx) --m_isbGalIdx;
}

void PppFilter::resetAmbiguity(GnssSystem system, int prn)
{
    removeAmbiguity(system, prn);
    m_prevGf.erase({system, prn});
}

void PppFilter::pruneAbsentSatellites(const std::vector<PppObservation>& obs)
{
    // Build set of currently visible satellites.
    std::set<SatKey> visible;
    for (const auto& o : obs)
        if (o.valid) visible.insert({o.system, o.prn});

    // Increment absent counter for satellites not currently visible.
    // Only remove ambiguity after it has been absent for ambiguityHoldEpochs.
    std::vector<SatKey> toRemove;
    for (const auto& kv : m_ambIdx) {
        if (visible.count(kv.first) == 0) {
            m_satAbsentCount[kv.first]++;
            if (m_satAbsentCount[kv.first] >= m_cfg.ambiguityHoldEpochs)
                toRemove.push_back(kv.first);
        } else {
            m_satAbsentCount.erase(kv.first);
        }
    }

    // Remove in reverse index order to keep indices consistent.
    std::sort(toRemove.begin(), toRemove.end(),
        [this](const SatKey& a, const SatKey& b){
            return m_ambIdx.at(a) > m_ambIdx.at(b);
        });
    for (const auto& k : toRemove) {
        m_satAbsentCount.erase(k);
        removeAmbiguity(k.first, k.second);
    }
    
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
    m_P = 0.5 * (m_P + m_P.transpose());
}

// =============================================================================
//  detectCycleSlip
// =============================================================================

bool PppFilter::detectCycleSlip(GnssSystem system, int prn,
                                  double gf_current, double elevRad)
{
    const SatKey key{system, prn};
    const auto it = m_prevGf.find(key);
    if (it == m_prevGf.end()) {
        m_prevGf[key] = gf_current;
        return false;
    }

    const double delta = std::abs(gf_current - it->second);
    it->second = gf_current;

    // Elevation-dependent threshold: at low elevations ionospheric
    // gradient can cause GF to drift ~0.02-0.03 m/epoch even without
    // a cycle slip, so we scale the threshold by 1/sin(el).
    const double sinEl  = std::sin(std::max(elevRad, 0.1));
    const double thresh = m_cfg.gfSlipThreshM / sinEl;

    // debug
    //if ((prn == 16 || prn == 7) && system == GnssSystem::GPS) {
      //  LOKI_INFO("PPP_DEBUG GF prn=" + std::to_string(prn)
        //          + " delta=" + std::to_string(delta)
          //        + " thresh=" + std::to_string(thresh)
            //      + " slip=" + (delta > thresh ? "YES" : "no"));
    //}
    // end debug
 
    return delta > thresh;
}

// =============================================================================
//  elevSigma
// =============================================================================

double PppFilter::elevSigma(double sigmaBase, double elevRad)
{
    return sigmaBase / std::sin(std::max(elevRad, 0.05));
}
