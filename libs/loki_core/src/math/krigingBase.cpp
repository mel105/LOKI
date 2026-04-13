#include <loki/math/krigingBase.hpp>

#include <loki/core/logger.hpp>
#include <loki/math/krigingVariogram.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace loki::math {

// =============================================================================
//  Helpers
// =============================================================================

double KrigingBase::_cov(double h) const
{
    if (m_variogram.model == "power") {
        // Intrinsic model: use -gamma convention
        return -variogramEval(h, m_variogram);
    }
    // Stationary models: C(h) = sill - gamma(h)
    if (h <= 0.0) return m_c0;
    return m_variogram.sill - variogramEval(h, m_variogram);
}

double KrigingBase::_zQuantile(double confidenceLevel)
{
    // Bisect on erf(x / sqrt(2)) = confidenceLevel for x in [0, 5]
    double lo = 0.0;
    double hi = 5.0;
    for (int i = 0; i < 60; ++i) {
        const double mid = 0.5 * (lo + hi);
        if (std::erf(mid / std::numbers::sqrt2) < confidenceLevel) lo = mid;
        else hi = mid;
    }
    return 0.5 * (lo + hi);
}

// =============================================================================
//  predictGrid
// =============================================================================

std::vector<KrigingPrediction> KrigingBase::predictGrid(
    const std::vector<double>& mjdPoints,
    double                     confidenceLevel) const
{
    const double z = _zQuantile(confidenceLevel);
    std::vector<KrigingPrediction> result;
    result.reserve(mjdPoints.size());

    for (const double mjd : mjdPoints) {
        KrigingPrediction pred = predictAt(mjd);
        const double sigma = std::sqrt(std::max(0.0, pred.variance));
        pred.ciLower = pred.value - z * sigma;
        pred.ciUpper = pred.value + z * sigma;

        pred.isObserved = false;
        for (const double t : m_mjd) {
            if (std::abs(t - mjd) < 1.0e-9) { pred.isObserved = true; break; }
        }
        result.push_back(pred);
    }
    return result;
}

// =============================================================================
//  crossValidate  --  O(n^2) LOO shortcut (Dubrule 1983)
//
//  For the Kriging system K * lambda = z, the full-data prediction at i is:
//    z*_i = z_i - alpha_i / K^{-1}_{ii}   where alpha = K^{-1} * z
//  so the LOO error is:
//    e_i  = alpha_i / K^{-1}_{ii}
//
//  LOO variance:
//    sigma^2_{-i} = C(0) - 1 / K^{-1}_{ii}
//
//  For Ordinary/Universal Kriging we use the upper-left n x n block of the
//  extended inverse (Lagrange rows/cols excluded).
// =============================================================================

CrossValidationResult KrigingBase::crossValidate(double /*confidenceLevel*/) const
{
    const int n = static_cast<int>(m_mjd.size());

    CrossValidationResult cv;
    cv.errors   .resize(static_cast<std::size_t>(n), 0.0);
    cv.stdErrors.resize(static_cast<std::size_t>(n), 0.0);
    cv.mjd      .resize(static_cast<std::size_t>(n), 0.0);

    if (m_Kinv.rows() < n || m_Kinv.cols() < n) {
        LOKI_WARNING("KrigingBase::crossValidate: cached inverse too small -- "
                     "skipping cross-validation.");
        return cv;
    }

    // Upper-left n x n block
    const Eigen::MatrixXd Knn = m_Kinv.topLeftCorner(n, n);
    const Eigen::Map<const Eigen::VectorXd> zVec(m_z.data(), n);
    const Eigen::VectorXd alpha = Knn * zVec;

    double sumSqErr = 0.0, sumAbsErr = 0.0, sumSE = 0.0, sumSSE = 0.0;

    for (int i = 0; i < n; ++i) {
        const std::size_t si = static_cast<std::size_t>(i);
        const double kii = Knn(i, i);
        cv.mjd[si] = m_mjd[si];

        if (std::abs(kii) < 1.0e-14) continue; // degenerate diagonal

        const double ei      = alpha(i) / kii;
        const double sigmaK2 = std::max(0.0, m_c0 - 1.0 / kii);
        const double sigmaK  = std::sqrt(sigmaK2 + 1.0e-15);
        const double sei     = ei / sigmaK;

        cv.errors   [si] = ei;
        cv.stdErrors[si] = sei;

        sumSqErr  += ei  * ei;
        sumAbsErr += std::abs(ei);
        sumSE     += sei;
        sumSSE    += sei * sei;
    }

    const double nn = static_cast<double>(n);
    cv.rmse    = std::sqrt(sumSqErr / nn);
    cv.mae     = sumAbsErr / nn;
    cv.mse     = sumSqErr  / nn;
    cv.meanSE  = sumSE     / nn;
    cv.meanSSE = sumSSE    / nn;
    return cv;
}

} // namespace loki::math
