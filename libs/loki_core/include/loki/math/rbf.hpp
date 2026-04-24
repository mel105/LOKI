#pragma once

#include <loki/math/spatialTypes.hpp>

#include <Eigen/Dense>

#include <string>
#include <vector>

namespace loki::math {

// =============================================================================
//  RBF kernel types
// =============================================================================

/**
 * @brief Supported radial basis function kernels.
 *
 * Given a Euclidean distance r between two points, each kernel defines
 * a basis function phi(r) used to construct the interpolant:
 *
 *   s(x, y) = sum_i lambda_i * phi( ||(x,y) - (xi,yi)|| ) + P(x,y)
 *
 * where P(x,y) is a low-degree polynomial term for global trend removal
 * (required for conditionally positive definite kernels).
 *
 * MULTIQUADRIC:
 *   phi(r) = sqrt(r^2 + epsilon^2)
 *   Conditionally positive definite of order 1. Good all-round performance.
 *   Shape parameter epsilon controls the flatness (larger = flatter).
 *
 * INVERSE_MULTIQUADRIC:
 *   phi(r) = 1 / sqrt(r^2 + epsilon^2)
 *   Strictly positive definite. Decays to zero at large r. May underfit
 *   at observation locations far from others.
 *
 * GAUSSIAN:
 *   phi(r) = exp(-epsilon^2 * r^2)
 *   Strictly positive definite. Very smooth interpolant. Ill-conditioned
 *   for small epsilon and dense point clouds.
 *
 * THIN_PLATE_SPLINE:
 *   phi(r) = r^2 * log(r)  (with phi(0) = 0 by convention)
 *   Natural 2-D extension of cubic spline. Minimises the bending energy
 *   integral. No shape parameter needed. Recommended for scientific scatter
 *   data interpolation. Requires a degree-1 polynomial augmentation.
 *
 * CUBIC:
 *   phi(r) = r^3
 *   Conditionally positive definite of order 2. Requires degree-1 polynomial
 *   augmentation. Simpler alternative to thin plate spline.
 */
enum class RbfKernel {
    MULTIQUADRIC,
    INVERSE_MULTIQUADRIC,
    GAUSSIAN,
    THIN_PLATE_SPLINE,
    CUBIC
};

// =============================================================================
//  RbfFitResult
// =============================================================================

/**
 * @brief Result of fitting an RBF interpolant to scatter data.
 */
struct RbfFitResult {
    Eigen::VectorXd lambda;      ///< RBF coefficients (nPts x 1).
    Eigen::VectorXd polyCoeffs;  ///< Polynomial augmentation coefficients.
    RbfKernel       kernel;      ///< Kernel type used.
    double          epsilon;     ///< Shape parameter (0 for TPS and Cubic).
    int             polyDegree;  ///< Polynomial augmentation degree (0 or 1).
    double          rmse;        ///< Training RMSE.
    double          rSquared;    ///< Coefficient of determination R^2.
    int             nPts;        ///< Number of input points.
    // Coordinate normalisation parameters (applied internally during fit).
    // evalRbf uses these to normalise query points consistently.
    double          xMin  = 0.0; ///< X minimum of training data.
    double          yMin  = 0.0; ///< Y minimum of training data.
    double          scale = 1.0; ///< Uniform scale: max(xRange, yRange).
    // Normalised training point coordinates (for eval distance computation).
    std::vector<double> normX; ///< Normalised X of training points.
    std::vector<double> normY; ///< Normalised Y of training points.
};

// =============================================================================
//  Kernel evaluation helpers
// =============================================================================

/**
 * @brief Parse kernel name string to RbfKernel enum.
 *
 * Accepted strings: "multiquadric", "inverse_multiquadric", "gaussian",
 *                   "thin_plate_spline", "cubic".
 * @throws ConfigException if name is not recognised.
 */
RbfKernel parseRbfKernel(const std::string& name);

/**
 * @brief Evaluate a single RBF kernel at distance r.
 *
 * @param r       Euclidean distance (>= 0).
 * @param kernel  Kernel type.
 * @param epsilon Shape parameter (ignored for TPS and Cubic).
 * @return        phi(r).
 */
double rbfKernelEval(double r, RbfKernel kernel, double epsilon);

/**
 * @brief Returns the required polynomial augmentation degree for a kernel.
 *
 * MULTIQUADRIC       : 1  (conditionally PD order 1)
 * INVERSE_MULTIQUADRIC: 0 (strictly PD)
 * GAUSSIAN           : 0  (strictly PD)
 * THIN_PLATE_SPLINE  : 1  (conditionally PD order 2)
 * CUBIC              : 1  (conditionally PD order 2)
 */
int rbfPolyDegree(RbfKernel kernel);

// =============================================================================
//  RBF fitting
// =============================================================================

/**
 * @brief Fit an RBF interpolant to 2-D scatter data.
 *
 * Assembles and solves the symmetric RBF system:
 *
 *   [ Phi  P ] [ lambda ]   [ z ]
 *   [ P^T  0 ] [ poly   ] = [ 0 ]
 *
 * where Phi_ij = phi(||(xi,yi) - (xj,yj)||) and P is the polynomial
 * augmentation matrix (constant + linear terms for polyDegree = 1).
 *
 * Solved via ColPivHouseholderQR. For strictly PD kernels (Gaussian,
 * inverse multiquadric) the system is always well-conditioned. For
 * conditionally PD kernels (TPS, multiquadric, cubic) the polynomial
 * term ensures a unique solution when points are in general position.
 *
 * @param points   Input scatter observations.
 * @param kernel   RBF kernel type.
 * @param epsilon  Shape parameter. Ignored for TPS and Cubic (pass 0.0).
 *                 A common heuristic for auto-selection is
 *                 epsilon = 1 / (sqrt(n) * d_avg) where d_avg is the
 *                 mean nearest-neighbour distance.
 * @return         Fitted RbfFitResult.
 * @throws DataException       if fewer than 3 points.
 * @throws AlgorithmException  if the RBF system is rank-deficient.
 */
RbfFitResult fitRbf(const std::vector<SpatialPoint>& points,
                    RbfKernel                         kernel,
                    double                            epsilon);

// =============================================================================
//  RBF evaluation
// =============================================================================

/**
 * @brief Evaluate a fitted RBF interpolant at a single query point.
 *
 * @param fit    Result from fitRbf().
 * @param pts    Original input points (needed for distance computation).
 * @param qx     Query X coordinate.
 * @param qy     Query Y coordinate.
 * @return       Estimated value at (qx, qy).
 */
double evalRbf(const RbfFitResult&              fit,
               const std::vector<SpatialPoint>& pts,
               double                           qx,
               double                           qy);

/**
 * @brief Evaluate a fitted RBF interpolant on a regular grid.
 *
 * @param fit    Result from fitRbf().
 * @param pts    Original input points.
 * @param extent Grid geometry (must have nRows, nCols, resX, resY, xMin, yMin set).
 * @return       Matrix of estimated values (nRows x nCols).
 */
Eigen::MatrixXd evalRbfGrid(const RbfFitResult&              fit,
                             const std::vector<SpatialPoint>& pts,
                             const GridExtent&                extent);

// =============================================================================
//  LOO cross-validation for RBF
// =============================================================================

/**
 * @brief Leave-one-out cross-validation for the RBF interpolant.
 *
 * Refit the RBF n times, each time leaving one point out and predicting it.
 * For small n (< 500) this is tractable. For large n prefer the approximate
 * LOO via the influence matrix (not implemented in v1).
 *
 * @param points   Input scatter data.
 * @param kernel   RBF kernel.
 * @param epsilon  Shape parameter.
 * @return         SpatialCrossValidationResult (stdErrors are zero -- no UQ).
 * @throws DataException if fewer than 4 points.
 */
SpatialCrossValidationResult crossValidateRbf(
    const std::vector<SpatialPoint>& points,
    RbfKernel                        kernel,
    double                           epsilon);

} // namespace loki::math
