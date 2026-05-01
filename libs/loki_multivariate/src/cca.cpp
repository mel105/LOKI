#include "loki/multivariate/cca.hpp"

#include "loki/core/exceptions.hpp"
#include "loki/core/logger.hpp"

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace loki::multivariate;

// -----------------------------------------------------------------------------
//  Construction
// -----------------------------------------------------------------------------

Cca::Cca(const MultivariateCcaConfig& cfg)
    : m_cfg(cfg)
{}

// -----------------------------------------------------------------------------
//  compute
// -----------------------------------------------------------------------------

CcaResult Cca::compute(const MultivariateSeries& data) const
{
    if (data.empty()) {
        throw DataException("Cca::compute(): data is empty.");
    }

    const int n = static_cast<int>(data.nObs());
    const int P = static_cast<int>(data.nChannels());

    // Validate groups.
    if (m_cfg.groupX.empty() || m_cfg.groupY.empty()) {
        throw DataException("Cca: group_x and group_y must not be empty.");
    }
    for (int idx : m_cfg.groupX) {
        if (idx < 0 || idx >= P) {
            throw DataException(
                "Cca: group_x index " + std::to_string(idx) + " out of range.");
        }
    }
    for (int idx : m_cfg.groupY) {
        if (idx < 0 || idx >= P) {
            throw DataException(
                "Cca: group_y index " + std::to_string(idx) + " out of range.");
        }
    }
    // Check for overlap.
    for (int xi : m_cfg.groupX) {
        for (int yi : m_cfg.groupY) {
            if (xi == yi) {
                throw DataException(
                    "Cca: channel " + std::to_string(xi)
                    + " appears in both group_x and group_y.");
            }
        }
    }

    const int p = static_cast<int>(m_cfg.groupX.size());
    const int q = static_cast<int>(m_cfg.groupY.size());
    const int maxPairs = std::min(p, q);
    const int k = (m_cfg.nComponents > 0)
        ? std::min(m_cfg.nComponents, maxPairs)
        : maxPairs;

    // -- Step 1: extract and standardise sub-matrices -------------------------
    Eigen::MatrixXd X(n, p), Y(n, q);
    for (int j = 0; j < p; ++j) {
        X.col(j) = data.data().col(m_cfg.groupX[static_cast<std::size_t>(j)]);
    }
    for (int j = 0; j < q; ++j) {
        Y.col(j) = data.data().col(m_cfg.groupY[static_cast<std::size_t>(j)]);
    }

    // Standardise.
    const Eigen::VectorXd mx = X.colwise().mean();
    const Eigen::VectorXd my = Y.colwise().mean();
    X = X.rowwise() - mx.transpose();
    Y = Y.rowwise() - my.transpose();
    for (int j = 0; j < p; ++j) {
        const double s = std::sqrt(X.col(j).squaredNorm() / (n - 1));
        if (s > 0.0) X.col(j) /= s;
    }
    for (int j = 0; j < q; ++j) {
        const double s = std::sqrt(Y.col(j).squaredNorm() / (n - 1));
        if (s > 0.0) Y.col(j) /= s;
    }

    // -- Step 2: covariance matrices ------------------------------------------
    const double invN = 1.0 / static_cast<double>(n - 1);
    const Eigen::MatrixXd Sxx = (X.transpose() * X) * invN;
    const Eigen::MatrixXd Syy = (Y.transpose() * Y) * invN;
    const Eigen::MatrixXd Sxy = (X.transpose() * Y) * invN;

    // -- Step 3: K = Sxx^{-1/2} * Sxy * Syy^{-1/2} --------------------------
    const Eigen::MatrixXd SxxInvSqrt = _invSqrtMat(Sxx);
    const Eigen::MatrixXd SyyInvSqrt = _invSqrtMat(Syy);
    const Eigen::MatrixXd K = SxxInvSqrt * Sxy * SyyInvSqrt;

    // -- Step 4: SVD of K -----------------------------------------------------
    Eigen::JacobiSVD<Eigen::MatrixXd> svd(K, Eigen::ComputeThinU | Eigen::ComputeThinV);
    const Eigen::VectorXd rho = svd.singularValues().head(k);
    const Eigen::MatrixXd U   = svd.matrixU().leftCols(k);
    const Eigen::MatrixXd V   = svd.matrixV().leftCols(k);

    // -- Step 5: canonical weights --------------------------------------------
    // a = Sxx^{-1/2} * U   (p x k)
    // b = Syy^{-1/2} * V   (q x k)
    const Eigen::MatrixXd a = SxxInvSqrt * U;
    const Eigen::MatrixXd b = SyyInvSqrt * V;

    // -- Step 6: canonical scores ---------------------------------------------
    const Eigen::MatrixXd scoresX = X * a; // (n x k)
    const Eigen::MatrixXd scoresY = Y * b; // (n x k)

    LOKI_INFO("Cca: computed " + std::to_string(k)
              + " canonical pairs, rho_1="
              + std::to_string(rho(0)) + ".");

    // -- Step 7: assemble result ----------------------------------------------
    CcaResult result;
    result.canonicalCorrelations = rho;
    result.weightsX  = a;
    result.weightsY  = b;
    result.scoresX   = scoresX;
    result.scoresY   = scoresY;
    result.nPairs    = k;
    result.nObs      = n;
    result.groupXIdx = m_cfg.groupX;
    result.groupYIdx = m_cfg.groupY;

    return result;
}

// -----------------------------------------------------------------------------
//  _sqrtMat / _invSqrtMat
// -----------------------------------------------------------------------------

Eigen::MatrixXd Cca::_sqrtMat(const Eigen::MatrixXd& S)
{
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> eig(S);
    const Eigen::VectorXd sqrtEv =
        eig.eigenvalues().array().max(0.0).sqrt();
    return eig.eigenvectors() * sqrtEv.asDiagonal()
         * eig.eigenvectors().transpose();
}

Eigen::MatrixXd Cca::_invSqrtMat(const Eigen::MatrixXd& S)
{
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> eig(S);
    const Eigen::VectorXd ev = eig.eigenvalues();
    Eigen::VectorXd invSqrt(ev.size());
    for (Eigen::Index i = 0; i < ev.size(); ++i) {
        invSqrt(i) = (ev(i) > 1.0e-12) ? (1.0 / std::sqrt(ev(i))) : 0.0;
    }
    return eig.eigenvectors() * invSqrt.asDiagonal()
         * eig.eigenvectors().transpose();
}
