#pragma once

#include <loki/core/exceptions.hpp>

#include <Eigen/SVD>

#include <cmath>
#include <limits>

namespace loki {

/**
 * @brief Wrapper around Eigen's BDCSVD providing higher-level SVD operations.
 *
 * Computes the singular value decomposition M = U * S * V^T, where:
 *   U  -- left singular vectors  (m x p, thin) or (m x m, full)
 *   S  -- singular values in descending order (p x 1)
 *   V  -- right singular vectors (n x p, thin) or (n x n, full)
 *   p  = min(m, n)
 *
 * Uses Eigen::BDCSVD (divide-and-conquer), which is recommended for matrices
 * larger than ~16x16. Thin U and V are computed by default.
 *
 * Implemented as a header-only class to avoid ODR/linking issues with
 * BDCSVD template instantiations in GCC static libraries on Windows.
 *
 * Intended uses:
 *   - CalibrationRegressor (TLS via smallest singular vector)
 *   - loki_svd module (SSA, PCA decomposition of time series)
 *   - Pseudoinverse for ill-conditioned systems
 */
class SvdDecomposition {
public:
    /**
     * @brief Constructs the SVD of matrix M.
     * @param M            Input matrix (m x n).
     * @param computeFullU If true, compute full m x m U matrix. Default: thin (m x p).
     * @param computeFullV If true, compute full n x n V matrix. Default: thin (n x p).
     * @throws AlgorithmException if M is empty or SVD fails to produce singular values.
     */
    explicit SvdDecomposition(const Eigen::MatrixXd& M,
                               bool computeFullU = false,
                               bool computeFullV = false)
        : m_rows(static_cast<int>(M.rows()))
        , m_cols(static_cast<int>(M.cols()))
    {
        if (m_rows == 0 || m_cols == 0) {
            throw AlgorithmException(
                "SvdDecomposition: input matrix must be non-empty.");
        }

        unsigned int flags = Eigen::ComputeThinU | Eigen::ComputeThinV;
        if (computeFullU)
            flags = (flags & ~static_cast<unsigned int>(Eigen::ComputeThinU))
                  | static_cast<unsigned int>(Eigen::ComputeFullU);
        if (computeFullV)
            flags = (flags & ~static_cast<unsigned int>(Eigen::ComputeThinV))
                  | static_cast<unsigned int>(Eigen::ComputeFullV);

        m_svd = Eigen::BDCSVD<Eigen::MatrixXd>(M, flags);

        if (m_svd.singularValues().size() == 0) {
            throw AlgorithmException(
                "SvdDecomposition: SVD failed to produce singular values.");
        }
    }

    /**
     * @brief Returns the left singular vectors U (m x p thin, or m x m full).
     */
    inline const Eigen::MatrixXd& U() const { return m_svd.matrixU(); }

    /**
     * @brief Returns the singular values in descending order (p x 1).
     */
    inline const Eigen::VectorXd& singularValues() const { return m_svd.singularValues(); }

    /**
     * @brief Returns the right singular vectors V (n x p thin, or n x n full).
     *
     * Note: Eigen stores V, not V^T. Use V().transpose() to get V^T.
     */
    inline const Eigen::MatrixXd& V() const { return m_svd.matrixV(); }

    /// Number of rows m of the original matrix.
    inline int rows() const { return m_rows; }

    /// Number of columns n of the original matrix.
    inline int cols() const { return m_cols; }

    /**
     * @brief Estimates the numerical rank of M.
     * @param tol Threshold for singular values. If tol < 0, uses the standard
     *            estimate: eps * max(m, n) * s_max.
     * @return Number of singular values exceeding tol.
     */
    inline int rank(double tol = -1.0) const
    {
        const double threshold = (tol < 0.0) ? autoTol() : tol;
        const Eigen::VectorXd& s = m_svd.singularValues();
        int r = 0;
        for (int i = 0; i < s.size(); ++i) {
            if (s[i] > threshold) ++r;
        }
        return r;
    }

    /**
     * @brief Returns the condition number s_max / s_min.
     *
     * Returns infinity if s_min is effectively zero (rank-deficient matrix).
     */
    inline double condition() const
    {
        const Eigen::VectorXd& s = m_svd.singularValues();
        const double sMax = s[0];
        const double sMin = s[s.size() - 1];
        if (sMin < std::numeric_limits<double>::epsilon() * sMax)
            return std::numeric_limits<double>::infinity();
        return sMax / sMin;
    }

    /**
     * @brief Reconstructs M using only the top-k singular components.
     *
     * Returns U_k * diag(S_k) * V_k^T, the rank-k approximation of M.
     *
     * @param k Number of components to retain (1 <= k <= p).
     * @throws AlgorithmException if k is out of range.
     */
    inline Eigen::MatrixXd truncate(int k) const
    {
        const int p = static_cast<int>(m_svd.singularValues().size());
        if (k < 1 || k > p) {
            throw AlgorithmException(
                "SvdDecomposition::truncate(): k=" + std::to_string(k) +
                " out of range [1, " + std::to_string(p) + "].");
        }
        const Eigen::MatrixXd Uk = m_svd.matrixU().leftCols(k);
        const Eigen::VectorXd Sk = m_svd.singularValues().head(k);
        const Eigen::MatrixXd Vk = m_svd.matrixV().leftCols(k);
        return Uk * Sk.asDiagonal() * Vk.transpose();
    }

    /**
     * @brief Returns the explained variance ratio per singular component.
     *
     * Ratio_i = s_i^2 / sum(s_j^2). Length p.
     */
    inline Eigen::VectorXd explainedVarianceRatio() const
    {
        const Eigen::VectorXd& s  = m_svd.singularValues();
        const Eigen::VectorXd  s2 = s.array().square();
        const double           tot = s2.sum();
        if (tot < std::numeric_limits<double>::epsilon())
            return Eigen::VectorXd::Zero(s.size());
        return s2 / tot;
    }

    /**
     * @brief Computes the Moore-Penrose pseudoinverse of M.
     *
     * Pseudoinverse = V * diag(1/s_i) * U^T, where only singular values
     * exceeding tol are inverted.
     *
     * @param tol Threshold for inversion. If tol < 0, uses auto-estimate.
     * @return Pseudoinverse matrix (n x m).
     */
    inline Eigen::MatrixXd pseudoinverse(double tol = -1.0) const
    {
        const double threshold   = (tol < 0.0) ? autoTol() : tol;
        const Eigen::VectorXd& s = m_svd.singularValues();
        const int p              = static_cast<int>(s.size());
        Eigen::VectorXd sInv(p);
        for (int i = 0; i < p; ++i)
            sInv[i] = (s[i] > threshold) ? (1.0 / s[i]) : 0.0;
        return m_svd.matrixV() * sInv.asDiagonal() * m_svd.matrixU().transpose();
    }

private:
    Eigen::BDCSVD<Eigen::MatrixXd> m_svd;
    int                             m_rows;
    int                             m_cols;

    /// Computes auto tolerance: eps * max(m, n) * s_max.
    inline double autoTol() const
    {
        const Eigen::VectorXd& s = m_svd.singularValues();
        const double sMax        = s[0];
        const double mn          = static_cast<double>(std::max(m_rows, m_cols));
        return std::numeric_limits<double>::epsilon() * mn * sMax;
    }
};

} // namespace loki