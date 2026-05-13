#include <loki/math/interpolation.hpp>
#include <loki/core/exceptions.hpp>

#include <algorithm>
#include <cstddef>

using namespace loki;

namespace loki::math {

// =============================================================================
//  Internal helpers
// =============================================================================

/**
 * @brief Selects the start index of the (order+1)-point interpolation window.
 *
 * The window is centred on the epoch nearest to t.  If the centred window
 * would extend beyond the array boundaries it is shifted inward.
 *
 * @param times   Sorted epoch array.
 * @param t       Query epoch.
 * @param order   Polynomial order (window size = order+1).
 * @return        Start index into times[].
 */
static std::size_t selectWindow(const std::vector<double>& times,
                                 double t,
                                 int order)
{
    const std::size_t n    = times.size();
    const int         w    = order + 1;           // window size

    // Find index of epoch closest to t.
    std::size_t nearest = 0;
    double      minDist = std::abs(times[0] - t);
    for (std::size_t i = 1; i < n; ++i) {
        const double d = std::abs(times[i] - t);
        if (d < minDist) { minDist = d; nearest = i; }
    }

    // Centre the window on nearest.
    std::size_t half  = static_cast<std::size_t>(w / 2);
    std::size_t start = (nearest >= half) ? (nearest - half) : 0;

    // Shift inward if window extends past the end.
    if (start + static_cast<std::size_t>(w) > n) {
        start = n - static_cast<std::size_t>(w);
    }
    return start;
}

// =============================================================================
//  lagrangeInterp
// =============================================================================

double lagrangeInterp(const std::vector<double>& times,
                      const std::vector<double>& values,
                      double t,
                      int order)
{
    const std::size_t n = times.size();
    if (n < static_cast<std::size_t>(order + 1)) {
        throw AlgorithmException(
            "lagrangeInterp: not enough points for requested order "
            "(need " + std::to_string(order + 1) +
            ", got "  + std::to_string(n) + ").");
    }

    const std::size_t start = selectWindow(times, t, order);
    const int         w     = order + 1;

    double result = 0.0;
    for (int i = 0; i < w; ++i) {
        const std::size_t ii = start + static_cast<std::size_t>(i);
        const double ti      = times[ii];
        double basis         = 1.0;
        for (int j = 0; j < w; ++j) {
            if (j == i) continue;
            const std::size_t jj = start + static_cast<std::size_t>(j);
            const double tj = times[jj];
            // Guard against duplicate epochs (degenerate SP3 files).
            if (std::abs(ti - tj) < 1.0e-9) continue;
            basis *= (t - tj) / (ti - tj);
        }
        result += values[ii] * basis;
    }
    return result;
}

// =============================================================================
//  lagrangeInterp3
// =============================================================================

std::array<double, 3> lagrangeInterp3(
    const std::vector<double>&               times,
    const std::vector<std::array<double, 3>>& positions,
    double t,
    int order)
{
    const std::size_t n = times.size();
    if (n < static_cast<std::size_t>(order + 1)) {
        throw AlgorithmException(
            "lagrangeInterp3: not enough points for requested order.");
    }
    if (positions.size() != n) {
        throw AlgorithmException(
            "lagrangeInterp3: times and positions arrays have different lengths.");
    }

    const std::size_t start = selectWindow(times, t, order);
    const int         w     = order + 1;

    std::array<double, 3> result{0.0, 0.0, 0.0};

    for (int i = 0; i < w; ++i) {
        const std::size_t ii = start + static_cast<std::size_t>(i);
        const double ti      = times[ii];
        double basis         = 1.0;
        for (int j = 0; j < w; ++j) {
            if (j == i) continue;
            const std::size_t jj = start + static_cast<std::size_t>(j);
            const double tj = times[jj];
            if (std::abs(ti - tj) < 1.0e-9) continue;
            basis *= (t - tj) / (ti - tj);
        }
        result[0] += positions[ii][0] * basis;
        result[1] += positions[ii][1] * basis;
        result[2] += positions[ii][2] * basis;
    }
    return result;
}

} // namespace loki::math
