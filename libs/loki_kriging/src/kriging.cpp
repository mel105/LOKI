#include <loki/kriging/kriging.hpp>

#include <loki/core/exceptions.hpp>
#include <loki/kriging/variogram.hpp>
#include <loki/core/logger.hpp>

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace loki;

namespace loki::kriging {

// =============================================================================
//  KrigingBase -- shared helpers
// =============================================================================

double KrigingBase::_cov(double h) const
{
    // For power model there is no finite sill -- use -gamma(h) convention
    if (m_variogram.model == "power") {
        return -variogramEval(h, m_variogram);
    }
    // For all stationary models: C(h) = (sill - nugget) - gamma(h) + nugget_at_0
    // i.e. C(0) = sill - nugget, C(h) = C(0) - (gamma(h) - nugget)
    //           = sill - gamma(h)   (because gamma(h) already contains nugget for h>0)
    // At h = 0: C(0) = sill - 0 = sill. Correct.
    const double c0 = m_variogram.sill - m_variogram.nugget; // partial sill
    if (h <= 0.0) return c0 + m_variogram.nugget; // = sill
    return (m_variogram.sill - m_variogram.nugget)
           - (variogramEval(h, m_variogram) - m_variogram.nugget);
}

double KrigingBase::_zQuantile(double confidenceLevel)
{
    // probit via erfinv: z = sqrt(2) * erfinv(level)
    // std::erfinv is not standard -- approximate via bisection on erf
    // for CI levels in [0.80, 0.999] the result is well-conditioned
    const double target = confidenceLevel; // = erf(z / sqrt(2))
    // Bisect on: erf(x / sqrt(2)) = target  => x in [0, 5]
    double lo = 0.0;
    double hi = 5.0;
    for (int i = 0; i < 60; ++i) {
        const double mid = 0.5 * (lo + hi);
        if (std::erf(mid / std::numbers::sqrt2) < target) lo = mid;
        else hi = mid;
    }
    return 0.5 * (lo + hi);
}

// -- predictGrid --------------------------------------------------------------

std::vector<KrigingPrediction> KrigingBase::predictGrid(
    const std::vector<double>& mjdPoints,
    double confidenceLevel) const
{
    std::vector<KrigingPrediction> result;
    result.reserve(mjdPoints.size());
    for (const double mjd : mjdPoints) {
        KrigingPrediction pred = predictAt(mjd);
        // Apply CI using requested confidence level
        const double z     = _zQuantile(confidenceLevel);
        const double sigma = std::sqrt(std::max(0.0, pred.variance));
        pred.ciLower       = pred.value - z * sigma;
        pred.ciUpper       = pred.value + z * sigma;

        // Mark as observed if coincides with a known data point (within 1e-9 MJD)
        pred.isObserved = false;
        for (const double t : m_mjd) {
            if (std::abs(t - mjd) < 1.0e-9) { pred.isObserved = true; break; }
        }
        result.push_back(pred);
    }
    return result;
}

// -- crossValidate ------------------------------------------------------------

CrossValidationResult KrigingBase::crossValidate(double /*confidenceLevel*/) const
{
    const std::size_t n = m_mjd.size();

    CrossValidationResult cv;
    cv.errors   .reserve(n);
    cv.stdErrors.reserve(n);
    cv.mjd      .reserve(n);

    double sumSqErr    = 0.0;
    double sumAbsErr   = 0.0;
    double sumSE       = 0.0;
    double sumSSE      = 0.0;

    for (std::size_t leaveOut = 0; leaveOut < n; ++leaveOut) {
        // Build reduced observation vectors
        std::vector<double> mjdRed;
        std::vector<double> zRed;
        mjdRed.reserve(n - 1);
        zRed  .reserve(n - 1);
        for (std::size_t k = 0; k < n; ++k) {
            if (k != leaveOut) {
                mjdRed.push_back(m_mjd[k]);
                zRed  .push_back(m_z[k]);
            }
        }

        const std::size_t nr = mjdRed.size();

        // Build reduced covariance matrix
        Eigen::MatrixXd K(static_cast<int>(nr), static_cast<int>(nr));
        for (std::size_t i = 0; i < nr; ++i)
            for (std::size_t j = 0; j < nr; ++j)
                K(static_cast<int>(i), static_cast<int>(j)) =
                    _cov(std::abs(mjdRed[i] - mjdRed[j]));

        // Add small jitter for numerical stability
        K += 1.0e-10 * Eigen::MatrixXd::Identity(static_cast<int>(nr),
                                                  static_cast<int>(nr));

        // Build RHS covariance vector
        Eigen::VectorXd kv(static_cast<int>(nr));
        for (std::size_t i = 0; i < nr; ++i)
            kv(static_cast<int>(i)) =
                _cov(std::abs(mjdRed[i] - m_mjd[leaveOut]));

        // Solve K * lambda = kv
        const Eigen::LLT<Eigen::MatrixXd> llt(K);
        if (llt.info() != Eigen::Success) continue; // skip degenerate

        const Eigen::VectorXd lam = llt.solve(kv);

        // Estimate
        const Eigen::Map<const Eigen::VectorXd> zVec(zRed.data(),
                                                      static_cast<int>(nr));
        const double zHat = lam.dot(zVec);

        // Kriging variance  (simplified: no Lagrange correction for base class LOO)
        const double c0      = _cov(0.0);
        const double sigmaK2 = std::max(0.0, c0 - lam.dot(kv));
        const double sigmaK  = std::sqrt(sigmaK2 + 1.0e-15);

        const double e = m_z[leaveOut] - zHat;
        const double se = e / sigmaK;

        cv.errors   .push_back(e);
        cv.stdErrors.push_back(se);
        cv.mjd      .push_back(m_mjd[leaveOut]);

        sumSqErr  += e  * e;
        sumAbsErr += std::abs(e);
        sumSE     += se;
        sumSSE    += se * se;
    }

    const double nn = static_cast<double>(cv.errors.size());
    cv.rmse   = (nn > 0) ? std::sqrt(sumSqErr / nn) : 0.0;
    cv.mae    = (nn > 0) ? sumAbsErr / nn            : 0.0;
    cv.mse    = (nn > 0) ? sumSqErr  / nn            : 0.0;
    cv.meanSE = (nn > 0) ? sumSE     / nn            : 0.0;
    cv.meanSSE= (nn > 0) ? sumSSE    / nn            : 0.0;

    return cv;
}

// =============================================================================
//  SimpleKriging
// =============================================================================

SimpleKriging::SimpleKriging(double knownMean, double confidenceLevel)
    : m_mu(knownMean)
{
    m_ciZ = _zQuantile(confidenceLevel);
}

void SimpleKriging::fit(const TimeSeries& ts, const VariogramFitResult& variogram)
{
    m_variogram = variogram;
    m_mjd.clear();
    m_z  .clear();

    for (std::size_t i = 0; i < ts.size(); ++i) {
        if (!std::isnan(ts[i].value)) {
            m_mjd.push_back(ts[i].time.mjd());
            m_z  .push_back(ts[i].value);
        }
    }

    const std::size_t n = m_mjd.size();
    if (n < 2) {
        throw DataException(
            "SimpleKriging::fit: need at least 2 valid observations, got "
            + std::to_string(n) + ".");
    }

    // Build covariance matrix K (n x n)
    Eigen::MatrixXd K(static_cast<int>(n), static_cast<int>(n));
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < n; ++j)
            K(static_cast<int>(i), static_cast<int>(j)) =
                _cov(std::abs(m_mjd[i] - m_mjd[j]));

