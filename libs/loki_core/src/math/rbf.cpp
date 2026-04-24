#include <loki/math/rbf.hpp>

#include <loki/core/exceptions.hpp>

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <numeric>

using namespace loki;

namespace loki::math {

// -----------------------------------------------------------------------------
//  Kernel helpers
// -----------------------------------------------------------------------------

RbfKernel parseRbfKernel(const std::string& name)
{
    if (name == "multiquadric")         return RbfKernel::MULTIQUADRIC;
    if (name == "inverse_multiquadric") return RbfKernel::INVERSE_MULTIQUADRIC;
    if (name == "gaussian")             return RbfKernel::GAUSSIAN;
    if (name == "thin_plate_spline")    return RbfKernel::THIN_PLATE_SPLINE;
    if (name == "cubic")                return RbfKernel::CUBIC;
    throw ConfigException(
        "parseRbfKernel: unrecognised kernel '" + name + "'. "
        "Valid: multiquadric, inverse_multiquadric, gaussian, "
        "thin_plate_spline, cubic.");
}

double rbfKernelEval(double r, RbfKernel kernel, double epsilon)
{
    switch (kernel) {
    case RbfKernel::MULTIQUADRIC:
        return std::sqrt(r * r + epsilon * epsilon);
    case RbfKernel::INVERSE_MULTIQUADRIC:
        return 1.0 / std::sqrt(r * r + epsilon * epsilon);
    case RbfKernel::GAUSSIAN:
        return std::exp(-(epsilon * epsilon) * (r * r));
    case RbfKernel::THIN_PLATE_SPLINE:
        // phi(0) = 0 by convention; lim_{r->0} r^2 * log(r) = 0
        if (r < 1.0e-15) return 0.0;
        return (r * r) * std::log(r);
    case RbfKernel::CUBIC:
        return r * r * r;
    }
    // unreachable
    return 0.0;
}

int rbfPolyDegree(RbfKernel kernel)
{
    switch (kernel) {
    case RbfKernel::INVERSE_MULTIQUADRIC: return 0;
    case RbfKernel::GAUSSIAN:             return 0;
    case RbfKernel::MULTIQUADRIC:         return 1;
    case RbfKernel::THIN_PLATE_SPLINE:    return 1;
    case RbfKernel::CUBIC:                return 1;
    }
    return 1;
}

// -----------------------------------------------------------------------------
//  Polynomial augmentation matrix P (n x nPoly)
//
//  polyDegree = 0 : P = ones column  (constant)
//  polyDegree = 1 : P = [1, x, y]    (constant + linear)
// -----------------------------------------------------------------------------

static Eigen::MatrixXd buildPolyMatrix(const std::vector<SpatialPoint>& pts,
                                       int                               polyDegree)
{
    const int n     = static_cast<int>(pts.size());
    const int nPoly = (polyDegree == 0) ? 1 : 3;
    Eigen::MatrixXd P(n, nPoly);
    for (int i = 0; i < n; ++i) {
        P(i, 0) = 1.0;
        if (polyDegree >= 1) {
            P(i, 1) = pts[static_cast<std::size_t>(i)].x;
            P(i, 2) = pts[static_cast<std::size_t>(i)].y;
        }
    }
    return P;
}

// -----------------------------------------------------------------------------
//  fitRbf
// -----------------------------------------------------------------------------

RbfFitResult fitRbf(const std::vector<SpatialPoint>& points,
                    RbfKernel                         kernel,
                    double                            epsilon)
{
    const int n = static_cast<int>(points.size());
    if (n < 3) {
        throw DataException(
            "fitRbf: need at least 3 points, got " + std::to_string(n) + ".");
    }

    const int polyDeg = rbfPolyDegree(kernel);
    const int nPoly   = (polyDeg == 0) ? 1 : 3;
    const int total   = n + nPoly;

    // Normalise coordinates to [0, 1] using a uniform scale factor.
    // This is critical for TPS and cubic kernels: without normalisation
    // phi values scale as r^2 * log(r) ~ 1e9 for metre-scale coordinates,
    // while the polynomial augmentation matrix P has entries ~ 1e4.
    // The resulting 1e5 dynamic range makes the saddle-point system
    // structurally rank-deficient regardless of regularisation strength.
    // After normalisation phi ~ 0.3 and P ~ 1, giving a well-conditioned system.
    double xMin = points[0].x, xMax = points[0].x;
    double yMin = points[0].y, yMax = points[0].y;
    for (const auto& p : points) {
        xMin = std::min(xMin, p.x); xMax = std::max(xMax, p.x);
        yMin = std::min(yMin, p.y); yMax = std::max(yMax, p.y);
    }
    const double scale = std::max({xMax - xMin, yMax - yMin, 1.0e-10});

    // Build normalised point set (coordinates in [0,1]).
    std::vector<SpatialPoint> normPts(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        normPts[static_cast<std::size_t>(i)].x     =
            (points[static_cast<std::size_t>(i)].x - xMin) / scale;
        normPts[static_cast<std::size_t>(i)].y     =
            (points[static_cast<std::size_t>(i)].y - yMin) / scale;
        normPts[static_cast<std::size_t>(i)].value =
            points[static_cast<std::size_t>(i)].value;
    }

    // Build symmetric RBF matrix Phi (n x n) using normalised coordinates.
    Eigen::MatrixXd Phi(n, n);
    for (int i = 0; i < n; ++i) {
        for (int j = i; j < n; ++j) {
            const double dx = normPts[static_cast<std::size_t>(i)].x
                            - normPts[static_cast<std::size_t>(j)].x;
            const double dy = normPts[static_cast<std::size_t>(i)].y
                            - normPts[static_cast<std::size_t>(j)].y;
            const double r  = std::sqrt(dx * dx + dy * dy);
            const double v  = rbfKernelEval(r, kernel, epsilon);
            Phi(i, j) = v;
            Phi(j, i) = v;
        }
    }

    // Build polynomial augmentation matrix P using normalised coordinates.
    Eigen::MatrixXd P = buildPolyMatrix(normPts, polyDeg);

    // Assemble full system [Phi + lambda*I   P; P^T   0]
    //
    // Regularisation strategy:
    //   Conditionally PD kernels (TPS, multiquadric, cubic) have phi(0) = 0,
    //   which makes the Phi matrix singular without regularisation.  We add
    //   lambda * I to the Phi block where lambda scales with the typical
    //   off-diagonal magnitude so the perturbation is numerically meaningful
    //   but physically negligible.
    //
    //   For TPS in particular: phi(r) = r^2 * log(r) grows with r, so we use
    //   lambda = 1e-6 * max(|Phi_ij|) which is safely above machine epsilon
    //   yet small relative to the data variance.
    //
    //   Strictly PD kernels (gaussian, inverse_multiquadric) have phi(0) > 0
    //   and do not need regularisation, but a tiny lambda does not hurt them.
    double phiMax = 0.0;
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            phiMax = std::max(phiMax, std::abs(Phi(i, j)));
    const double lambda = 1.0e-6 * (phiMax > 0.0 ? phiMax : 1.0);

    Eigen::MatrixXd A = Eigen::MatrixXd::Zero(total, total);
    A.topLeftCorner(n, n)         = Phi;
    A.topLeftCorner(n, n).diagonal().array() += lambda;
    A.topRightCorner(n, nPoly)    = P;
    A.bottomLeftCorner(nPoly, n)  = P.transpose();

    // RHS [z; 0]
    Eigen::VectorXd rhs(total);
    for (int i = 0; i < n; ++i) {
        rhs(i) = points[static_cast<std::size_t>(i)].value;
    }
    rhs.tail(nPoly).setZero();

    // Solve. ColPivHouseholderQR is robust for mildly ill-conditioned systems.
    // If rank-deficient even after regularisation, the input is pathological.
    Eigen::ColPivHouseholderQR<Eigen::MatrixXd> qr(A);
    if (qr.rank() < total) {
        throw AlgorithmException(
            "fitRbf: RBF system is rank-deficient even after regularisation "
            "(rank=" + std::to_string(qr.rank())
            + ", expected=" + std::to_string(total) + ", "
            "lambda=" + std::to_string(lambda) + "). "
            "Check for duplicate or collinear input points. "
            "Strictly PD kernels 'inverse_multiquadric' and 'gaussian' "
            "are more robust for irregular point configurations.");
    }
    const Eigen::VectorXd sol = qr.solve(rhs);

    RbfFitResult result;
    result.lambda     = sol.head(n);
    result.polyCoeffs = sol.tail(nPoly);
    result.kernel     = kernel;
    result.epsilon    = epsilon;
    result.polyDegree = polyDeg;
    result.nPts       = n;
    result.xMin       = xMin;
    result.yMin       = yMin;
    result.scale      = scale;

    // Store normalised training coordinates for use in evalRbf.
    result.normX.resize(static_cast<std::size_t>(n));
    result.normY.resize(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        result.normX[static_cast<std::size_t>(i)] = normPts[static_cast<std::size_t>(i)].x;
        result.normY[static_cast<std::size_t>(i)] = normPts[static_cast<std::size_t>(i)].y;
    }

    // Training RMSE and R^2
    double sse   = 0.0;
    double ssTot = 0.0;
    const double zMean = std::accumulate(points.begin(), points.end(), 0.0,
        [](double acc, const SpatialPoint& p) { return acc + p.value; }) / n;

    for (int i = 0; i < n; ++i) {
        const double pred = Phi.row(i).dot(result.lambda)
                          + P.row(i).dot(result.polyCoeffs);
        const double res  = points[static_cast<std::size_t>(i)].value - pred;
        sse   += res * res;
        const double dev = points[static_cast<std::size_t>(i)].value - zMean;
        ssTot += dev * dev;
    }
    result.rmse     = std::sqrt(sse / static_cast<double>(n));
    result.rSquared = (ssTot > 0.0) ? (1.0 - sse / ssTot) : 1.0;

    return result;
}

// -----------------------------------------------------------------------------
//  evalRbf
// -----------------------------------------------------------------------------

double evalRbf(const RbfFitResult&              fit,
               const std::vector<SpatialPoint>& /*pts*/,
               double                           qx,
               double                           qy)
{
    // Normalise query point using the same parameters as the fit.
    const double qxN = (qx - fit.xMin) / fit.scale;
    const double qyN = (qy - fit.yMin) / fit.scale;

    const int n = fit.nPts;
    double val = 0.0;

    // RBF sum using normalised distances.
    for (int i = 0; i < n; ++i) {
        const double dx = qxN - fit.normX[static_cast<std::size_t>(i)];
        const double dy = qyN - fit.normY[static_cast<std::size_t>(i)];
        const double r  = std::sqrt(dx * dx + dy * dy);
        val += fit.lambda(i) * rbfKernelEval(r, fit.kernel, fit.epsilon);
    }

    // Polynomial augmentation uses normalised coordinates.
    val += fit.polyCoeffs(0);
    if (fit.polyDegree >= 1) {
        val += fit.polyCoeffs(1) * qxN;
        val += fit.polyCoeffs(2) * qyN;
    }

    return val;
}

// -----------------------------------------------------------------------------
//  evalRbfGrid
// -----------------------------------------------------------------------------

Eigen::MatrixXd evalRbfGrid(const RbfFitResult&              fit,
                             const std::vector<SpatialPoint>& pts,
                             const GridExtent&                extent)
{
    Eigen::MatrixXd grid(extent.nRows, extent.nCols);
    for (int row = 0; row < extent.nRows; ++row) {
        const double qy = extent.yMin + static_cast<double>(row) * extent.resY;
        for (int col = 0; col < extent.nCols; ++col) {
            const double qx = extent.xMin + static_cast<double>(col) * extent.resX;
            grid(row, col)  = evalRbf(fit, pts, qx, qy);  // pts unused in eval
        }
    }
    return grid;
}

// -----------------------------------------------------------------------------
//  crossValidateRbf
// -----------------------------------------------------------------------------

SpatialCrossValidationResult crossValidateRbf(
    const std::vector<SpatialPoint>& points,
    RbfKernel                        kernel,
    double                           epsilon)
{
    const int n = static_cast<int>(points.size());
    if (n < 4) {
        throw DataException(
            "crossValidateRbf: need at least 4 points, got " + std::to_string(n) + ".");
    }

    SpatialCrossValidationResult cv;
    cv.errors.resize(static_cast<std::size_t>(n));
    cv.stdErrors.resize(static_cast<std::size_t>(n), 0.0);  // no UQ for RBF
    cv.x.resize(static_cast<std::size_t>(n));
    cv.y.resize(static_cast<std::size_t>(n));

    // LOO: refit n times
    for (int i = 0; i < n; ++i) {
        std::vector<SpatialPoint> trainPts;
        trainPts.reserve(static_cast<std::size_t>(n - 1));
        for (int j = 0; j < n; ++j) {
            if (j != i) trainPts.push_back(points[static_cast<std::size_t>(j)]);
        }

        const RbfFitResult fit = fitRbf(trainPts, kernel, epsilon);
        const double pred = evalRbf(fit, trainPts,
                                    points[static_cast<std::size_t>(i)].x,
                                    points[static_cast<std::size_t>(i)].y);

        cv.errors[static_cast<std::size_t>(i)] =
            points[static_cast<std::size_t>(i)].value - pred;
        cv.x[static_cast<std::size_t>(i)] = points[static_cast<std::size_t>(i)].x;
        cv.y[static_cast<std::size_t>(i)] = points[static_cast<std::size_t>(i)].y;
    }

    // Summary statistics
    double sumSq  = 0.0;
    double sumAbs = 0.0;
    for (int i = 0; i < n; ++i) {
        sumSq  += cv.errors[static_cast<std::size_t>(i)]
                * cv.errors[static_cast<std::size_t>(i)];
        sumAbs += std::abs(cv.errors[static_cast<std::size_t>(i)]);
    }
    cv.rmse    = std::sqrt(sumSq / static_cast<double>(n));
    cv.mae     = sumAbs / static_cast<double>(n);
    cv.meanSE  = 0.0;  // no variance model
    cv.meanSSE = 0.0;

    return cv;
}

} // namespace loki::math
