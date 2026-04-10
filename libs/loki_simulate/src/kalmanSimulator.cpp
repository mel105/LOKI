#include <loki/simulate/kalmanSimulator.hpp>

#include <loki/core/exceptions.hpp>

#include <cmath>
#include <random>
#include <string>

using namespace loki;

namespace loki::simulate {

// -----------------------------------------------------------------------------
//  Constructor
// -----------------------------------------------------------------------------

KalmanSimulator::KalmanSimulator(Config cfg) : m_cfg(std::move(cfg))
{
    if (m_cfg.model != "local_level"
        && m_cfg.model != "local_trend"
        && m_cfg.model != "constant_velocity")
    {
        throw ConfigException(
            "KalmanSimulator: unknown model '" + m_cfg.model
            + "'. Valid: local_level, local_trend, constant_velocity.");
    }
    if (m_cfg.Q <= 0.0)
        throw ConfigException("KalmanSimulator: Q must be > 0, got "
                              + std::to_string(m_cfg.Q) + ".");
    if (m_cfg.R <= 0.0)
        throw ConfigException("KalmanSimulator: R must be > 0, got "
                              + std::to_string(m_cfg.R) + ".");
    if (m_cfg.dt <= 0.0)
        throw ConfigException("KalmanSimulator: dt must be > 0, got "
                              + std::to_string(m_cfg.dt) + ".");

    if (m_cfg.seed == 0) {
        m_cfg.seed = std::random_device{}();
    }
}

// -----------------------------------------------------------------------------
//  Public: generate
// -----------------------------------------------------------------------------

std::vector<double> KalmanSimulator::generate(int n) const
{
    if (n < 1)
        throw ConfigException("KalmanSimulator: n must be >= 1, got "
                              + std::to_string(n) + ".");
    return _generateOnce(n, m_cfg.seed);
}

// -----------------------------------------------------------------------------
//  Public: generateBatch
// -----------------------------------------------------------------------------

std::vector<std::vector<double>> KalmanSimulator::generateBatch(int n, int nSim) const
{
    if (n < 1)
        throw ConfigException("KalmanSimulator: n must be >= 1, got "
                              + std::to_string(n) + ".");
    if (nSim < 1)
        throw ConfigException("KalmanSimulator: nSim must be >= 1, got "
                              + std::to_string(nSim) + ".");

    std::vector<std::vector<double>> batch;
    batch.reserve(static_cast<std::size_t>(nSim));
    for (int s = 0; s < nSim; ++s) {
        batch.push_back(_generateOnce(n, m_cfg.seed + static_cast<uint64_t>(s)));
    }
    return batch;
}

// -----------------------------------------------------------------------------
//  Private: _generateOnce
// -----------------------------------------------------------------------------

std::vector<double> KalmanSimulator::_generateOnce(int n, uint64_t seed) const
{
    const int total = n + BURN_IN;
    const bool is1D = (m_cfg.model == "local_level");
    const int  dim  = is1D ? 1 : 2;
    const double dt = m_cfg.dt;
    const double qv = m_cfg.Q;
    const double rv = m_cfg.R;

    // Build F and Q matrices (flat row-major for 1D/2D).
    // For local_level: F = [1], Q = [qv], H = [1].
    // For local_trend / constant_velocity: F = [[1,dt],[0,1]], Q = qv*I2, H = [1,0].

    std::mt19937_64 rng(seed);
    std::normal_distribution<double> noise(0.0, 1.0);

    // State vector (max 2 components).
    double x0 = m_cfg.x0;
    double x1 = 0.0;  // Only used in 2D models.

    std::vector<double> obs;
    obs.reserve(static_cast<std::size_t>(total));

    const double sqrtQ = std::sqrt(qv);
    const double sqrtR = std::sqrt(rv);

    for (int t = 0; t < total; ++t) {
        if (is1D) {
            // Propagate: x[t] = x[t-1] + w, w ~ N(0, Q)
            x0 = x0 + sqrtQ * noise(rng);
            // Observe: y[t] = x[t] + v, v ~ N(0, R)
            const double y = x0 + sqrtR * noise(rng);
            obs.push_back(y);
        } else {
            // Propagate: [x0; x1] = F * [x0; x1] + w
            const double w0 = sqrtQ * noise(rng);
            const double w1 = sqrtQ * noise(rng);
            const double newX0 = x0 + dt * x1 + w0;
            const double newX1 = x1 + w1;
            x0 = newX0;
            x1 = newX1;
            // Observe: y[t] = x0 + v
            const double y = x0 + sqrtR * noise(rng);
            obs.push_back(y);
        }
    }

    // Discard burn-in.
    std::vector<double> result(obs.begin() + BURN_IN, obs.end());
    // Suppress unused variable warning when dim is not used beyond declaration.
    (void)dim;
    return result;
}

} // namespace loki::simulate