    K += 1.0e-10 * Eigen::MatrixXd::Identity(static_cast<int>(n),
                                              static_cast<int>(n));

    const Eigen::LLT<Eigen::MatrixXd> llt(K);
    if (llt.info() != Eigen::Success) {
        throw AlgorithmException(
            "SimpleKriging::fit: covariance matrix is not positive definite. "
            "Consider adding a nugget or checking the variogram parameters.");
    }

    // Store factored system for reuse in predictAt
    // Serialise Cholesky L to m_L (row-major)
    const Eigen::MatrixXd L = llt.matrixL();
    m_L.resize(static_cast<std::size_t>(n * n));
    for (int i = 0; i < static_cast<int>(n); ++i)
        for (int j = 0; j < static_cast<int>(n); ++j)
            m_L[static_cast<std::size_t>(i * static_cast<int>(n) + j)] =
                L(i, j);
}

KrigingPrediction SimpleKriging::predictAt(double mjd) const
{
    const std::size_t n = m_mjd.size();
    const int         ni = static_cast<int>(n);

    // RHS covariance vector k(t)
    Eigen::VectorXd kv(ni);
    for (std::size_t i = 0; i < n; ++i)
        kv(static_cast<int>(i)) = _cov(std::abs(mjd - m_mjd[i]));

    // Reconstruct L from stored data
    Eigen::MatrixXd L(ni, ni);
    for (int i = 0; i < ni; ++i)
        for (int j = 0; j < ni; ++j)
            L(i, j) = m_L[static_cast<std::size_t>(i * ni + j)];

    // Solve K * lambda = kv via L * L^T * lambda = kv
    const Eigen::VectorXd lam =
        L.triangularView<Eigen::Lower>().solve(
        L.triangularView<Eigen::Lower>().transpose().solve(kv));

    // Prediction: Z*(t) = mu + lambda^T * (z - mu)
    const Eigen::Map<const Eigen::VectorXd> zVec(m_z.data(), ni);
    const double zStar = m_mu + lam.dot(zVec - m_mu * Eigen::VectorXd::Ones(ni));

    // Kriging variance: sigma^2 = C(0) - k^T * lambda
    const double c0      = _cov(0.0);
    const double sigmaK2 = std::max(0.0, c0 - lam.dot(kv));

    KrigingPrediction pred;
    pred.mjd      = mjd;
    pred.value    = zStar;
    pred.variance = sigmaK2;
    const double sigma = std::sqrt(sigmaK2);
    pred.ciLower  = zStar - m_ciZ * sigma;
    pred.ciUpper  = zStar + m_ciZ * sigma;
    pred.isObserved = false;
    return pred;
}

