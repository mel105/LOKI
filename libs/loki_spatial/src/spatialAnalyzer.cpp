#include <loki/spatial/spatialAnalyzer.hpp>

#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/math/rbf.hpp>
#include <loki/math/naturalNeighbor.hpp>
#include <loki/math/spatialVariogram.hpp>
#include <loki/spatial/spatialInterp.hpp>
#include <loki/spatial/spatialKrigingFactory.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>

using namespace loki;

namespace loki::spatial {

// =============================================================================
//  Construction
// =============================================================================

SpatialAnalyzer::SpatialAnalyzer(const AppConfig& cfg)
    : m_cfg(cfg)
{}

// =============================================================================
//  run
// =============================================================================

SpatialResult SpatialAnalyzer::run(
    const std::vector<loki::math::SpatialPoint>& points,
    const std::string&                            datasetName,
    const std::string&                            varName)
{
    const auto& scfg = m_cfg.spatial;

    LOKI_INFO("SpatialAnalyzer: method='" + scfg.method
              + "'  variable='" + varName + "'  n=" + std::to_string(points.size()));

    if (points.size() < 3) {
        throw DataException(
            "SpatialAnalyzer: need at least 3 input points, got "
            + std::to_string(points.size()) + ".");
    }

    const loki::math::GridExtent extent = _buildGridExtent(points);

    LOKI_INFO("SpatialAnalyzer: grid " + std::to_string(extent.nCols)
              + " x " + std::to_string(extent.nRows)
              + "  resX=" + std::to_string(extent.resX)
              + "  resY=" + std::to_string(extent.resY));

    SpatialResult result;
    if      (scfg.method == "kriging")          result = _runKriging        (points, extent, varName);
    else if (scfg.method == "idw")              result = _runIDW            (points, extent, varName);
    else if (scfg.method == "rbf")              result = _runRBF            (points, extent, varName);
    else if (scfg.method == "natural_neighbor") result = _runNaturalNeighbor(points, extent, varName);
    else if (scfg.method == "bspline_surface")  result = _runBSplineSurface (points, extent, varName);
    else if (scfg.method == "nurbs") {
        fitNurbsSurface(points, 3, 3, 4, 4);  // always throws AlgorithmException
    } else {
        throw ConfigException(
            "SpatialAnalyzer: unrecognised method '" + scfg.method + "'. "
            "Valid: kriging, idw, rbf, natural_neighbor, bspline_surface, nurbs.");
    }

    result.grid.variableName = varName;

    _writeProtocol(result, datasetName);
    _writeCsv     (result, datasetName);

    return result;
}

// =============================================================================
//  Grid extent
// =============================================================================

double SpatialAnalyzer::_estimateResolution(
    const std::vector<loki::math::SpatialPoint>& points)
{
    const int n = static_cast<int>(points.size());
    std::vector<double> nnDist;
    nnDist.reserve(static_cast<std::size_t>(n));

    for (int i = 0; i < n; ++i) {
        double minD = std::numeric_limits<double>::max();
        for (int j = 0; j < n; ++j) {
            if (i == j) continue;
            const double dx = points[static_cast<std::size_t>(i)].x
                            - points[static_cast<std::size_t>(j)].x;
            const double dy = points[static_cast<std::size_t>(i)].y
                            - points[static_cast<std::size_t>(j)].y;
            minD = std::min(minD, std::sqrt(dx*dx + dy*dy));
        }
        if (minD < std::numeric_limits<double>::max()) nnDist.push_back(minD);
    }

    if (nnDist.empty()) return 1.0;

    std::sort(nnDist.begin(), nnDist.end());
    const double median = nnDist[nnDist.size() / 2];
    return std::max(median * 0.5, 1.0e-10);
}

loki::math::GridExtent SpatialAnalyzer::_buildGridExtent(
    const std::vector<loki::math::SpatialPoint>& points) const
{
    const auto& gcfg = m_cfg.spatial.grid;

    // Determine bounds from data if not configured.
    double xMin = points[0].x, xMax = points[0].x;
    double yMin = points[0].y, yMax = points[0].y;
    for (const auto& p : points) {
        xMin = std::min(xMin, p.x); xMax = std::max(xMax, p.x);
        yMin = std::min(yMin, p.y); yMax = std::max(yMax, p.y);
    }

    loki::math::GridExtent ext;
    ext.resX = (gcfg.resolutionX > 0.0) ? gcfg.resolutionX : _estimateResolution(points);
    ext.resY = (gcfg.resolutionY > 0.0) ? gcfg.resolutionY : ext.resX;

    // Apply a small padding = 1 * resolution on each side for auto bounds.
    const double padX = ext.resX;
    const double padY = ext.resY;

    ext.xMin = (gcfg.xMin < gcfg.xMax) ? gcfg.xMin : xMin - padX;
    ext.xMax = (gcfg.xMin < gcfg.xMax) ? gcfg.xMax : xMax + padX;
    ext.yMin = (gcfg.yMin < gcfg.yMax) ? gcfg.yMin : yMin - padY;
    ext.yMax = (gcfg.yMin < gcfg.yMax) ? gcfg.yMax : yMax + padY;

    ext.nCols = std::max(2, static_cast<int>(std::round((ext.xMax - ext.xMin) / ext.resX)) + 1);
    ext.nRows = std::max(2, static_cast<int>(std::round((ext.yMax - ext.yMin) / ext.resY)) + 1);

    return ext;
}

// =============================================================================
//  Statistics
// =============================================================================

void SpatialAnalyzer::_computeStats(
    const std::vector<loki::math::SpatialPoint>& points,
    double& mean, double& var)
{
    const double n = static_cast<double>(points.size());
    mean = 0.0;
    for (const auto& p : points) mean += p.value;
    mean /= n;

    var = 0.0;
    for (const auto& p : points) {
        const double d = p.value - mean;
        var += d * d;
    }
    var /= (n > 1.0 ? n - 1.0 : 1.0);
}

// =============================================================================
//  _runKriging
// =============================================================================

SpatialResult SpatialAnalyzer::_runKriging(
    const std::vector<loki::math::SpatialPoint>& points,
    const loki::math::GridExtent&                extent,
    const std::string&                            varName) const
{
    const auto& scfg = m_cfg.spatial;

    SpatialResult result;
    result.method = "kriging";
    result.variableName = varName;
    result.nObs = static_cast<int>(points.size());
    _computeStats(points, result.meanValue, result.sampleVariance);
    result.xMin = extent.xMin; result.xMax = extent.xMax;
    result.yMin = extent.yMin; result.yMax = extent.yMax;

    // Variogram
    result.empiricalVariogram = loki::math::computeSpatialVariogram(
        points, scfg.kriging.variogram.nLagBins, scfg.kriging.variogram.maxLag);

    LOKI_INFO("SpatialAnalyzer: fitting variogram model='"
              + scfg.kriging.variogram.model + "'");
    result.variogram = loki::math::fitSpatialVariogram(
        result.empiricalVariogram, scfg.kriging.variogram);

    LOKI_INFO("SpatialAnalyzer: variogram fit  nugget="
              + std::to_string(result.variogram.nugget)
              + "  sill="    + std::to_string(result.variogram.sill)
              + "  range="   + std::to_string(result.variogram.range)
              + "  rmse="    + std::to_string(result.variogram.rmse));

    // Kriging fit
    auto kriging = createSpatialKriging(scfg);
    kriging->fit(points, result.variogram);

    // Grid prediction
    result.grid = kriging->predictGrid(extent, scfg.confidenceLevel);

    // Cross-validation
    if (scfg.crossValidate) {
        LOKI_INFO("SpatialAnalyzer: running LOO cross-validation");
        result.crossValidation = kriging->crossValidate(scfg.confidenceLevel);
        LOKI_INFO("SpatialAnalyzer: CV RMSE=" + std::to_string(result.crossValidation.rmse)
                  + "  MAE=" + std::to_string(result.crossValidation.mae));
    }

    return result;
}

// =============================================================================
//  _runIDW
// =============================================================================

SpatialResult SpatialAnalyzer::_runIDW(
    const std::vector<loki::math::SpatialPoint>& points,
    const loki::math::GridExtent&                extent,
    const std::string&                            varName) const
{
    const auto& scfg = m_cfg.spatial;

    SpatialResult result;
    result.method = "idw";
    result.variableName = varName;
    result.nObs = static_cast<int>(points.size());
    _computeStats(points, result.meanValue, result.sampleVariance);
    result.xMin = extent.xMin; result.xMax = extent.xMax;
    result.yMin = extent.yMin; result.yMax = extent.yMax;

    result.grid.extent  = extent;
    result.grid.values  = interpIDWGrid(points, extent, scfg.idw.power);
    result.grid.variance = Eigen::MatrixXd::Zero(extent.nRows, extent.nCols);
    result.grid.ciLower  = Eigen::MatrixXd::Zero(extent.nRows, extent.nCols);
    result.grid.ciUpper  = Eigen::MatrixXd::Zero(extent.nRows, extent.nCols);

    if (scfg.crossValidate) {
        result.crossValidation = crossValidateIDW(points, scfg.idw.power);
        LOKI_INFO("SpatialAnalyzer: IDW CV RMSE=" + std::to_string(result.crossValidation.rmse));
    }

    return result;
}

// =============================================================================
//  _runRBF
// =============================================================================

SpatialResult SpatialAnalyzer::_runRBF(
    const std::vector<loki::math::SpatialPoint>& points,
    const loki::math::GridExtent&                extent,
    const std::string&                            varName) const
{
    const auto& scfg = m_cfg.spatial;
    const loki::math::RbfKernel kernel = loki::math::parseRbfKernel(scfg.rbf.kernel);

    // Auto epsilon: 1 / (sqrt(n) * medianNNdist)
    double epsilon = scfg.rbf.epsilon;
    if (epsilon <= 0.0) {
        const double res = _estimateResolution(points);
        epsilon = 1.0 / (std::sqrt(static_cast<double>(points.size())) * res);
        LOKI_INFO("SpatialAnalyzer: RBF auto epsilon=" + std::to_string(epsilon));
    }

    SpatialResult result;
    result.method = "rbf";
    result.variableName = varName;
    result.nObs = static_cast<int>(points.size());
    _computeStats(points, result.meanValue, result.sampleVariance);
    result.xMin = extent.xMin; result.xMax = extent.xMax;
    result.yMin = extent.yMin; result.yMax = extent.yMax;

    const loki::math::RbfFitResult fit = loki::math::fitRbf(points, kernel, epsilon);
    LOKI_INFO("SpatialAnalyzer: RBF kernel='" + scfg.rbf.kernel
              + "'  RMSE=" + std::to_string(fit.rmse)
              + "  R2=" + std::to_string(fit.rSquared));

    result.grid.extent   = extent;
    result.grid.values   = loki::math::evalRbfGrid(fit, points, extent);
    result.grid.variance = Eigen::MatrixXd::Zero(extent.nRows, extent.nCols);
    result.grid.ciLower  = Eigen::MatrixXd::Zero(extent.nRows, extent.nCols);
    result.grid.ciUpper  = Eigen::MatrixXd::Zero(extent.nRows, extent.nCols);

    if (scfg.crossValidate) {
        result.crossValidation = loki::math::crossValidateRbf(points, kernel, epsilon);
        LOKI_INFO("SpatialAnalyzer: RBF CV RMSE=" + std::to_string(result.crossValidation.rmse));
    }

    return result;
}

// =============================================================================
//  _runNaturalNeighbor
// =============================================================================

SpatialResult SpatialAnalyzer::_runNaturalNeighbor(
    const std::vector<loki::math::SpatialPoint>& points,
    const loki::math::GridExtent&                extent,
    const std::string&                            varName) const
{
    const auto& scfg = m_cfg.spatial;

    SpatialResult result;
    result.method = "natural_neighbor";
    result.variableName = varName;
    result.nObs = static_cast<int>(points.size());
    _computeStats(points, result.meanValue, result.sampleVariance);
    result.xMin = extent.xMin; result.xMax = extent.xMax;
    result.yMin = extent.yMin; result.yMax = extent.yMax;

    loki::math::NaturalNeighborInterpolator nn(points);

    result.grid.extent   = extent;
    result.grid.values   = nn.interpolateGrid(extent);
    result.grid.variance = Eigen::MatrixXd::Zero(extent.nRows, extent.nCols);
    result.grid.ciLower  = Eigen::MatrixXd::Zero(extent.nRows, extent.nCols);
    result.grid.ciUpper  = Eigen::MatrixXd::Zero(extent.nRows, extent.nCols);

    if (scfg.crossValidate) {
        result.crossValidation = nn.crossValidate();
        LOKI_INFO("SpatialAnalyzer: NN CV RMSE=" + std::to_string(result.crossValidation.rmse));
    }

    return result;
}

// =============================================================================
//  _runBSplineSurface
// =============================================================================

SpatialResult SpatialAnalyzer::_runBSplineSurface(
    const std::vector<loki::math::SpatialPoint>& points,
    const loki::math::GridExtent&                extent,
    const std::string&                            varName) const
{
    const auto& scfg = m_cfg.spatial;
    const auto& bcfg = scfg.bsplineSurface;

    SpatialResult result;
    result.method = "bspline_surface";
    result.variableName = varName;
    result.nObs = static_cast<int>(points.size());
    _computeStats(points, result.meanValue, result.sampleVariance);
    result.xMin = extent.xMin; result.xMax = extent.xMax;
    result.yMin = extent.yMin; result.yMax = extent.yMax;

    const BSplineSurfaceResult surf = fitBSplineSurface(
        points,
        bcfg.degreeU, bcfg.degreeV,
        bcfg.nCtrlU,  bcfg.nCtrlV,
        bcfg.knotPlacement);

    LOKI_INFO("SpatialAnalyzer: B-spline surface fit  nCtrl="
              + std::to_string(bcfg.nCtrlU) + "x" + std::to_string(bcfg.nCtrlV)
              + "  RMSE=" + std::to_string(surf.rmse)
              + "  R2="   + std::to_string(surf.rSquared));

    result.bsplineSurface    = surf;
    result.hasBSplineSurface = true;

    result.grid.extent  = extent;
    result.grid.values  = evalBSplineSurfaceGrid(surf, extent);
    result.grid.variance = Eigen::MatrixXd::Zero(extent.nRows, extent.nCols);
    result.grid.ciLower  = Eigen::MatrixXd::Zero(extent.nRows, extent.nCols);
    result.grid.ciUpper  = Eigen::MatrixXd::Zero(extent.nRows, extent.nCols);

    // LOO CV for B-spline surface: refit n times.
    if (scfg.crossValidate) {
        const int n = static_cast<int>(points.size());
        loki::math::SpatialCrossValidationResult cv;
        cv.errors.resize(static_cast<std::size_t>(n));
        cv.stdErrors.resize(static_cast<std::size_t>(n), 0.0);
        cv.x.resize(static_cast<std::size_t>(n));
        cv.y.resize(static_cast<std::size_t>(n));

        double sumSq = 0.0, sumAbs = 0.0;
        for (int i = 0; i < n; ++i) {
            std::vector<loki::math::SpatialPoint> trainPts;
            trainPts.reserve(static_cast<std::size_t>(n - 1));
            for (int j = 0; j < n; ++j) {
                if (j != i) trainPts.push_back(points[static_cast<std::size_t>(j)]);
            }
            // Compute x/y bounds from training set for consistent normalisation.
            double xMinT = trainPts[0].x, xMaxT = trainPts[0].x;
            double yMinT = trainPts[0].y, yMaxT = trainPts[0].y;
            for (const auto& p : trainPts) {
                xMinT = std::min(xMinT, p.x); xMaxT = std::max(xMaxT, p.x);
                yMinT = std::min(yMinT, p.y); yMaxT = std::max(yMaxT, p.y);
            }
            const BSplineSurfaceResult looSurf = fitBSplineSurface(
                trainPts, bcfg.degreeU, bcfg.degreeV,
                bcfg.nCtrlU, bcfg.nCtrlV, bcfg.knotPlacement);
            const double pred = evalBSplineSurface(looSurf,
                xMinT, xMaxT, yMinT, yMaxT,
                points[static_cast<std::size_t>(i)].x,
                points[static_cast<std::size_t>(i)].y);
            const double err = points[static_cast<std::size_t>(i)].value - pred;
            cv.errors[static_cast<std::size_t>(i)] = err;
            cv.x     [static_cast<std::size_t>(i)] = points[static_cast<std::size_t>(i)].x;
            cv.y     [static_cast<std::size_t>(i)] = points[static_cast<std::size_t>(i)].y;
            sumSq  += err * err;
            sumAbs += std::abs(err);
        }
        const double nd = static_cast<double>(n);
        cv.rmse    = std::sqrt(sumSq / nd);
        cv.mae     = sumAbs / nd;
        cv.meanSE  = 0.0;
        cv.meanSSE = 0.0;
        result.crossValidation = cv;
        LOKI_INFO("SpatialAnalyzer: B-spline CV RMSE=" + std::to_string(cv.rmse));
    }

    return result;
}

// =============================================================================
//  _writeProtocol
// =============================================================================

void SpatialAnalyzer::_writeProtocol(const SpatialResult& result,
                                      const std::string&   datasetName) const
{
    const std::filesystem::path path =
        m_cfg.protocolsDir / ("spatial_" + datasetName + "_"
                              + result.variableName + "_protocol.txt");
    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        LOKI_WARNING("SpatialAnalyzer: cannot write protocol to " + path.string());
        return;
    }

