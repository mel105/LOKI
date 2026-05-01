#include "loki/multivariate/pca.hpp"

#include "loki/core/exceptions.hpp"
#include "loki/core/logger.hpp"
#include "loki/math/randomizedSvd.hpp"

#include <Eigen/SVD>

#include <cmath>
#include <numeric>
#include <string>

using namespace loki::multivariate;

// -----------------------------------------------------------------------------
//  Construction
// -----------------------------------------------------------------------------

Pca::Pca(const MultivariatePcaConfig& cfg)
    : m_cfg(cfg)
{}

// -----------------------------------------------------------------------------
//  compute
// -----------------------------------------------------------------------------

PcaResult Pca::compute(const MultivariateSeries& data) const
{
    if (data.empty()) {
        throw DataException("Pca::compute(): data series is empty.");
    }
    if (data.nChannels() < 2) {
        throw DataException(
            "Pca::compute(): at least 2 channels required, got "
            + std::to_string(data.nChannels()) + ".");
    }

    const Eigen::Index n = data.nObs();
    const Eigen::Index p = data.nChannels();

    // -- Step 1: column-centre the data matrix --------------------------------
    // Compute column means (channel means).
    const Eigen::VectorXd mean = data.data().colwise().mean();
    const Eigen::MatrixXd Xc  = data.data().rowwise() - mean.transpose();

    LOKI_INFO("Pca: centred data matrix (" + std::to_string(n)
              + " x " + std::to_string(p) + ").");

    // -- Step 2: SVD of centred matrix ----------------------------------------
    // X = U * S * V^T
    // Loadings = V  (p x k)
    // Scores   = U * S  (n x k)

    Eigen::VectorXd sv;
    Eigen::MatrixXd V;
    Eigen::MatrixXd U;

    const int maxK = static_cast<int>(std::min(n, p));

    if (m_cfg.useRandomizedSvd) {
        // Rank for randomized SVD: if nComponents == 0 use maxK, else nComponents.
        const int k = (m_cfg.nComponents > 0)
            ? std::min(m_cfg.nComponents, maxK)
            : maxK;

        LOKI_INFO("Pca: using randomized SVD, rank=" + std::to_string(k)
                  + " oversampling=" + std::to_string(m_cfg.randomizedSvdOversampling) + ".");

        auto res = loki::math::randomizedSvd(
            Xc, k, m_cfg.randomizedSvdOversampling, /*nPowerIter=*/2);

        sv = std::move(res.sv);
        U  = std::move(res.U);
        V  = std::move(res.V);
    } else {
        // JacobiSVD -- exact, safe in .cpp compiled into static lib on Windows/GCC 13.
        LOKI_INFO("Pca: using JacobiSVD (exact).");
        Eigen::JacobiSVD<Eigen::MatrixXd> jsvd(
            Xc, Eigen::ComputeThinU | Eigen::ComputeThinV);
        sv = jsvd.singularValues();
        U  = jsvd.matrixU();
        V  = jsvd.matrixV();
    }

    // -- Step 3: explained variance ratio -------------------------------------
    // Variance explained by component i = sv_i^2 / (n - 1)
    // Ratio_i = sv_i^2 / sum(sv_j^2)
    const Eigen::VectorXd sv2  = sv.array().square();
    const double          tot  = sv2.sum();
    Eigen::VectorXd       ratio(sv.size());
    if (tot > 0.0) {
        ratio = sv2 / tot;
    } else {
        ratio.setZero();
    }

    // Cumulative variance.
    Eigen::VectorXd cumVar(sv.size());
    double running = 0.0;
    for (Eigen::Index i = 0; i < sv.size(); ++i) {
        running  += ratio(i);
        cumVar(i) = running;
    }

    // -- Step 4: select number of components ----------------------------------
    const int k = _selectNComponents(ratio, static_cast<int>(sv.size()));

    LOKI_INFO("Pca: retaining " + std::to_string(k) + " components ("
              + std::to_string(static_cast<int>(cumVar(k - 1) * 100.0))
              + "% variance).");

    // -- Step 5: assemble result ----------------------------------------------
    PcaResult result;
    result.nObs      = static_cast<int>(n);
    result.nChannels = static_cast<int>(p);
    result.nComponents = k;
    result.mean        = mean;

    result.loadings          = V.leftCols(k);                         // (p x k)
    result.scores            = (U * sv.asDiagonal()).leftCols(k);     // (n x k)
    result.explainedVar      = sv2.head(k) / static_cast<double>(n - 1);
    result.explainedVarRatio = ratio.head(k);
    result.cumulativeVar     = cumVar.head(k);

    return result;
}

// -----------------------------------------------------------------------------
//  _selectNComponents
// -----------------------------------------------------------------------------

int Pca::_selectNComponents(const Eigen::VectorXd& ratio, int maxK) const
{
    if (m_cfg.nComponents > 0) {
        const int k = std::min(m_cfg.nComponents, maxK);
        if (k < m_cfg.nComponents) {
            LOKI_WARNING("Pca: requested n_components=" + std::to_string(m_cfg.nComponents)
                         + " exceeds rank " + std::to_string(maxK)
                         + " -- clamped to " + std::to_string(k) + ".");
        }
        return k;
    }

    // Auto: find smallest k s.t. cumulative variance >= threshold.
    double cumul = 0.0;
    for (int i = 0; i < maxK; ++i) {
        cumul += ratio(i);
        if (cumul >= m_cfg.varianceThreshold) {
            return i + 1;
        }
    }
    return maxK; // threshold not reached -- keep all
}
