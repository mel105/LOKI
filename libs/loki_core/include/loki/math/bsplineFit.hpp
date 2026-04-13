#pragma once

#include <loki/math/bspline.hpp>

#include <string>
#include <vector>

namespace loki::math {

// =============================================================================
//  BSplineFitResult
// =============================================================================

/**
 * @brief Result of a B-spline LSQ fit.
 *
 * Stores everything needed to evaluate the fitted spline at arbitrary
 * parameter values and to reproduce the fit diagnostics.
 */
struct BSplineFitResult {
    std::vector<double> controlPoints;  ///< Fitted B-spline control point values (nCtrl).
    std::vector<double> knots;          ///< Full clamped knot vector (nCtrl + p + 1).
    std::vector<double> tNorm;          ///< Normalised parameter values used for fitting [0,1].
    double              tMin = 0.0;     ///< Original parameter minimum (for de-normalisation).
    double              tMax = 1.0;     ///< Original parameter maximum.
    int                 degree = 3;     ///< B-spline polynomial degree.
    int                 nCtrl  = 0;     ///< Number of control points.
    int                 nObs   = 0;     ///< Number of observations used in fit.
    double              rmse   = 0.0;   ///< Root mean squared error on training data.
    double              rSquared = 0.0; ///< Coefficient of determination R^2.
    double              residualStd = 0.0; ///< Std dev of residuals (used for CI band).
    std::string         knotPlacement;  ///< "uniform" | "chord_length"
    bool                isExact = false; ///< True when nCtrl == nObs (interpolation mode).
};

// =============================================================================
//  CvPoint
// =============================================================================

/**
 * @brief A single point on the k-fold cross-validation curve.
 *
 * Used to plot CV RMSE vs number of control points and to select the
 * optimal number of control points automatically.
 */
struct CvPoint {
    int    nCtrl = 0;
    double rmse  = 0.0;
};

// =============================================================================
//  fitBSpline
// =============================================================================

/**
 * @brief Fit a B-spline to (t, z) data using least squares.
 *
 * Constructs the basis matrix N (nObs x nCtrl), then solves N * c = z
 * via ColPivHouseholderQR. For approximation (nCtrl < nObs) this is an
 * overdetermined LSQ. For exact interpolation (nCtrl == nObs) it is a
 * square system.
 *
 * Knot placement:
 *   "uniform"      -- evenly spaced interior knots regardless of data density.
 *   "chord_length" -- Hartley-Judd averaging; denser knots where data changes
 *                     rapidly. Recommended for non-uniformly sampled series.
 *
 * @param t             Original parameter values (strictly increasing).
 *                      Typically sample indices 0, 1, 2, ... for sensor data.
 * @param z             Observation values, same size as t.
 * @param degree        B-spline degree (1-5).
 * @param nCtrl         Number of control points. Must be >= degree + 1.
 *                      nCtrl == t.size() triggers exact interpolation mode.
 * @param knotPlacement "uniform" or "chord_length".
 * @return              Populated BSplineFitResult.
 * @throws DataException       if t.size() != z.size() or inputs are too short.
 * @throws ConfigException     if degree or nCtrl are out of range.
 * @throws AlgorithmException  if the LSQ system is rank-deficient.
 */
BSplineFitResult fitBSpline(const std::vector<double>& t,
                            const std::vector<double>& z,
                            int degree,
                            int nCtrl,
                            const std::string& knotPlacement);

// =============================================================================
//  crossValidateBSpline
// =============================================================================

/**
 * @brief Sweep nCtrl from nCtrlMin to nCtrlMax and compute k-fold CV RMSE.
 *
 * For each candidate nCtrl, k-fold cross-validation is performed:
 *   - The data is split into k approximately equal folds.
 *   - For each fold: train on the remaining k-1 folds, predict on the fold.
 *   - CV RMSE = sqrt( mean( (z_held - z_pred)^2 ) ) across all held-out points.
 *
 * The CV curve typically decreases steeply then levels off (bias-variance
 * trade-off). selectOptimalNCtrl() finds the elbow of this curve.
 *
 * Performance note: for n=1500, nCtrlMax=200, k=5, the total number of
 * LSQ solves is (nCtrlMax - nCtrlMin + 1) * k ~ 1000. Each solve is
 * O(n * nCtrl^2) via QR. This is fast enough for interactive use.
 * For n=36524 the cap of 200 keeps this tractable.
 *
 * @param t          Original parameter values.
 * @param z          Observation values.
 * @param degree     B-spline degree.
 * @param nCtrlMin   Minimum control points to try (>= degree + 1).
 * @param nCtrlMax   Maximum control points to try (<= t.size()).
 * @param knotPlacement "uniform" or "chord_length".
 * @param folds      Number of CV folds (k >= 2).
 * @return           Vector of CvPoint sorted by ascending nCtrl.
 * @throws DataException if inputs are invalid.
 * @throws ConfigException if nCtrlMin/nCtrlMax are out of range.
 */
std::vector<CvPoint> crossValidateBSpline(const std::vector<double>& t,
                                          const std::vector<double>& z,
                                          int degree,
                                          int nCtrlMin,
                                          int nCtrlMax,
                                          const std::string& knotPlacement,
                                          int folds);

// =============================================================================
//  selectOptimalNCtrl
// =============================================================================

/**
 * @brief Select the optimal number of control points from a CV curve.
 *
 * Uses the "one-standard-error rule" elbow heuristic:
 *   1. Find the minimum CV RMSE (best fit).
 *   2. Compute the standard deviation of CV RMSE values across the curve.
 *   3. Select the *smallest* nCtrl whose CV RMSE <= minRmse + 1 * stdRmse.
 *      This prefers a simpler (more parsimonious) model when the CV
 *      improvement from adding more control points is within noise.
 *
 * @param cv   CV curve from crossValidateBSpline(). Must not be empty.
 * @return     Optimal nCtrl value.
 * @throws DataException if cv is empty.
 */
int selectOptimalNCtrl(const std::vector<CvPoint>& cv);

} // namespace loki::math
