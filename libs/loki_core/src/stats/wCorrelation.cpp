#include <loki/stats/wCorrelation.hpp>
#include <loki/core/exceptions.hpp>

#include <cmath>
#include <limits>

using namespace loki;

namespace loki::stats {

Eigen::MatrixXd computeWCorrelation(
    const std::vector<std::vector<double>>& components,
    int L)
{
    if (components.empty()) {
        throw AlgorithmException(
            "computeWCorrelation: components vector is empty.");
    }
    if (L < 2) {
        throw AlgorithmException(
            "computeWCorrelation: window length L=" + std::to_string(L)
            + " must be >= 2.");
    }
    if (static_cast<int>(components.size()) != L) {
        throw AlgorithmException(
            "computeWCorrelation: components.size()="
            + std::to_string(components.size())
            + " must equal L=" + std::to_string(L) + ".");
    }

    const std::size_t n = components[0].size();
    if (n == 0) {
        throw AlgorithmException(
            "computeWCorrelation: component series are empty.");
    }

    for (int i = 1; i < L; ++i) {
        if (components[static_cast<std::size_t>(i)].size() != n) {
            throw AlgorithmException(
                "computeWCorrelation: component " + std::to_string(i)
                + " has length " + std::to_string(components[static_cast<std::size_t>(i)].size())
                + ", expected " + std::to_string(n) + ".");
        }
    }

    // Hankel weights: w_k = min(k+1, L, K, n-k) where K = n - L + 1.
    // For k in [0, L-2]:   w_k = k+1       (ramp up)
    // For k in [L-1, K-1]: w_k = L          (plateau, only if K > L)
    // For k in [K, n-1]:   w_k = n-k        (ramp down)
    // This is correct for both cases L <= K and L > K (i.e., L > n/2).
    const int K = static_cast<int>(n) - L + 1;

    std::vector<double> w(n);
    for (std::size_t k = 0; k < n; ++k) {
        const int ik = static_cast<int>(k);
        const int a  = ik + 1;
        const int b  = L;
        const int c  = K;
        const int d  = static_cast<int>(n) - ik;
        // min of four values
        int wk = a < b ? a : b;
        if (c < wk) wk = c;
        if (d < wk) wk = d;
        w[k] = static_cast<double>(wk);
    }

    // Pre-compute weighted norms for each component.
    // ||F_i||_w^2 = <F_i, F_i>_w
    std::vector<double> norm2(static_cast<std::size_t>(L), 0.0);
    for (int i = 0; i < L; ++i) {
        const auto& fi = components[static_cast<std::size_t>(i)];
        double s = 0.0;
        for (std::size_t k = 0; k < n; ++k) {
            s += w[k] * fi[k] * fi[k];
        }
        norm2[static_cast<std::size_t>(i)] = s;
    }

    // Build symmetric L x L w-correlation matrix.
    Eigen::MatrixXd W(L, L);

    for (int i = 0; i < L; ++i) {
        W(i, i) = 1.0;   // diagonal is always 1

        const double ni2 = norm2[static_cast<std::size_t>(i)];
        const auto&  fi  = components[static_cast<std::size_t>(i)];

        for (int j = i + 1; j < L; ++j) {
            const double nj2 = norm2[static_cast<std::size_t>(j)];
            const auto&  fj  = components[static_cast<std::size_t>(j)];

            // Weighted inner product <F_i, F_j>_w
            double inner = 0.0;
            for (std::size_t k = 0; k < n; ++k) {
                inner += w[k] * fi[k] * fj[k];
            }

            // W[i][j] = |<F_i, F_j>_w| / (||F_i||_w * ||F_j||_w)
            const double denom = std::sqrt(ni2 * nj2);
            double wij = 0.0;
            if (denom > std::numeric_limits<double>::epsilon()) {
                wij = std::fabs(inner) / denom;
                // Clamp to [0, 1] for numerical safety
                if (wij > 1.0) wij = 1.0;
            }

            W(i, j) = wij;
            W(j, i) = wij;   // symmetric
        }
    }

    return W;
}

} // namespace loki::stats
