#pragma once

#include <loki/math/krigingTypes.hpp>
#include <loki/math/spatialTypes.hpp>
#include <loki/spatial/spatialInterp.hpp>

#include <string>
#include <vector>

namespace loki::spatial {

using loki::math::VariogramPoint;
using loki::math::VariogramFitResult;
using loki::math::SpatialPoint;
using loki::math::SpatialGrid;
using loki::math::SpatialCrossValidationResult;

/**
 * @brief Top-level result container for the spatial interpolation pipeline.
 *
 * Produced by SpatialAnalyzer::run() for one variable and consumed by
 * PlotSpatial and the protocol/CSV writers.
 */
struct SpatialResult {
    std::string  method;        ///< "kriging"|"idw"|"rbf"|"natural_neighbor"|"bspline_surface"|"nurbs"
    std::string  variableName;  ///< Variable label (from CSV header or config).
    int          nObs;          ///< Number of input scatter observations.
    double       meanValue;     ///< Sample mean of observed values.
    double       sampleVariance;///< Sample variance.
    double       xMin, xMax;    ///< Coordinate bounds of input data.
    double       yMin, yMax;

    // -- Kriging-specific fields (empty for other methods) --------------------
    std::vector<VariogramPoint>    empiricalVariogram;
    VariogramFitResult             variogram;

    // -- Grid output ----------------------------------------------------------
    SpatialGrid                    grid;   ///< Interpolated regular grid.

    // -- Cross-validation -----------------------------------------------------
    SpatialCrossValidationResult   crossValidation;

    // -- B-spline surface (populated when method == "bspline_surface") --------
    BSplineSurfaceResult           bsplineSurface;
    bool                           hasBSplineSurface = false;
};

} // namespace loki::spatial
