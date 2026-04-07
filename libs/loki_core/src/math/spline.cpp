#include <loki/math/spline.hpp>

#include <loki/core/exceptions.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <stdexcept>

using namespace loki;
using namespace loki::math;

// ---------------------------------------------------------------------------
//  Factory
// ---------------------------------------------------------------------------

CubicSpline CubicSpline::interpolate(
    const std::vector<double>& x,
    const std::vector<double>& y,
    const Config&              cfg)
{
    if (x.size() < 3) {
        throw SeriesTooShortException(
            "CubicSpline::interpolate: at least 3 knots required, got "
            + std::to_string(x.size()) + ".");
    }
    if (x.size() != y.size()) {
        throw DataException(
            "CubicSpline::interpolate: x and y must have the same size ("
            + std::to_string(x.size()) + " vs " + std::to_string(y.size()) + ").");
    }

    // Verify strict monotonicity.
    for (std::size_t i = 1; i < x.size(); ++i) {
        if (x[i] <= x[i - 1]) {
            throw DataException(
                "CubicSpline::interpolate: x values must be strictly increasing "
                "(x[" + std::to_string(i - 1) + "] = " + std::to_string(x[i - 1])
                + " >= x[" + std::to_string(i) + "] = " + std::to_string(x[i]) + ").");
        }
    }

    CubicSpline spline;
    spline.m_x = x;
    spline.m_y = y;
    spline.m_b.resize(x.size(), 0.0);
    spline.m_c.resize(x.size(), 0.0);
    spline.m_d.resize(x.size(), 0.0);
    spline._buildCoefficients(cfg);
    return spline;
}

// ---------------------------------------------------------------------------
//  _buildCoefficients
//
//  Solves the classic tridiagonal system for natural / not-a-knot / clamped
//  splines.  The unknown vector is sigma[i] = S''(x[i]) / 2  (half the
//  second derivative at knot i).
//
//  For a spline on n knots (n-1 intervals), h[i] = x[i+1] - x[i] and
//  dy[i] = y[i+1] - y[i].  The C^2 continuity conditions give the system:
//
//    h[i-1]*sigma[i-1] + 2*(h[i-1]+h[i])*sigma[i] + h[i]*sigma[i+1]
//      = 3*(dy[i]/h[i] - dy[i-1]/h[i-1])
//
//  for i = 1 .. n-2.  Boundary conditions supply the two missing equations.
//
//  Once sigma is known, the per-interval coefficients are:
//    a[i] = y[i]
//    b[i] = dy[i]/h[i] - h[i]*(2*sigma[i] + sigma[i+1])/3
//    c[i] = sigma[i]
//    d[i] = (sigma[i+1] - sigma[i]) / (3*h[i])
// ---------------------------------------------------------------------------

