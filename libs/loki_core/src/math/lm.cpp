#include <loki/math/lm.hpp>
#include <loki/core/exceptions.hpp>

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <string>

using namespace loki;

// ---------------------------------------------------------------------------
//  LmSolver::evalResiduals
// ---------------------------------------------------------------------------

Eigen::VectorXd LmSolver::evalResiduals(const ModelFn&         model,
                                          const Eigen::VectorXd& x,
                                          const Eigen::VectorXd& y,
                                          const Eigen::VectorXd& params)
{
    const int m = static_cast<int>(x.size());
    Eigen::VectorXd r(m);
    for (int i = 0; i < m; ++i) {
        r(i) = model(x(i), params) - y(i);
    }
    return r;
}

// ---------------------------------------------------------------------------
//  LmSolver::numericalJacobian
// ---------------------------------------------------------------------------

Eigen::MatrixXd LmSolver::numericalJacobian(const ModelFn&         model,
                                              const Eigen::VectorXd& x,
                                              const Eigen::VectorXd& params,
                                              double                 h)
{
    const int m = static_cast<int>(x.size());
    const int p = static_cast<int>(params.size());

    Eigen::MatrixXd J(m, p);

    for (int j = 0; j < p; ++j) {
        Eigen::VectorXd pFwd = params;
        Eigen::VectorXd pBwd = params;

        // Scale step by parameter magnitude to handle varied scales
        const double hj = h * (1.0 + std::abs(params(j)));

        pFwd(j) += hj;
        pBwd(j) -= hj;

        for (int i = 0; i < m; ++i) {
            J(i, j) = (model(x(i), pFwd) - model(x(i), pBwd)) / (2.0 * hj);
        }
    }

    return J;
}

// ---------------------------------------------------------------------------
//  LmSolver::solve
// ---------------------------------------------------------------------------

LmResult LmSolver::solve(const ModelFn&         model,
                          const Eigen::VectorXd& x,
                          const Eigen::VectorXd& y,
                          const Eigen::VectorXd& p0,
                          const LmConfig&        cfg)
{
    const int m = static_cast<int>(x.size());
    const int p = static_cast<int>(p0.size());

    // Input validation
    if (m == 0) {
        throw AlgorithmException("LmSolver::solve: x vector is empty.");
    }
    if (static_cast<int>(y.size()) != m) {
        throw AlgorithmException(
            "LmSolver::solve: x and y must have equal size (x=" +
            std::to_string(m) + ", y=" + std::to_string(y.size()) + ").");
    }
    if (p == 0) {
        throw AlgorithmException("LmSolver::solve: initial parameter vector p0 is empty.");
    }
    if (m < p) {
        throw AlgorithmException(
            "LmSolver::solve: underdetermined system (m=" + std::to_string(m) +
            " < p=" + std::to_string(p) + ").");
    }

    LmResult result;
    result.params = p0;

    Eigen::VectorXd r = evalResiduals(model, x, y, result.params);
    double cost = r.squaredNorm();

    double lambda = cfg.lambdaInit;

    for (int iter = 0; iter < cfg.maxIterations; ++iter) {
        result.iterations = iter + 1;

        Eigen::MatrixXd J = numericalJacobian(model, x, result.params, cfg.finiteDiffH);
        Eigen::MatrixXd JtJ = J.transpose() * J;
        Eigen::VectorXd Jtr = J.transpose() * r;

        // Gradient convergence check: max |J^T r|
        if (Jtr.cwiseAbs().maxCoeff() < cfg.gradTol) {
            result.converged  = true;
            result.stopReason = LmStopReason::CONVERGED_GRADIENT;
            result.jacobian   = J;
            break;
        }

        // Marquardt damping: scale diagonal of J^T J
        Eigen::VectorXd diagJtJ = JtJ.diagonal();
        Eigen::MatrixXd damped  = JtJ;
        for (int j = 0; j < p; ++j) {
            // Use diagonal element (not identity) for scale-invariant damping
            damped(j, j) += lambda * diagJtJ(j);
        }

        // Solve (J^T J + lambda * D) * delta = -J^T r
        Eigen::VectorXd delta =
            damped.colPivHouseholderQr().solve(-Jtr);

        Eigen::VectorXd pNew  = result.params + delta;
        Eigen::VectorXd rNew  = evalResiduals(model, x, y, pNew);
        double          costNew = rNew.squaredNorm();

        if (costNew < cost) {
            // Step accepted
            result.params = pNew;
            r             = rNew;
            cost          = costNew;
            lambda       /= cfg.lambdaFactor;

            // Step convergence check
            const double stepNorm  = delta.norm();
            const double paramNorm = result.params.norm() + cfg.stepTol;
            if (stepNorm / paramNorm < cfg.stepTol) {
                result.converged  = true;
                result.stopReason = LmStopReason::CONVERGED_STEP;
                // Recompute J at new params for output
                result.jacobian = numericalJacobian(
                    model, x, result.params, cfg.finiteDiffH);
                break;
            }
        } else {
            // Step rejected -- increase damping and retry next iteration
            lambda *= cfg.lambdaFactor;
        }
    }

    // Ensure jacobian is populated even if we exited via MAX_ITERATIONS
    if (!result.converged) {
        result.stopReason = LmStopReason::MAX_ITERATIONS;
        result.jacobian   = numericalJacobian(
            model, x, result.params, cfg.finiteDiffH);
    }

    // Final residuals at converged (or best) parameters
    result.residuals = evalResiduals(model, x, y, result.params);

    // A-posteriori sigma0
    const int dof = m - p;
    result.sigma0 = (dof > 0)
        ? std::sqrt(result.residuals.squaredNorm() / static_cast<double>(dof))
        : 0.0;

    // Approximate covariance: (J^T J)^{-1} * sigma0^2
    // Uses pseudoinverse for robustness in near-singular cases
    Eigen::MatrixXd JtJ_final = result.jacobian.transpose() * result.jacobian;
    Eigen::JacobiSVD<Eigen::MatrixXd> svd(
        JtJ_final, Eigen::ComputeThinU | Eigen::ComputeThinV);

    // Threshold for treating singular values as zero (relative to largest)
    const double svThreshold = 1.0e-10 * svd.singularValues()(0);
    Eigen::VectorXd invS(p);
    for (int j = 0; j < p; ++j) {
        const double sv = svd.singularValues()(j);
        invS(j) = (sv > svThreshold) ? (1.0 / sv) : 0.0;
    }

    result.covariance =
        svd.matrixV() * invS.asDiagonal() * svd.matrixU().transpose()
        * (result.sigma0 * result.sigma0);

    return result;
}
