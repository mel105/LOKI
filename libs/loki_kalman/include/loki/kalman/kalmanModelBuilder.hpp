#pragma once

#include "loki/kalman/kalmanModel.hpp"

namespace loki::kalman {

/**
 * @brief Factory for building standard KalmanModel configurations.
 *
 * Each static method constructs the F, H, Q, R, x0, and P0 matrices
 * appropriate for the named model. The caller supplies the scalar noise
 * parameters q and r; all matrix structure is handled internally.
 *
 * ### Models
 *
 * **local_level** (random walk + noise)
 *   State:  x = [level]
 *   F = [1],  H = [1]
 *   Best for: slowly varying climatological signals (IWV, tropospheric delay).
 *
 * **local_trend** (integrated random walk)
 *   State:  x = [level, trend]
 *   F = [[1, dt], [0, 1]],  H = [1, 0]
 *   Best for: GNSS coordinate series with long-term drift.
 *
 * **constant_velocity** (velocity + acceleration state)
 *   State:  x = [velocity, acceleration]
 *   F = [[1, dt], [0, 1]],  H = [1, 0]
 *   Best for: train velocity measured directly from encoder or Doppler radar.
 *   Note: F/H structure is identical to local_trend but physical meaning and
 *   recommended q/r scales differ (ms resolution, not days).
 *
 * ### Initial covariance
 * P0 is set to (r / q) * Identity, which provides a diffuse but finite start
 * that is appropriate for all three models when q and r are estimated from data.
 */
class KalmanModelBuilder {
public:

    /**
     * @brief Builds a local level (random walk + noise) model.
     *
     * @param q    Process noise variance (controls smoothness: small q -> smooth).
     * @param r    Measurement noise variance.
     * @param x0   Initial level estimate (default 0.0 -- caller should set to
     *             first valid observation before running the filter).
     * @throws ConfigException if q <= 0 or r <= 0.
     */
    static KalmanModel localLevel(double q, double r, double x0 = 0.0);

    /**
     * @brief Builds a local trend (integrated random walk) model.
     *
     * @param dt   Sampling interval in seconds (e.g. 21600.0 for 6h data).
     * @param q    Scalar process noise variance. Q = q * I_2.
     * @param r    Measurement noise variance.
     * @param x0   Initial level estimate.
     * @throws ConfigException if dt <= 0, q <= 0, or r <= 0.
     */
    static KalmanModel localTrend(double dt, double q, double r, double x0 = 0.0);

    /**
     * @brief Builds a constant velocity (velocity + acceleration) model.
     *
     * @param dt   Sampling interval in seconds (e.g. 0.001 for 1 kHz data).
     * @param q    Scalar process noise variance. Q = q * I_2.
     * @param r    Measurement noise variance.
     * @param x0   Initial velocity estimate.
     * @throws ConfigException if dt <= 0, q <= 0, or r <= 0.
     */
    static KalmanModel constantVelocity(double dt, double q, double r, double x0 = 0.0);
};

} // namespace loki::kalman
