#pragma once

#include <vector>

namespace loki::math {

// ---------------------------------------------------------------------------
//  BoundaryCondition (defined outside CubicSpline -- GCC 13 aggregate-init)
// ---------------------------------------------------------------------------

/**
 * @brief Boundary condition type for cubic spline interpolation.
 *
 * NATURAL     : second derivative is zero at both endpoints.
 *               Produces a smooth, "relaxed" spline with no curvature at edges.
 *               Best choice when no endpoint derivative information is available.
 *
 * NOT_A_KNOT  : third derivative is continuous at the second and second-to-last
 *               knots. Forces the first two and last two cubic pieces to form
 *               a single cubic polynomial each. Default for most interpolation tasks.
 *               Matches MATLAB's default spline() behaviour.
 *
 * CLAMPED     : first derivative is prescribed at both endpoints.
 *               Use when the slope at the boundaries is known (e.g. from physics).
 *               Requires leftSlope and rightSlope to be set in CubicSplineConfig.
 */
enum class BoundaryCondition {
    NATURAL,
    NOT_A_KNOT,
    CLAMPED
};

// ---------------------------------------------------------------------------
//  CubicSplineConfig (defined outside CubicSpline -- GCC 13 aggregate-init)
// ---------------------------------------------------------------------------

/**
 * @brief Configuration for CubicSpline construction.
 */
struct CubicSplineConfig {
    BoundaryCondition bc;          ///< Boundary condition type.
    double            leftSlope;   ///< Prescribed first derivative at x[0] (CLAMPED only).
    double            rightSlope;  ///< Prescribed first derivative at x[n-1] (CLAMPED only).

    CubicSplineConfig()
        : bc(BoundaryCondition::NOT_A_KNOT)
        , leftSlope(0.0)
        , rightSlope(0.0)
    {}

    CubicSplineConfig(BoundaryCondition b, double ls, double rs)
        : bc(b)
        , leftSlope(ls)
        , rightSlope(rs)
    {}
};

// ---------------------------------------------------------------------------
//  CubicSpline
// ---------------------------------------------------------------------------

/**
 * @brief Piecewise cubic spline interpolant over a set of knots.
 *
 * Given n knots (x[0] < x[1] < ... < x[n-1]) and corresponding values
 * y[0..n-1], constructs a C^2-continuous piecewise cubic polynomial S such
 * that S(x[i]) = y[i] for all i.
 *
 * On each interval [x[i], x[i+1]] the spline is represented in the standard
 * polynomial form:
 *
 *   S_i(t) = a[i] + b[i]*t + c[i]*t^2 + d[i]*t^3,   t = x - x[i]
 *
 * Coefficients are computed once during construction by solving a tridiagonal
 * linear system. Evaluation is O(log n) via binary search for the interval.
 *
 * Extrapolation beyond [x[0], x[n-1]] uses the cubic polynomial of the
 * nearest end interval (no clamping to endpoint values).
 *
 * Primary use cases in LOKI:
 *   - GapFiller::Strategy::SPLINE  (smooth gap filling in TimeSeries)
 *   - SplineFilter                 (smoothing spline -- needs derivative())
 *   - loki_spline app              (general curve fitting)
 *
 * Construction: use the static factory CubicSpline::interpolate().
 *
 * @throws SeriesTooShortException if fewer than 3 knots are provided.
 * @throws DataException           if x values are not strictly increasing,
 *                                 or if x and y have different sizes.
 * @throws AlgorithmException      if the tridiagonal system is singular
 *                                 (should not occur for strictly increasing x).
 */
class CubicSpline {
public:
    using Config = CubicSplineConfig;

    // -----------------------------------------------------------------------
    //  Factory
    // -----------------------------------------------------------------------