// =============================================================================
//  OrdinaryKriging
// =============================================================================

OrdinaryKriging::OrdinaryKriging(double confidenceLevel)
{
    m_ciZ = _zQuantile(confidenceLevel);
}

void OrdinaryKriging::fit(const TimeSeries& ts, const VariogramFitResult& variogram)
{
    m_variogram = variogram;
    m_mjd.clear();
    m_z  .clear();

    for (std::size_t i = 0; i < ts.size(); ++i) {
        if (!std::isnan(ts[i].value)) {
            m_mjd.push_back(ts[i].time.mjd());
            m_z  .push_back(ts[i].value);
        }
    }

    const std::size_t n  = m_mjd.size();
    const int         ni = static_cast<int>(n);
    if (n < 2) {
        throw DataException(
            "OrdinaryKriging::fit: need at least 2 valid observations, got "
            + std::to_string(n) + ".");
    }

    // Extended system (n+1) x (n+1):
    //   [ K   1 ]
    //   [ 1^T 0 ]
    const int ext = ni + 1;
    Eigen::MatrixXd Kext(ext, ext);
    Kext.setZero();

    for (int i = 0; i < ni; ++i) {
        for (int j = 0; j < ni; ++j) {
            Kext(i, j) = _cov(std::abs(m_mjd[static_cast<std::size_t>(i)]
                                      - m_mjd[static_cast<std::size_t>(j)]));
        }
        Kext(i,  ni) = 1.0;
        Kext(ni, i ) = 1.0;
    }
    // Add jitter to K block only
    for (int i = 0; i < ni; ++i)
        Kext(i, i) += 1.0e-10;

    // LU decomposition with partial pivoting (system is symmetric but not
    // positive definite due to the Lagrange block)
    const Eigen::FullPivLU<Eigen::MatrixXd> lu(Kext);
    if (!lu.isInvertible()) {
        throw AlgorithmException(
            "OrdinaryKriging::fit: extended Kriging system is singular. "
            "Check for duplicate observation times or degenerate variogram.");
    }

    // Store inverse (n+1) x (n+1) for fast prediction
    const Eigen::MatrixXd Kinv = lu.inverse();
    const int ext2 = ext * ext;
    m_Kext.resize(static_cast<std::size_t>(ext));    // store original diag for variance
    m_Linv.resize(static_cast<std::size_t>(ext2));

    for (int i = 0; i < ext; ++i)
        for (int j = 0; j < ext; ++j)
            m_Linv[static_cast<std::size_t>(i * ext + j)] = Kinv(i, j);

    // Store diagonal of original K for C(0) -- use first element
    m_Kext.resize(1);
    m_Kext[0] = _cov(0.0);
}

