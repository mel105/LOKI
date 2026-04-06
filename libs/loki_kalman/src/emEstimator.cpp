#include "loki/kalman/emEstimator.hpp"

#include "loki/core/exceptions.hpp"
#include "loki/core/logger.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace loki;

namespace loki::kalman {

// -----------------------------------------------------------------------------
//  Construction
// -----------------------------------------------------------------------------

EmEstimator::EmEstimator(KalmanModel model, int maxIter, double tol)
    : m_model(std::move(model))
    , m_maxIter(maxIter)
    , m_tol(tol)
{}

// -----------------------------------------------------------------------------
//  estimate
// -----------------------------------------------------------------------------

std::pair<KalmanModel, EmResult> EmEstimator::estimate(
    const std::vector<double>& measurements) const
{
    // Count finite observations
    int nObs = 0;
    for (const double v : measurements) {
        if (std::isfinite(v)) { ++nObs; }
    }
    if (nObs < 4) {
        throw DataException(
            "EmEstimator::estimate: fewer than 4 valid observations -- cannot estimate noise.");
    }

    const int T = static_cast<int>(measurements.size());
    const int n = static_cast<int>(m_model.F.rows()); // state dimension

    // Working copies of noise scalars (start from initial model)
    double q = m_model.Q(0, 0);
    double r = m_model.R(0, 0);

    EmResult result;
    result.converged          = false;
    result.finalLogLikelihood = -std::numeric_limits<double>::infinity();

    double prevLogL = -std::numeric_limits<double>::infinity();

    for (int iter = 0; iter < m_maxIter; ++iter) {

        // ---- E-step: forward filter + RTS smoother --------------------------
        KalmanFilter kf = KalmanFilter(m_model).withNoise(q, r);
        const auto filterSteps   = kf.run(measurements);
        const double logL        = kf.logLikelihood(filterSteps);

        RtsSmoother rts(kf.model());
        const auto smootherSteps = rts.smooth(filterSteps);

        // ---- M-step: update q and r -----------------------------------------

        // Accumulator for Q update (scalar trace term)
        double sumQ = 0.0;
        // Accumulator for R update (scalar)
        double sumR = 0.0;

        // We need the lag-1 cross-covariance P[t, t-1|T] = G[t-1] * P[t|T].
        // Iterate t = 1 .. T-1 for Q; t = 0 .. T-1 for R.

        for (int t = 1; t < T; ++t) {
            const SmootherStep& sCur  = smootherSteps[static_cast<std::size_t>(t)];
            const SmootherStep& sPrev = smootherSteps[static_cast<std::size_t>(t - 1)];

            // Lag-1 cross-covariance
            const Eigen::MatrixXd P_t_tm1 = lag1CrossCov(sPrev.G, sCur.PSmooth);

            // E[x[t] x[t]']  = P[t|T] + x[t|T] * x[t|T]'
            const Eigen::MatrixXd Exx  = sCur.PSmooth  + sCur.xSmooth  * sCur.xSmooth.transpose();
            // E[x[t-1] x[t-1]'] = P[t-1|T] + x[t-1|T] * x[t-1|T]'
            const Eigen::MatrixXd Exx0 = sPrev.PSmooth + sPrev.xSmooth * sPrev.xSmooth.transpose();
            // E[x[t] x[t-1]'] = P[t,t-1|T] + x[t|T] * x[t-1|T]'
            const Eigen::MatrixXd Exx1 = P_t_tm1 + sCur.xSmooth * sPrev.xSmooth.transpose();

            // Q M-step contribution:
            // trace(Exx - F * Exx1' - Exx1 * F' + F * Exx0 * F')
            const Eigen::MatrixXd contrib =
                Exx
                - m_model.F * Exx1.transpose()
                - Exx1 * m_model.F.transpose()
                + m_model.F * Exx0 * m_model.F.transpose();

            sumQ += contrib.trace();
        }

        // q_new = sumQ / (n * (T-1))
        const double qNew = clampNoise(sumQ / (static_cast<double>(n) * static_cast<double>(T - 1)));

        // R update: sum over observed epochs only
        for (int t = 0; t < T; ++t) {
            if (!std::isfinite(measurements[static_cast<std::size_t>(t)])) { continue; }
            const SmootherStep& sCur = smootherSteps[static_cast<std::size_t>(t)];
            const double y     = measurements[static_cast<std::size_t>(t)];
            const double Hx    = (m_model.H * sCur.xSmooth)(0, 0);
            const double HPH   = (m_model.H * sCur.PSmooth * m_model.H.transpose())(0, 0);
            sumR += (y - Hx) * (y - Hx) + HPH;
        }

        const double rNew = clampNoise(sumR / static_cast<double>(nObs));

        // ---- Convergence check ----------------------------------------------
        const double relChange =
            std::abs(logL - prevLogL) / (1.0 + std::abs(prevLogL));

        LOKI_INFO("EM iter " + std::to_string(iter + 1)
                  + ": logL=" + std::to_string(logL)
                  + "  q=" + std::to_string(qNew)
                  + "  r=" + std::to_string(rNew)
                  + "  relChange=" + std::to_string(relChange));

        q = qNew;
        r = rNew;
        prevLogL = logL;

        if (iter > 0 && relChange < m_tol) {
            result.converged = true;
            result.iterations = iter + 1;
            result.finalLogLikelihood = logL;
            break;
        }

        if (iter == m_maxIter - 1) {
            result.iterations = m_maxIter;
            result.finalLogLikelihood = logL;
        }
    }

    result.estimatedQ = q;
    result.estimatedR = r;

    // Return updated model
    KalmanModel updatedModel = m_model;
    const int nd = static_cast<int>(m_model.F.rows());
    updatedModel.Q = Eigen::MatrixXd::Identity(nd, nd) * q;
    updatedModel.R = Eigen::MatrixXd::Identity(1, 1) * r;

    return {updatedModel, result};
}

// -----------------------------------------------------------------------------
//  helpers
// -----------------------------------------------------------------------------

Eigen::MatrixXd EmEstimator::lag1CrossCov(const Eigen::MatrixXd& G_prev,
                                           const Eigen::MatrixXd& PSmooth)
{
    // P[t, t-1|T] = G[t-1] * P[t|T]
    return G_prev * PSmooth;
}

double EmEstimator::clampNoise(double v, double lo, double hi)
{
    return std::max(lo, std::min(hi, v));
}

} // namespace loki::kalman
