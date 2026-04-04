#include <loki/ssa/ssaAnalyzer.hpp>
#include <loki/ssa/ssaGrouper.hpp>
#include <loki/math/embedMatrix.hpp>
#include <loki/math/randomizedSvd.hpp>
#include <loki/stats/wCorrelation.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <string>

using namespace loki;
using namespace loki::ssa;

// ----------------------------------------------------------------------------
//  Construction
// ----------------------------------------------------------------------------

SsaAnalyzer::SsaAnalyzer(const AppConfig& cfg)
    : m_cfg{cfg}
{}

// ----------------------------------------------------------------------------
//  resolveWindowLength
// ----------------------------------------------------------------------------

int SsaAnalyzer::resolveWindowLength(std::size_t n) const
{
    const SsaWindowConfig& wcfg = m_cfg.ssa.window;
    const int halfN = static_cast<int>(n / 2);

    auto clamp = [&](int L) -> int {
        L = std::min(L, wcfg.maxWindowLength);
        L = std::min(L, halfN);
        L = std::max(L, 2);
        return L;
    };

    if (wcfg.windowLength > 0) {
        const int L = wcfg.windowLength;
        if (L < 2 || L > halfN || L > wcfg.maxWindowLength) {
            LOKI_WARNING("SsaAnalyzer: window_length=" + std::to_string(L)
                         + " violates constraints -- falling back to auto-selection.");
        } else {
            return L;
        }
    }

    int L = (wcfg.period > 0) ? wcfg.period * wcfg.periodMultiplier : halfN;
    return clamp(L);
}

// ----------------------------------------------------------------------------
//  analyze
// ----------------------------------------------------------------------------

