#include <loki/math/universalKriging.hpp>

#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <Eigen/Dense>

#include <cmath>

namespace loki::math {

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

    const int n   = static_cast<int>(m_mjd.size());
    const int p   = m_degree + 1;
    const int ext = n + p;
    if (n < 2) {
        throw DataException(
            "UniversalKriging::fit: need at least 2 valid observations, got "
            + std::to_string(n) + ".");
    }

    m_tRef = m_mjd[0];
    m_c0   = _cov(0.0);

    // Extended system (n+p) x (n+p):  [ K   F ]
    //                                  [ F^T 0 ]
    Eigen::MatrixXd Kext(ext, ext);
    Kext.setZero();

    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            Kext(i, j) = _cov(std::abs(m_mjd[static_cast<std::size_t>(i)]
                                      - m_mjd[static_cast<std::size_t>(j)]));

    for (int i = 0; i < n; ++i) {
        const auto fi = _driftBasis(m_mjd[static_cast<std::size_t>(i)]);
        for (int k = 0; k < p; ++k) {
            Kext(i,     n + k) = fi[static_cast<std::size_t>(k)];
            Kext(n + k, i    ) = fi[static_cast<std::size_t>(k)];
        }
    }
    for (int i = 0; i < n; ++i) Kext(i, i) += 1.0e-10;

    const Eigen::FullPivLU<Eigen::MatrixXd> lu(Kext);
    if (!lu.isInvertible()) {
        throw AlgorithmException(
            "UniversalKriging::fit: extended Kriging system is singular. "
            "Check for duplicate times, degenerate variogram, or too high trendDegree.");
    }

    m_Kinv = lu.inverse();
    LOKI_INFO("  UniversalKriging fitted: n=" + std::to_string(n)
              + "  degree=" + std::to_string(m_degree));
}

KrigingPrediction UniversalKriging::predictAt(double mjd) const
{
    const int n   = static_cast<int>(m_mjd.size());
    const int p   = m_degree + 1;
    const int ext = n + p;

    Eigen::VectorXd rhs(ext);
    for (int i = 0; i < n; ++i)
        rhs(i) = _cov(std::abs(mjd - m_mjd[static_cast<std::size_t>(i)]));
    const auto ft = _driftBasis(mjd);
    for (int k = 0; k < p; ++k)
        rhs(n + k) = ft[static_cast<std::size_t>(k)];

    const Eigen::VectorXd sol  = m_Kinv * rhs;
    const Eigen::VectorXd lam  = sol.head(n);
    const Eigen::VectorXd beta = sol.tail(p);

    const Eigen::Map<const Eigen::VectorXd> zVec(m_z.data(), n);
    const double zStar   = lam.dot(zVec);
    const double sigmaK2 = std::max(m_variogram.nugget,
        m_c0 - rhs.head(n).dot(lam) - rhs.tail(p).dot(beta));

    KrigingPrediction pred;
    pred.mjd        = mjd;
    pred.value      = zStar;
    pred.variance   = sigmaK2;
    const double sigma = std::sqrt(sigmaK2);
    pred.ciLower    = zStar - m_ciZ * sigma;
    pred.ciUpper    = zStar + m_ciZ * sigma;
    pred.isObserved = false;
    return pred;
}

} // namespace loki::math
