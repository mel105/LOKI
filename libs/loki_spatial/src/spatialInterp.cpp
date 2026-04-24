#include <loki/spatial/spatialInterp.hpp>

#include <loki/core/exceptions.hpp>
#include <loki/math/bspline.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>

using namespace loki;

namespace loki::spatial {

// =============================================================================
//  IDW
// =============================================================================

double interpIDW(const std::vector<loki::math::SpatialPoint>& points,
                 double qx, double qy, double power)
{
    if (points.empty()) {
        throw DataException("interpIDW: points vector is empty.");
    }

    double sumW = 0.0;
    double sumWZ = 0.0;

    for (const auto& p : points) {
        const double dx = qx - p.x;
        const double dy = qy - p.y;
        const double d  = std::sqrt(dx * dx + dy * dy);

        if (d < 1.0e-12) {
            // Query coincides with observation -- return exact value.
            return p.value;
        }

        const double w = 1.0 / std::pow(d, power);
        sumW  += w;
        sumWZ += w * p.value;
    }

    return sumWZ / sumW;
}

Eigen::MatrixXd interpIDWGrid(
    const std::vector<loki::math::SpatialPoint>& points,
    const loki::math::GridExtent&                extent,
    double                                        power)
{
    Eigen::MatrixXd grid(extent.nRows, extent.nCols);
    for (int row = 0; row < extent.nRows; ++row) {
        const double qy = extent.yMin + static_cast<double>(row) * extent.resY;
        for (int col = 0; col < extent.nCols; ++col) {
            const double qx  = extent.xMin + static_cast<double>(col) * extent.resX;
            grid(row, col)   = interpIDW(points, qx, qy, power);
        }
    }
    return grid;
}

loki::math::SpatialCrossValidationResult crossValidateIDW(
    const std::vector<loki::math::SpatialPoint>& points,
    double                                        power)
{
    const int n = static_cast<int>(points.size());
    if (n < 4) {
        throw DataException(
            "crossValidateIDW: need at least 4 points, got " + std::to_string(n) + ".");
    }

    loki::math::SpatialCrossValidationResult cv;
    cv.errors.resize(static_cast<std::size_t>(n));
    cv.stdErrors.resize(static_cast<std::size_t>(n), 0.0);
    cv.x.resize(static_cast<std::size_t>(n));
    cv.y.resize(static_cast<std::size_t>(n));

    for (int i = 0; i < n; ++i) {
        std::vector<loki::math::SpatialPoint> trainPts;
        trainPts.reserve(static_cast<std::size_t>(n - 1));
        for (int j = 0; j < n; ++j) {
            if (j != i) trainPts.push_back(points[static_cast<std::size_t>(j)]);
        }
        const double pred = interpIDW(trainPts,
                                      points[static_cast<std::size_t>(i)].x,
                                      points[static_cast<std::size_t>(i)].y,
                                      power);
        cv.errors[static_cast<std::size_t>(i)] =
            points[static_cast<std::size_t>(i)].value - pred;
        cv.x[static_cast<std::size_t>(i)] = points[static_cast<std::size_t>(i)].x;
        cv.y[static_cast<std::size_t>(i)] = points[static_cast<std::size_t>(i)].y;
    }

    double sumSq = 0.0, sumAbs = 0.0;
    for (std::size_t i = 0; i < static_cast<std::size_t>(n); ++i) {
        sumSq  += cv.errors[i] * cv.errors[i];
        sumAbs += std::abs(cv.errors[i]);
    }
    cv.rmse    = std::sqrt(sumSq / static_cast<double>(n));
    cv.mae     = sumAbs / static_cast<double>(n);
    cv.meanSE  = 0.0;
    cv.meanSSE = 0.0;
    return cv;
}

// =============================================================================
//  Bilinear
// =============================================================================

Eigen::MatrixXd interpBilinear(
    const Eigen::MatrixXd&        inputGrid,
    const loki::math::GridExtent& inputExtent,
    const loki::math::GridExtent& extent)
{
    const int nRowsIn = inputGrid.rows();
    const int nColsIn = inputGrid.cols();
    if (nRowsIn < 2 || nColsIn < 2) {
        throw DataException(
            "interpBilinear: input grid must have at least 2 rows and 2 columns.");
    }

    Eigen::MatrixXd out(extent.nRows, extent.nCols);

    for (int row = 0; row < extent.nRows; ++row) {
        const double qy = extent.yMin + static_cast<double>(row) * extent.resY;

        // Map qy to input grid fractional row index
        const double fy = (inputExtent.resY > 0.0)
            ? (qy - inputExtent.yMin) / inputExtent.resY
            : 0.0;

        const int r0 = std::max(0, std::min(static_cast<int>(fy),       nRowsIn - 2));
        const int r1 = r0 + 1;
        const double ty = std::clamp(fy - static_cast<double>(r0), 0.0, 1.0);

        for (int col = 0; col < extent.nCols; ++col) {
            const double qx = extent.xMin + static_cast<double>(col) * extent.resX;
            const double fx = (inputExtent.resX > 0.0)
                ? (qx - inputExtent.xMin) / inputExtent.resX
                : 0.0;

            const int c0 = std::max(0, std::min(static_cast<int>(fx), nColsIn - 2));
            const int c1 = c0 + 1;
            const double tx = std::clamp(fx - static_cast<double>(c0), 0.0, 1.0);

            const double f00 = inputGrid(r0, c0);
            const double f10 = inputGrid(r0, c1);
            const double f01 = inputGrid(r1, c0);
            const double f11 = inputGrid(r1, c1);

            out(row, col) = (1.0 - tx) * (1.0 - ty) * f00
                          +        tx  * (1.0 - ty) * f10
                          + (1.0 - tx) *        ty  * f01
                          +        tx  *        ty  * f11;
        }
    }
    return out;
}

// =============================================================================
//  Tensor product B-spline surface
// =============================================================================

BSplineSurfaceResult fitBSplineSurface(
    const std::vector<loki::math::SpatialPoint>& points,
    int                                           degreeU,
    int                                           degreeV,
    int                                           nCtrlU,
    int                                           nCtrlV,
    const std::string&                            knotPlacement)
{
    const int n = static_cast<int>(points.size());
    const int nCtrl = nCtrlU * nCtrlV;
    if (n < nCtrl) {
        throw DataException(
            "fitBSplineSurface: need at least " + std::to_string(nCtrl)
            + " points for a " + std::to_string(nCtrlU) + " x "
            + std::to_string(nCtrlV) + " control grid, got " + std::to_string(n) + ".");
    }
    if (degreeU < 1 || degreeU > 5 || degreeV < 1 || degreeV > 5) {
        throw ConfigException(
            "fitBSplineSurface: degree must be in [1, 5].");
    }
    if (nCtrlU < degreeU + 1 || nCtrlV < degreeV + 1) {
        throw ConfigException(
            "fitBSplineSurface: nCtrl must be >= degree + 1 in each direction.");
    }

    // Normalise x, y to [0, 1] separately.
    double xMin = points[0].x, xMax = points[0].x;
    double yMin = points[0].y, yMax = points[0].y;
    for (const auto& p : points) {
        xMin = std::min(xMin, p.x); xMax = std::max(xMax, p.x);
        yMin = std::min(yMin, p.y); yMax = std::max(yMax, p.y);
    }
    const double xRange = (xMax - xMin > 1.0e-12) ? xMax - xMin : 1.0;
    const double yRange = (yMax - yMin > 1.0e-12) ? yMax - yMin : 1.0;

    std::vector<double> u(static_cast<std::size_t>(n));
    std::vector<double> v(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        u[static_cast<std::size_t>(i)] = (points[static_cast<std::size_t>(i)].x - xMin) / xRange;
        v[static_cast<std::size_t>(i)] = (points[static_cast<std::size_t>(i)].y - yMin) / yRange;
    }

    // Build knot vectors.
    std::vector<double> knotsU, knotsV;
    if (knotPlacement == "chord_length") {
        knotsU = loki::math::buildChordLengthKnots(u, nCtrlU, degreeU);
        knotsV = loki::math::buildChordLengthKnots(v, nCtrlV, degreeV);
    } else {
        knotsU = loki::math::buildUniformKnots(nCtrlU, degreeU);
        knotsV = loki::math::buildUniformKnots(nCtrlV, degreeV);
    }

    // Build the tensor product basis matrix A (n x nCtrlU*nCtrlV).
    // Row i: basis_U(u_i) (x) basis_V(v_i) as a Kronecker product.
    Eigen::MatrixXd A(n, nCtrl);
    for (int i = 0; i < n; ++i) {
        const auto rowU = loki::math::bsplineBasisRow(
            u[static_cast<std::size_t>(i)], degreeU, knotsU);
        const auto rowV = loki::math::bsplineBasisRow(
            v[static_cast<std::size_t>(i)], degreeV, knotsV);

        // Kronecker row: entry (j*nCtrlV + k) = rowU[j] * rowV[k]
        for (int j = 0; j < nCtrlU; ++j) {
            for (int k = 0; k < nCtrlV; ++k) {
                A(i, j * nCtrlV + k) = rowU[static_cast<std::size_t>(j)]
                                     * rowV[static_cast<std::size_t>(k)];
            }
        }
    }

    // Build RHS z vector.
    Eigen::VectorXd z(n);
    for (int i = 0; i < n; ++i) {
        z(i) = points[static_cast<std::size_t>(i)].value;
    }

    // Solve overdetermined system A * c = z via ColPivHouseholderQR.
    Eigen::ColPivHouseholderQR<Eigen::MatrixXd> qr(A);
    if (qr.rank() < nCtrl) {
        throw AlgorithmException(
            "fitBSplineSurface: design matrix is rank-deficient (rank="
            + std::to_string(qr.rank()) + ", nCtrl=" + std::to_string(nCtrl)
            + "). Reduce nCtrlU/nCtrlV or add more input points.");
    }
    const Eigen::VectorXd cFlat = qr.solve(z);

    // Training RMSE and R^2.
    const Eigen::VectorXd residuals = A * cFlat - z;
    const double sse    = residuals.squaredNorm();
    const double zMean  = z.mean();
    const double ssTot  = (z.array() - zMean).square().sum();
    const double rmse   = std::sqrt(sse / static_cast<double>(n));
    const double r2     = (ssTot > 0.0) ? (1.0 - sse / ssTot) : 1.0;

    // Reshape flat control vector to nCtrlU x nCtrlV matrix.
    Eigen::MatrixXd ctrlGrid(nCtrlU, nCtrlV);
    for (int j = 0; j < nCtrlU; ++j) {
        for (int k = 0; k < nCtrlV; ++k) {
            ctrlGrid(j, k) = cFlat(j * nCtrlV + k);
        }
    }

    BSplineSurfaceResult result;
    result.controlPoints = ctrlGrid;
    result.knotsU        = knotsU;
    result.knotsV        = knotsV;
    result.degreeU       = degreeU;
    result.degreeV       = degreeV;
    result.nCtrlU        = nCtrlU;
    result.nCtrlV        = nCtrlV;
    result.rmse          = rmse;
    result.rSquared      = r2;
    result.knotPlacement = knotPlacement;
    return result;
}

double evalBSplineSurface(const BSplineSurfaceResult& surf,
                          double xMin, double xMax,
                          double yMin, double yMax,
                          double qx, double qy)
{
    const double xRange = (xMax - xMin > 1.0e-12) ? xMax - xMin : 1.0;
    const double yRange = (yMax - yMin > 1.0e-12) ? yMax - yMin : 1.0;
    const double u = std::clamp((qx - xMin) / xRange, 0.0, 1.0);
    const double v = std::clamp((qy - yMin) / yRange, 0.0, 1.0);

    const auto rowU = loki::math::bsplineBasisRow(u, surf.degreeU, surf.knotsU);
    const auto rowV = loki::math::bsplineBasisRow(v, surf.degreeV, surf.knotsV);

    double val = 0.0;
    for (int j = 0; j < surf.nCtrlU; ++j) {
        for (int k = 0; k < surf.nCtrlV; ++k) {
            val += rowU[static_cast<std::size_t>(j)]
                 * rowV[static_cast<std::size_t>(k)]
                 * surf.controlPoints(j, k);
        }
    }
    return val;
}

Eigen::MatrixXd evalBSplineSurfaceGrid(const BSplineSurfaceResult&   surf,
                                        const loki::math::GridExtent& extent)
{
    // Recover original x, y bounds from the grid extent for normalisation.
    // (The analyzer stores these in SpatialResult and passes them through.)
    const double xMin = extent.xMin;
    const double xMax = extent.xMax;
    const double yMin = extent.yMin;
    const double yMax = extent.yMax;

    Eigen::MatrixXd grid(extent.nRows, extent.nCols);
    for (int row = 0; row < extent.nRows; ++row) {
        const double qy = yMin + static_cast<double>(row) * extent.resY;
        for (int col = 0; col < extent.nCols; ++col) {
            const double qx  = xMin + static_cast<double>(col) * extent.resX;
            grid(row, col)   = evalBSplineSurface(surf, xMin, xMax, yMin, yMax, qx, qy);
        }
    }
    return grid;
}

// =============================================================================
//  NURBS surface  (placeholder -- throws AlgorithmException)
// =============================================================================

BSplineSurfaceResult fitNurbsSurface(
    const std::vector<loki::math::SpatialPoint>& /*points*/,
    int                                           /*degreeU*/,
    int                                           /*degreeV*/,
    int                                           /*nCtrlU*/,
    int                                           /*nCtrlV*/,
    const std::string&                            /*knotPlacement*/)
{
    throw AlgorithmException(
        "fitNurbsSurface: NURBS surface fitting is not yet implemented in "
        "loki_spatial v1. Use method 'bspline_surface', 'rbf', or 'kriging' "
        "instead. NURBS surface support is planned for a future release.");
}

} // namespace loki::spatial