    ofs << std::fixed << std::setprecision(6);
    ofs << "==========================================================\n";
    ofs << "  LOKI spatial interpolation protocol\n";
    ofs << "==========================================================\n\n";
    ofs << "Dataset        : " << datasetName << "\n";
    ofs << "Variable       : " << result.variableName << "\n";
    ofs << "Method         : " << result.method << "\n";
    ofs << "Observations   : " << result.nObs << "\n";
    ofs << "Mean value     : " << result.meanValue << "\n";
    ofs << "Sample var     : " << result.sampleVariance << "\n";
    ofs << "X range        : [" << result.xMin << ", " << result.xMax << "]\n";
    ofs << "Y range        : [" << result.yMin << ", " << result.yMax << "]\n\n";

    ofs << "----------------------------------------------------------\n";
    ofs << "  Grid\n";
    ofs << "----------------------------------------------------------\n";
    const auto& ext = result.grid.extent;
    ofs << "Grid size      : " << ext.nCols << " x " << ext.nRows << "\n";
    ofs << "Resolution X   : " << ext.resX << "\n";
    ofs << "Resolution Y   : " << ext.resY << "\n\n";

    if (result.method == "kriging") {
        ofs << "----------------------------------------------------------\n";
        ofs << "  Variogram\n";
        ofs << "----------------------------------------------------------\n";
        ofs << "Model          : " << result.variogram.model << "\n";
        ofs << "Nugget         : " << result.variogram.nugget << "\n";
        ofs << "Sill           : " << result.variogram.sill << "\n";
        ofs << "Range          : " << result.variogram.range << "\n";
        ofs << "Fit RMSE       : " << result.variogram.rmse << "\n";
        ofs << "Converged      : " << (result.variogram.converged ? "yes" : "no") << "\n\n";
    }

