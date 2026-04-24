#pragma once

#include <Eigen/Dense>

#include <string>
#include <vector>

namespace loki::math {

// =============================================================================
//  SpatialPoint
// =============================================================================

/**
 * @brief A single observation at a 2-D spatial location.
 *
 * Coordinate system is user-defined (Cartesian, geographic, projected).
 * loki_spatial does not perform coordinate transformations -- that is the
 * domain of loki_geodesy.
 *
 * For geographic coordinates (lon/lat), the caller is responsible for
 * choosing the appropriate distance metric (Euclidean or haversine) via
 * SpatialDistanceMetric in the config.
 */
struct SpatialPoint {
    double x;      ///< Horizontal coordinate (easting, longitude, or arbitrary X).
    double y;      ///< Vertical coordinate (northing, latitude, or arbitrary Y).
    double value;  ///< Observed scalar value at (x, y).
};

// =============================================================================
//  GridExtent
// =============================================================================

/**
 * @brief Defines the extent and resolution of a regular output grid.
 *
 * nCols = round((xMax - xMin) / resX) + 1
 * nRows = round((yMax - yMin) / resY) + 1
 *
 * All fields with value 0.0 are interpreted as "auto" and computed from
 * input data by the pipeline (median nearest-neighbour distance heuristic).
 */
struct GridExtent {
    double xMin = 0.0; ///< Left boundary of the grid.
    double xMax = 0.0; ///< Right boundary of the grid.
    double yMin = 0.0; ///< Bottom boundary of the grid.
    double yMax = 0.0; ///< Top boundary of the grid.
    double resX = 0.0; ///< Grid spacing in the X direction (0 = auto).
    double resY = 0.0; ///< Grid spacing in the Y direction (0 = auto).
    int    nCols = 0;  ///< Number of columns (derived from extent + resX).
    int    nRows = 0;  ///< Number of rows (derived from extent + resY).
};

// =============================================================================
//  SpatialGrid
// =============================================================================

/**
 * @brief Interpolated regular grid for a single scalar variable.
 *
 * Row index increases in the Y direction (bottom to top).
 * Column index increases in the X direction (left to right).
 *
 * grid(row, col) corresponds to:
 *   x = extent.xMin + col * extent.resX
 *   y = extent.yMin + row * extent.resY
 *
 * variance and CI matrices are populated only when the interpolation
 * method supports uncertainty quantification (Kriging, RBF with LOO).
 * For IDW and bilinear they remain zero-valued.
 */
struct SpatialGrid {
    GridExtent      extent;        ///< Grid geometry (bounds + resolution + dims).
    Eigen::MatrixXd values;        ///< Interpolated values (nRows x nCols).
    Eigen::MatrixXd variance;      ///< Prediction variance (nRows x nCols).
    Eigen::MatrixXd ciLower;       ///< Lower CI bound (nRows x nCols).
    Eigen::MatrixXd ciUpper;       ///< Upper CI bound (nRows x nCols).
    std::string     variableName;  ///< Label for output naming.
};

// =============================================================================
//  SpatialPrediction
// =============================================================================

/**
 * @brief Kriging or RBF estimate at a single spatial location.
 *
 * Used for arbitrary query points (not necessarily on the regular grid).
 * isObserved is true when (x, y) coincides with an input SpatialPoint.
 */
struct SpatialPrediction {
    double x;          ///< Query X coordinate.
    double y;          ///< Query Y coordinate.
    double value;      ///< Estimated value.
    double variance;   ///< Prediction variance (0 for methods without UQ).
    double ciLower;    ///< Lower CI bound.
    double ciUpper;    ///< Upper CI bound.
    bool   isObserved; ///< True if this point coincides with an input observation.
};

// =============================================================================
//  SpatialCrossValidationResult
// =============================================================================

/**
 * @brief Leave-one-out cross-validation diagnostics for spatial interpolation.
 *
 * Mirrors loki::math::CrossValidationResult but uses (x, y) coordinates
 * instead of MJD for spatial localisation.
 *
 * Ideal values:
 *   meanSE  ~= 0  (unbiased estimator)
 *   meanSSE ~= 1  (correctly calibrated variance -- Kriging only)
 */
struct SpatialCrossValidationResult {
    double              rmse;       ///< sqrt( mean(e_i^2) )
    double              mae;        ///< mean( |e_i| )
    double              meanSE;     ///< mean( e_i / sigma_i ) -- bias indicator
    double              meanSSE;    ///< mean( (e_i/sigma_i)^2 ) -- variance calibration
    std::vector<double> errors;     ///< Raw LOO errors e_i = z_i - z_hat_i
    std::vector<double> stdErrors;  ///< Standardised errors (Kriging only; 0 otherwise)
    std::vector<double> x;          ///< X coordinates of LOO points.
    std::vector<double> y;          ///< Y coordinates of LOO points.
};

} // namespace loki::math
