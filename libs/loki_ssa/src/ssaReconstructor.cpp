#include <loki/ssa/ssaReconstructor.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <algorithm>
#include <cmath>
#include <string>

using namespace loki;
using namespace loki::ssa;

// ----------------------------------------------------------------------------
//  Construction
// ----------------------------------------------------------------------------

SsaReconstructor::SsaReconstructor(const AppConfig& cfg)
    : m_cfg{cfg}
{}

// ----------------------------------------------------------------------------
//  reconstruct
// ----------------------------------------------------------------------------

void SsaReconstructor::reconstruct(SsaResult&             result,
                                    const Eigen::MatrixXd& U,
                                    const Eigen::VectorXd& sv,
                                    const Eigen::MatrixXd& V) const
{
    const int K = result.K;
    const int L = result.L;
    const int n = static_cast<int>(result.n);
    const int r = static_cast<int>(sv.size());

    if (U.rows() != K || U.cols() != r) {
        throw AlgorithmException(
            "SsaReconstructor: U must be " + std::to_string(K) + " x "
            + std::to_string(r) + ", got "
            + std::to_string(U.rows()) + " x " + std::to_string(U.cols()) + ".");
    }
    if (V.rows() != L || V.cols() != r) {
        throw AlgorithmException(
            "SsaReconstructor: V must be " + std::to_string(L) + " x "
            + std::to_string(r) + ", got "
            + std::to_string(V.rows()) + " x " + std::to_string(V.cols()) + ".");
    }
    if (K + L - 1 != n) {
        throw AlgorithmException(
            "SsaReconstructor: K + L - 1 = "
            + std::to_string(K + L - 1)
            + " but n = " + std::to_string(n) + ".");
    }

    const std::string& method = m_cfg.ssa.reconstruction.method;

    // -------------------------------------------------------------------------
    //  Step 1: Elementary reconstructed series for all r eigentriples
    // -------------------------------------------------------------------------

    result.components.resize(static_cast<std::size_t>(r));

    for (int i = 0; i < r; ++i) {
        // Elementary matrix E_i = s_i * u_i * v_i^T  (K x L)
        const Eigen::MatrixXd Ei =
            sv[i] * U.col(i) * V.col(i).transpose();

        if (method == "simple") {
            result.components[static_cast<std::size_t>(i)] =
                _simpleReconstruct(Ei, K, L);
        } else {
            // "diagonal_averaging" is the default
            result.components[static_cast<std::size_t>(i)] =
                _diagonalAverage(Ei, K, L);
        }
    }

    LOKI_INFO("SsaReconstructor: computed " + std::to_string(r)
              + " elementary components  method=" + method);

    // -------------------------------------------------------------------------
    //  Step 2: Per-group reconstruction = sum of assigned components
    // -------------------------------------------------------------------------

    for (auto& g : result.groups) {
        g.reconstruction.assign(static_cast<std::size_t>(n), 0.0);

        for (int idx : g.indices) {
            if (idx < 0 || idx >= r) {
                throw AlgorithmException(
                    "SsaReconstructor: group '" + g.name
                    + "' contains out-of-range eigentriple index "
                    + std::to_string(idx) + ".");
            }
            const auto& comp = result.components[static_cast<std::size_t>(idx)];
            for (int k = 0; k < n; ++k) {
                g.reconstruction[static_cast<std::size_t>(k)] +=
                    comp[static_cast<std::size_t>(k)];
            }
        }
    }

    // -------------------------------------------------------------------------
    //  Step 3: Fill convenience aliases from named groups
    // -------------------------------------------------------------------------

    result.trend.clear();
    result.noise.clear();

    for (const auto& g : result.groups) {
        if (g.name == "trend") result.trend = g.reconstruction;
        if (g.name == "noise") result.noise = g.reconstruction;
    }

    LOKI_INFO("SsaReconstructor: filled " + std::to_string(result.groups.size())
              + " group reconstructions.");
}

// ----------------------------------------------------------------------------
//  _diagonalAverage
// ----------------------------------------------------------------------------

std::vector<double> SsaReconstructor::_diagonalAverage(const Eigen::MatrixXd& E,
                                                         int K,
                                                         int L)
{
    // n = K + L - 1
    const int n = K + L - 1;

    // Hankel weights: w_k = min(k+1, L, K, n-k)
    // Same formula as in wCorrelation.cpp.
    std::vector<double> w(static_cast<std::size_t>(n));
    for (int k = 0; k < n; ++k) {
        const int a = k + 1;
        const int b = L;
        const int c = K;
        const int d = n - k;
        int wk = a < b ? a : b;
        if (c < wk) wk = c;
        if (d < wk) wk = d;
        w[static_cast<std::size_t>(k)] = static_cast<double>(wk);
    }

    // Diagonal averaging:
    // F[k] = (1 / w_k) * sum_{r+c=k, 0<=r<K, 0<=c<L} E(r, c)
    std::vector<double> F(static_cast<std::size_t>(n), 0.0);

    for (int k = 0; k < n; ++k) {
        // r ranges over max(0, k-L+1) .. min(k, K-1)
        const int rMin = std::max(0, k - L + 1);
        const int rMax = std::min(k, K - 1);

        double sum = 0.0;
        for (int row = rMin; row <= rMax; ++row) {
            const int col = k - row;
            sum += E(row, col);
        }
        F[static_cast<std::size_t>(k)] = sum / w[static_cast<std::size_t>(k)];
    }

    return F;
}

// ----------------------------------------------------------------------------
//  _simpleReconstruct
// ----------------------------------------------------------------------------

std::vector<double> SsaReconstructor::_simpleReconstruct(const Eigen::MatrixXd& E,
                                                           int K,
                                                           int L)
{
    // Simple: take the first column of E (the L trajectory windows starting
    // at each position) as the reconstruction. Specifically:
    //   F[i]     = E(i, 0)       for i = 0..K-1    (first col of each row)
    //   F[K-1+j] = E(K-1, j)     for j = 1..L-1    (last row remaining cols)
    // This stitches the K rows and the trailing (L-1) tail.

    const int n = K + L - 1;
    std::vector<double> F(static_cast<std::size_t>(n), 0.0);

    for (int i = 0; i < K; ++i) {
        F[static_cast<std::size_t>(i)] = E(i, 0);
    }
    for (int j = 1; j < L; ++j) {
        F[static_cast<std::size_t>(K - 1 + j)] = E(K - 1, j);
    }

    return F;
}