void CubicSpline::_buildCoefficients(const Config& cfg)
{
    const std::size_t n  = m_x.size();
    const std::size_t nm1 = n - 1;  // number of intervals

    // Interval widths and divided differences.
    std::vector<double> h(nm1), dy(nm1);
    for (std::size_t i = 0; i < nm1; ++i) {
        h[i]  = m_x[i + 1] - m_x[i];
        dy[i] = m_y[i + 1] - m_y[i];
    }

    // sigma[i] = S''(x[i]) / 2.  Solved via Thomas algorithm.
    std::vector<double> sigma(n, 0.0);

    // Build tridiagonal system: sub[], main[], super[], rhs[].
    // Interior equations: indices 1 .. n-2  (size = n-2).
    // Two boundary equations are added depending on bc type.

    // We work with the full n x n system but only the interior rows need
    // filling; the first and last rows encode the boundary conditions.

    std::vector<double> sub(n, 0.0);   // sub-diagonal  (a_i)
    std::vector<double> diag(n, 0.0);  // main diagonal (b_i)
    std::vector<double> sup(n, 0.0);   // super-diagonal (c_i)
    std::vector<double> rhs(n, 0.0);

    // Interior rows (same for all boundary conditions).
    for (std::size_t i = 1; i + 1 < n; ++i) {
        sub[i]  = h[i - 1];
        diag[i] = 2.0 * (h[i - 1] + h[i]);
        sup[i]  = h[i];
        rhs[i]  = 3.0 * (dy[i] / h[i] - dy[i - 1] / h[i - 1]);
    }

    // Boundary conditions.
    switch (cfg.bc) {
        case BoundaryCondition::NATURAL:
            // sigma[0] = 0, sigma[n-1] = 0.
            diag[0]     = 1.0;
            sup[0]      = 0.0;
            rhs[0]      = 0.0;
            sub[nm1]    = 0.0;
            diag[nm1]   = 1.0;
            rhs[nm1]    = 0.0;
            break;

        case BoundaryCondition::NOT_A_KNOT:
            // Force the first two intervals to form one cubic:
            //   d[0] = d[1]  =>  (sigma[1]-sigma[0])/h[0] = (sigma[2]-sigma[1])/h[1]
            //   => h[1]*sigma[0] - (h[0]+h[1])*sigma[1] + h[0]*sigma[2] = 0
            // First row (i=0):
            diag[0] =  h[1];
            sup[0]  = -(h[0] + h[1]);
            // sub[0] would be for sigma[-1] -- doesn't exist, leave 0.
            // We need a third entry:  coefficient of sigma[2] is h[0].
            // Thomas algorithm handles tridiagonal only, so we embed the
            // h[0]*sigma[2] term by modifying the system slightly.
            // Standard trick: treat row 0 as   h[1]*s[0] - (h[0]+h[1])*s[1] + h[0]*s[2] = 0
            // We store the s[2] coefficient in a separate pass after elimination.
            // Simpler: use the equivalent form normalised so diag[0] is the pivot.
            //
            // Row 0 as written above is already tridiagonal if we treat sup[0]
            // as the coefficient of s[1] and add a "fill-in" at s[2].
            // We handle this by doing one step of elimination manually before
            // the Thomas forward sweep.
            //
            // Re-encode as a 2-band extra fill: store fill_sup0 = h[0].
            // The Thomas algorithm below checks for this case via cfg.bc.
            rhs[0]  = 0.0;

            // Last row (i=n-1): force last two intervals to one cubic:
            //   (sigma[n-2]-sigma[n-3])/h[n-3] = (sigma[n-1]-sigma[n-2])/h[n-2]
            //   => h[n-2]*sigma[n-3] - (h[n-3]+h[n-2])*sigma[n-2] + h[n-3]*sigma[n-1] = 0
            sub[nm1]  = -(h[nm1 - 2] + h[nm1 - 1]);
            diag[nm1] =  h[nm1 - 2];
            // coefficient of sigma[n-3] is h[nm1-1]; stored as fill_sub_last.
            rhs[nm1]  = 0.0;
            break;

        case BoundaryCondition::CLAMPED:
            // S'(x[0]) = leftSlope:
            //   b[0] = leftSlope
            //   dy[0]/h[0] - h[0]*(2*sigma[0] + sigma[1])/3 = leftSlope
            //   => 2*h[0]*sigma[0] + h[0]*sigma[1] = 3*(dy[0]/h[0] - leftSlope)
            diag[0] = 2.0 * h[0];
            sup[0]  = h[0];
            rhs[0]  = 3.0 * (dy[0] / h[0] - cfg.leftSlope);

            // S'(x[n-1]) = rightSlope:
            //   dy[n-2]/h[n-2] - h[n-2]*(2*sigma[n-2]+sigma[n-1])/3
            //     + 2*sigma[n-1]*h[n-2]... (work from right-interval b formula)
            //   => h[n-2]*sigma[n-2] + 2*h[n-2]*sigma[n-1]
            //        = 3*(rightSlope - dy[n-2]/h[n-2])
            sub[nm1]  = h[nm1 - 1];
            diag[nm1] = 2.0 * h[nm1 - 1];
            rhs[nm1]  = 3.0 * (cfg.rightSlope - dy[nm1 - 1] / h[nm1 - 1]);
            break;
    }

    // -----------------------------------------------------------------------
    //  Thomas algorithm (tridiagonal solve).
    //  For NOT_A_KNOT the system has one extra off-diagonal entry per boundary
    //  row.  We eliminate these fill-ins with one manual step each, then
    //  run the standard Thomas sweep on the remaining interior rows.
    // -----------------------------------------------------------------------

    if (cfg.bc == BoundaryCondition::NOT_A_KNOT && n >= 4) {
        // Extra entry at (0, 2): coefficient h[0].
        // Eliminate by combining row 0 with row 1 (which has entries at 0,1,2).
        // Row 1: sub[1]*s[0] + diag[1]*s[1] + sup[1]*s[2] = rhs[1].
        // Pivot on row 0 to zero out sub[1]:
        //   factor = sub[1] / diag[0]   (diag[0] = h[1], guaranteed > 0)
        // But row 0 has entries at columns 0,1,2 (width 3), and row 1 also has
        // columns 0,1,2 -- so elimination produces a normal tridiagonal row 1.
        // After elimination, the extra entry at col 2 of row 0 is absorbed.

        // Forward step for row 0 -> eliminate from row 1.
        double factor = sub[1] / diag[0];
        // Row 1 updated: sub[1] -> 0; diag[1] -= factor*sup[0];
        //                sup[1] -= factor*h[0]; rhs[1] -= factor*rhs[0].
        diag[1] -= factor * sup[0];
        sup[1]  -= factor * h[0];   // h[0] is the fill-in at (0,2)
        rhs[1]  -= factor * rhs[0];
        sub[1]   = 0.0;

        // Similarly eliminate the fill-in at (n-1, n-3): coefficient h[nm1-1].
        // Row n-1 has entries at columns n-3, n-2, n-1.
        // Row n-2 has entries at columns n-3, n-2, n-1 (standard interior).
        // Eliminate (n-1, n-3) by combining with row n-2.
        const std::size_t last = nm1;
        const std::size_t prev = nm1 - 1;
        factor = h[nm1 - 1] / diag[prev];
        // The fill-in is at column prev-1 = n-3; row[prev] has sub at prev-1.
        // After elimination: sub[last] and the fill-in become zero;
        // diag[last] and rhs[last] are updated.
        diag[last] -= factor * sub[prev];
        // sup[prev] coefficient at col last: update sub of last accordingly.
        sub[last]   = 0.0;
        rhs[last]  -= factor * rhs[prev];
        // Also update diag[last] for the sup[prev] contribution.
        // sub[last] was h[nm1-1+1] which does not exist; the fill column is
        // prev-1 (already zero after this step). No sup update needed for last row.
        (void)factor; // suppress warning if reassigned
    }

    // Standard Thomas forward elimination (skip rows already processed above).
    for (std::size_t i = 1; i < n; ++i) {
        if (std::abs(diag[i - 1]) < 1e-15) {
            throw AlgorithmException(
                "CubicSpline::_buildCoefficients: singular tridiagonal system "
                "(zero pivot at row " + std::to_string(i - 1) + ").");
        }
        if (sub[i] == 0.0) continue;  // already eliminated
        const double factor = sub[i] / diag[i - 1];
        diag[i] -= factor * sup[i - 1];
        rhs[i]  -= factor * rhs[i - 1];
        sub[i]   = 0.0;
    }

    // Back substitution.
    if (std::abs(diag[nm1]) < 1e-15) {
        throw AlgorithmException(
            "CubicSpline::_buildCoefficients: singular tridiagonal system "
            "(zero pivot at last row).");
    }
    sigma[nm1] = rhs[nm1] / diag[nm1];
    for (std::size_t i = nm1; i-- > 0;) {
        sigma[i] = (rhs[i] - sup[i] * sigma[i + 1]) / diag[i];
    }

    // Compute per-interval b, c, d from sigma.
    // a[i] = y[i]  (not stored separately -- returned directly in evaluate).
    for (std::size_t i = 0; i < nm1; ++i) {
        m_c[i] = sigma[i];
        m_d[i] = (sigma[i + 1] - sigma[i]) / (3.0 * h[i]);
        m_b[i] = dy[i] / h[i] - h[i] * (2.0 * sigma[i] + sigma[i + 1]) / 3.0;
    }

    // Coefficients for the last knot are unused in evaluation (closed interval),
    // but initialise consistently for safety.
    m_b[nm1] = m_b[nm1 - 1] + 2.0 * m_c[nm1 - 1] * h[nm1 - 1]
               + 3.0 * m_d[nm1 - 1] * h[nm1 - 1] * h[nm1 - 1];
    m_c[nm1] = sigma[nm1];
    m_d[nm1] = 0.0;
}

