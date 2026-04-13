#pragma once

#include <Eigen/Dense>

#include <string>
#include <vector>

namespace loki::math {

// =============================================================================
//  B-spline basis functions
//
//  Conventions used throughout:
//    p   -- polynomial degree (1 = linear, 2 = quadratic, 3 = cubic, ...)
//    n   -- number of control points  (nCtrl)
//    m+1 -- number of knots           (m = n + p)
//    t   -- parameter values in [0, 1] (normalised)
//
//  Knot vector structure (clamped):
//    k[0] = ... = k[p] = 0   (p+1 repeated zeros)
//    k[p+1] ... k[n-1]       (interior knots)
//    k[n] = ... = k[n+p] = 1 (p+1 repeated ones)
//
//  De Boor recursion (Cox-de Boor formula):
//    N_{i,0}(t) = 1  if k[i] <= t < k[i+1], else 0
//    N_{i,p}(t) = (t - k[i])   / (k[i+p]   - k[i])   * N_{i,p-1}(t)
//               + (k[i+p+1]-t) / (k[i+p+1] - k[i+1]) * N_{i+1,p-1}(t)
//  with the convention 0/0 = 0.
// =============================================================================

// -----------------------------------------------------------------------------
//  Knot vector construction
// -----------------------------------------------------------------------------

/**
 * @brief Build a clamped uniform knot vector for a B-spline of degree p
 *        with nCtrl control points.
 *
 * Interior knots are spaced uniformly in (0, 1).
 * The result has nCtrl + p + 1 elements.
 *
 * @param nCtrl  Number of control points (>= p + 1).
 * @param p      Polynomial degree (>= 1).
 * @return       Clamped knot vector of length nCtrl + p + 1.
 * @throws DataException if nCtrl < p + 1.
 */
std::vector<double> buildUniformKnots(int nCtrl, int p);

/**
 * @brief Build a clamped chord-length knot vector for a B-spline of degree p
 *        with nCtrl control points.
 *
 * The parameter values t (already normalised to [0, 1]) are used to place
 * interior knots where the data is densest. This is the Hartley-Judd
 * averaging method: each interior knot is the mean of p consecutive
 * parameter values in the Greville abscissae.
 *
 * Use chord-length knots when the data sampling is non-uniform (e.g. GPS
 * series with variable observation rates). For uniformly-sampled sensor
 * data the result is nearly identical to uniform knots.
 *
 * @param tNorm  Normalised parameter values in [0, 1], strictly increasing.
 *               Typically set to (i - i_min) / (i_max - i_min).
 * @param nCtrl  Number of control points (>= p + 1).
 * @param p      Polynomial degree (>= 1).
 * @return       Clamped knot vector of length nCtrl + p + 1.
 * @throws DataException if tNorm.size() < nCtrl or nCtrl < p + 1.
 */
std::vector<double> buildChordLengthKnots(const std::vector<double>& tNorm,
                                          int nCtrl, int p);

// -----------------------------------------------------------------------------
//  Basis evaluation
// -----------------------------------------------------------------------------

/**
 * @brief Evaluate all B-spline basis functions at a single parameter value t.
 *
 * Uses the de Boor recursion (Cox-de Boor formula). The result is the row
 * [N_{0,p}(t), N_{1,p}(t), ..., N_{n-1,p}(t)] of the basis matrix.
 *
 * The last knot span [k[n-1], k[n]] is treated as half-open on the right
 * to ensure N(1.0) == 1 for the last basis function.
 *
 * @param t      Parameter value in [0, 1].
 * @param p      Polynomial degree.
 * @param knots  Clamped knot vector (length nCtrl + p + 1).
 * @return       Basis row vector of length nCtrl = knots.size() - p - 1.
 */
std::vector<double> bsplineBasisRow(double t, int p,
                                    const std::vector<double>& knots);

/**
 * @brief Build the full B-spline basis matrix N (nObs x nCtrl).
 *
 * Row i of N contains [N_{0,p}(t[i]), ..., N_{nCtrl-1,p}(t[i])].
 * This matrix is used directly in the LSQ system:
 *   N * c = z    (c = control point values)
 *
 * @param tNorm  Normalised parameter values in [0, 1] (length nObs).
 * @param p      Polynomial degree.
 * @param knots  Clamped knot vector (length nCtrl + p + 1).
 * @return       Dense matrix of shape (nObs x nCtrl).
 * @throws DataException if tNorm is empty or knots has wrong length.
 */
Eigen::MatrixXd bsplineBasisMatrix(const std::vector<double>& tNorm,
                                   int p,
                                   const std::vector<double>& knots);

// -----------------------------------------------------------------------------
//  Evaluation of a fitted B-spline
// -----------------------------------------------------------------------------

/**
 * @brief Evaluate a fitted B-spline at a set of query parameter values.
 *
 * Computes sum_i c[i] * N_{i,p}(tQuery[j]) for each query point.
 *
 * @param tQuery         Query parameter values (normalised to [0, 1]).
 * @param controlPoints  Fitted control point values (length nCtrl).
 * @param p              Polynomial degree.
 * @param knots          Clamped knot vector.
 * @return               Evaluated values, same length as tQuery.
 */
std::vector<double> evalBSpline(const std::vector<double>& tQuery,
                                const std::vector<double>& controlPoints,
                                int p,
                                const std::vector<double>& knots);

// -----------------------------------------------------------------------------
//  Parameter normalisation helper
// -----------------------------------------------------------------------------

/**
 * @brief Normalise a parameter vector to [0, 1].
 *
 * t_norm[i] = (t[i] - t[0]) / (t[n-1] - t[0])
 *
 * For uniformly-sampled data (t = 0, 1, 2, ...) this yields exact uniform
 * spacing. For non-uniform data the spacing is preserved proportionally.
 *
 * @param t  Input parameter values (strictly increasing, length >= 2).
 * @return   Normalised values in [0, 1].
 * @throws DataException if t has fewer than 2 elements or is not increasing.
 */
std::vector<double> normaliseParams(const std::vector<double>& t);

} // namespace loki::math
