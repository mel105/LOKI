#pragma once

#include <loki/math/lsqResult.hpp>

#include <Eigen/Dense>

namespace loki {

/**
 * @brief Stateless least-squares solver with optional weighting and robust IRLS.
 *
 * Solves the linear system A * x = l in the least-squares sense, minimising
 * v^T * P * v, where P is the diagonal weight matrix (identity if unweighted).
 *
 * Normal equations: (A^T P A) * x = A^T P l
 * Solved via ColPivHouseholderQR for numerical robustness.
 *
 * Robust mode uses Iteratively Reweighted Least Squares (IRLS): at each
 * iteration the weight of each observation is adjusted according to the
 * magnitude of its residual, downweighting suspected outliers.
 */

// -----------------------------------------------------------------------------
//  Robust weight function (defined outside LsqSolver to avoid GCC 13
//  aggregate-init bug with enum members inside nested structs).
// -----------------------------------------------------------------------------

/** @brief Robust weight function used in IRLS. */
enum class LsqWeightFunction {
    HUBER,    ///< Huber M-estimator: quadratic inside threshold, linear outside.
    BISQUARE  ///< Tukey bisquare: zero weight beyond threshold (more aggressive).
};

// -----------------------------------------------------------------------------
//  LsqSolverConfig (defined outside LsqSolver for the same reason)
// -----------------------------------------------------------------------------

/**
 * @brief Configuration for LsqSolver::solve().
 */
struct LsqSolverConfig {
    bool             weighted{false};           ///< Use external weights w.
    bool             robust{false};             ///< Enable IRLS.
    int              maxIterations{10};         ///< IRLS iteration limit.
    double           convergenceTol{1.0e-6};    ///< IRLS convergence threshold.
    LsqWeightFunction weightFn{LsqWeightFunction::HUBER}; ///< Weight function.
};

// -----------------------------------------------------------------------------
//  LsqSolver
// -----------------------------------------------------------------------------

class LsqSolver {
public:

    // Aliases so existing code using LsqSolver::Config / LsqSolver::WeightFunction
    // continues to compile without changes.
    using WeightFunction = LsqWeightFunction;
    using Config         = LsqSolverConfig;

    /**
     * @brief Solve the weighted least-squares problem.
     *
     * @param A    Design matrix (m x n). Must have m >= n.
     * @param l    Observation vector (m x 1).
     * @param cfg  Solver configuration.
     * @param w    Diagonal weights (m x 1). Ignored if cfg.weighted == false.
     *             Must have same size as l when cfg.weighted == true.
     * @return     Fully populated LsqResult.
     * @throws AlgorithmException if A is empty, underdetermined (m < n),
     *         or w has wrong size when weighted == true.
     * @throws SingularMatrixException if the normal matrix is rank-deficient.
     */
    static LsqResult solve(const Eigen::MatrixXd& A,
                           const Eigen::VectorXd& l,
                           const Config& cfg = Config{},
                           const Eigen::VectorXd& w = Eigen::VectorXd{});

private:
    LsqSolver() = delete;

    /**
     * @brief Single WLS solve step given combined weight vector p.
     */
    static LsqResult solveWeighted(const Eigen::MatrixXd& A,
                                   const Eigen::VectorXd& l,
                                   const Eigen::VectorXd& p);

    /**
     * @brief Compute IRLS weights from residuals using the selected weight function.
     */
    static Eigen::VectorXd irlsWeights(const Eigen::VectorXd& residuals,
                                        double                  sigma,
                                        WeightFunction          fn);
};

} // namespace loki