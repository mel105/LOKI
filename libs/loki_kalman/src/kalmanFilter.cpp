#include "loki/kalman/kalmanFilter.hpp"

#include "loki/core/exceptions.hpp"

#include <cmath>
#include <numbers>
#include <stdexcept>

using namespace loki;

namespace loki::kalman {

// -----------------------------------------------------------------------------
//  Construction
// -----------------------------------------------------------------------------

KalmanFilter::KalmanFilter(KalmanModel model)
    : m_model(std::move(model))
{}

// -----------------------------------------------------------------------------
//  run -- forward pass
// -----------------------------------------------------------------------------

std::vector<FilterStep> KalmanFilter::run(const std::vector<double>& measurements) const
{
    if (measurements.empty()) {
        throw DataException("KalmanFilter::run: measurement sequence is empty.");
    }

    const int n = static_cast<int>(m_model.F.rows()); // state dimension
    const Eigen::MatrixXd I = Eigen::MatrixXd::Identity(n, n);

    std::vector<FilterStep> steps;
    steps.reserve(measurements.size());

    // Initialise with prior
    Eigen::VectorXd xCur = m_model.x0;
    Eigen::MatrixXd PCur = m_model.P0;

    for (const double y : measurements) {
        FilterStep step;

        // ---- Predict --------------------------------------------------------
        Eigen::VectorXd xPred = m_model.F * xCur;
        Eigen::MatrixXd PPred = m_model.F * PCur * m_model.F.transpose() + m_model.Q;

        step.xPred = xPred;
        step.PPred = PPred;

        // ---- Update (skip when observation is NaN) ---------------------------
        const bool hasObs = std::isfinite(y);

        if (hasObs) {
            // Innovation and its variance
            const double innovation = y - (m_model.H * xPred)(0, 0);
            const double S = (m_model.H * PPred * m_model.H.transpose())(0, 0)
                             + m_model.R(0, 0);

            // Kalman gain [n x 1]
            const Eigen::MatrixXd K = (PPred * m_model.H.transpose()) / S;

            // State update
            const Eigen::VectorXd xFilt = xPred + K * innovation;

            // Joseph-form covariance update (numerically more stable than I-KH form)
            const Eigen::MatrixXd PFilt = josephUpdate(PPred, K);

            step.xFilt       = xFilt;
            step.PFilt       = PFilt;
            step.innovation  = innovation;
            step.innovationVar = S;
            step.kalmanGain0 = K(0, 0);
            step.hasObservation = true;

            xCur = xFilt;
            PCur = PFilt;
        } else {
            // No observation -- propagate prediction as filtered estimate
            step.xFilt         = xPred;
            step.PFilt         = PPred;
            step.innovation    = std::numeric_limits<double>::quiet_NaN();
            step.innovationVar = std::numeric_limits<double>::quiet_NaN();
            step.kalmanGain0   = std::numeric_limits<double>::quiet_NaN();
            step.hasObservation = false;

            xCur = xPred;
            PCur = PPred;
        }

        steps.push_back(std::move(step));
    }

    return steps;
}

// -----------------------------------------------------------------------------
//  logLikelihood
// -----------------------------------------------------------------------------

double KalmanFilter::logLikelihood(const std::vector<FilterStep>& steps) const
{
    double ll = 0.0;
    const double log2pi = std::log(2.0 * std::numbers::pi);

    for (const auto& step : steps) {
        if (!step.hasObservation) { continue; }
        const double S   = step.innovationVar;
        const double inn = step.innovation;
        ll += -0.5 * (log2pi + std::log(S) + (inn * inn) / S);
    }
    return ll;
}

// -----------------------------------------------------------------------------
//  model accessor
// -----------------------------------------------------------------------------

const KalmanModel& KalmanFilter::model() const
{
    return m_model;
}

// -----------------------------------------------------------------------------
//  withNoise -- create updated filter with new Q and R
// -----------------------------------------------------------------------------

KalmanFilter KalmanFilter::withNoise(double q, double r) const
{
    if (q <= 0.0) {
        throw ConfigException(
            "KalmanFilter::withNoise: q must be > 0, got " + std::to_string(q) + ".");
    }
    if (r <= 0.0) {
        throw ConfigException(
            "KalmanFilter::withNoise: r must be > 0, got " + std::to_string(r) + ".");
    }

    KalmanModel updated = m_model;
    const int n = static_cast<int>(m_model.F.rows());
    updated.Q = Eigen::MatrixXd::Identity(n, n) * q;
    updated.R = Eigen::MatrixXd::Identity(1, 1) * r;

    return KalmanFilter(std::move(updated));
}

// -----------------------------------------------------------------------------
//  josephUpdate -- private
// -----------------------------------------------------------------------------

Eigen::MatrixXd KalmanFilter::josephUpdate(const Eigen::MatrixXd& P,
                                            const Eigen::MatrixXd& K) const
{
    const int n = static_cast<int>(P.rows());
    const Eigen::MatrixXd IKH = Eigen::MatrixXd::Identity(n, n) - K * m_model.H;
    return IKH * P * IKH.transpose() + K * m_model.R * K.transpose();
}

} // namespace loki::kalman
