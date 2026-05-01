#pragma once

#include "loki/multivariate/multivariateResult.hpp"
#include "loki/multivariate/multivariateSeries.hpp"
#include "loki/core/config.hpp"

namespace loki::multivariate {

/**
 * @brief Principal Component Analysis for multivariate time series.
 *
 * Implements SVD-based PCA on the (optionally pre-standardised) data matrix.
 * The data matrix X (n x p) is first column-centred. The SVD X = U * S * V^T
 * yields loadings V and scores U * S.
 *
 * SVD backend selection (controlled by MultivariatePcaConfig):
 *   useRandomizedSvd = false -- Eigen::JacobiSVD (exact, safe in .cpp files,
 *                               recommended for p <= 500).
 *   useRandomizedSvd = true  -- loki::math::randomizedSvd (Halko 2011,
 *                               recommended for large p or n).
 *
 * Component selection:
 *   nComponents = 0 -- auto: retain components until cumulative explained
 *                      variance >= varianceThreshold (default 95%).
 *   nComponents > 0 -- manual: retain exactly nComponents.
 *
 * The BDCSVD-based SvdDecomposition wrapper is intentionally NOT used here
 * due to the known BDCSVD linking bug in static libraries on Windows/GCC 13.
 */
class Pca {
public:

    /**
     * @brief Constructs a PCA engine with the given configuration.
     * @param cfg PCA configuration from AppConfig.
     */
    explicit Pca(const MultivariatePcaConfig& cfg);

    /**
     * @brief Computes PCA on the given multivariate series.
     *
     * @param data Synchronised multivariate series (n x p).
     * @return     Fully populated PcaResult.
     * @throws DataException       if data is empty or has fewer than 2 channels.
     * @throws AlgorithmException  if SVD fails or nComponents exceeds p.
     */
    [[nodiscard]] PcaResult compute(const MultivariateSeries& data) const;

private:

    MultivariatePcaConfig m_cfg;

    /**
     * @brief Selects number of components to retain.
     *
     * Returns cfg.nComponents if > 0. Otherwise finds the smallest k such
     * that cumulative explained variance >= cfg.varianceThreshold.
     *
     * @param ratio  Explained variance ratio per component (descending).
     * @param maxK   Maximum allowed k (= number of singular values).
     * @return       Number of components to retain.
     */
    [[nodiscard]]
    int _selectNComponents(const Eigen::VectorXd& ratio, int maxK) const;
};

} // namespace loki::multivariate
