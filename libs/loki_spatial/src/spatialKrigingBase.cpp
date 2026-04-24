#include <loki/spatial/spatialKrigingBase.hpp>

#include <loki/core/exceptions.hpp>
#include <loki/math/krigingVariogram.hpp>
#include <loki/math/spatialVariogram.hpp>

#include <cmath>
#include <numbers>
#include <numeric>

using namespace loki;

namespace loki::spatial {

// -----------------------------------------------------------------------------
//  _cov
// -----------------------------------------------------------------------------

double SpatialKrigingBase::_cov(double h) const
{
    using loki::math::variogramEval;
    if (m_variogram.model == "power") {
        // Power model is intrinsic -- no finite sill.
        return -variogramEval(h, m_variogram);
    }
    return m_c0 - variogramEval(h, m_variogram);
}

// -----------------------------------------------------------------------------
//  _zQuantile  (bisection on erf)
// -----------------------------------------------------------------------------

double SpatialKrigingBase::_zQuantile(double confidenceLevel)
{
    // We need z such that 2*Phi(z)-1 = confidenceLevel, i.e. Phi(z) = (1+CL)/2.
    // Phi(z) = 0.5 * (1 + erf(z/sqrt(2)))
    // => erf(z/sqrt(2)) = CL  => z = sqrt(2) * erfinv(CL)
    const double target = confidenceLevel;
    double lo = 0.0, hi = 5.0;
    for (int i = 0; i < 60; ++i) {
        const double mid  = (lo + hi) * 0.5;
        const double fmid = std::erf(mid / std::sqrt(2.0));
        if (fmid < target) lo = mid; else hi = mid;
    }
    return (lo + hi) * 0.5;
}

// -----------------------------------------------------------------------------
//  predictGrid
// -----------------------------------------------------------------------------

loki::math::SpatialGrid SpatialKrigingBase::predictGrid(
    const loki::math::GridExtent& extent,
    double                        confidenceLevel) const
{
    const double ciZ = _zQuantile(confidenceLevel);

    loki::math::SpatialGrid grid;
    grid.extent   = extent;
    grid.values   = Eigen::MatrixXd::Zero(extent.nRows, extent.nCols);
    grid.variance = Eigen::MatrixXd::Zero(extent.nRows, extent.nCols);
    grid.ciLower  = Eigen::MatrixXd::Zero(extent.nRows, extent.nCols);
    grid.ciUpper  = Eigen::MatrixXd::Zero(extent.nRows, extent.nCols);

    for (int row = 0; row < extent.nRows; ++row) {
        const double qy = extent.yMin + static_cast<double>(row) * extent.resY;
        for (int col = 0; col < extent.nCols; ++col) {
            const double qx = extent.xMin + static_cast<double>(col) * extent.resX;
            const auto   p  = predictAt(qx, qy);

            grid.values  (row, col) = p.value;
            grid.variance(row, col) = p.variance;
            const double sigma = std::sqrt(std::max(0.0, p.variance));
            grid.ciLower (row, col) = p.value - ciZ * sigma;
            grid.ciUpper (row, col) = p.value + ciZ * sigma;
        }
    }
    return grid;
}

// -----------------------------------------------------------------------------
//  crossValidate  (LOO shortcut: e_i = alpha_i / K^{-1}_{ii})
// -----------------------------------------------------------------------------

loki::math::SpatialCrossValidationResult SpatialKrigingBase::crossValidate(
    double confidenceLevel) const
{
    const int n = static_cast<int>(m_z.size());
    if (n < 2) {
        throw DataException(
            "SpatialKrigingBase::crossValidate: need at least 2 observations.");
    }

    // LOO cross-validation via n explicit refits.
    //
    // The Dubrule (1983) shortcut e_i = alpha_i / [K^{-1}]_{ii} is exact only
    // for Simple Kriging where K_ext == K (n x n).  For Ordinary and Universal
    // Kriging the extended matrix K_ext is (n+p) x (n+p), and the upper-left
    // block of its inverse is NOT the inverse of K alone.  Using the shortcut
    // on that block produces incorrect LOO errors.
    //
    // We therefore refit the model n times (O(n^3) each), leaving one point out
    // each time.  For typical spatial datasets (n < 500) this is tractable.
    // Runtime: O(n^4) total -- acceptable for n <= 200, slow for n > 500.

    loki::math::SpatialCrossValidationResult cv;
    cv.errors.resize(static_cast<std::size_t>(n));
    cv.stdErrors.resize(static_cast<std::size_t>(n));
    cv.x.resize(static_cast<std::size_t>(n));
    cv.y.resize(static_cast<std::size_t>(n));

    double sumSq  = 0.0;
    double sumAbs = 0.0;
    double sumSE  = 0.0;
    double sumSSE = 0.0;

    for (int i = 0; i < n; ++i) {
        const std::size_t si = static_cast<std::size_t>(i);

        // Build leave-one-out point set.
        std::vector<loki::math::SpatialPoint> looPts;
        looPts.reserve(static_cast<std::size_t>(n - 1));
        for (int j = 0; j < n; ++j) {
            if (j == i) continue;
            looPts.push_back({ m_x[static_cast<std::size_t>(j)],
                               m_y[static_cast<std::size_t>(j)],
                               m_z[static_cast<std::size_t>(j)] });
        }

        // Clone this estimator type and refit on training set.
        auto looModel = _cloneEmpty();
        looModel->fit(looPts, m_variogram);

        // Predict at the left-out point.
        const auto pred = looModel->predictAt(m_x[si], m_y[si]);

        const double ei      = m_z[si] - pred.value;
        const double sigma_i = std::sqrt(std::max(0.0, pred.variance));

        cv.errors   [si] = ei;
        cv.stdErrors[si] = (sigma_i > 1.0e-15) ? ei / sigma_i : 0.0;
        cv.x        [si] = m_x[si];
        cv.y        [si] = m_y[si];

        sumSq  += ei * ei;
        sumAbs += std::abs(ei);
        sumSE  += cv.stdErrors[si];
        sumSSE += cv.stdErrors[si] * cv.stdErrors[si];
    }

    const double nd = static_cast<double>(n);
    cv.rmse    = std::sqrt(sumSq  / nd);
    cv.mae     = sumAbs / nd;
    cv.meanSE  = sumSE  / nd;
    cv.meanSSE = sumSSE / nd;

    return cv;
}

} // namespace loki::spatial
