#include <loki/decomposition/stlDecomposer.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

using namespace loki;

namespace {

constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

// Tricube kernel weight: K(u) = (1 - u^3)^3 for u in [0, 1].
double tricube(double u)
{
    const double uc = std::min(std::max(u, 0.0), 1.0);
    const double t  = 1.0 - uc * uc * uc;
    return t * t * t;
}

// Median of a copy of v (avoids modifying caller's data).
double medianCopy(std::vector<double> v)
{
    if (v.empty()) return 0.0;
    const std::size_t mid = v.size() / 2;
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(mid), v.end());
    if (v.size() % 2 == 1) return v[mid];
    const double upper = v[mid];
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(mid - 1), v.end());
    return 0.5 * (v[mid - 1] + upper);
}

} // anonymous namespace

// -----------------------------------------------------------------------------
//  StlDecomposer
// -----------------------------------------------------------------------------

StlDecomposer::StlDecomposer(Config cfg)
    : m_cfg{std::move(cfg)}
{}

// -----------------------------------------------------------------------------

DecompositionResult StlDecomposer::decompose(const TimeSeries& ts, int period) const
{
    if (period < 2) {
        throw ConfigException(
            "StlDecomposer::decompose: period must be >= 2, got "
            + std::to_string(period) + ".");
    }

    const int n = static_cast<int>(ts.size());
    if (n < 2 * period) {
        throw DataException(
            "StlDecomposer::decompose: series length " + std::to_string(n)
            + " must be at least 2 * period = " + std::to_string(2 * period) + ".");
    }

    // Check for NaN.
    for (int i = 0; i < n; ++i) {
        if (std::isnan(ts[static_cast<std::size_t>(i)].value)) {
            throw DataException(
                "StlDecomposer::decompose: input series contains NaN at index "
                + std::to_string(i) + ". Run GapFiller before decomposing.");
        }
    }

    // Copy values.
    std::vector<double> y(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        y[static_cast<std::size_t>(i)] = ts[static_cast<std::size_t>(i)].value;
    }

    // Resolve auto bandwidths.
    const double sBw = (m_cfg.sBandwidth > 0.0)
        ? m_cfg.sBandwidth
        : std::min(1.0, 1.5 / static_cast<double>(period));

    const double tBw = (m_cfg.tBandwidth > 0.0)
        ? m_cfg.tBandwidth
        : std::min(1.0, 1.5 * static_cast<double>(period) / static_cast<double>(n));

    LOKI_INFO("STL bandwidths: s_bw=" + std::to_string(sBw)
              + ", t_bw=" + std::to_string(tBw));

    // Initialise trend to zero and robustness weights to 1.
    std::vector<double> trend   (static_cast<std::size_t>(n), 0.0);
    std::vector<double> seasonal(static_cast<std::size_t>(n), 0.0);
    std::vector<double> robWeights; // empty = all ones for first outer pass

    const int nOuter = std::max(m_cfg.nOuter, 0);
    const int totalOuter = nOuter + 1; // at least one pass (non-robust)

    for (int outerIt = 0; outerIt < totalOuter; ++outerIt) {

        // Inner loop.
        for (int innerIt = 0; innerIt < m_cfg.nInner; ++innerIt) {
            innerLoop(y, trend, period, sBw, tBw, robWeights, seasonal, trend);
        }

        // Update robustness weights for next outer iteration (skip on last pass).
        if (outerIt < totalOuter - 1) {
            std::vector<double> residual(static_cast<std::size_t>(n));
            for (int i = 0; i < n; ++i) {
                const std::size_t si = static_cast<std::size_t>(i);
                residual[si] = y[si] - trend[si] - seasonal[si];
            }
            robWeights = bisquareWeights(residual);
        }
    }

    // Final residual.
    std::vector<double> residual(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        const std::size_t si = static_cast<std::size_t>(i);
        residual[si] = y[si] - trend[si] - seasonal[si];
    }

    DecompositionResult result;
    result.trend    = std::move(trend);
    result.seasonal = std::move(seasonal);
    result.residual = std::move(residual);
    result.method   = DecompositionMethod::STL;
    result.period   = period;
    return result;
}

// -----------------------------------------------------------------------------
//  Private: innerLoop
// -----------------------------------------------------------------------------

