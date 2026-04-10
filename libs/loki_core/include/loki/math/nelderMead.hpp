#pragma once

#include <functional>
#include <vector>

namespace loki::math {

/**
 * @brief Result of a Nelder-Mead minimisation.
 */
struct NelderMeadResult {
    std::vector<double> params;    ///< Best parameter vector found.
    double              fval;      ///< Objective function value at params.
    int                 nIter;     ///< Number of iterations performed.
    bool                converged; ///< True if tolerance criterion was met.
};

/**
 * @brief Minimises a scalar objective function using the Nelder-Mead simplex method.
 *
 * A standard implementation of the Nelder-Mead downhill simplex algorithm
 * (Nelder & Mead 1965). Suitable for smooth, low-dimensional unconstrained
 * minimisation (typically n <= 10 parameters).
 *
 * No gradient information is required. Convergence is declared when the
 * standard deviation of function values across simplex vertices falls below tol.
 *
 * Usage example:
 * @code
 *   auto res = loki::math::nelderMead(
 *       [](const std::vector<double>& p) { return p[0]*p[0] + p[1]*p[1]; },
 *       {1.0, 2.0});
 *   // res.params ~ {0, 0}, res.converged == true
 * @endcode
 *
 * @param f       Objective function to minimise. Must accept a const reference
 *                to std::vector<double> and return a double.
 * @param x0      Initial parameter vector. Must be non-empty.
 * @param maxIter Maximum number of iterations (default: 2000).
 * @param tol     Convergence tolerance on function value spread (default: 1e-8).
 * @param step    Initial simplex step size as a fraction of |x0[i]| (default: 0.05).
 *                For components where x0[i] == 0, an absolute step of 0.00025 is used.
 * @return NelderMeadResult with best params, fval, nIter, converged flag.
 */
NelderMeadResult nelderMead(
    std::function<double(const std::vector<double>&)> f,
    std::vector<double>                               x0,
    int                                               maxIter = 2000,
    double                                            tol     = 1.0e-8,
    double                                            step    = 0.05);

} // namespace loki::math
