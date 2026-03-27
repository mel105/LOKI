#include <loki/math/lsq.hpp>
#include <loki/core/exceptions.hpp>

#include <Eigen/Dense>
#include <algorithm>
#include <cmath>
#include <vector>

using namespace loki;

// ---------------------------------------------------------------------------
// IRLS constants
// ---------------------------------------------------------------------------

static constexpr double HUBER_C    = 1.345; // 95% efficiency for Gaussian noise
static constexpr double BISQUARE_C = 4.685; // 95% efficiency for Gaussian noise
static constexpr double MAD_SCALE  = 1.4826; // MAD -> sigma for Gaussian distribution

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

/** Robust scale estimate: MAD * 1.4826 (consistent for Gaussian distribution). */
static double robustSigma(const Eigen::VectorXd& v)
{
    const int n = static_cast<int>(v.size());
    std::vector<double> absv(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        absv[static_cast<std::size_t>(i)] = std::abs(v(i));
    }
    std::sort(absv.begin(), absv.end());
    double mad = (n % 2 == 0)
        ? 0.5 * (absv[static_cast<std::size_t>(n / 2 - 1)] +
                 absv[static_cast<std::size_t>(n / 2)])
        : absv[static_cast<std::size_t>(n / 2)];
    return MAD_SCALE * mad;
}

// ---------------------------------------------------------------------------
// LsqSolver::irlsWeights
// ---------------------------------------------------------------------------

Eigen::VectorXd LsqSolver::irlsWeights(const Eigen::VectorXd& residuals,
                                         double sigma,
                                         WeightFunction fn)
{
    const int n = static_cast<int>(residuals.size());
    Eigen::VectorXd w(n);

    // Guard against degenerate sigma (all residuals zero -- perfect fit).
    if (sigma < 1.0e-12) {
        return Eigen::VectorXd::Ones(n);
    }

    for (int i = 0; i < n; ++i) {
        const double u = std::abs(residuals(i)) / sigma;
        switch (fn) {
            case WeightFunction::HUBER:
                w(i) = (u <= HUBER_C) ? 1.0 : HUBER_C / u;
                break;
            case WeightFunction::BISQUARE:
                if (u >= BISQUARE_C) {
                    w(i) = 0.0;
                } else {
                    const double t = 1.0 - (u / BISQUARE_C) * (u / BISQUARE_C);
                    w(i) = t * t;
                }
                break;
        }
    }
    return w;
}

// ---------------------------------------------------------------------------
// LsqSolver::solveWeighted
// ---------------------------------------------------------------------------

LsqResult LsqSolver::solveWeighted(const Eigen::MatrixXd& A,
                                    const Eigen::VectorXd& l,
                                    const Eigen::VectorXd& p)
{
    const int m = static_cast<int>(A.rows());
    const int n = static_cast<int>(A.cols());

    // Build weighted system: sqrt(P) * A * x = sqrt(P) * l
    // This lets us reuse Eigen's QR on an equivalent unweighted system.
    Eigen::VectorXd sqrtP = p.array().sqrt();
    Eigen::MatrixXd Aw = sqrtP.asDiagonal() * A;
    Eigen::VectorXd lw = sqrtP.array() * l.array();

    Eigen::ColPivHouseholderQR<Eigen::MatrixXd> qr(Aw);

    if (qr.rank() < n) {
        throw SingularMatrixException(
            "LsqSolver: normal matrix is rank-deficient (rank " +
            std::to_string(qr.rank()) + " < " + std::to_string(n) + ").");
    }

    LsqResult res;
    res.nObs    = m;
    res.nParams = n;
    res.dof     = m - n;

    res.coefficients = qr.solve(lw);
    res.residuals    = A * res.coefficients - l;

    // Weighted sum of squared residuals v^T P v
    res.vTPv = (p.array() * res.residuals.array().square()).sum();

    // A-posteriori unit weight standard deviation
    res.sigma0 = (res.dof > 0) ? std::sqrt(res.vTPv / static_cast<double>(res.dof)) : 0.0;

    // Cofactor matrix of unknowns: (A^T P A)^{-1}
    // Computed via QR: (A^T P A) = (Aw^T Aw), so its inverse follows from QR.
    Eigen::MatrixXd AtPA = A.transpose() * p.asDiagonal() * A;
    res.cofactorX = AtPA.inverse(); // safe: rank already verified above

    return res;
}

// ---------------------------------------------------------------------------
// LsqSolver::solve
// ---------------------------------------------------------------------------

LsqResult LsqSolver::solve(const Eigen::MatrixXd& A,
                             const Eigen::VectorXd& l,
                             const Config& cfg,
                             const Eigen::VectorXd& w)
{
    const int m = static_cast<int>(A.rows());
    const int n = static_cast<int>(A.cols());

    if (m == 0 || n == 0) {
        throw AlgorithmException("LsqSolver::solve: design matrix A is empty.");
    }
    if (static_cast<int>(l.size()) != m) {
        throw AlgorithmException(
            "LsqSolver::solve: l size (" + std::to_string(l.size()) +
            ") does not match A rows (" + std::to_string(m) + ").");
    }
    if (m < n) {
        throw AlgorithmException(
            "LsqSolver::solve: underdetermined system (m=" + std::to_string(m) +
            " < n=" + std::to_string(n) + "). Cannot solve.");
    }
    if (cfg.weighted && static_cast<int>(w.size()) != m) {
        throw AlgorithmException(
            "LsqSolver::solve: weight vector w size (" + std::to_string(w.size()) +
            ") does not match A rows (" + std::to_string(m) + ").");
    }

    // Base observation weights: external if provided, unit otherwise.
    Eigen::VectorXd p = cfg.weighted ? w : Eigen::VectorXd::Ones(m);

    if (!cfg.robust) {
        return solveWeighted(A, l, p);
    }

    // IRLS loop
    LsqResult res;
    Eigen::VectorXd xPrev = Eigen::VectorXd::Zero(n);

    for (int iter = 0; iter < cfg.maxIterations; ++iter) {
        res = solveWeighted(A, l, p);

        // Update IRLS weights based on current residuals
        double sigma = robustSigma(res.residuals);
        Eigen::VectorXd irlsW = irlsWeights(res.residuals, sigma, cfg.weightFn);

        // Combined weights: observation weights * IRLS weights
        p = (cfg.weighted ? w : Eigen::VectorXd::Ones(m)).array() * irlsW.array();

        // Convergence check: max change in coefficients
        double delta = (res.coefficients - xPrev).cwiseAbs().maxCoeff();
        xPrev = res.coefficients;

        if (iter > 0 && delta < cfg.convergenceTol) {
            res.converged = true;
            return res;
        }
    }

    // Reached max iterations without convergence
    res.converged = false;
    return res;
}
