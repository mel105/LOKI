#include "loki/math/nelderMead.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace loki::math {

// Reflection, expansion, contraction, shrink coefficients (standard values).
static constexpr double ALPHA = 1.0;   // reflection
static constexpr double GAMMA = 2.0;   // expansion
static constexpr double RHO   = 0.5;   // contraction
static constexpr double SIGMA = 0.5;   // shrink

// Helper: centroid of the n best vertices (excluding worst).
static std::vector<double> centroid(
    const std::vector<std::vector<double>>& simplex,
    const std::vector<std::size_t>&         order,
    std::size_t                             n)
{
    const std::size_t dim = simplex[0].size();
    std::vector<double> c(dim, 0.0);
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t d = 0; d < dim; ++d)
            c[d] += simplex[order[i]][d];
    for (std::size_t d = 0; d < dim; ++d)
        c[d] /= static_cast<double>(n);
    return c;
}

// Helper: linear combination a*x + b*y.
static std::vector<double> lerp(
    double a, const std::vector<double>& x,
    double b, const std::vector<double>& y)
{
    const std::size_t n = x.size();
    std::vector<double> r(n);
    for (std::size_t i = 0; i < n; ++i)
        r[i] = a * x[i] + b * y[i];
    return r;
}

NelderMeadResult nelderMead(
    std::function<double(const std::vector<double>&)> f,
    std::vector<double>                               x0,
    int                                               maxIter,
    double                                            tol,
    double                                            step)
{
    const std::size_t n = x0.size();
    if (n == 0)
        throw std::invalid_argument("nelderMead: x0 must be non-empty.");

    // Build initial simplex: one vertex at x0, n vertices perturbed by step.
    const std::size_t nv = n + 1;
    std::vector<std::vector<double>> simplex(nv, x0);
    for (std::size_t i = 0; i < n; ++i) {
        const double delta = (x0[i] != 0.0) ? step * std::abs(x0[i]) : 0.00025;
        simplex[i + 1][i] += delta;
    }

    // Evaluate all vertices.
    std::vector<double> fvals(nv);
    for (std::size_t i = 0; i < nv; ++i)
        fvals[i] = f(simplex[i]);

    // Order indices by ascending function value.
    std::vector<std::size_t> ord(nv);
    std::iota(ord.begin(), ord.end(), 0);

    NelderMeadResult result;
    result.converged = false;

    for (int iter = 0; iter < maxIter; ++iter) {

        // Sort vertices: ord[0] = best, ord[n] = worst.
        std::sort(ord.begin(), ord.end(),
                  [&](std::size_t a, std::size_t b){ return fvals[a] < fvals[b]; });

        // Check convergence: std dev of fvals < tol.
        double mean = 0.0;
        for (std::size_t i = 0; i < nv; ++i) mean += fvals[i];
        mean /= static_cast<double>(nv);
        double var = 0.0;
        for (std::size_t i = 0; i < nv; ++i) {
            const double d = fvals[i] - mean;
            var += d * d;
        }
        if (std::sqrt(var / static_cast<double>(nv)) < tol) {
            result.converged = true;
            result.nIter     = iter;
            break;
        }

        // Compute centroid of n best vertices.
        const std::vector<double> xc = centroid(simplex, ord, n);

        // Reflection.
        const std::vector<double> xr = lerp(1.0 + ALPHA, xc, -ALPHA, simplex[ord[n]]);
        const double fr = f(xr);

        if (fr < fvals[ord[0]]) {
            // Expansion.
            const std::vector<double> xe = lerp(1.0 + GAMMA, xc, -GAMMA, simplex[ord[n]]);
            const double fe = f(xe);
            if (fe < fr) {
                simplex[ord[n]] = xe;
                fvals[ord[n]]   = fe;
            } else {
                simplex[ord[n]] = xr;
                fvals[ord[n]]   = fr;
            }
        } else if (fr < fvals[ord[n - 1]]) {
            // Accept reflection.
            simplex[ord[n]] = xr;
            fvals[ord[n]]   = fr;
        } else {
            // Contraction.
            const bool outside = fr < fvals[ord[n]];
            const std::vector<double> xk = outside
                ? lerp(RHO, xc, 1.0 - RHO, xr)
                : lerp(RHO, xc, 1.0 - RHO, simplex[ord[n]]);
            const double fk = f(xk);

            if (fk < (outside ? fr : fvals[ord[n]])) {
                simplex[ord[n]] = xk;
                fvals[ord[n]]   = fk;
            } else {
                // Shrink: move all vertices toward best.
                for (std::size_t i = 1; i < nv; ++i) {
                    simplex[ord[i]] = lerp(1.0 - SIGMA, simplex[ord[0]],
                                           SIGMA, simplex[ord[i]]);
                    fvals[ord[i]]   = f(simplex[ord[i]]);
                }
            }
        }
    }

    if (!result.converged)
        result.nIter = maxIter;

    // Final sort.
    std::sort(ord.begin(), ord.end(),
              [&](std::size_t a, std::size_t b){ return fvals[a] < fvals[b]; });

    result.params = simplex[ord[0]];
    result.fval   = fvals[ord[0]];
    return result;
}

} // namespace loki::math
