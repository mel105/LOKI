#include "loki/kalman/kalmanModelBuilder.hpp"

#include "loki/core/exceptions.hpp"

using namespace loki;

namespace loki::kalman {

// ---- helpers ----------------------------------------------------------------

static void validateNoise(double q, double r)
{
    if (q <= 0.0) {
        throw ConfigException(
            "KalmanModelBuilder: process noise variance q must be > 0, got "
            + std::to_string(q) + ".");
    }
    if (r <= 0.0) {
        throw ConfigException(
            "KalmanModelBuilder: measurement noise variance r must be > 0, got "
            + std::to_string(r) + ".");
    }
}

// ---- local_level ------------------------------------------------------------

KalmanModel KalmanModelBuilder::localLevel(double q, double r, double x0)
{
    validateNoise(q, r);

    KalmanModel m;
    m.name = "local_level";

    // State: x = [level]  (1-dimensional)
    m.F = Eigen::MatrixXd::Identity(1, 1);
    m.H = Eigen::MatrixXd::Ones(1, 1);
    m.Q = Eigen::MatrixXd::Identity(1, 1) * q;
    m.R = Eigen::MatrixXd::Identity(1, 1) * r;
    m.x0.resize(1);
    m.x0(0) = x0;
    // Diffuse but finite initial covariance
    m.P0 = Eigen::MatrixXd::Identity(1, 1) * (r / q);

    return m;
}

// ---- local_trend ------------------------------------------------------------

KalmanModel KalmanModelBuilder::localTrend(double dt, double q, double r, double x0)
{
    if (dt <= 0.0) {
        throw ConfigException(
            "KalmanModelBuilder: sampling interval dt must be > 0, got "
            + std::to_string(dt) + ".");
    }
    validateNoise(q, r);

    KalmanModel m;
    m.name = "local_trend";

    // State: x = [level, trend]  (2-dimensional)
    // F = [[1, dt], [0, 1]]
    m.F.resize(2, 2);
    m.F << 1.0, dt,
           0.0, 1.0;

    // H = [1, 0]  -- only level is observed
    m.H.resize(1, 2);
    m.H << 1.0, 0.0;

    m.Q = Eigen::MatrixXd::Identity(2, 2) * q;
    m.R = Eigen::MatrixXd::Identity(1, 1) * r;

    m.x0.resize(2);
    m.x0(0) = x0;
    m.x0(1) = 0.0; // zero initial trend

    // Diffuse initial covariance: scale by r/q for each diagonal
    m.P0 = Eigen::MatrixXd::Identity(2, 2) * (r / q);

    return m;
}

// ---- constant_velocity ------------------------------------------------------

KalmanModel KalmanModelBuilder::constantVelocity(double dt, double q, double r, double x0)
{
    if (dt <= 0.0) {
        throw ConfigException(
            "KalmanModelBuilder: sampling interval dt must be > 0, got "
            + std::to_string(dt) + ".");
    }
    validateNoise(q, r);

    KalmanModel m;
    m.name = "constant_velocity";

    // State: x = [velocity, acceleration]  (2-dimensional)
    // F = [[1, dt], [0, 1]]  -- identical structure to local_trend
    m.F.resize(2, 2);
    m.F << 1.0, dt,
           0.0, 1.0;

    // H = [1, 0]  -- only velocity is directly measured
    m.H.resize(1, 2);
    m.H << 1.0, 0.0;

    m.Q = Eigen::MatrixXd::Identity(2, 2) * q;
    m.R = Eigen::MatrixXd::Identity(1, 1) * r;

    m.x0.resize(2);
    m.x0(0) = x0;
    m.x0(1) = 0.0; // zero initial acceleration

    m.P0 = Eigen::MatrixXd::Identity(2, 2) * (r / q);

    return m;
}

} // namespace loki::kalman