    if (result.hasBSplineSurface) {
        ofs << "----------------------------------------------------------\n";
        ofs << "  B-spline surface\n";
        ofs << "----------------------------------------------------------\n";
        ofs << "Degree (U x V) : " << result.bsplineSurface.degreeU
            << " x " << result.bsplineSurface.degreeV << "\n";
        ofs << "Control (U x V): " << result.bsplineSurface.nCtrlU
            << " x " << result.bsplineSurface.nCtrlV << "\n";
        ofs << "Knot placement : " << result.bsplineSurface.knotPlacement << "\n";
        ofs << "Train RMSE     : " << result.bsplineSurface.rmse << "\n";
        ofs << "R-squared      : " << result.bsplineSurface.rSquared << "\n\n";
    }

    if (!result.crossValidation.errors.empty()) {
        ofs << "----------------------------------------------------------\n";
        ofs << "  Leave-one-out cross-validation\n";
        ofs << "----------------------------------------------------------\n";
        ofs << "CV RMSE        : " << result.crossValidation.rmse << "\n";
        ofs << "CV MAE         : " << result.crossValidation.mae  << "\n";
        if (result.method == "kriging") {
            ofs << "Mean std error : " << result.crossValidation.meanSE  << "\n";
            ofs << "Mean sq std err: " << result.crossValidation.meanSSE << "\n";
        }
        ofs << "\n";
    }

