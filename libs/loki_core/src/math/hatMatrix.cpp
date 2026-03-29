#include <loki/math/hatMatrix.hpp>
#include <loki/core/exceptions.hpp>

#include <Eigen/QR>

using namespace loki;

HatMatrix::HatMatrix(const Eigen::MatrixXd& X)
    : m_n(static_cast<int>(X.rows()))
    , m_p(static_cast<int>(X.cols()))
{
    if (m_n < MIN_ROWS) {
        throw AlgorithmException(
            "HatMatrix: design matrix must have at least " +
            std::to_string(MIN_ROWS) + " rows, got " + std::to_string(m_n) + ".");
    }
    if (m_p > m_n) {
        throw AlgorithmException(
            "HatMatrix: underdetermined system -- more parameters (" +
            std::to_string(m_p) + ") than observations (" +
            std::to_string(m_n) + ").");
    }

    // Thin QR: X = Q * R, Q is n x p with orthonormal columns.
    // h_ii = ||Q_i||^2 = sum_j Q(i,j)^2 = row-wise squared norm.
    Eigen::HouseholderQR<Eigen::MatrixXd> qr(X);
    m_Q = qr.householderQ() * Eigen::MatrixXd::Identity(m_n, m_p);

    m_leverages = m_Q.rowwise().squaredNorm();
}

const Eigen::VectorXd& HatMatrix::leverages() const
{
    return m_leverages;
}

const Eigen::MatrixXd& HatMatrix::matrix() const
{
    if (!m_H.has_value()) {
        // H = Q * Q^T
        m_H = m_Q * m_Q.transpose();
    }
    return m_H.value();
}

int HatMatrix::rankP() const
{
    return m_p;
}

int HatMatrix::n() const
{
    return m_n;
}
