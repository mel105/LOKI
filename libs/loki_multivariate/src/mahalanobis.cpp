#include "loki/multivariate/mahalanobis.hpp"

#include "loki/core/exceptions.hpp"
#include "loki/core/logger.hpp"

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <string>
#include <vector>

using namespace loki::multivariate;

// -----------------------------------------------------------------------------
//  Construction
// -----------------------------------------------------------------------------

Mahalanobis::Mahalanobis(const MultivariateMahalanobisConfig& cfg)
    : m_cfg(cfg)
{}

// -----------------------------------------------------------------------------
//  compute
// -----------------------------------------------------------------------------

MahalanobisResult Mahalanobis::compute(const MultivariateSeries& data) const
{
    if (data.empty()) {
        throw DataException("Mahalanobis::compute(): data is empty.");
    }

    const int n = static_cast<int>(data.nObs());
    const int p = static_cast<int>(data.nChannels());

    if (n < p + 1) {
        throw DataException(
            "Mahalanobis: n=" + std::to_string(n)
            + " must be >= p+1=" + std::to_string(p + 1) + ".");
    }

    Eigen::VectorXd mu;
    Eigen::MatrixXd S;

    if (m_cfg.robust) {
        // -- Approximate MCD via C-steps --------------------------------------
        // Subset size h = floor(0.75 * n), minimum p + 1.
        const int h = std::max(p + 1, static_cast<int>(0.75 * n));

        // Start from the full sample estimate.
        mu = data.data().colwise().mean();
        S  = (data.data().rowwise() - mu.transpose()).transpose()
           * (data.data().rowwise() - mu.transpose())
           / static_cast<double>(n - 1);

        // C-step iterations.
        for (int iter = 0; iter < 10; ++iter) {
            // Compute distances to current (mu, S).
            const Eigen::MatrixXd Sinv = _pseudoinverse(S);
            std::vector<std::pair<double,int>> dists;
            dists.reserve(static_cast<std::size_t>(n));
            for (int i = 0; i < n; ++i) {
                const Eigen::VectorXd d =
                    data.data().row(i).transpose() - mu;
                dists.emplace_back(d.dot(Sinv * d), i);
            }
            std::sort(dists.begin(), dists.end());

            // Select h observations with smallest distances.
            Eigen::MatrixXd subset(h, p);
            for (int i = 0; i < h; ++i) {
                subset.row(i) = data.data().row(dists[static_cast<std::size_t>(i)].second);
            }

            // Recompute mu and S on subset.
            const Eigen::VectorXd muNew = subset.colwise().mean();
            const Eigen::MatrixXd Xc   = subset.rowwise() - muNew.transpose();
            const Eigen::MatrixXd SNew = (Xc.transpose() * Xc) / static_cast<double>(h - 1);

            const double diff = (muNew - mu).norm();
            mu = muNew;
            S  = SNew;
            if (diff < 1.0e-8) break;
        }

        LOKI_INFO("Mahalanobis: robust MCD estimate computed (h="
                  + std::to_string(h) + ").");
    } else {
        // Standard sample estimates.
        mu = data.data().colwise().mean();
        const Eigen::MatrixXd Xc = data.data().rowwise() - mu.transpose();
        S  = (Xc.transpose() * Xc) / static_cast<double>(n - 1);
    }

    // -- Compute Mahalanobis distances ----------------------------------------
    const Eigen::MatrixXd Sinv = _pseudoinverse(S);

    Eigen::VectorXd d2(n);
    for (int i = 0; i < n; ++i) {
        const Eigen::VectorXd diff = data.data().row(i).transpose() - mu;
        d2(i) = diff.dot(Sinv * diff);
    }

    // -- Threshold and outlier flags ------------------------------------------
    const double chi2crit = _chi2Critical(p, m_cfg.significanceLevel);

    std::vector<bool>   isOutlier(static_cast<std::size_t>(n), false);
    std::vector<int>    outlierIdx;
    for (int i = 0; i < n; ++i) {
        if (d2(i) > chi2crit) {
            isOutlier[static_cast<std::size_t>(i)] = true;
            outlierIdx.push_back(i);
        }
    }

    LOKI_INFO("Mahalanobis: chi2_crit(" + std::to_string(p)
              + ", " + std::to_string(m_cfg.significanceLevel)
              + ")=" + std::to_string(chi2crit)
              + "  outliers=" + std::to_string(outlierIdx.size()) + ".");

    // -- Assemble result ------------------------------------------------------
    MahalanobisResult result;
    result.distances         = d2;
    result.isOutlier         = isOutlier;
    result.outlierIndices    = outlierIdx;
    result.chi2Critical      = chi2crit;
    result.mean              = mu;
    result.covariance        = S;
    result.nOutliers         = static_cast<int>(outlierIdx.size());
    result.nObs              = n;
    result.nChannels         = p;
    result.robust            = m_cfg.robust;
    result.significanceLevel = m_cfg.significanceLevel;

    return result;
}

// -----------------------------------------------------------------------------
//  _chi2Critical  (Wilson-Hilferty approximation)
// -----------------------------------------------------------------------------

double Mahalanobis::_chi2Critical(int p, double alpha)
{
    // z_{1-alpha} standard normal quantile (approximate).
    // For alpha = 0.05: z = 1.6449.
    // For alpha = 0.01: z = 2.3263.
    // Using Abramowitz & Stegun rational approximation.
    const double t = std::sqrt(-2.0 * std::log(alpha < 0.5 ? alpha : 1.0 - alpha));
    const double z = t - (2.515517 + 0.802853*t + 0.010328*t*t)
                       / (1.0 + 1.432788*t + 0.189269*t*t + 0.001308*t*t*t);
    const double za = (alpha < 0.5) ? z : -z;

    const double pd  = static_cast<double>(p);
    const double h   = 1.0 - 2.0 / (9.0 * pd);
    const double s   = std::sqrt(2.0 / (9.0 * pd));
    const double val = pd * std::pow(h + za * s, 3.0);

    return std::max(0.0, val);
}

// -----------------------------------------------------------------------------
//  _pseudoinverse
// -----------------------------------------------------------------------------

Eigen::MatrixXd Mahalanobis::_pseudoinverse(const Eigen::MatrixXd& S, double tol)
{
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> eig(S);
    const Eigen::VectorXd& ev = eig.eigenvalues();
    const double threshold = tol * ev.cwiseAbs().maxCoeff();

    Eigen::VectorXd invEv(ev.size());
    bool singular = false;
    for (Eigen::Index i = 0; i < ev.size(); ++i) {
        if (ev(i) > threshold) {
            invEv(i) = 1.0 / ev(i);
        } else {
            invEv(i) = 0.0;
            singular = true;
        }
    }

    if (singular) {
        LOKI_WARNING("Mahalanobis: covariance matrix is singular or near-singular "
                     "-- using pseudoinverse.");
    }

    return eig.eigenvectors() * invEv.asDiagonal()
         * eig.eigenvectors().transpose();
}
