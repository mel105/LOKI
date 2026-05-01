#include "loki/multivariate/mssa.hpp"

#include "loki/core/exceptions.hpp"
#include "loki/core/logger.hpp"
#include "loki/math/embedMatrix.hpp"
#include "loki/math/randomizedSvd.hpp"

#include <Eigen/SVD>

#include <cmath>
#include <string>
#include <vector>

using namespace loki::multivariate;

// -----------------------------------------------------------------------------
//  Construction
// -----------------------------------------------------------------------------

Mssa::Mssa(const MultivariateMssaConfig& cfg)
    : m_cfg(cfg)
{}

// -----------------------------------------------------------------------------
//  compute
// -----------------------------------------------------------------------------

MssaResult Mssa::compute(const MultivariateSeries& data) const
{
    if (data.empty()) {
        throw DataException("Mssa::compute(): data series is empty.");
    }
    if (data.nChannels() < 2) {
        throw DataException(
            "Mssa::compute(): at least 2 channels required, got "
            + std::to_string(data.nChannels()) + ".");
    }

    const int n = static_cast<int>(data.nObs());
    const int p = static_cast<int>(data.nChannels());
    const int L = m_cfg.window;

    if (L < 2 || L >= n) {
        throw AlgorithmException(
            "Mssa::compute(): window L=" + std::to_string(L)
            + " must satisfy 2 <= L < n=" + std::to_string(n) + ".");
    }

    const int K = n - L + 1; // number of lagged windows

    LOKI_INFO("Mssa: n=" + std::to_string(n)
              + " p=" + std::to_string(p)
              + " L=" + std::to_string(L)
              + " K=" + std::to_string(K)
              + " block matrix shape (" + std::to_string(K)
              + " x " + std::to_string(L * p) + ").");

    // -- Step 1: build block trajectory matrix (K x L*p) ---------------------
    // Stack per-channel Hankel matrices horizontally:
    //   X_block = [ X_1 | X_2 | ... | X_p ]
    // where X_j is the (K x L) Hankel matrix for channel j.

    Eigen::MatrixXd Xblock(K, L * p);

    for (int j = 0; j < p; ++j) {
        // Extract channel j as std::vector<double>.
        const Eigen::VectorXd col = data.data().col(j);
        std::vector<double> y(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) {
            y[static_cast<std::size_t>(i)] = col(i);
        }

        // Build (K x L) Hankel matrix for this channel.
        const Eigen::MatrixXd Xj = loki::math::buildEmbedMatrix(y, L);
        Xblock.block(0, j * L, K, L) = Xj;
    }

    // -- Step 2: SVD of block trajectory matrix -------------------------------
    const int maxK = std::min(K, L * p);
    const int kReq = std::min(m_cfg.nComponents, maxK);

    Eigen::VectorXd sv;
    Eigen::MatrixXd U;
    Eigen::MatrixXd V;

    if (m_cfg.useRandomizedSvd) {
        LOKI_INFO("Mssa: using randomized SVD, rank=" + std::to_string(kReq) + ".");
        auto res = loki::math::randomizedSvd(Xblock, kReq, /*nOversampling=*/10, /*nPowerIter=*/2);
        sv = std::move(res.sv);
        U  = std::move(res.U);
        V  = std::move(res.V);
    } else {
        LOKI_INFO("Mssa: using JacobiSVD (exact).");
        Eigen::JacobiSVD<Eigen::MatrixXd> jsvd(
            Xblock, Eigen::ComputeThinU | Eigen::ComputeThinV);
        sv = jsvd.singularValues();
        U  = jsvd.matrixU();
        V  = jsvd.matrixV();
        // Truncate to kReq.
        sv = sv.head(kReq).eval();
        U  = U.leftCols(kReq).eval();
        V  = V.leftCols(kReq).eval();
    }

    // -- Step 3: explained variance ratio -------------------------------------
    const Eigen::VectorXd sv2  = sv.array().square();
    const double          tot  = sv2.sum();
    Eigen::VectorXd       ratio(kReq);
    if (tot > 0.0) {
        ratio = sv2 / tot;
    } else {
        ratio.setZero();
    }

    Eigen::VectorXd cumVar(kReq);
    double running = 0.0;
    for (int i = 0; i < kReq; ++i) {
        running   += ratio(i);
        cumVar(i)  = running;
    }

    LOKI_INFO("Mssa: " + std::to_string(kReq) + " components explain "
              + std::to_string(static_cast<int>(cumVar(kReq - 1) * 100.0))
              + "% of variance in block trajectory matrix.");

    // -- Step 4: reconstruct each component by diagonal averaging -------------
    // For component i, the elementary block matrix is:
    //   E_i = sv_i * U_i * V_i^T   (K x L*p)
    // We split E_i into p sub-blocks of (K x L), apply diagonal averaging
    // to each sub-block to get a series of length n, then sum over channels.

    Eigen::MatrixXd reconstruction(n, kReq);

    for (int i = 0; i < kReq; ++i) {
        // Elementary block matrix for component i.
        const Eigen::MatrixXd Ei =
            sv(i) * U.col(i) * V.col(i).transpose(); // (K x L*p)

        Eigen::VectorXd compSum = Eigen::VectorXd::Zero(n);

        for (int j = 0; j < p; ++j) {
            // Sub-block for channel j.
            const Eigen::MatrixXd subBlock = Ei.block(0, j * L, K, L);
            // Diagonal average -> series of length n.
            const Eigen::VectorXd rec = _diagonalAverage(subBlock, n);
            compSum += rec;
        }

        reconstruction.col(i) = compSum;
    }

    // -- Step 5: assemble result ----------------------------------------------
    MssaResult result;
    result.eigenvalues        = sv;
    result.explainedVarRatio  = ratio;
    result.cumulativeVar      = cumVar;
    result.reconstruction     = reconstruction;
    result.window             = L;
    result.nComponents        = kReq;
    result.nObs               = n;
    result.nChannels          = p;

    return result;
}

// -----------------------------------------------------------------------------
//  _diagonalAverage
// -----------------------------------------------------------------------------

Eigen::VectorXd Mssa::_diagonalAverage(const Eigen::MatrixXd& mat, int n)
{
    const int K = static_cast<int>(mat.rows());
    const int L = static_cast<int>(mat.cols());
    // Sanity: n should equal K + L - 1.

    Eigen::VectorXd out = Eigen::VectorXd::Zero(n);

    for (int t = 0; t < n; ++t) {
        // Anti-diagonal t: collect pairs (i, j) with i + j = t.
        double sum   = 0.0;
        int    count = 0;

        // i ranges from max(0, t-L+1) to min(K-1, t).
        const int iMin = std::max(0, t - L + 1);
        const int iMax = std::min(K - 1, t);

        for (int i = iMin; i <= iMax; ++i) {
            const int j = t - i;
            if (j >= 0 && j < L) {
                sum   += mat(i, j);
                ++count;
            }
        }

        out(t) = (count > 0) ? sum / count : 0.0;
    }

    return out;
}
