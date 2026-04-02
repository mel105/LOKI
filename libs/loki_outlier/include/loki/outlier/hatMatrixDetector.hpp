#pragma once

#include <loki/core/exceptions.hpp>

#include <Eigen/Dense>

#include <cstddef>
#include <vector>

namespace loki::outlier {

// ----------------------------------------------------------------------------
// HatMatrixResult
// ----------------------------------------------------------------------------

/**
 * @brief Result of one HatMatrixDetector::detect() call.
 *
 * Indices in outlierIndices refer to positions in the ORIGINAL input series
 * (i.e. they are offset by arOrder relative to the lag matrix rows).
 *
 * The leverages vector has length n - arOrder, where n is the input length.
 * leverages[i] corresponds to original series index i + arOrder.
 *
 * Consecutive outlier indices may appear in groups of up to arOrder elements
 * because a single anomalous observation Y_{t0} contributes to arOrder lag
 * vectors z_{t0+1}, ..., z_{t0+p}. This is expected behaviour documented in
 * Hau & Tong (1989), Section 6.
 */
struct HatMatrixResult {
    /// Indices into the original input series where leverage exceeds threshold.
    /// Sorted ascending. May contain consecutive runs due to AR lag propagation.
    std::vector<std::size_t> outlierIndices;

    /// Full leverage vector h_ii, length = n - arOrder.
    /// Element i corresponds to original series index i + arOrder.
    Eigen::VectorXd leverages;

    /// Detection threshold: chi2Quantile(1 - alpha, p) / n.
    double threshold{0.0};

    /// AR order actually used (equals Config::arOrder).
    int arOrder{0};

    /// Total input series length n.
    std::size_t n{0};

    /// Number of detected outliers (equals outlierIndices.size()).
    std::size_t nOutliers{0};
};

// ----------------------------------------------------------------------------
// HatMatrixConfig  (defined outside class to avoid GCC 13 aggregate-init bug)
// ----------------------------------------------------------------------------

/**
 * @brief Configuration for HatMatrixDetector.
 */
struct HatMatrixDetectorConfig {
    /// AR lag order p. Each row of the design matrix contains p lagged values.
    /// Higher p captures more temporal context; typical values: 3-10.
    int arOrder{5};

    /// Significance level alpha for the chi-squared threshold.
    /// Threshold = chi2Quantile(1 - alpha, p) / n.
    double significanceLevel{0.05};
};

// ----------------------------------------------------------------------------
// HatMatrixDetector
// ----------------------------------------------------------------------------

/**
 * @brief Detects outliers in autoregressive time series via hat matrix leverages.
 *
 * Implements the DEH (Diagonal Elements of the Hat matrix) method from
 * Hau & Tong (1989): "A practical method for outlier detection in
 * autoregressive time series modelling", Stochastic Hydrology and Hydraulics.
 *
 * Algorithm:
 *   1. Build the AR(p) lag design matrix Gamma from the input residuals.
 *      Row i = [ y[i+p-1], ..., y[i] ], shape (n-p) x p.
 *   2. Compute hat matrix leverages h_ii = ||Q_i||^2 via thin QR of Gamma,
 *      using HatMatrix from loki_core/math.
 *   3. Flag positions where h_ii > chi2Quantile(1 - alpha, p) / n.
 *
 * Key property (Hau & Tong, Section 3): trace(H) = p = constant.
 * Outlying leverages therefore stand out clearly against the background.
 *
 * Key property (Section 3.1): n * h_ii converges to the Mahalanobis distance
 * d_t = z_t^T Sigma^{-1} z_t under stationarity, and d_t ~ chi2(p) under H0
 * (Gaussian AR, no outlier). This justifies the chi-squared threshold.
 *
 * Does NOT inherit from OutlierDetector -- the interface is fundamentally
 * different (no location/scale estimates, output includes full leverage vector).
 * Integration in main.cpp is handled via a separate processing branch.
 */
class HatMatrixDetector {
public:

    /// Convenience alias for config struct.
    using Config = HatMatrixDetectorConfig;

    /**
     * @brief Constructs a HatMatrixDetector.
     * @param cfg Configuration. Default: arOrder=5, significanceLevel=0.05.
     * @throws AlgorithmException if cfg.arOrder < 1.
     * @throws AlgorithmException if cfg.significanceLevel not in (0, 1).
     */
    explicit HatMatrixDetector(Config cfg = {});

    /**
     * @brief Runs DEH-based outlier detection on a residual series.
     *
     * @param y Input residuals (NaN-free, length > arOrder).
     * @return  HatMatrixResult with leverages, threshold, and outlier indices.
     * @throws SeriesTooShortException if y.size() <= arOrder.
     * @throws MissingValueException   if any element of y is NaN.
     */
    [[nodiscard]]
    HatMatrixResult detect(const std::vector<double>& y) const;

    /// Returns the configured AR order.
    int arOrder() const { return m_cfg.arOrder; }

    /// Returns the configured significance level.
    double significanceLevel() const { return m_cfg.significanceLevel; }

private:
    Config m_cfg;
};

} // namespace loki::outlier