SsaResult SsaAnalyzer::analyze(const std::vector<double>& y) const
{
    const std::size_t n = y.size();

    if (n < 3) {
        throw SeriesTooShortException(
            "SsaAnalyzer::analyze: series must have at least 3 observations, got "
            + std::to_string(n) + ".");
    }

    // -------------------------------------------------------------------------
    //  Step 1: Resolve window length L and effective SVD rank k
    // -------------------------------------------------------------------------

    const int L = resolveWindowLength(n);
    const int K = static_cast<int>(n) - L + 1;

    // svdRank == 0  -> full eigendecomposition (only feasible for small L)
    // svdRank >  0  -> randomized SVD, compute only first svdRank components
    const int svdRank = m_cfg.ssa.svdRank;
    const int minKL   = std::min(K, L);

    // Effective rank r: how many eigentriples we will compute
    const int r = (svdRank > 0)
                  ? std::min(svdRank, minKL - 1)   // -1: randomizedSvd requires k < min(m,n)
                  : minKL;

    LOKI_INFO("SsaAnalyzer: n=" + std::to_string(n)
              + "  L=" + std::to_string(L)
              + "  K=" + std::to_string(K)
              + "  svdRank=" + std::to_string(svdRank)
              + "  r=" + std::to_string(r));

    SsaResult result;
    result.n = n;
    result.L = L;
    result.K = K;

    // -------------------------------------------------------------------------
    //  Step 2: Build trajectory matrix X  (K x L)
    // -------------------------------------------------------------------------

    LOKI_INFO("SsaAnalyzer: building trajectory matrix...");
    const Eigen::MatrixXd X = loki::math::buildEmbedMatrix(y, L);

    // -------------------------------------------------------------------------
    //  Step 3: SVD -- randomized or full eigendecomposition
    //
    //  Randomized SVD (svdRank > 0):
    //    Complexity O(K * L * r) -- for K=35064, L=1461, r=40: ~2e9 ops
    //    Runtime: seconds on modern hardware.
    //
    //  Full eigendecomposition of C = X^T*X (svdRank == 0):
    //    Complexity O(K * L^2) -- only feasible for L < ~500.
    //    Use for small L or when all eigentriples are needed.
    //
    //  Result in both cases: sv (r), V (L x r), U computed on-the-fly below.
    // -------------------------------------------------------------------------

    Eigen::VectorXd sv;
    Eigen::MatrixXd V;   // L x r  (right singular vectors)
    Eigen::MatrixXd U;   // K x r  (left singular vectors)

    if (svdRank > 0) {
        LOKI_INFO("SsaAnalyzer: randomized SVD  k=" + std::to_string(r)
                  + "  oversampling=" + std::to_string(m_cfg.ssa.svdOversampling)
                  + "  powerIter=" + std::to_string(m_cfg.ssa.svdPowerIter) + "...");

        const loki::math::RandomizedSvdResult rsvd =
            loki::math::randomizedSvd(X, r,
                                      m_cfg.ssa.svdOversampling,
                                      m_cfg.ssa.svdPowerIter);
        sv = rsvd.sv;   // length r
        U  = rsvd.U;    // K x r
        V  = rsvd.V;    // L x r

    } else {
        LOKI_INFO("SsaAnalyzer: full eigendecomposition of C = X^T*X  ("
                  + std::to_string(L) + " x " + std::to_string(L) + ")...");

        const Eigen::MatrixXd C = X.transpose() * X;
        Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> eig(C);
        if (eig.info() != Eigen::Success) {
            throw AlgorithmException(
                "SsaAnalyzer::analyze: SelfAdjointEigenSolver failed to converge.");
        }

        // Reverse to descending order
        const Eigen::VectorXd sv2 = eig.eigenvalues().reverse();
        V  = eig.eigenvectors().rowwise().reverse();   // L x L
        sv = sv2.array().max(0.0).sqrt();

        // U_i = X * v_i / sv_i  (compute full U for all r=L components)
        U.resize(K, r);
        for (int i = 0; i < r; ++i) {
            if (sv[i] > std::numeric_limits<double>::epsilon()) {
                U.col(i) = X * V.col(i) / sv[i];
            } else {
                U.col(i).setZero();
            }
        }
    }

    LOKI_INFO("SsaAnalyzer: SVD complete  r=" + std::to_string(r)
              + "  sv[0]=" + std::to_string(sv[0])
              + "  sv[r-1]=" + std::to_string(sv[r - 1]));

    // -------------------------------------------------------------------------
    //  Step 4: Eigenvalues and variance fractions
    //  SSA eigenvalue_i = sv_i^2
    //  Note: if randomized SVD is used, varianceFractions are relative to the
    //  captured variance only (sum of first r eigenvalues), not total variance.
    //  We log this distinction clearly.
    // -------------------------------------------------------------------------

    result.eigenvalues.resize(static_cast<std::size_t>(r));
    double capturedVar = 0.0;
    for (int i = 0; i < r; ++i) {
        result.eigenvalues[static_cast<std::size_t>(i)] = sv[i] * sv[i];
        capturedVar += sv[i] * sv[i];
    }

    if (capturedVar < std::numeric_limits<double>::epsilon()) {
        throw AlgorithmException(
            "SsaAnalyzer::analyze: captured variance is effectively zero.");
    }

    result.varianceFractions.resize(static_cast<std::size_t>(r));
    for (int i = 0; i < r; ++i) {
        result.varianceFractions[static_cast<std::size_t>(i)] =
            result.eigenvalues[static_cast<std::size_t>(i)] / capturedVar;
    }

    if (svdRank > 0) {
        LOKI_INFO("SsaAnalyzer: variance fractions are relative to captured "
                  "variance (first " + std::to_string(r) + " components).");
    }

    {
        std::ostringstream oss;
        const int nLog = std::min(r, 10);
        oss << "SsaAnalyzer: variance fractions (first " << nLog << "):";
        double cumVar = 0.0;
        for (int i = 0; i < nLog; ++i) {
            cumVar += result.varianceFractions[static_cast<std::size_t>(i)];
            oss << "  [" << i << "]="
                << result.varianceFractions[static_cast<std::size_t>(i)];
        }
        oss << "  cumul=" << cumVar;
        LOKI_INFO(oss.str());
    }

    // -------------------------------------------------------------------------
    //  Step 5: Elementary reconstructed components
    //
    //  E_i = sv[i] * U[:,i] * V[:,i]^T   (K x L elementary matrix)
    //  F_i = diagonal_average(E_i)        (length n)
    //
    //  Diagonal averaging is inlined to avoid materialising all K*L*r elements.
    //  For each eigentriple i, we iterate over the n anti-diagonals of E_i,
    //  accumulating the weighted sum using only U[:,i] and V[:,i].
    // -------------------------------------------------------------------------

    LOKI_INFO("SsaAnalyzer: computing elementary components...");

    result.components.resize(static_cast<std::size_t>(r));

    // Pre-compute Hankel weights (shared across all components)
    const int ni = static_cast<int>(n);   // K + L - 1 == n
    std::vector<double> hW(static_cast<std::size_t>(ni));
    for (int k = 0; k < ni; ++k) {
        const int a = k + 1;
        const int b = L;
        const int c = K;
        const int d = ni - k;
        int wk = a < b ? a : b;
        if (c < wk) wk = c;
        if (d < wk) wk = d;
        hW[static_cast<std::size_t>(k)] = static_cast<double>(wk);
    }

    const bool useSimple = (m_cfg.ssa.reconstruction.method == "simple");

    for (int i = 0; i < r; ++i) {
        std::vector<double> Fi(static_cast<std::size_t>(ni), 0.0);

        if (sv[i] > std::numeric_limits<double>::epsilon()) {
            const double svi      = sv[i];
            const auto&  ui_col   = U.col(i);   // K-vector view
            const auto&  vi_col   = V.col(i);   // L-vector view

            if (useSimple) {
                for (int row = 0; row < K; ++row) {
                    Fi[static_cast<std::size_t>(row)] =
                        svi * ui_col[row] * vi_col[0];
                }
                for (int col = 1; col < L; ++col) {
                    Fi[static_cast<std::size_t>(K - 1 + col)] =
                        svi * ui_col[K - 1] * vi_col[col];
                }
            } else {
                // Diagonal averaging
                for (int k = 0; k < ni; ++k) {
                    const int rMin = std::max(0, k - L + 1);
                    const int rMax = std::min(k, K - 1);
                    double sum = 0.0;
                    for (int row = rMin; row <= rMax; ++row) {
                        sum += svi * ui_col[row] * vi_col[k - row];
                    }
                    Fi[static_cast<std::size_t>(k)] =
                        sum / hW[static_cast<std::size_t>(k)];
                }
            }
        }

        result.components[static_cast<std::size_t>(i)] = std::move(Fi);
    }

    LOKI_INFO("SsaAnalyzer: computed " + std::to_string(r)
              + " elementary components.");

    // -------------------------------------------------------------------------
    //  Step 6: W-correlation matrix
    //  Computed only on the first wcorrMaxComponents components to limit cost.
    //  Full r x r w-corr for r=1461 would be O(r^2 * n) -- prohibitive.
    // -------------------------------------------------------------------------

    if (m_cfg.ssa.computeWCorr) {
        try {
            const int rWCorr = (m_cfg.ssa.wcorrMaxComponents > 0)
                               ? std::min(m_cfg.ssa.wcorrMaxComponents, r)
                               : r;

            LOKI_INFO("SsaAnalyzer: computing w-correlation matrix ("
                      + std::to_string(rWCorr) + " x "
                      + std::to_string(rWCorr) + ")...");

            // Slice to first rWCorr components
            const std::vector<std::vector<double>> compsSlice(
                result.components.begin(),
                result.components.begin() + rWCorr);

            const Eigen::MatrixXd W =
                loki::stats::computeWCorrelation(compsSlice, rWCorr);

            result.wCorrMatrix.resize(
                static_cast<std::size_t>(rWCorr),
                std::vector<double>(static_cast<std::size_t>(rWCorr)));
            for (int i = 0; i < rWCorr; ++i) {
                for (int j = 0; j < rWCorr; ++j) {
                    result.wCorrMatrix[static_cast<std::size_t>(i)]
                                      [static_cast<std::size_t>(j)] = W(i, j);
                }
            }
            LOKI_INFO("SsaAnalyzer: w-correlation matrix done.");
        } catch (const LOKIException& ex) {
            LOKI_WARNING("SsaAnalyzer: w-correlation failed: "
                         + std::string(ex.what()) + " -- skipping.");
        }
    }

    // -------------------------------------------------------------------------
    //  Step 7: Grouping
    // -------------------------------------------------------------------------

    SsaGrouper grouper(m_cfg);
    grouper.group(result);

    // -------------------------------------------------------------------------
    //  Step 8: Per-group reconstruction sums + convenience aliases
    // -------------------------------------------------------------------------

    for (auto& g : result.groups) {
        g.reconstruction.assign(n, 0.0);
        for (int idx : g.indices) {
            const auto& comp = result.components[static_cast<std::size_t>(idx)];
            for (std::size_t k = 0; k < n; ++k) {
                g.reconstruction[k] += comp[k];
            }
        }
    }

    result.trend.clear();
    result.noise.clear();
    for (const auto& g : result.groups) {
        if (g.name == "trend") result.trend = g.reconstruction;
        if (g.name == "noise") result.noise = g.reconstruction;
    }

    LOKI_INFO("SsaAnalyzer: pipeline complete.");
    return result;
}