    ofs << "==========================================================\n";
    LOKI_INFO("Protocol written: " + path.string());
}

// =============================================================================
//  _writeCsv
// =============================================================================

void SpatialAnalyzer::_writeCsv(const SpatialResult& result,
                                 const std::string&   datasetName) const
{
    // Write grid as CSV: x;y;value;variance (if available)
    const std::filesystem::path path =
        m_cfg.csvDir / ("spatial_" + datasetName + "_"
                        + result.variableName + "_grid.csv");
    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        LOKI_WARNING("SpatialAnalyzer: cannot write CSV to " + path.string());
        return;
    }

    const bool hasVar = (result.method == "kriging");
    ofs << std::fixed << std::setprecision(9);
    if (hasVar) ofs << "x;y;value;variance;ci_lower;ci_upper\n";
    else        ofs << "x;y;value\n";

    const auto& ext = result.grid.extent;
    for (int row = 0; row < ext.nRows; ++row) {
        const double qy = ext.yMin + static_cast<double>(row) * ext.resY;
        for (int col = 0; col < ext.nCols; ++col) {
            const double qx = ext.xMin + static_cast<double>(col) * ext.resX;
            ofs << qx << ";" << qy << ";" << result.grid.values(row, col);
            if (hasVar) {
                ofs << ";" << result.grid.variance(row, col)
                    << ";" << result.grid.ciLower(row, col)
                    << ";" << result.grid.ciUpper(row, col);
            }
            ofs << "\n";
        }
    }

    LOKI_INFO("Grid CSV written: " + path.string());

    // Write cross-validation CSV if available.
    if (!result.crossValidation.errors.empty()) {
        const std::filesystem::path cvPath =
            m_cfg.csvDir / ("spatial_" + datasetName + "_"
                            + result.variableName + "_cv.csv");
        std::ofstream cofs(cvPath);
        if (cofs.is_open()) {
            cofs << "x;y;error";
            if (result.method == "kriging") cofs << ";std_error";
            cofs << "\n";
            cofs << std::fixed << std::setprecision(9);
            const int n = static_cast<int>(result.crossValidation.errors.size());
            for (int i = 0; i < n; ++i) {
                const std::size_t si = static_cast<std::size_t>(i);
                cofs << result.crossValidation.x[si] << ";"
                     << result.crossValidation.y[si] << ";"
                     << result.crossValidation.errors[si];
                if (result.method == "kriging")
                    cofs << ";" << result.crossValidation.stdErrors[si];
                cofs << "\n";
            }
            LOKI_INFO("CV CSV written: " + cvPath.string());
        }
    }
}

} // namespace loki::spatial