// ---------------------------------------------------------------------------
//  _findInterval
// ---------------------------------------------------------------------------

std::size_t CubicSpline::_findInterval(double xq) const noexcept
{
    // lower_bound returns iterator to first element >= xq.
    auto it = std::lower_bound(m_x.begin(), m_x.end(), xq);

    if (it == m_x.end()) {
        // xq is beyond the last knot -- use last interval.
        return m_x.size() - 2;
    }
    if (it == m_x.begin()) {
        // xq is at or before the first knot -- use first interval.
        return 0;
    }

    // Normal case: interval i such that m_x[i] <= xq < m_x[i+1].
    std::size_t idx = static_cast<std::size_t>(it - m_x.begin()) - 1;

    // Clamp to valid interval range [0, n-2].
    const std::size_t maxIdx = m_x.size() - 2;
    if (idx > maxIdx) idx = maxIdx;
    return idx;
}

// ---------------------------------------------------------------------------
//  evaluate (scalar)
// ---------------------------------------------------------------------------

double CubicSpline::evaluate(double xq) const
{
    const std::size_t i = _findInterval(xq);
    const double t = xq - m_x[i];
    return m_y[i] + t * (m_b[i] + t * (m_c[i] + t * m_d[i]));
}

// ---------------------------------------------------------------------------
//  evaluate (vector)
// ---------------------------------------------------------------------------

std::vector<double> CubicSpline::evaluate(const std::vector<double>& xq) const
{
    std::vector<double> result;
    result.reserve(xq.size());
    for (double x : xq) {
        result.push_back(evaluate(x));
    }
    return result;
}

// ---------------------------------------------------------------------------
//  derivative
// ---------------------------------------------------------------------------

double CubicSpline::derivative(double xq) const
{
    const std::size_t i = _findInterval(xq);
    const double t = xq - m_x[i];
    return m_b[i] + t * (2.0 * m_c[i] + t * 3.0 * m_d[i]);
}

// ---------------------------------------------------------------------------
//  secondDerivative
// ---------------------------------------------------------------------------

double CubicSpline::secondDerivative(double xq) const
{
    const std::size_t i = _findInterval(xq);
    const double t = xq - m_x[i];
    return 2.0 * m_c[i] + 6.0 * m_d[i] * t;
}

// ---------------------------------------------------------------------------
//  Accessors
// ---------------------------------------------------------------------------

std::size_t CubicSpline::size() const noexcept
{
    return m_x.size();
}

const std::vector<double>& CubicSpline::knotsX() const noexcept
{
    return m_x;
}

const std::vector<double>& CubicSpline::knotsY() const noexcept
{
    return m_y;
}
