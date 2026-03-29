#include <loki/math/svd.hpp>
#include <loki/core/exceptions.hpp>

#include <cmath>
#include <limits>

using namespace loki;

// -----------------------------------------------------------------------------
//  Construction
// -----------------------------------------------------------------------------

SvdDecomposition::SvdDecomposition(const Eigen::MatrixXd& M,
                                    bool computeFullU,
                                    bool computeFullV)
    : m_rows(static_cast<int>(M.rows()))
    , m_cols(static_cast<int>(M.cols()))
{
    if (m_rows == 0 || m_cols == 0) {
        throw AlgorithmException(
            "SvdDecomposition: input matrix must be non-empty.");
    }

    unsigned int flags = Eigen::ComputeThinU | Eigen::ComputeThinV;
    if (computeFullU) flags = (flags & ~Eigen::ComputeThinU) | Eigen::ComputeFullU;
    if (computeFullV) flags = (flags & ~Eigen::ComputeThinV) | Eigen::ComputeFullV;

    m_svd = Eigen::BDCSVD<Eigen::MatrixXd>(M, flags);

    if (m_svd.singularValues().size() == 0) {
        throw AlgorithmException(
            "SvdDecomposition: SVD failed to produce singular values.");
    }
}

// -----------------------------------------------------------------------------
//  Accessors
// -----------------------------------------------------------------------------

const Eigen::MatrixXd& SvdDecomposition::U() const
{
    return m_svd.matrixU();
}

const Eigen::VectorXd& SvdDecomposition::singularValues() const
{
    return m_svd.singularValues();
}

const Eigen::MatrixXd& SvdDecomposition::V() const
{
    return m_svd.matrixV();
}

int SvdDecomposition::rows() const { return m_rows; }
int SvdDecomposition::cols() const { return m_cols; }

// -----------------------------------------------------------------------------
//  rank()
// -----------------------------------------------------------------------------

int SvdDecomposition::rank(double tol) const
{
    const double threshold = (tol < 0.0) ? autoTol() : tol;
    const Eigen::VectorXd& s = m_svd.singularValues();
    int r = 0;
    for (int i = 0; i < s.size(); ++i) {
        if (s[i] > threshold) ++r;
    }
    return r;
}

// -----------------------------------------------------------------------------
//  condition()
// -----------------------------------------------------------------------------

double SvdDecomposition::condition() const
{
    const Eigen::VectorXd& s = m_svd.singularValues();
    const double sMax = s[0];
    const double sMin = s[s.size() - 1];

    if (sMin < std::numeric_limits<double>::epsilon() * sMax)
        return std::numeric_limits<double>::infinity();

    return sMax / sMin;
}

// -----------------------------------------------------------------------------
//  truncate()
// -----------------------------------------------------------------------------

Eigen::MatrixXd SvdDecomposition::truncate(int k) const
{
    const int p = static_cast<int>(m_svd.singularValues().size());

    if (k < 1 || k > p) {
        throw AlgorithmException(
            "SvdDecomposition::truncate(): k=" + std::to_string(k) +
            " out of range [1, " + std::to_string(p) + "].");
    }

    // Rank-k approximation: U_k * diag(S_k) * V_k^T
    const Eigen::MatrixXd Uk = m_svd.matrixU().leftCols(k);
    const Eigen::VectorXd Sk = m_svd.singularValues().head(k);
    const Eigen::MatrixXd Vk = m_svd.matrixV().leftCols(k);

    return Uk * Sk.asDiagonal() * Vk.transpose();
}

// -----------------------------------------------------------------------------
//  explainedVarianceRatio()
// -----------------------------------------------------------------------------

Eigen::VectorXd SvdDecomposition::explainedVarianceRatio() const
{
    const Eigen::VectorXd& s   = m_svd.singularValues();
    const Eigen::VectorXd  s2  = s.array().square();
    const double           tot = s2.sum();

    if (tot < std::numeric_limits<double>::epsilon())
        return Eigen::VectorXd::Zero(s.size());

    return s2 / tot;
}

// -----------------------------------------------------------------------------
//  pseudoinverse()
// -----------------------------------------------------------------------------

Eigen::MatrixXd SvdDecomposition::pseudoinverse(double tol) const
{
    const double threshold       = (tol < 0.0) ? autoTol() : tol;
    const Eigen::VectorXd& s     = m_svd.singularValues();
    const int              p     = static_cast<int>(s.size());

    // Invert singular values above threshold, zero out the rest.
    Eigen::VectorXd sInv(p);
    for (int i = 0; i < p; ++i)
        sInv[i] = (s[i] > threshold) ? (1.0 / s[i]) : 0.0;

    // Pseudoinverse = V * diag(sInv) * U^T
    return m_svd.matrixV() * sInv.asDiagonal() * m_svd.matrixU().transpose();
}

// -----------------------------------------------------------------------------
//  autoTol()  -- private
// -----------------------------------------------------------------------------

double SvdDecomposition::autoTol() const
{
    const Eigen::VectorXd& s = m_svd.singularValues();
    const double sMax        = s[0];
    const double mn          = static_cast<double>(std::max(m_rows, m_cols));
    return std::numeric_limits<double>::epsilon() * mn * sMax;
}
