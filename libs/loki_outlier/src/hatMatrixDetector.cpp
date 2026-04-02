#include <loki/outlier/hatMatrixDetector.hpp>
#include <loki/math/hatMatrix.hpp>
#include <loki/math/lagMatrix.hpp>
#include <loki/stats/distributions.hpp>
#include <loki/core/exceptions.hpp>

#include <cmath>

using namespace loki;

namespace loki::outlier {

HatMatrixDetector::HatMatrixDetector(Config cfg)
    : m_cfg(cfg)
{
    if (cfg.arOrder < 1) {
        throw AlgorithmException(
            "HatMatrixDetector: arOrder must be >= 1, got "
            + std::to_string(cfg.arOrder) + ".");
    }
    if (cfg.significanceLevel <= 0.0 || cfg.significanceLevel >= 1.0) {
        throw AlgorithmException(
            "HatMatrixDetector: significanceLevel must be in (0, 1), got "
            + std::to_string(cfg.significanceLevel) + ".");
    }
}

HatMatrixResult HatMatrixDetector::detect(const std::vector<double>& y) const
{
    const std::size_t n = y.size();
    const int         p = m_cfg.arOrder;

    // Validate length
    if (n <= static_cast<std::size_t>(p)) {
        throw SeriesTooShortException(
            "HatMatrixDetector::detect: series length " + std::to_string(n)
            + " must be greater than arOrder=" + std::to_string(p) + ".");
    }

    // Validate no NaN
    for (std::size_t i = 0; i < n; ++i) {
        if (std::isnan(y[i])) {
            throw MissingValueException(
                "HatMatrixDetector::detect: NaN at index "
                + std::to_string(i) + ". Input must be NaN-free.");
        }
    }

    // Build AR(p) lag design matrix Gamma, shape (n-p) x p
    const Eigen::MatrixXd G = loki::math::buildLagMatrix(y, p);

    // Compute hat matrix leverages h_ii via thin QR
    const HatMatrix hm(G);
    const Eigen::VectorXd& lev = hm.leverages();

    // Threshold: under H0 (Gaussian AR, no outlier), n*h_t ~ chi2(p).
    // Flag when h_t > chi2Quantile(1 - alpha, p) / n.
    const double chi2q    = loki::stats::chi2Quantile(1.0 - m_cfg.significanceLevel,
                                                       static_cast<double>(p));
    const double threshold = chi2q / static_cast<double>(n);

    HatMatrixResult result;
    result.leverages  = lev;
    result.threshold  = threshold;
    result.arOrder    = p;
    result.n          = n;

    // Collect outlier indices -- offset by p to map back to original series
    const std::size_t rows = static_cast<std::size_t>(lev.size());
    for (std::size_t i = 0; i < rows; ++i) {
        if (lev(static_cast<Eigen::Index>(i)) > threshold) {
            // Row i of lag matrix corresponds to original index i + p
            // (because we dropped the first p observations)
            result.outlierIndices.push_back(i + static_cast<std::size_t>(p));
        }
    }

    result.nOutliers = result.outlierIndices.size();
    return result;
}

} // namespace loki::outlier