    /**
     * @brief Constructs a cubic spline interpolant through the given knots.
     *
     * @param x   Strictly increasing knot abscissae (n >= 3).
     * @param y   Knot values, same size as x.
     * @param cfg Configuration (boundary condition and optional slopes).
     * @return    Fully initialised CubicSpline ready for evaluation.
     * @throws SeriesTooShortException if x.size() < 3.
     * @throws DataException           if x and y sizes differ, or x is not
     *                                 strictly increasing.
     * @throws AlgorithmException      if the tridiagonal system is singular.
     */
    static CubicSpline interpolate(
        const std::vector<double>& x,
        const std::vector<double>& y,
        const Config&              cfg = Config{});

    // -----------------------------------------------------------------------
    //  Evaluation
    // -----------------------------------------------------------------------

    /**
     * @brief Evaluates the spline at a single point.
     *
     * Points outside [x[0], x[n-1]] are extrapolated using the polynomial
     * of the nearest end interval.
     *
     * @param xq Query point.
     * @return   Interpolated (or extrapolated) value.
     */
    [[nodiscard]] double evaluate(double xq) const;

    /**
     * @brief Evaluates the spline at multiple query points.
     *
     * Each point is located independently via binary search.
     * The query vector need not be sorted.
     *
     * @param xq Query points.
     * @return   Interpolated values, same size as xq.
     */
    [[nodiscard]] std::vector<double> evaluate(const std::vector<double>& xq) const;

    /**
     * @brief Evaluates the first derivative of the spline at a single point.
     *
     * S'_i(t) = b[i] + 2*c[i]*t + 3*d[i]*t^2,   t = xq - x[i]
     *
     * Used by SplineFilter to compute the roughness penalty integral.
     *
     * @param xq Query point.
     * @return   First derivative value.
     */
    [[nodiscard]] double derivative(double xq) const;

    /**
     * @brief Evaluates the second derivative of the spline at a single point.
     *
     * S''_i(t) = 2*c[i] + 6*d[i]*t,   t = xq - x[i]
     *
     * The second derivative is piecewise linear and continuous at knots
     * (C^2 property). Zero at both endpoints for NATURAL boundary condition.
     *
     * @param xq Query point.
     * @return   Second derivative value.
     */
    [[nodiscard]] double secondDerivative(double xq) const;

    // -----------------------------------------------------------------------
    //  Accessors
    // -----------------------------------------------------------------------

    /// @brief Returns the number of knots.
    [[nodiscard]] std::size_t size() const noexcept;

    /// @brief Returns the knot abscissae.
    [[nodiscard]] const std::vector<double>& knotsX() const noexcept;

    /// @brief Returns the knot values.
    [[nodiscard]] const std::vector<double>& knotsY() const noexcept;

private:
    // Private constructor -- use interpolate() factory.
    CubicSpline() = default;

    // -----------------------------------------------------------------------
    //  Coefficient storage
    // -----------------------------------------------------------------------

    std::vector<double> m_x;  ///< Knot abscissae (strictly increasing).
    std::vector<double> m_y;  ///< Knot values.
    std::vector<double> m_b;  ///< Linear coefficients b[i].
    std::vector<double> m_c;  ///< Quadratic coefficients c[i].
    std::vector<double> m_d;  ///< Cubic coefficients d[i].

    // -----------------------------------------------------------------------
    //  Internal helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Solves the tridiagonal system for the second derivatives (sigma).
     *
     * Fills m_b, m_c, m_d from the solved sigma vector and the knot data.
     * Uses the Thomas algorithm (O(n) forward elimination + back substitution).
     *
     * @param cfg Boundary condition configuration.
     */
    void _buildCoefficients(const Config& cfg);

    /**
     * @brief Returns the interval index i such that m_x[i] <= xq < m_x[i+1].
     *
     * Uses std::lower_bound for O(log n) lookup. Clamps to [0, n-2] for
     * extrapolation outside the knot range.
     *
     * @param xq Query point.
     * @return   Interval index in [0, n-2].
     */
    [[nodiscard]] std::size_t _findInterval(double xq) const noexcept;
};

} // namespace loki::math
