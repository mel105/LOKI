#pragma once

#include <Eigen/Dense>

#include <functional>

namespace loki {

// -----------------------------------------------------------------------------
//  ModelFn
// -----------------------------------------------------------------------------

/**
 * @brief Function type for a scalar nonlinear model.
 *
 * @param x      Single predictor value (mjd - tRef).
 * @param params Current parameter vector.
 * @return       Model prediction f(x, params).
 */
using ModelFn = std::function<double(double x, const Eigen::VectorXd& params)>;

// -----------------------------------------------------------------------------
//  LmStopReason  (defined outside LmSolver to avoid GCC 13 aggregate-init bug)
// -----------------------------------------------------------------------------

/** @brief Reason the LM iteration loop terminated. */
enum class LmStopReason {
    CONVERGED_GRADIENT,  ///< max |J^T r| < gradTol  (true gradient convergence)
    CONVERGED_STEP,      ///< relative step norm < stepTol
    MAX_ITERATIONS       ///< reached maxIterations without satisfying either criterion
};

// -----------------------------------------------------------------------------
//  LmConfig  (defined outside LmSolver for the same reason)
// -----------------------------------------------------------------------------

/**
 * @brief Configuration for LmSolver::solve().
 */
struct LmConfig {
    int    maxIterations {100};
    double gradTol       {1.0e-8};  ///< Convergence: max |J^T r| < gradTol.
    double stepTol       {1.0e-8};  ///< Convergence: step norm / param norm < stepTol.
    double lambdaInit    {1.0e-3};  ///< Initial Marquardt damping parameter.
    double lambdaFactor  {10.0};    ///< Multiply/divide lambda on rejection/acceptance.
    double finiteDiffH   {1.0e-5};  ///< Step size for numerical central-difference Jacobian.
};

// -----------------------------------------------------------------------------
//  LmResult
// -----------------------------------------------------------------------------

/**
 * @brief Output of LmSolver::solve().
 */
struct LmResult {
    Eigen::VectorXd params;      ///< Converged (or best) parameter vector.
    Eigen::VectorXd residuals;   ///< r_i = f(x_i, params) - y_i for each observation.
    Eigen::MatrixXd jacobian;    ///< Numerical Jacobian J at solution (m x p).
    Eigen::MatrixXd covariance;  ///< Approx. covariance (J^T J)^{-1} * sigma0^2.
    double          sigma0      {0.0};
    int             iterations  {0};
    bool            converged   {false};
    LmStopReason    stopReason  {LmStopReason::MAX_ITERATIONS};
};

// -----------------------------------------------------------------------------
//  LmSolver
// -----------------------------------------------------------------------------

/**
 * @brief Stateless Levenberg-Marquardt nonlinear least-squares solver.
 *
 * Minimises sum of squared residuals r_i = f(x_i, p) - y_i with respect to
 * the parameter vector p, starting from initial estimate p0.
 *
 * The Jacobian is computed numerically via central differences:
 *   J_ij = (f(x_i, p + h*e_j) - f(x_i, p - h*e_j)) / (2h)
 *
 * The damped normal equations solved at each step are:
 *   (J^T J + lambda * diag(J^T J)) * delta = J^T r
 *
 * On acceptance (cost decreases): lambda /= lambdaFactor
 * On rejection  (cost increases): lambda *= lambdaFactor  (step retried)
 *
 * Convergence is declared when either:
 *   - max |J^T r| < gradTol  (gradient criterion), or
 *   - ||delta|| / (||p|| + stepTol) < stepTol  (step criterion)
 */
class LmSolver {
public:

    using Config = LmConfig;
    using Result = LmResult;

    /**
     * @brief Run the LM solver.
     *
     * @param model  Scalar model function f(x, params).
     * @param x      Predictor vector (m observations).
     * @param y      Response vector  (m observations).
     * @param p0     Initial parameter vector (p parameters).
     * @param cfg    Solver configuration.
     * @return       Populated LmResult. converged==false is not an error --
     *               the caller (NonlinearRegressor) decides how to handle it.
     * @throws AlgorithmException if x, y, p0 are empty or x.size() != y.size().
     * @throws AlgorithmException if m < p (underdetermined system).
     */
    static LmResult solve(const ModelFn&         model,
                          const Eigen::VectorXd& x,
                          const Eigen::VectorXd& y,
                          const Eigen::VectorXd& p0,
                          const LmConfig&        cfg = LmConfig{});

private:
    LmSolver() = delete;

    /**
     * @brief Numerical central-difference Jacobian.
     *
     * @param model   Model function.
     * @param x       Predictor vector (m x 1).
     * @param params  Current parameter vector (p x 1).
     * @param h       Finite-difference step size.
     * @return        Jacobian matrix (m x p).
     */
    static Eigen::MatrixXd numericalJacobian(const ModelFn&         model,
                                              const Eigen::VectorXd& x,
                                              const Eigen::VectorXd& params,
                                              double                 h);

    /**
     * @brief Evaluate residuals r_i = f(x_i, params) - y_i.
     */
    static Eigen::VectorXd evalResiduals(const ModelFn&         model,
                                          const Eigen::VectorXd& x,
                                          const Eigen::VectorXd& y,
                                          const Eigen::VectorXd& params);
};

} // namespace loki
