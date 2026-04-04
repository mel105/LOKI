#include <loki/math/randomizedSvd.hpp>
#include <loki/core/exceptions.hpp>

#include <Eigen/SVD>
#include <Eigen/QR>

#include <algorithm>
#include <random>

using namespace loki;

namespace loki::math {

RandomizedSvdResult randomizedSvd(const Eigen::MatrixXd& X,
                                   int                    k,
                                   int                    nOversampling,
                                   int                    nPowerIter,
                                   unsigned int           seed)
{
    const int m = static_cast<int>(X.rows());
    const int n = static_cast<int>(X.cols());

    if (m == 0 || n == 0) {
        throw AlgorithmException(
            "randomizedSvd: input matrix must be non-empty.");
    }
    if (k < 1) {
        throw AlgorithmException(
            "randomizedSvd: k must be >= 1, got " + std::to_string(k) + ".");
    }

    const int minMN = std::min(m, n);
    if (k >= minMN) {
        throw AlgorithmException(
            "randomizedSvd: k=" + std::to_string(k)
            + " must be < min(m,n)=" + std::to_string(minMN) + ".");
    }

    // Working rank l = k + oversampling, clamped to min(m, n).
    const int l = std::min(k + nOversampling, minMN);

    // -------------------------------------------------------------------------
    //  Step 1: Random Gaussian test matrix Omega  (n x l)
    // -------------------------------------------------------------------------

    std::mt19937                     rng(seed);
    std::normal_distribution<double> dist(0.0, 1.0);

    Eigen::MatrixXd Omega(n, l);
    for (int j = 0; j < l; ++j) {
        for (int i = 0; i < n; ++i) {
            Omega(i, j) = dist(rng);
        }
    }

    // -------------------------------------------------------------------------
    //  Step 2: Y = X * Omega  (m x l)
    //  Random projection onto the approximate column space of X.
    // -------------------------------------------------------------------------

    Eigen::MatrixXd Y = X * Omega;

    // -------------------------------------------------------------------------
    //  Step 3: Power iteration  Y <- X * (X^T * Y)
    //  Improves accuracy for matrices with slowly decaying singular values
    //  (typical for climatological covariance structure).
    //  Each iteration costs O(m * n * l) -- same as the initial projection.
    // -------------------------------------------------------------------------

    for (int iter = 0; iter < nPowerIter; ++iter) {
        // Orthonormalise before each multiply to avoid numerical overflow
        Y = X * (X.transpose() * Y);
        Eigen::HouseholderQR<Eigen::MatrixXd> qr(Y);
        Y = qr.householderQ() * Eigen::MatrixXd::Identity(m, l);
    }

    // -------------------------------------------------------------------------
    //  Step 4: Orthonormal basis Q for the range of Y  (m x l)
    // -------------------------------------------------------------------------

    Eigen::HouseholderQR<Eigen::MatrixXd> qr(Y);
    // Extract thin Q  (m x l)
    const Eigen::MatrixXd Q =
        qr.householderQ() * Eigen::MatrixXd::Identity(m, l);

    // -------------------------------------------------------------------------
    //  Step 5: Project X into the small subspace  B = Q^T * X  (l x n)
    // -------------------------------------------------------------------------

    const Eigen::MatrixXd B = Q.transpose() * X;

    // -------------------------------------------------------------------------
    //  Step 6: Thin SVD of the small matrix B  (l x n, l << m)
    //  JacobiSVD is fine here -- B is at most (k+10) x n, typically 50 x 1461.
    // -------------------------------------------------------------------------

    Eigen::JacobiSVD<Eigen::MatrixXd> svd(B,
        Eigen::ComputeThinU | Eigen::ComputeThinV);

    // -------------------------------------------------------------------------
    //  Step 7: Recover full-space U = Q * U_b, truncate to rank k
    // -------------------------------------------------------------------------

    RandomizedSvdResult res;
    res.sv = svd.singularValues().head(k);
    res.V  = svd.matrixV().leftCols(k);          // n x k
    res.U  = Q * svd.matrixU().leftCols(k);      // m x k

    return res;
}

} // namespace loki::math