void StlDecomposer::innerLoop(const std::vector<double>& y,
                               const std::vector<double>& trendIn,
                               int period,
                               double sBw,
                               double tBw,
                               const std::vector<double>& robWeights,
                               std::vector<double>& seasonal,
                               std::vector<double>& trendOut) const
{
    const int n = static_cast<int>(y.size());

    // Step 1: detrend W = Y - T.
    std::vector<double> w(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        w[static_cast<std::size_t>(i)] = y[static_cast<std::size_t>(i)]
                                        - trendIn[static_cast<std::size_t>(i)];
    }

    // Step 2: smooth each cycle-subseries with LOESS -> C (length n).
    // For slot s, the subsequence is w[s], w[s+p], w[s+2p], ...
    std::vector<double> cycle(static_cast<std::size_t>(n), 0.0);
    for (int s = 0; s < period; ++s) {
        // Build subseries and matching robustness weights.
        std::vector<double> sub;
        std::vector<double> subRob;
        sub.reserve(static_cast<std::size_t>(n / period + 1));
        subRob.reserve(sub.capacity());

        for (int t = s; t < n; t += period) {
            sub.push_back(w[static_cast<std::size_t>(t)]);
            if (!robWeights.empty()) {
                subRob.push_back(robWeights[static_cast<std::size_t>(t)]);
            }
        }

        // LOESS on the subseries (indices 0, 1, 2, ... count periods).
        const std::vector<double> smoothed = loess(sub, sBw, m_cfg.sDegree, subRob);

        // Write back.
        int idx = 0;
        for (int t = s; t < n; t += period) {
            cycle[static_cast<std::size_t>(t)] = smoothed[static_cast<std::size_t>(idx)];
            ++idx;
        }
    }

    // Step 3: low-pass filter L = MA(p, MA(p, MA(3, C))).
    const std::vector<double> lp1 = ma3(cycle);
    const std::vector<double> lp2 = maFilled(lp1, period);
    const std::vector<double> lp3 = maFilled(lp2, period);
    // Final LOESS pass on the low-pass output.
    const std::vector<double> lowPass = loess(lp3, tBw, m_cfg.tDegree, robWeights);

    // Step 4: seasonal S = C - L.
    seasonal.resize(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        seasonal[static_cast<std::size_t>(i)] =
            cycle[static_cast<std::size_t>(i)] - lowPass[static_cast<std::size_t>(i)];
    }

    // Step 5: deseasoned V = Y - S.
    std::vector<double> v(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        v[static_cast<std::size_t>(i)] =
            y[static_cast<std::size_t>(i)] - seasonal[static_cast<std::size_t>(i)];
    }

    // Step 6: trend T = LOESS(V).
    trendOut = loess(v, tBw, m_cfg.tDegree, robWeights);
}

// -----------------------------------------------------------------------------
//  Private: loess
// -----------------------------------------------------------------------------

std::vector<double> StlDecomposer::loess(const std::vector<double>& y,
                                          double bandwidth,
                                          int degree,
                                          const std::vector<double>& robWeights) const
{
    if (degree != 1 && degree != 2) {
        throw ConfigException(
            "StlDecomposer::loess: degree must be 1 or 2, got "
            + std::to_string(degree) + ".");
    }
    if (bandwidth <= 0.0 || bandwidth > 1.0) {
        throw ConfigException(
            "StlDecomposer::loess: bandwidth must be in (0, 1], got "
            + std::to_string(bandwidth) + ".");
    }

    const int n = static_cast<int>(y.size());
    if (n == 0) return {};
    if (n == 1) return y;

    const int k = std::max(degree + 1,
                           static_cast<int>(std::ceil(bandwidth * static_cast<double>(n))));
    const bool hasRob = !robWeights.empty();

    std::vector<double> smoothed(static_cast<std::size_t>(n), 0.0);
    std::vector<int>    idx(static_cast<std::size_t>(n));
    std::iota(idx.begin(), idx.end(), 0);

    for (int i = 0; i < n; ++i) {
        // Sort indices by distance to i.
        std::partial_sort(idx.begin(),
                          idx.begin() + k,
                          idx.end(),
                          [i](int a, int b) {
                              return std::abs(a - i) < std::abs(b - i);
                          });

        const double maxDist = static_cast<double>(std::abs(idx[k - 1] - i));

        // Build local system: A (k x (degree+1)), w (k), l (k).
        Eigen::MatrixXd A(k, degree + 1);
        Eigen::VectorXd wVec(k);
        Eigen::VectorXd lVec(k);

        for (int s = 0; s < k; ++s) {
            const int    j   = idx[static_cast<std::size_t>(s)];
            const double xj  = static_cast<double>(j - i);
            const double u   = (maxDist < 1.0e-12)
                                   ? 0.0
                                   : std::abs(xj) / maxDist;
            double w = tricube(u);
            if (hasRob) {
                w *= robWeights[static_cast<std::size_t>(j)];
            }

            lVec(s) = y[static_cast<std::size_t>(j)];
            wVec(s) = w;

            // Design row: [1, xj, xj^2, ...]
            A(s, 0) = 1.0;
            if (degree >= 1) A(s, 1) = xj;
            if (degree >= 2) A(s, 2) = xj * xj;
        }

        // Weighted least squares: min ||W^(1/2)(A*beta - l)||^2
        // -> (A^T W A) beta = A^T W l
        const Eigen::MatrixXd Aw = wVec.asDiagonal() * A;
        const Eigen::VectorXd rhs = Aw.transpose() * lVec;
        const Eigen::MatrixXd lhs = Aw.transpose() * A;

        // Solve via Cholesky (positive-definite if weights are positive).
        Eigen::VectorXd beta = lhs.ldlt().solve(rhs);

        // Fitted value at i is beta(0) (constant term, since xj=0 at i).
        smoothed[static_cast<std::size_t>(i)] = beta(0);
    }

    return smoothed;
}

