#include <memory>
#include <loki/spatial/spatialKrigingSimple.hpp>
#include <loki/spatial/spatialKrigingOrdinary.hpp>
#include <loki/spatial/spatialKrigingUniversal.hpp>

#include <loki/core/exceptions.hpp>
#include <loki/math/krigingVariogram.hpp>
#include <loki/math/spatialVariogram.hpp>

#include <cmath>
#include <numeric>

using namespace loki;

// =============================================================================
//  Internal helpers shared by all variants
// =============================================================================

namespace {

// Build the covariance vector k(x,y) of length n (one entry per input point).
static Eigen::VectorXd buildCovVector(
    const std::vector<double>& xPts,
    const std::vector<double>& yPts,
    double                     qx,
    double                     qy,
    const loki::math::VariogramFitResult& variogram,
    double                     c0)
{
    const int n = static_cast<int>(xPts.size());
    Eigen::VectorXd k(n);
    for (int i = 0; i < n; ++i) {
        const double dx = qx - xPts[static_cast<std::size_t>(i)];
        const double dy = qy - yPts[static_cast<std::size_t>(i)];
        const double h  = std::sqrt(dx * dx + dy * dy);
        const double gamma = loki::math::variogramEval(h, variogram);
        if (variogram.model == "power") {
            k(i) = -gamma;
        } else {
            k(i) = c0 - gamma;
        }
    }
    return k;
}

// Build n x n covariance matrix K from scatter points.
static Eigen::MatrixXd buildCovMatrix(
    const std::vector<double>& xPts,
    const std::vector<double>& yPts,
    const loki::math::VariogramFitResult& variogram,
    double c0)
{
    const int n = static_cast<int>(xPts.size());
    Eigen::MatrixXd K(n, n);
    for (int i = 0; i < n; ++i) {
        K(i, i) = c0;  // C(0)
        for (int j = i + 1; j < n; ++j) {
            const double dx = xPts[static_cast<std::size_t>(i)]
                            - xPts[static_cast<std::size_t>(j)];
            const double dy = yPts[static_cast<std::size_t>(i)]
                            - yPts[static_cast<std::size_t>(j)];
            const double h  = std::sqrt(dx * dx + dy * dy);
            const double gamma = loki::math::variogramEval(h, variogram);
            const double cov = (variogram.model == "power") ? -gamma : c0 - gamma;
            K(i, j) = cov;
            K(j, i) = cov;
        }
    }
    return K;
}

} // anonymous namespace

// =============================================================================
//  SpatialSimpleKriging
// =============================================================================

