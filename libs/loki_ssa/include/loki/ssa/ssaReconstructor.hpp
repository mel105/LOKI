#pragma once

#include <loki/ssa/ssaResult.hpp>
#include <loki/core/config.hpp>

#include <Eigen/Dense>

#include <vector>

namespace loki::ssa {

/**
 * @brief Produces per-group reconstructed signals from SSA eigentriples.
 *
 * Two reconstruction methods are provided:
 *
 *   "diagonal_averaging" -- Standard SSA reconstruction. Each elementary
 *     matrix E_i = s_i * u_i * v_i^T (K x L) is anti-diagonally averaged
 *     to yield a length-n series. Group reconstructions are sums of the
 *     individual elementary series belonging to that group.
 *     This is the mathematically correct reconstruction and should be used
 *     for all final results.
 *
 *   "simple" -- Direct sum of rank-1 matrices without diagonal averaging:
 *     reconstructed[k] = sum_{i in group} (U[:,i] * s_i * V[:,i]^T)[..][k]
 *     mapped back to a length-n series by taking the first element of each
 *     trajectory window. Faster but less accurate. Useful for exploration.
 *
 * The elementary reconstructed series (result.components) are also filled
 * here for all eigentriples, using the configured method.
 */
class SsaReconstructor {
public:

    /**
     * @brief Constructs a reconstructor bound to the given SSA configuration.
     * @param cfg Application configuration (SsaReconstructionConfig used).
     */
    explicit SsaReconstructor(const AppConfig& cfg);

    /**
     * @brief Fills result.components and per-group result.groups[*].reconstruction.
     *
     * Expects result.groups to be already populated by SsaGrouper::group().
     * Expects U, singularValues, V passed as separate arguments (not stored
     * in SsaResult to avoid Eigen dependency in ssaResult.hpp).
     *
     * On return:
     *   - result.components[i] contains the elementary reconstruction of
     *     eigentriple i, length n.
     *   - result.groups[g].reconstruction contains the sum of components
     *     for all eigentriples in group g, length n.
     *   - result.trend and result.noise are filled from the groups named
     *     "trend" and "noise" respectively.
     *
     * @param result   SsaResult with groups already assigned.
     * @param U        Left singular vectors, K x r (thin).
     * @param sv       Singular values, length r, descending.
     * @param V        Right singular vectors, L x r (thin).
     * @throws AlgorithmException if dimensions are inconsistent.
     */
    void reconstruct(SsaResult&              result,
                     const Eigen::MatrixXd&  U,
                     const Eigen::VectorXd&  sv,
                     const Eigen::MatrixXd&  V) const;

private:

    AppConfig m_cfg;

    /// Diagonal (anti-diagonal) averaging of a K x L elementary matrix.
    /// Returns a vector of length n = K + L - 1.
    static std::vector<double> _diagonalAverage(const Eigen::MatrixXd& E,
                                                  int K,
                                                  int L);

    /// Simple reconstruction: first element of each trajectory window.
    /// Returns a vector of length n = K + L - 1.
    static std::vector<double> _simpleReconstruct(const Eigen::MatrixXd& E,
                                                   int K,
                                                   int L);
};

} // namespace loki::ssa