// -----------------------------------------------------------------------------
//  Private: bisquareWeights
// -----------------------------------------------------------------------------

std::vector<double> StlDecomposer::bisquareWeights(const std::vector<double>& residual)
{
    const int n = static_cast<int>(residual.size());

    // Compute |R[t]|.
    std::vector<double> absRes(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        absRes[static_cast<std::size_t>(i)] = std::abs(residual[static_cast<std::size_t>(i)]);
    }

    const double h = 6.0 * medianCopy(absRes);
    std::vector<double> weights(static_cast<std::size_t>(n), 1.0);

    if (h < std::numeric_limits<double>::epsilon() * 100.0) {
        // All residuals near zero -- return all-ones.
        return weights;
    }

    for (int i = 0; i < n; ++i) {
        const double u = absRes[static_cast<std::size_t>(i)] / h;
        if (u >= 1.0) {
            weights[static_cast<std::size_t>(i)] = 0.0;
        } else {
            const double t = 1.0 - u * u;
            weights[static_cast<std::size_t>(i)] = t * t;
        }
    }

    return weights;
}

// -----------------------------------------------------------------------------
//  Private: ma3
// -----------------------------------------------------------------------------

std::vector<double> StlDecomposer::ma3(const std::vector<double>& y)
{
    const int n = static_cast<int>(y.size());
    std::vector<double> out(static_cast<std::size_t>(n));

    for (int i = 0; i < n; ++i) {
        const int lo = std::max(0,     i - 1);
        const int hi = std::min(n - 1, i + 1);
        double sum   = 0.0;
        int    count = 0;
        for (int j = lo; j <= hi; ++j) {
            sum += y[static_cast<std::size_t>(j)];
            ++count;
        }
        out[static_cast<std::size_t>(i)] = sum / static_cast<double>(count);
    }

    return out;
}

// -----------------------------------------------------------------------------
//  Private: maFilled
// -----------------------------------------------------------------------------

std::vector<double> StlDecomposer::maFilled(const std::vector<double>& y, int width)
{
    const int n = static_cast<int>(y.size());
    // Ensure odd window.
    const int w    = (width % 2 == 0) ? width + 1 : width;
    const int half = w / 2;

    std::vector<double> out(static_cast<std::size_t>(n), 0.0);

    // Build prefix sums.
    std::vector<double> prefix(static_cast<std::size_t>(n + 1), 0.0);
    for (int i = 0; i < n; ++i) {
        prefix[static_cast<std::size_t>(i + 1)] =
            prefix[static_cast<std::size_t>(i)] + y[static_cast<std::size_t>(i)];
    }

    for (int i = 0; i < n; ++i) {
        const int lo = std::max(0,     i - half);
        const int hi = std::min(n - 1, i + half);
        const int cnt = hi - lo + 1;
        out[static_cast<std::size_t>(i)] =
            (prefix[static_cast<std::size_t>(hi + 1)] - prefix[static_cast<std::size_t>(lo)])
            / static_cast<double>(cnt);
    }

    return out;
}
