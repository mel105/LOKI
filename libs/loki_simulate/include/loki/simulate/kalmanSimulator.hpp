#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace loki::simulate {

/**
 * @brief Generates synthetic Kalman state-space time series realizations.
 *
 * Supports the three standard loki_kalman models:
 *   - "local_level"        : 1-D random walk + observation noise
 *   - "local_trend"        : 2-D integrated random walk (level + trend)
 *   - "constant_velocity"  : 2-D velocity + acceleration model
 *
 * Generation algorithm (forward simulation):
 *   x[0] ~ N(x0, P0)    (initial state)
 *   x[t] = F * x[t-1] + w[t],   w[t] ~ N(0, Q)
 *   y[t] = H * x[t]   + v[t],   v[t] ~ N(0, R)
 *
 * The observation y[t] = H * x[t] + v[t] is a scalar (H selects the first
 * state component). Only observations are returned; internal states are
 * discarded unless the caller needs them via a future extension.
 *
 * A burn-in of 50 steps is discarded to avoid initialization transients.
 */
class KalmanSimulator {
public:

    /**
     * @brief Configuration for Kalman state-space generation.
     */
    struct Config {
        std::string model = "local_level";  ///< "local_level"|"local_trend"|"constant_velocity"
        double      Q     = 0.001;          ///< Process noise variance (scalar, applied to Q diagonal).
        double      R     = 0.01;           ///< Observation noise variance.
        double      dt    = 1.0;            ///< Sampling interval in seconds.
        double      x0    = 0.0;            ///< Initial state (first component).
        uint64_t    seed  = 42;             ///< RNG seed (0 = random from std::random_device).
    };

    /**
     * @brief Constructs from a KalmanSimulator::Config.
     * @throws ConfigException if model name is unrecognised, Q <= 0, R <= 0, or dt <= 0.
     */
    explicit KalmanSimulator(Config cfg);

    /**
     * @brief Generates a single observation series of length n.
     * @param n Number of samples (burn-in is additional and discarded).
     * @return  Observation vector of length n.
     * @throws ConfigException if n < 1.
     */
    [[nodiscard]] std::vector<double> generate(int n) const;

    /**
     * @brief Generates nSim independent observation series, each of length n.
     *
     * Each simulation uses seed + simIndex for reproducibility.
     *
     * @param n    Series length.
     * @param nSim Number of simulations.
     * @return     Outer: simulation index; inner: time step.
     * @throws ConfigException if n < 1 or nSim < 1.
     */
    [[nodiscard]] std::vector<std::vector<double>> generateBatch(int n, int nSim) const;

private:

    Config m_cfg;

    static constexpr int BURN_IN = 50;  ///< Warm-up steps discarded per realization.

    /**
     * @brief Core generation for a single realization.
     * @param n    Output length.
     * @param seed RNG seed for this specific realization.
     */
    [[nodiscard]] std::vector<double> _generateOnce(int n, uint64_t seed) const;
};

} // namespace loki::simulate
