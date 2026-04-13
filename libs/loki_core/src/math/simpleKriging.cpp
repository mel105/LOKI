#include <loki/math/simpleKriging.hpp>

#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/math/krigingVariogram.hpp>

#include <Eigen/Dense>

#include <cmath>

namespace loki::math {

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

    const int n = static_cast<int>(m_mjd.size());
    if (n < 2) {
        throw DataException(
            "SimpleKriging::fit: need at least 2 valid observations, got "
            + std::to_string(n) + ".");
    }

    m_c0 = _cov(0.0);

    Eigen::MatrixXd K(n, n);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            K(i, j) = _cov(std::abs(m_mjd[static_cast<std::size_t>(i)]
                                   - m_mjd[static_cast<std::size_t>(j)]));
    K += 1.0e-10 * Eigen::MatrixXd::Identity(n, n);

    const Eigen::LLT<Eigen::MatrixXd> llt(K);
    if (llt.info() != Eigen::Success) {
        throw AlgorithmException(
            "SimpleKriging::fit: covariance matrix is not positive definite. "
            "Consider adding a nugget or checking variogram parameters.");
    }

    m_Kinv = llt.solve(Eigen::MatrixXd::Identity(n, n));
    LOKI_INFO("  SimpleKriging fitted: n=" + std::to_string(n));
}

KrigingPrediction SimpleKriging::predictAt(double mjd) const
{
    const int n = static_cast<int>(m_mjd.size());

    Eigen::VectorXd kv(n);
    for (int i = 0; i < n; ++i)
        kv(i) = _cov(std::abs(mjd - m_mjd[static_cast<std::size_t>(i)]));

    const Eigen::VectorXd lam = m_Kinv * kv;
    const Eigen::Map<const Eigen::VectorXd> zVec(m_z.data(), n);
    const double zStar = m_mu + lam.dot(zVec - m_mu * Eigen::VectorXd::Ones(n));

    const double sigmaK2 = std::max(m_variogram.nugget, m_c0 - lam.dot(kv));

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
