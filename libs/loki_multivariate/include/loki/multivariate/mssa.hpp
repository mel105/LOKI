#pragma once

#include "loki/multivariate/multivariateResult.hpp"
#include "loki/multivariate/multivariateSeries.hpp"
#include "loki/core/config.hpp"

namespace loki::multivariate {

/**
 * @brief Multivariate Singular Spectrum Analysis (MSSA).
 *
 * MSSA extends single-channel SSA to N simultaneous channels by stacking
 * the individual Hankel trajectory matrices horizontally into one block
 * trajectory matrix of shape (K x L*p), where:
 *   K = n - L + 1  (number of lagged windows)
 *   L = window     (embedding window length in samples)
 *   p = nChannels
 *
 * The SVD of this block matrix yields multivariate eigentriples that capture
 * common oscillatory modes across all channels simultaneously. Each eigentriple
 * is reconstructed via diagonal averaging back to length n per channel, then
 * summed over channels to produce one reconstructed component per eigentriple.
 *
 * Reuses:
 *   - loki::math::buildEmbedMatrix() for per-channel Hankel embedding.
 *   - loki::math::randomizedSvd() when useRandomizedSvd = true.
 *   - Eigen::JacobiSVD when useRandomizedSvd = false.
 *
 * Window guideline: L should be a multiple of the dominant period for clean
 * mode separation. For 6h climate data: L = 1461 (1 year). For 1ms sensor
 * data: L must be chosen relative to the expected signal period in samples.
 */
class Mssa {
public:

    /**
     * @brief Constructs an MSSA engine with the given configuration.
     * @param cfg MSSA configuration from AppConfig.
     */
    explicit Mssa(const MultivariateMssaConfig& cfg);

    /**
     * @brief Computes MSSA on the given multivariate series.
     *
     * @param data Synchronised multivariate series (n x p). Must be NaN-free
     *             (MultivariateAssembler guarantees this after gap filling).
     * @return     Fully populated MssaResult.
     * @throws DataException      if data is empty or has fewer than 2 channels.
     * @throws AlgorithmException if window >= n or nComponents exceeds rank.
     */
    [[nodiscard]] MssaResult compute(const MultivariateSeries& data) const;

private:

    MultivariateMssaConfig m_cfg;

    /**
     * @brief Diagonal averaging (anti-diagonal means) of a single (K x L) matrix.
     *
     * Reconstructs a series of length n = K + L - 1 from a trajectory matrix
     * by averaging along each anti-diagonal. Standard SSA reconstruction step.
     *
     * @param mat  K x L elementary matrix (U_i * sv_i * V_i^T).
     * @param n    Target series length.
     * @return     Reconstructed series of length n.
     */
    [[nodiscard]]
    static Eigen::VectorXd _diagonalAverage(const Eigen::MatrixXd& mat,
                                             int                    n);
};

} // namespace loki::multivariate