KrigingPrediction OrdinaryKriging::predictAt(double mjd) const
{
    const std::size_t n  = m_mjd.size();
    const int         ni = static_cast<int>(n);
    const int         ext = ni + 1;

    // Extended RHS: [ k(t) ]
    //               [  1   ]
    Eigen::VectorXd rhs(ext);
    for (int i = 0; i < ni; ++i)
        rhs(i) = _cov(std::abs(mjd - m_mjd[static_cast<std::size_t>(i)]));
    rhs(ni) = 1.0;

    // Reconstruct inverse matrix
    Eigen::MatrixXd Kinv(ext, ext);
    for (int i = 0; i < ext; ++i)
        for (int j = 0; j < ext; ++j)
            Kinv(i, j) = m_Linv[static_cast<std::size_t>(i * ext + j)];

    const Eigen::VectorXd sol = Kinv * rhs; // [ lambda_1 ... lambda_n, mu ]

    const Eigen::VectorXd lam     = sol.head(ni);
    const double          lagrangeMu = sol(ni);

    const Eigen::Map<const Eigen::VectorXd> zVec(m_z.data(), ni);
    const double zStar = lam.dot(zVec);

    // Kriging variance: C(0) - k^T * lambda - mu (Lagrange multiplier)
    const double c0      = m_Kext[0]; // C(0)
    const double kTlam   = rhs.head(ni).dot(lam);
    const double sigmaK2 = std::max(0.0, c0 - kTlam - lagrangeMu);

    KrigingPrediction pred;
    pred.mjd      = mjd;
    pred.value    = zStar;
    pred.variance = sigmaK2;
    const double sigma = std::sqrt(sigmaK2);
    pred.ciLower  = zStar - m_ciZ * sigma;
    pred.ciUpper  = zStar + m_ciZ * sigma;
    pred.isObserved = false;
    return pred;
}

// =============================================================================
//  UniversalKriging
// =============================================================================

UniversalKriging::UniversalKriging(int trendDegree, double confidenceLevel)
    : m_degree(trendDegree)
    , m_tRef(0.0)
{
    m_ciZ = _zQuantile(confidenceLevel);
}

std::vector<double> UniversalKriging::_driftBasis(double mjd) const
{
    const double t = mjd - m_tRef;
    std::vector<double> f(static_cast<std::size_t>(m_degree + 1));
    double power = 1.0;
    for (int k = 0; k <= m_degree; ++k) {
        f[static_cast<std::size_t>(k)] = power;
        power *= t;
    }
    return f;
}

void UniversalKriging::fit(const TimeSeries& ts, const VariogramFitResult& variogram)
{
    m_variogram = variogram;
    m_mjd.clear();
    m_z  .clear();

    for (std::size_t i = 0; i < ts.size(); ++i) {
        if (!std::isnan(ts[i].value)) {
            m_mjd.push_back(ts[i].time.mjd());
            m_z  .push_back(ts[i].value);
        }
    }

    const std::size_t n  = m_mjd.size();
    const int         ni = static_cast<int>(n);
    if (n < 2) {
        throw DataException(
            "UniversalKriging::fit: need at least 2 valid observations, got "
            + std::to_string(n) + ".");
    }

    m_tRef = m_mjd[0]; // anchor for numerical stability

    const int p   = m_degree + 1; // number of drift terms
    const int ext = ni + p;       // extended system size

    // Build extended system:
    //   [ K    F ]
    //   [ F^T  0 ]
    Eigen::MatrixXd Kext(ext, ext);
    Kext.setZero();

    // K block
    for (int i = 0; i < ni; ++i)
        for (int j = 0; j < ni; ++j)
            Kext(i, j) = _cov(std::abs(m_mjd[static_cast<std::size_t>(i)]
                                      - m_mjd[static_cast<std::size_t>(j)]));

    // F block
    for (int i = 0; i < ni; ++i) {
        const auto fi = _driftBasis(m_mjd[static_cast<std::size_t>(i)]);
        for (int k = 0; k < p; ++k) {
            Kext(i,      ni + k) = fi[static_cast<std::size_t>(k)];
            Kext(ni + k, i     ) = fi[static_cast<std::size_t>(k)];
        }
    }

    // Jitter on K block
    for (int i = 0; i < ni; ++i)
        Kext(i, i) += 1.0e-10;

    const Eigen::FullPivLU<Eigen::MatrixXd> lu(Kext);
    if (!lu.isInvertible()) {
        throw AlgorithmException(
            "UniversalKriging::fit: extended Kriging system is singular. "
            "Check for duplicate observation times, degenerate variogram, "
            "or too high a trendDegree relative to number of observations.");
    }

    const Eigen::MatrixXd Kinv = lu.inverse();
    m_Linv.resize(static_cast<std::size_t>(ext * ext));
    for (int i = 0; i < ext; ++i)
        for (int j = 0; j < ext; ++j)
            m_Linv[static_cast<std::size_t>(i * ext + j)] = Kinv(i, j);

    m_Kext.resize(1);
    m_Kext[0] = _cov(0.0);

    // Store pivot count as ext for predictAt
    m_piv.resize(1, ext);
}

