#include <loki/math/bspline.hpp>

#include <loki/core/exceptions.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>

using namespace loki;

namespace loki::math {

// =============================================================================
//  normaliseParams
// =============================================================================

std::vector<double> normaliseParams(const std::vector<double>& t)
{
    if (t.size() < 2) {
        throw DataException(
            "bspline::normaliseParams: need at least 2 parameter values, got "
            + std::to_string(t.size()) + ".");
    }
    const double tMin = t.front();
    const double tMax = t.back();
    if (tMax <= tMin) {
        throw DataException(
            "bspline::normaliseParams: parameter range is zero or negative "
            "(tMin=" + std::to_string(tMin) + ", tMax=" + std::to_string(tMax) + ").");
    }
    const double range = tMax - tMin;
    std::vector<double> tNorm(t.size());
    for (std::size_t i = 0; i < t.size(); ++i) {
        tNorm[i] = (t[i] - tMin) / range;
    }
    // Force exact endpoints to avoid floating-point drift.
    tNorm.front() = 0.0;
    tNorm.back()  = 1.0;
    return tNorm;
}

// =============================================================================
//  buildUniformKnots
// =============================================================================

std::vector<double> buildUniformKnots(int nCtrl, int p)
{
    if (nCtrl < p + 1) {
        throw DataException(
            "bspline::buildUniformKnots: need nCtrl >= p+1, got nCtrl="
            + std::to_string(nCtrl) + ", p=" + std::to_string(p) + ".");
    }

    // Total knot count: nCtrl + p + 1.
    const int m = nCtrl + p;
    std::vector<double> knots(static_cast<std::size_t>(m + 1), 0.0);

    // Clamped: first p+1 knots = 0, last p+1 knots = 1.
    // Interior knots uniformly spaced.
    const int nInterior = nCtrl - p - 1; // number of interior knots
    for (int i = 0; i <= p; ++i)      knots[static_cast<std::size_t>(i)]     = 0.0;
    for (int i = 0; i <= p; ++i)      knots[static_cast<std::size_t>(m - i)] = 1.0;

    if (nInterior > 0) {
        const double step = 1.0 / static_cast<double>(nInterior + 1);
        for (int j = 1; j <= nInterior; ++j) {
            knots[static_cast<std::size_t>(p + j)] =
                static_cast<double>(j) * step;
        }
    }

    return knots;
}

// =============================================================================
//  buildChordLengthKnots
// =============================================================================

std::vector<double> buildChordLengthKnots(const std::vector<double>& tNorm,
                                          int nCtrl, int p)
{
    if (nCtrl < p + 1) {
        throw DataException(
            "bspline::buildChordLengthKnots: need nCtrl >= p+1, got nCtrl="
            + std::to_string(nCtrl) + ", p=" + std::to_string(p) + ".");
    }
    if (static_cast<int>(tNorm.size()) < nCtrl) {
        throw DataException(
            "bspline::buildChordLengthKnots: need at least nCtrl data points, got "
            + std::to_string(tNorm.size()) + " points, nCtrl=" + std::to_string(nCtrl) + ".");
    }

    const int m        = nCtrl + p;
    const int nObs     = static_cast<int>(tNorm.size());
    const int nInterior = nCtrl - p - 1;

    std::vector<double> knots(static_cast<std::size_t>(m + 1), 0.0);

    // Clamped endpoints.
    for (int i = 0; i <= p; ++i)      knots[static_cast<std::size_t>(i)]     = 0.0;
    for (int i = 0; i <= p; ++i)      knots[static_cast<std::size_t>(m - i)] = 1.0;

    if (nInterior > 0) {
        // Hartley-Judd averaging: knot[p+j] = mean of tNorm[j..j+p-1]
        // after mapping nCtrl Greville abscissae from tNorm.
        // We distribute nObs parameter values evenly across nCtrl-1 intervals
        // and average p consecutive ones per interior knot.
        const double step = static_cast<double>(nObs - 1)
                          / static_cast<double>(nCtrl);

        for (int j = 1; j <= nInterior; ++j) {
            double sum = 0.0;
            for (int k = 0; k < p; ++k) {
                const int idx = static_cast<int>(
                    std::round(static_cast<double>(j + k) * step));
                const int clamped = std::min(std::max(idx, 0), nObs - 1);
                sum += tNorm[static_cast<std::size_t>(clamped)];
            }
            knots[static_cast<std::size_t>(p + j)] = sum / static_cast<double>(p);
        }
    }

    return knots;
}

// =============================================================================
//  bsplineBasisRow  --  de Boor / Cox-de Boor recursion
// =============================================================================

std::vector<double> bsplineBasisRow(double t, int p,
                                    const std::vector<double>& knots)
{
    const int nCtrl = static_cast<int>(knots.size()) - p - 1;
    if (nCtrl <= 0) {
        throw DataException(
            "bspline::bsplineBasisRow: knot vector too short for degree "
            + std::to_string(p) + " (knots.size()=" + std::to_string(knots.size()) + ").");
    }

    // Clamp t to [0, 1] to handle floating-point edge cases.
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;

    std::vector<double> N(static_cast<std::size_t>(nCtrl), 0.0);

    // --- Degree-0 basis (piecewise indicator) --------------------------------
    for (int i = 0; i < nCtrl; ++i) {
        const double ki   = knots[static_cast<std::size_t>(i)];
        const double ki1  = knots[static_cast<std::size_t>(i + 1)];
        // The last non-zero span is [k[n-1], k[n]] (closed on the right at t=1).
        if (t == 1.0) {
            // Only the last active basis function is 1 at t=1.
            N[static_cast<std::size_t>(i)] =
                (i == nCtrl - 1) ? 1.0 : 0.0;
        } else {
            N[static_cast<std::size_t>(i)] =
                (ki <= t && t < ki1) ? 1.0 : 0.0;
        }
    }

    // --- Degree-r recursion for r = 1 .. p -----------------------------------
    for (int r = 1; r <= p; ++r) {
        std::vector<double> Nnew(static_cast<std::size_t>(nCtrl), 0.0);
        for (int i = 0; i < nCtrl; ++i) {
            const double ki  = knots[static_cast<std::size_t>(i)];
            const double kir = knots[static_cast<std::size_t>(i + r)];
            const double ki1 = knots[static_cast<std::size_t>(i + 1)];
            const double kir1 = knots[static_cast<std::size_t>(i + r + 1)];

            double left  = 0.0;
            double right = 0.0;

            // Left term: (t - k[i]) / (k[i+r] - k[i]) * N_{i,r-1}
            const double dLeft = kir - ki;
            if (dLeft > 0.0) {
                left = (t - ki) / dLeft * N[static_cast<std::size_t>(i)];
            }

            // Right term: (k[i+r+1] - t) / (k[i+r+1] - k[i+1]) * N_{i+1,r-1}
            const double dRight = kir1 - ki1;
            if (i + 1 < nCtrl && dRight > 0.0) {
                right = (kir1 - t) / dRight * N[static_cast<std::size_t>(i + 1)];
            }

            Nnew[static_cast<std::size_t>(i)] = left + right;
        }
        N = std::move(Nnew);
    }

    return N;
}

// =============================================================================
//  bsplineBasisMatrix
// =============================================================================

Eigen::MatrixXd bsplineBasisMatrix(const std::vector<double>& tNorm,
                                   int p,
                                   const std::vector<double>& knots)
{
    if (tNorm.empty()) {
        throw DataException(
            "bspline::bsplineBasisMatrix: tNorm is empty.");
    }
    const int nObs  = static_cast<int>(tNorm.size());
    const int nCtrl = static_cast<int>(knots.size()) - p - 1;
    if (nCtrl <= 0) {
        throw DataException(
            "bspline::bsplineBasisMatrix: knot vector is too short for degree "
            + std::to_string(p) + ".");
    }

    Eigen::MatrixXd B(nObs, nCtrl);
    for (int i = 0; i < nObs; ++i) {
        const std::vector<double> row =
            bsplineBasisRow(tNorm[static_cast<std::size_t>(i)], p, knots);
        for (int j = 0; j < nCtrl; ++j) {
            B(i, j) = row[static_cast<std::size_t>(j)];
        }
    }
    return B;
}

// =============================================================================
//  evalBSpline
// =============================================================================

std::vector<double> evalBSpline(const std::vector<double>& tQuery,
                                const std::vector<double>& controlPoints,
                                int p,
                                const std::vector<double>& knots)
{
    std::vector<double> result;
    result.reserve(tQuery.size());

    for (const double t : tQuery) {
        const std::vector<double> row = bsplineBasisRow(t, p, knots);
        double val = 0.0;
        const std::size_t n = std::min(row.size(), controlPoints.size());
        for (std::size_t i = 0; i < n; ++i) {
            val += row[i] * controlPoints[i];
        }
        result.push_back(val);
    }
    return result;
}

} // namespace loki::math