namespace loki::spatial {

SpatialSimpleKriging::SpatialSimpleKriging(double knownMean, double confidenceLevel)
    : m_mu(knownMean)
{
    m_ciZ = _zQuantile(confidenceLevel);
}

void SpatialSimpleKriging::fit(
    const std::vector<loki::math::SpatialPoint>& points,
    const loki::math::VariogramFitResult&        variogram)
{
    m_pts      = points;
    m_variogram = variogram;
    m_x.clear(); m_y.clear(); m_z.clear();
    for (const auto& p : points) {
        if (!std::isnan(p.value)) {
            m_x.push_back(p.x);
            m_y.push_back(p.y);
            m_z.push_back(p.value);
        }
    }
    if (m_z.size() < 2) {
        throw DataException("SpatialSimpleKriging::fit: need >= 2 valid points.");
    }

    m_c0 = (variogram.model == "power") ? 0.0 : variogram.sill;

    Eigen::MatrixXd K = buildCovMatrix(m_x, m_y, variogram, m_c0);
    Eigen::FullPivLU<Eigen::MatrixXd> lu(K);
    if (!lu.isInvertible()) {
        throw AlgorithmException(
            "SpatialSimpleKriging::fit: Kriging matrix is singular. "
            "Check for duplicate observation locations.");
    }
    m_Kinv = lu.inverse();
}

std::unique_ptr<SpatialKrigingBase> SpatialSimpleKriging::_cloneEmpty() const
{
    return std::make_unique<SpatialSimpleKriging>(m_mu, 0.95);
    // Note: confidence level not stored; use default. Only affects CI bounds in LOO, not errors.
}

loki::math::SpatialPrediction SpatialSimpleKriging::predictAt(double x, double y) const
{
    const int n = static_cast<int>(m_z.size());
    Eigen::VectorXd k = buildCovVector(m_x, m_y, x, y, m_variogram, m_c0);

    // lambda = K^{-1} * k
    const Eigen::VectorXd lambda = m_Kinv * k;

    // Z*(x,y) = mu + lambda^T * (z - mu)
    Eigen::VectorXd zCentered(n);
    for (int i = 0; i < n; ++i) {
        zCentered(i) = m_z[static_cast<std::size_t>(i)] - m_mu;
    }
    const double value = m_mu + lambda.dot(zCentered);

    // sigma^2 = C(0) - k^T * lambda
    const double var  = std::max(m_variogram.nugget, m_c0 - k.dot(lambda));
    const double sigma = std::sqrt(var);

    loki::math::SpatialPrediction pred;
    pred.x         = x;
    pred.y         = y;
    pred.value     = value;
    pred.variance  = var;
    pred.ciLower   = value - m_ciZ * sigma;
    pred.ciUpper   = value + m_ciZ * sigma;
    pred.isObserved = false;
    return pred;
}

// =============================================================================
//  SpatialOrdinaryKriging
// =============================================================================

SpatialOrdinaryKriging::SpatialOrdinaryKriging(double confidenceLevel)
{
    m_ciZ = _zQuantile(confidenceLevel);
}

void SpatialOrdinaryKriging::fit(
    const std::vector<loki::math::SpatialPoint>& points,
    const loki::math::VariogramFitResult&        variogram)
{
    m_pts      = points;
    m_variogram = variogram;
    m_x.clear(); m_y.clear(); m_z.clear();
    for (const auto& p : points) {
        if (!std::isnan(p.value)) {
            m_x.push_back(p.x);
            m_y.push_back(p.y);
            m_z.push_back(p.value);
        }
    }
    if (m_z.size() < 2) {
        throw DataException("SpatialOrdinaryKriging::fit: need >= 2 valid points.");
    }

    const int n = static_cast<int>(m_z.size());
    m_c0 = (variogram.model == "power") ? 0.0 : variogram.sill;

    // Extended (n+1) x (n+1) system: [ K  1; 1^T 0 ]
    Eigen::MatrixXd Kext = Eigen::MatrixXd::Zero(n + 1, n + 1);
    Kext.topLeftCorner(n, n) = buildCovMatrix(m_x, m_y, variogram, m_c0);
    Kext.col(n).head(n).setOnes();
    Kext.row(n).head(n).setOnes();
    // Kext(n, n) = 0

    Eigen::FullPivLU<Eigen::MatrixXd> lu(Kext);
    if (!lu.isInvertible()) {
        throw AlgorithmException(
            "SpatialOrdinaryKriging::fit: extended Kriging matrix is singular.");
    }
    m_Kinv = lu.inverse();
}

std::unique_ptr<SpatialKrigingBase> SpatialOrdinaryKriging::_cloneEmpty() const
{
    return std::make_unique<SpatialOrdinaryKriging>(0.95);
}

loki::math::SpatialPrediction SpatialOrdinaryKriging::predictAt(double x, double y) const
{
    const int n = static_cast<int>(m_z.size());
    const Eigen::VectorXd k = buildCovVector(m_x, m_y, x, y, m_variogram, m_c0);

    // Extended RHS [k; 1]
    Eigen::VectorXd rhs(n + 1);
    rhs.head(n) = k;
    rhs(n)      = 1.0;

    // [lambda; mu_lagrange] = K_ext^{-1} * rhs
    const Eigen::VectorXd sol    = m_Kinv * rhs;
    const Eigen::VectorXd lambda = sol.head(n);
    const double          mu_lag = sol(n);

    // Z*(x,y) = lambda^T * z
    Eigen::VectorXd zVec(n);
    for (int i = 0; i < n; ++i) zVec(i) = m_z[static_cast<std::size_t>(i)];
    const double value = lambda.dot(zVec);

    // sigma^2 = C(0) - k^T*lambda - mu_lagrange
    const double var   = std::max(m_variogram.nugget,
                                  m_c0 - k.dot(lambda) - mu_lag);
    const double sigma = std::sqrt(var);

    loki::math::SpatialPrediction pred;
    pred.x          = x;
    pred.y          = y;
    pred.value      = value;
    pred.variance   = var;
    pred.ciLower    = value - m_ciZ * sigma;
    pred.ciUpper    = value + m_ciZ * sigma;
    pred.isObserved = false;
    return pred;
}

// =============================================================================
//  SpatialUniversalKriging
// =============================================================================

SpatialUniversalKriging::SpatialUniversalKriging(int trendDegree,
                                                   double confidenceLevel)
    : m_degree(trendDegree)
{
    m_ciZ = _zQuantile(confidenceLevel);
}

std::unique_ptr<SpatialKrigingBase> SpatialUniversalKriging::_cloneEmpty() const
{
    return std::make_unique<SpatialUniversalKriging>(m_degree, 0.95);
}

std::vector<double> SpatialUniversalKriging::_driftBasis(double x, double y) const
{
    // Degree 1: [1, dx, dy]
    // Degree 2: [1, dx, dy, dx^2, dx*dy, dy^2]
    const double dx = x - m_xRef;
    const double dy = y - m_yRef;
    std::vector<double> basis;
    basis.push_back(1.0);
    if (m_degree >= 1) { basis.push_back(dx); basis.push_back(dy); }
    if (m_degree >= 2) {
        basis.push_back(dx * dx);
        basis.push_back(dx * dy);
        basis.push_back(dy * dy);
    }
    return basis;
}

void SpatialUniversalKriging::fit(
    const std::vector<loki::math::SpatialPoint>& points,
    const loki::math::VariogramFitResult&        variogram)
{
    m_pts      = points;
    m_variogram = variogram;
    m_x.clear(); m_y.clear(); m_z.clear();
    for (const auto& p : points) {
        if (!std::isnan(p.value)) {
            m_x.push_back(p.x);
            m_y.push_back(p.y);
            m_z.push_back(p.value);
        }
    }
    if (m_z.size() < 2) {
        throw DataException("SpatialUniversalKriging::fit: need >= 2 valid points.");
    }

    const int n = static_cast<int>(m_z.size());

    // Centroid for numerical stability
    m_xRef = std::accumulate(m_x.begin(), m_x.end(), 0.0) / n;
    m_yRef = std::accumulate(m_y.begin(), m_y.end(), 0.0) / n;

    m_c0 = (variogram.model == "power") ? 0.0 : variogram.sill;

    const int nDrift = static_cast<int>(_driftBasis(0.0, 0.0).size());
    const int total  = n + nDrift;

    // Build drift matrix F (n x nDrift)
    Eigen::MatrixXd F(n, nDrift);
    for (int i = 0; i < n; ++i) {
        const auto b = _driftBasis(m_x[static_cast<std::size_t>(i)],
                                   m_y[static_cast<std::size_t>(i)]);
        for (int d = 0; d < nDrift; ++d) F(i, d) = b[static_cast<std::size_t>(d)];
    }

    // Extended system [ K  F; F^T  0 ]
    Eigen::MatrixXd Kext = Eigen::MatrixXd::Zero(total, total);
    Kext.topLeftCorner(n, n)         = buildCovMatrix(m_x, m_y, variogram, m_c0);
    Kext.topRightCorner(n, nDrift)   = F;
    Kext.bottomLeftCorner(nDrift, n) = F.transpose();

    Eigen::FullPivLU<Eigen::MatrixXd> lu(Kext);
    if (!lu.isInvertible()) {
        throw AlgorithmException(
            "SpatialUniversalKriging::fit: extended Kriging matrix is singular. "
            "Try reducing trend degree or check for collinear observations.");
    }
    m_Kinv = lu.inverse();
}

loki::math::SpatialPrediction SpatialUniversalKriging::predictAt(
    double x, double y) const
{
    const int n      = static_cast<int>(m_z.size());
    const auto bVec  = _driftBasis(x, y);
    const int nDrift = static_cast<int>(bVec.size());

    const Eigen::VectorXd k = buildCovVector(m_x, m_y, x, y, m_variogram, m_c0);

    Eigen::VectorXd f(nDrift);
    for (int d = 0; d < nDrift; ++d) f(d) = bVec[static_cast<std::size_t>(d)];

    // Extended RHS [k; f]
    Eigen::VectorXd rhs(n + nDrift);
    rhs.head(n)    = k;
    rhs.tail(nDrift) = f;

    const Eigen::VectorXd sol    = m_Kinv * rhs;
    const Eigen::VectorXd lambda = sol.head(n);
    const Eigen::VectorXd beta   = sol.tail(nDrift);

    Eigen::VectorXd zVec(n);
    for (int i = 0; i < n; ++i) zVec(i) = m_z[static_cast<std::size_t>(i)];
    const double value = lambda.dot(zVec);

    // sigma^2 = C(0) - k^T*lambda - f^T*beta
    const double var   = std::max(m_variogram.nugget,
                                  m_c0 - k.dot(lambda) - f.dot(beta));
    const double sigma = std::sqrt(var);

    loki::math::SpatialPrediction pred;
    pred.x          = x;
    pred.y          = y;
    pred.value      = value;
    pred.variance   = var;
    pred.ciLower    = value - m_ciZ * sigma;
    pred.ciUpper    = value + m_ciZ * sigma;
    pred.isObserved = false;
    return pred;
}

} // namespace loki::spatial
