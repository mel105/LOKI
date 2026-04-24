#pragma once

#include <loki/math/bspline.hpp>
#include <loki/math/bsplineFit.hpp>
#include <loki/math/spatialTypes.hpp>

#include <Eigen/Dense>

#include <string>
#include <vector>

namespace loki::spatial {

// =============================================================================
//  IDW -- Inverse Distance Weighting
// =============================================================================

/**
 * @brief Inverse Distance Weighting interpolation for 2-D scatter data.
 *
 * Estimate at query point (qx, qy):
 *   Z*(qx, qy) = sum_i [ w_i * z_i ] / sum_i [ w_i ]
 *   w_i = 1 / d_i^power
 *
 * For d_i = 0 (query coincides with observation), returns z_i exactly.
 *
 * IDW is a simple, fast baseline. It does not provide uncertainty estimates.
 *
 * @param points   Input scatter observations.
 * @param qx       Query X coordinate.
 * @param qy       Query Y coordinate.
 * @param power    Distance decay exponent. Typical range [1, 3]. Default 2.
 * @return         Estimated value.
 * @throws DataException if points is empty.
 */
double interpIDW(const std::vector<loki::math::SpatialPoint>& points,
                 double qx, double qy, double power = 2.0);

/**
 * @brief Evaluate IDW on a regular grid.
 *
 * @param points  Input scatter observations.
 * @param extent  Grid geometry.
 * @param power   Distance decay exponent.
 * @return        Matrix of values (nRows x nCols). No variance output.
 */
Eigen::MatrixXd interpIDWGrid(
    const std::vector<loki::math::SpatialPoint>& points,
    const loki::math::GridExtent&                extent,
    double                                        power = 2.0);

/**
 * @brief LOO cross-validation for IDW.
 *
 * @param points  Scatter data.
 * @param power   Distance decay exponent.
 * @return        SpatialCrossValidationResult (stdErrors = 0, no UQ).
 * @throws DataException if fewer than 4 points.
 */
loki::math::SpatialCrossValidationResult crossValidateIDW(
    const std::vector<loki::math::SpatialPoint>& points,
    double                                        power);

// =============================================================================
//  Bilinear interpolation  (regular grids only)
// =============================================================================

/**
 * @brief Bilinear interpolation from a regular input grid to query points.
 *
 * Interpolates values on a regular input grid (nRowsIn x nColsIn) to
 * arbitrary query locations. Standard bilinear formula:
 *   f(x,y) = (1-tx)*(1-ty)*f00 + tx*(1-ty)*f10
 *           + (1-tx)*ty*f01 + tx*ty*f11
 *
 * Queries outside the input grid extent are clamped to the boundary.
 *
 * @param inputGrid   Input values (nRowsIn x nColsIn).
 * @param inputExtent Geometry of the input grid.
 * @param extent      Geometry of the output grid.
 * @return            Interpolated values (nRowsOut x nColsOut).
 * @throws DataException if input grid has fewer than 2 rows or 2 columns.
 */
Eigen::MatrixXd interpBilinear(
    const Eigen::MatrixXd&       inputGrid,
    const loki::math::GridExtent& inputExtent,
    const loki::math::GridExtent& extent);

// =============================================================================
//  Tensor product B-spline surface
// =============================================================================

/**
 * @brief Result of fitting a tensor product B-spline surface to scatter data.
 *
 * The surface is defined as:
 *   S(u, v) = sum_i sum_j c_ij * N_i^p(u) * N_j^q(v)
 *
 * where u and v are normalised parameters in [0, 1] derived from
 * the x and y coordinates of the input points via nearest-knot projection.
 *
 * Note: tensor product B-splines are best suited for quasi-regular input
 * grids. For truly irregular scatter data, RBF thin-plate spline or
 * Natural Neighbor interpolation is generally preferred.
 */
struct BSplineSurfaceResult {
    Eigen::MatrixXd controlPoints; ///< Fitted control point grid (nCtrlU x nCtrlV).
    std::vector<double> knotsU;    ///< Knot vector in U direction.
    std::vector<double> knotsV;    ///< Knot vector in V direction.
    int                 degreeU;   ///< B-spline degree in U.
    int                 degreeV;   ///< B-spline degree in V.
    int                 nCtrlU;    ///< Number of control points in U.
    int                 nCtrlV;    ///< Number of control points in V.
    double              rmse;      ///< Training RMSE.
    double              rSquared;  ///< R^2 on training data.
    std::string         knotPlacement; ///< "uniform" | "chord_length"
};

/**
 * @brief Fit a tensor product B-spline surface to 2-D scatter data.
 *
 * Projects scatter (x, y) to normalised parameters (u, v) via min-max
 * normalisation. Assembles the Kronecker basis matrix N_u (x) N_v and
 * solves the overdetermined LSQ system for the control point grid.
 *
 * @param points       Input scatter observations.
 * @param degreeU      B-spline degree in U (x) direction (1-5).
 * @param degreeV      B-spline degree in V (y) direction (1-5).
 * @param nCtrlU       Number of control points in U (>= degreeU + 1).
 * @param nCtrlV       Number of control points in V (>= degreeV + 1).
 * @param knotPlacement "uniform" | "chord_length"
 * @return             Fitted BSplineSurfaceResult.
 * @throws DataException      if fewer than (nCtrlU * nCtrlV) points.
 * @throws AlgorithmException if the LSQ system is rank-deficient.
 */
BSplineSurfaceResult fitBSplineSurface(
    const std::vector<loki::math::SpatialPoint>& points,
    int                                           degreeU,
    int                                           degreeV,
    int                                           nCtrlU,
    int                                           nCtrlV,
    const std::string&                            knotPlacement = "uniform");

/**
 * @brief Evaluate a fitted B-spline surface at a single query point (qx, qy).
 *
 * Projects (qx, qy) to normalised (u, v) using the same min-max scheme
 * as the fitting step. Clamps u, v to [0, 1] for extrapolation.
 *
 * @param surf  Fitted BSplineSurfaceResult.
 * @param xMin  Original X minimum (from fitting data).
 * @param xMax  Original X maximum.
 * @param yMin  Original Y minimum.
 * @param yMax  Original Y maximum.
 * @param qx    Query X.
 * @param qy    Query Y.
 * @return      Estimated surface value.
 */
double evalBSplineSurface(const BSplineSurfaceResult& surf,
                          double xMin, double xMax,
                          double yMin, double yMax,
                          double qx, double qy);

/**
 * @brief Evaluate a fitted B-spline surface on a regular grid.
 */
Eigen::MatrixXd evalBSplineSurfaceGrid(const BSplineSurfaceResult&  surf,
                                        const loki::math::GridExtent& extent);

// =============================================================================
//  NURBS surface
// =============================================================================

/**
 * @brief Fit a NURBS surface to 2-D scatter data.
 *
 * Non-Uniform Rational B-Splines (NURBS) extend the tensor product B-spline
 * with rational weights w_ij:
 *
 *   S(u,v) = sum_ij (w_ij * P_ij * N_i(u) * N_j(v))
 *            / sum_ij (w_ij * N_i(u) * N_j(v))
 *
 * Weight optimisation via Nelder-Mead is computationally expensive for large
 * grids. For most scientific scatter data (no conic sections needed), the
 * thin-plate spline RBF or B-spline surface provides equivalent accuracy
 * with lower complexity.
 *
 * @param points       Input scatter observations.
 * @param degreeU      NURBS degree in U (1-5).
 * @param degreeV      NURBS degree in V (1-5).
 * @param nCtrlU       Number of control points in U.
 * @param nCtrlV       Number of control points in V.
 * @param knotPlacement "uniform" | "chord_length"
 * @return             BSplineSurfaceResult (weights stored in controlPoints
 *                     denominator -- see implementation).
 * @throws AlgorithmException always in v1 (NURBS not yet implemented).
 */
BSplineSurfaceResult fitNurbsSurface(
    const std::vector<loki::math::SpatialPoint>& points,
    int                                           degreeU,
    int                                           degreeV,
    int                                           nCtrlU,
    int                                           nCtrlV,
    const std::string&                            knotPlacement = "uniform");

} // namespace loki::spatial
