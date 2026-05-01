#include "loki/multivariate/factorAnalysis.hpp"

#include "loki/core/exceptions.hpp"
#include "loki/core/logger.hpp"

#include <Eigen/Dense>

#include <cmath>
#include <string>

using namespace loki::multivariate;

// -----------------------------------------------------------------------------
//  Construction
// -----------------------------------------------------------------------------

FactorAnalysis::FactorAnalysis(const MultivariateFactorConfig& cfg)
    : m_cfg(cfg)
{}

// -----------------------------------------------------------------------------
//  compute
// -----------------------------------------------------------------------------

FactorAnalysisResult FactorAnalysis::compute(const MultivariateSeries& data) const
{
    if (data.empty()) {
        throw DataException("FactorAnalysis::compute(): data is empty.");
    }

    const int n = static_cast<int>(data.nObs());
    const int p = static_cast<int>(data.nChannels());
    const int k = m_cfg.nFactors;

    if (k < 1 || k >= p) {
        throw DataException(
            "FactorAnalysis: n_factors=" + std::to_string(k)
            + " must be in [1, p-1] where p=" + std::to_string(p) + ".");
    }
    if (n < p) {
        throw DataException(
            "FactorAnalysis: n=" + std::to_string(n)
            + " must be >= p=" + std::to_string(p) + ".");
    }

    // -- Step 1: correlation matrix ------------------------------------------
    // Standardise columns (zero mean, unit variance).
    const Eigen::VectorXd mean = data.data().colwise().mean();
    Eigen::MatrixXd Xs = data.data().rowwise() - mean.transpose();
    for (int j = 0; j < p; ++j) {
        const double sd = std::sqrt(Xs.col(j).squaredNorm() / (n - 1));
        if (sd > 0.0) Xs.col(j) /= sd;
    }
    const Eigen::MatrixXd R = (Xs.transpose() * Xs) / static_cast<double>(n - 1);

    // -- Step 2: initial communalities (SMC) ---------------------------------
    Eigen::VectorXd h2 = _initialCommunalities(R);

    // -- Step 3: iterative PAF -----------------------------------------------
    Eigen::MatrixXd loadings;
    int iter = 0;
    for (; iter < m_cfg.maxIter; ++iter) {
        Eigen::MatrixXd Rr = R;
        for (int j = 0; j < p; ++j) {
            Rr(j, j) = h2(j); // reduced correlation matrix
        }

        // Eigen-decomposition (symmetric).
        Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> eig(Rr);
        // Eigenvalues in ascending order -- take top k.
        const int total = static_cast<int>(eig.eigenvalues().size());
        Eigen::MatrixXd Lk(p, k);
        for (int i = 0; i < k; ++i) {
            const int idx = total - 1 - i; // descending
            const double ev = eig.eigenvalues()(idx);
            const double sv = (ev > 0.0) ? std::sqrt(ev) : 0.0;
            Lk.col(i) = eig.eigenvectors().col(idx) * sv;
        }

        // Update communalities.
        const Eigen::VectorXd h2new = Lk.rowwise().squaredNorm();
        const double delta = (h2new - h2).cwiseAbs().maxCoeff();
        h2       = h2new;
        loadings = Lk;

        if (delta < m_cfg.tolerance) break;
    }

    LOKI_INFO("FactorAnalysis: converged in " + std::to_string(iter) + " iterations.");

    // -- Step 4: uniquenesses ------------------------------------------------
    Eigen::VectorXd uniqueness(p);
    for (int j = 0; j < p; ++j) {
        uniqueness(j) = std::max(0.0, 1.0 - h2(j));
    }

    // -- Step 5: Varimax rotation --------------------------------------------
    Eigen::MatrixXd rotatedLoadings = loadings;
    Eigen::MatrixXd rotationMatrix  = Eigen::MatrixXd::Identity(k, k);

    if (m_cfg.rotation == "varimax" && k > 1) {
        _varimax(rotatedLoadings, m_cfg.maxIter, m_cfg.tolerance);
        // Rotation matrix: R = L_orig^+ * L_rot  (least-squares)
        rotationMatrix = loadings.colPivHouseholderQr().solve(rotatedLoadings);
    }

    // -- Step 6: factor scores (regression method) ---------------------------
    // F = Xs * L * (L^T * L)^{-1}  -- approximate Thompson-factor scores.
    const Eigen::MatrixXd& L = rotatedLoadings;
    const Eigen::MatrixXd LtL = L.transpose() * L;
    Eigen::MatrixXd scores =
        Xs * L * LtL.colPivHouseholderQr().solve(
            Eigen::MatrixXd::Identity(k, k));

    // -- Step 7: assemble result ---------------------------------------------
    FactorAnalysisResult result;
    result.loadings        = rotatedLoadings;
    result.scores          = scores;
    result.communalities   = h2;
    result.uniqueness      = uniqueness;
    result.rotationMatrix  = rotationMatrix;
    result.nFactors        = k;
    result.nObs            = n;
    result.nChannels       = p;
    result.rotationMethod  = m_cfg.rotation;

    return result;
}

