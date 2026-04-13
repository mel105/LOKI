#include <loki/math/ordinaryKriging.hpp>

#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <Eigen/Dense>

#include <cmath>

namespace loki::math {

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

    const int n   = static_cast<int>(m_mjd.size());
    const int ext = n + 1;
    if (n < 2) {
        throw DataException(
            "OrdinaryKriging::fit: need at least 2 valid observations, got "
            + std::to_string(n) + ".");
    }

    m_c0 = _cov(0.0);

    // Extended system (n+1) x (n+1):  [ K  1 ]
    //                                  [ 1T 0 ]
    Eigen::MatrixXd Kext(ext, ext);
    Kext.setZero();
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j)
            Kext(i, j) = _cov(std::abs(m_mjd[static_cast<std::size_t>(i)]
                                      - m_mjd[static_cast<std::size_t>(j)]));
        Kext(i,  n) = 1.0;
        Kext(n,  i) = 1.0;
    }
    for (int i = 0; i < n; ++i) Kext(i, i) += 1.0e-10;

    const Eigen::FullPivLU<Eigen::MatrixXd> lu(Kext);
    if (!lu.isInvertible()) {
        throw AlgorithmException(
            "OrdinaryKriging::fit: extended Kriging system is singular. "
            "Check for duplicate observation times or degenerate variogram.");
    }

    m_Kinv = lu.inverse();
    LOKI_INFO("  OrdinaryKriging fitted: n=" + std::to_string(n));
}

KrigingPrediction OrdinaryKriging::predictAt(double mjd) const
{
    const int n   = static_cast<int>(m_mjd.size());
    const int ext = n + 1;

    Eigen::VectorXd rhs(ext);
    for (int i = 0; i < n; ++i)
        rhs(i) = _cov(std::abs(mjd - m_mjd[static_cast<std::size_t>(i)]));
    rhs(n) = 1.0;

    const Eigen::VectorXd sol       = m_Kinv * rhs;
    const Eigen::VectorXd lam       = sol.head(n);
    const double          lagrangeMu = sol(n);

    const Eigen::Map<const Eigen::VectorXd> zVec(m_z.data(), n);
    const double zStar   = lam.dot(zVec);
    const double sigmaK2 = std::max(m_variogram.nugget,
        m_c0 - rhs.head(n).dot(lam) - lagrangeMu);

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
