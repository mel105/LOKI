#include <loki/simulate/arimaSimulator.hpp>

#include <loki/core/exceptions.hpp>

#include <cmath>
#include <numeric>
#include <random>
#include <string>

using namespace loki;

namespace loki::simulate {

// -----------------------------------------------------------------------------
//  Constructors
// -----------------------------------------------------------------------------

ArimaSimulator::ArimaSimulator(const ParametricConfig& cfg)
{
    if (cfg.p < 0) throw ConfigException("ArimaSimulator: p must be >= 0.");
    if (cfg.q < 0) throw ConfigException("ArimaSimulator: q must be >= 0.");
    if (cfg.d < 0) throw ConfigException("ArimaSimulator: d must be >= 0.");
    if (cfg.sigma <= 0.0)
        throw ConfigException("ArimaSimulator: sigma must be > 0, got "
                              + std::to_string(cfg.sigma) + ".");

    m_sigma     = cfg.sigma;
    m_d         = cfg.d;
    m_intercept = 0.0;

    // AR lags 1..p with equal stable coefficients summing to < 1.
    for (int i = 1; i <= cfg.p; ++i) {
        m_arLags.push_back(i);
        m_arCoeffs.push_back(cfg.p > 0 ? 0.5 / static_cast<double>(cfg.p) : 0.0);
    }

    // MA lags 1..q with equal coefficients.
    for (int j = 1; j <= cfg.q; ++j) {
        m_maLags.push_back(j);
        m_maCoeffs.push_back(cfg.q > 0 ? 0.3 / static_cast<double>(cfg.q) : 0.0);
    }

    m_seed = (cfg.seed == 0)
        ? std::random_device{}()
        : cfg.seed;
}

ArimaSimulator::ArimaSimulator(const loki::arima::ArimaResult& result, uint64_t seed)
{
    if (result.fitted.empty())
        throw ConfigException("ArimaSimulator: ArimaResult has no fitted values -- "
                              "model was not fitted.");

    m_arLags    = result.arLags;
    m_arCoeffs  = result.arCoeffs;
    m_maLags    = result.maLags;
    m_maCoeffs  = result.maCoeffs;
    m_intercept = result.intercept;
    m_sigma     = std::sqrt(result.sigma2 > 0.0 ? result.sigma2 : 1.0);
    m_d         = result.order.d;

    m_seed = (seed == 0)
        ? std::random_device{}()
        : seed;
}

// -----------------------------------------------------------------------------
//  Public: generate
// -----------------------------------------------------------------------------

std::vector<double> ArimaSimulator::generate(int n) const
{
    if (n < 1)
        throw ConfigException("ArimaSimulator: n must be >= 1, got "
                              + std::to_string(n) + ".");
    return _generateArma(n, m_seed);
}

// -----------------------------------------------------------------------------
//  Public: generateBatch
// -----------------------------------------------------------------------------

std::vector<std::vector<double>> ArimaSimulator::generateBatch(int n, int nSim) const
{
    if (n < 1)
        throw ConfigException("ArimaSimulator: n must be >= 1, got "
                              + std::to_string(n) + ".");
    if (nSim < 1)
        throw ConfigException("ArimaSimulator: nSim must be >= 1, got "
                              + std::to_string(nSim) + ".");

    std::vector<std::vector<double>> batch;
    batch.reserve(static_cast<std::size_t>(nSim));

    for (int s = 0; s < nSim; ++s) {
        // Advance seed by index for reproducibility across batch sizes.
        batch.push_back(_generateArma(n, m_seed + static_cast<uint64_t>(s)));
    }
    return batch;
}

// -----------------------------------------------------------------------------
//  Private: _generateArma
// -----------------------------------------------------------------------------

std::vector<double> ArimaSimulator::_generateArma(int n, uint64_t seed) const
{
    const int total = n + BURN_IN;

    // Determine required history buffer size.
    int maxArLag = 0;
    for (int lag : m_arLags) maxArLag = std::max(maxArLag, lag);
    int maxMaLag = 0;
    for (int lag : m_maLags) maxMaLag = std::max(maxMaLag, lag);

    const int histLen = std::max(maxArLag, maxMaLag) + 1;

    // Allocate series and innovations buffers (index 0 = oldest).
    std::vector<double> y(static_cast<std::size_t>(total + histLen), 0.0);
    std::vector<double> eps(static_cast<std::size_t>(total + histLen), 0.0);

    std::mt19937_64 rng(seed);
    std::normal_distribution<double> dist(0.0, m_sigma);

    for (int t = histLen; t < total + histLen; ++t) {
        eps[static_cast<std::size_t>(t)] = dist(rng);

        double val = m_intercept + eps[static_cast<std::size_t>(t)];

        for (std::size_t i = 0; i < m_arLags.size(); ++i) {
            const int lag = m_arLags[i];
            val += m_arCoeffs[i] * y[static_cast<std::size_t>(t - lag)];
        }
        for (std::size_t j = 0; j < m_maLags.size(); ++j) {
            const int lag = m_maLags[j];
            val += m_maCoeffs[j] * eps[static_cast<std::size_t>(t - lag)];
        }

        y[static_cast<std::size_t>(t)] = val;
    }

    // Extract post-burn-in samples.
    std::vector<double> result(static_cast<std::size_t>(n));
    const int startIdx = histLen + BURN_IN;
    for (int i = 0; i < n; ++i) {
        result[static_cast<std::size_t>(i)] = y[static_cast<std::size_t>(startIdx + i)];
    }

    // Apply d-order integration if needed.
    if (m_d > 0) {
        _integrate(result, m_d);
    }

    return result;
}

// -----------------------------------------------------------------------------
//  Private: _integrate
// -----------------------------------------------------------------------------

void ArimaSimulator::_integrate(std::vector<double>& y, int d)
{
    for (int pass = 0; pass < d; ++pass) {
        // Cumulative sum in place.
        std::partial_sum(y.begin(), y.end(), y.begin());
    }
}

} // namespace loki::simulate
