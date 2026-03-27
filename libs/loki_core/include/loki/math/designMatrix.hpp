#pragma once

#include <Eigen/Dense>
#include <vector>

namespace loki {

/**
 * @brief Static factory for constructing common design matrices.
 *
 * All methods return a matrix A suitable for use in LsqSolver::solve().
 * The caller is responsible for converting TimeSeries data to Eigen vectors
 * before calling these methods.
 */
class DesignMatrix {
public:
    /**
     * @brief Polynomial design matrix: columns [1, x, x^2, ..., x^degree].
     *
     * @param x    Observation coordinate vector (m x 1).
     * @param degree  Polynomial degree (>= 0). Degree 0 gives a column of ones.
     * @return     Matrix of size (m x degree+1).
     * @throws AlgorithmException if degree < 0 or x is empty.
     */
    static Eigen::MatrixXd polynomial(const Eigen::VectorXd& x, int degree);

    /**
     * @brief Harmonic design matrix with a constant term.
     *
     * Columns: [1, sin(2*pi*t/T1), cos(2*pi*t/T1), sin(2*pi*t/T2), cos(2*pi*t/T2), ...]
     * One sin+cos pair is added per period. The leading 1 column allows
     * the solver to fit a mean offset.
     *
     * @param t        Time coordinate vector (m x 1), same units as periods.
     * @param periods  List of periods T (same unit as t). Must be non-empty.
     * @return         Matrix of size (m x 1+2*periods.size()).
     * @throws AlgorithmException if periods is empty or t is empty.
     */
    static Eigen::MatrixXd harmonic(const Eigen::VectorXd& t,
                                    const std::vector<double>& periods);

    /**
     * @brief Identity design matrix (m x m).
     *
     * Useful for a pure observation adjustment model where A = I.
     *
     * @param n  Size of the identity matrix.
     * @return   Identity matrix of size (n x n).
     * @throws AlgorithmException if n <= 0.
     */
    static Eigen::MatrixXd identity(int n);

private:
    DesignMatrix() = delete;
};

} // namespace loki