KrigingPrediction UniversalKriging::predictAt(double mjd) const
{
    const std::size_t n  = m_mjd.size();
    const int         ni = static_cast<int>(n);
    const int         p  = m_degree + 1;
    const int         ext = ni + p;

    // Extended RHS: [ k(t) ]
    //               [ f(t) ]
    Eigen::VectorXd rhs(ext);
    for (int i = 0; i < ni; ++i)
        rhs(i) = _cov(std::abs(mjd - m_mjd[static_cast<std::size_t>(i)]));

    const auto ft = _driftBasis(mjd);
    for (int k = 0; k < p; ++k)
        rhs(ni + k) = ft[static_cast<std::size_t>(k)];

    // Reconstruct inverse
    Eigen::MatrixXd Kinv(ext, ext);
    for (int i = 0; i < ext; ++i)
        for (int j = 0; j < ext; ++j)
            Kinv(i, j) = m_Linv[static_cast<std::size_t>(i * ext + j)];

    const Eigen::VectorXd sol  = Kinv * rhs;
    const Eigen::VectorXd lam  = sol.head(ni);   // Kriging weights
    // sol.tail(p) = Lagrange multipliers beta

    const Eigen::Map<const Eigen::VectorXd> zVec(m_z.data(), ni);
    const double zStar = lam.dot(zVec);

    // Kriging variance: C(0) - k^T * lambda - f^T * beta
    const double c0      = m_Kext[0];
    const double kTlam   = rhs.head(ni).dot(lam);
    const double fTbeta  = rhs.tail(p).dot(sol.tail(p));
    const double sigmaK2 = std::max(0.0, c0 - kTlam - fTbeta);

    KrigingPrediction pred;
    pred.mjd      = mjd;
    pred.value    = zStar;
    pred.variance = sigmaK2;
    const double sigma = std::sqrt(sigmaK2);
    pred.ciLower  = zStar - m_ciZ * sigma;
    pred.ciUpper  = zStar + m_ciZ * sigma;
    pred.isObserved = false;
    return pred;
}

// =============================================================================
//  Factory
// =============================================================================

std::unique_ptr<KrigingBase> createKriging(const KrigingConfig& cfg)
{
    // Spatial / space-time: PLACEHOLDER
    if (cfg.mode == "spatial" || cfg.mode == "space_time") {
        throw AlgorithmException(
            "createKriging: mode '" + cfg.mode + "' is not yet implemented. "
            "Currently only temporal Kriging (mode = \"temporal\") is supported. "
            "Spatial and space-time Kriging are planned for a future release.");
    }
    if (cfg.mode != "temporal") {
        throw ConfigException(
            "createKriging: unrecognised mode '" + cfg.mode
            + "'. Valid: temporal, spatial (placeholder), space_time (placeholder).");
    }

    if (cfg.method == "simple") {
        return std::make_unique<SimpleKriging>(cfg.knownMean, cfg.confidenceLevel);
    }
    if (cfg.method == "ordinary") {
        return std::make_unique<OrdinaryKriging>(cfg.confidenceLevel);
    }
    if (cfg.method == "universal") {
        if (cfg.trendDegree < 1 || cfg.trendDegree > 5) {
            throw ConfigException(
                "createKriging: trendDegree must be in [1, 5], got "
                + std::to_string(cfg.trendDegree) + ".");
        }
        return std::make_unique<UniversalKriging>(cfg.trendDegree,
                                                  cfg.confidenceLevel);
    }

    throw ConfigException(
        "createKriging: unrecognised method '" + cfg.method
        + "'. Valid: simple, ordinary, universal.");
}

} // namespace loki::kriging