// -----------------------------------------------------------------------------
//  _initialCommunalities
// -----------------------------------------------------------------------------

Eigen::VectorXd FactorAnalysis::_initialCommunalities(const Eigen::MatrixXd& R)
{
    const int p = static_cast<int>(R.rows());
    Eigen::VectorXd h2(p);

    // h2_j = 1 - 1 / R^{-1}_{jj}
    // Compute R^{-1} via LLT (positive definite check) or LDLT.
    const Eigen::MatrixXd Rinv = R.ldlt().solve(
        Eigen::MatrixXd::Identity(p, p));

    for (int j = 0; j < p; ++j) {
        const double diag = Rinv(j, j);
        h2(j) = (diag > 1.0) ? (1.0 - 1.0 / diag) : 0.5; // fallback for near-singular R
        h2(j) = std::max(0.01, std::min(0.99, h2(j)));
    }

    return h2;
}

// -----------------------------------------------------------------------------
//  _varimax
// -----------------------------------------------------------------------------

void FactorAnalysis::_varimax(Eigen::MatrixXd& L, int maxIter, double tol)
{
    const int p = static_cast<int>(L.rows());
    const int k = static_cast<int>(L.cols());

    // Normalise rows by communalities (Kaiser normalisation).
    Eigen::VectorXd h = L.rowwise().norm();
    for (int j = 0; j < p; ++j) {
        if (h(j) > 0.0) L.row(j) /= h(j);
    }

    for (int iter = 0; iter < maxIter; ++iter) {
        double maxRot = 0.0;

        // Sweep over all pairs of factors.
        for (int i = 0; i < k - 1; ++i) {
            for (int j = i + 1; j < k; ++j) {
                const Eigen::VectorXd xi = L.col(i);
                const Eigen::VectorXd xj = L.col(j);

                const Eigen::VectorXd u = xi.array().square() - xj.array().square();
                const Eigen::VectorXd v = 2.0 * xi.array() * xj.array();

                const double A = u.sum();
                const double B = v.sum();
                const double C = (u.array().square() - v.array().square()).sum();
                const double D = (u.array() * v.array()).sum() * 2.0;

                const double num = D - 2.0 * A * B / static_cast<double>(p);
                const double den = C - (A * A - B * B) / static_cast<double>(p);

                if (std::abs(den) < 1.0e-12) continue;

                const double phi   = 0.25 * std::atan2(num, den);
                const double cosPhi = std::cos(phi);
                const double sinPhi = std::sin(phi);

                const Eigen::VectorXd newI =  cosPhi * xi + sinPhi * xj;
                const Eigen::VectorXd newJ = -sinPhi * xi + cosPhi * xj;

                L.col(i) = newI;
                L.col(j) = newJ;

                maxRot = std::max(maxRot, std::abs(phi));
            }
        }

        if (maxRot < tol) break;
    }

    // De-normalise.
    for (int j = 0; j < p; ++j) {
        L.row(j) *= h(j);
    }
}
