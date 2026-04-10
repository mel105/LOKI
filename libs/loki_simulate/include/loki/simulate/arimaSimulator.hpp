#pragma once

#include <loki/arima/arimaResult.hpp>

#include <cstdint>
#include <vector>

namespace loki::simulate {

/**
 * @brief Generates synthetic ARIMA(p,d,q) time series realizations.
 *
 * Two construction modes:
 *
 *   1. Parametric (unit-process): supply p, q, d, sigma. AR and MA
 *      coefficients are set to 1/p and 1/q respectively (stable, generic process).
 *      Useful for synthetic data generation without fitting.
 *
 *   2. Fitted (bootstrap): supply an ArimaResult from ArimaAnalyzer.
 *      The simulator uses the fitted arCoeffs, maCoeffs, arLags, maLags,
 *      and sigma2 to reproduce the estimated process stochastically.
 *
 * Generation algorithm:
 *   - Draw innovations eps[t] ~ N(0, sigma^2) using mt19937_64.
 *   - Apply the ARMA recursion:
 *       y[t] = intercept
 *              + sum_i phi[i] * y[t - arLags[i]]
 *              + sum_j theta[j] * eps[t - maLags[j]]
 *              + eps[t]
 *   - For d > 0: apply d cumulative sums to produce an I(d) series.
 *   - Burn-in of 200 samples is discarded to remove initialization effects.
 *
 * Thread safety: each call to generate() / generateBatch() is independent
 * and uses its own RNG seeded deterministically from the constructor seed.
 * generateBatch() advances the seed by 1 per simulation for reproducibility.
 */
class ArimaSimulator {
public:

    /**
     * @brief Config for parametric (synthetic) mode.
     *
     * AR coefficients: phi[i] = 1.0 / p  for i = 1..p
     * MA coefficients: theta[j] = 1.0 / q  for j = 1..q
     * These are stable and give a generic autocorrelated process.
     */
    struct ParametricConfig {
        int      p     = 1;    ///< AR order.
        int      d     = 0;    ///< Differencing order.
        int      q     = 0;    ///< MA order.
        double   sigma = 1.0;  ///< Innovation standard deviation.
        uint64_t seed  = 42;   ///< RNG seed (0 = random from std::random_device).
    };

    /**
     * @brief Constructs in parametric mode.
     * @param cfg Parametric configuration.
     * @throws ConfigException if p < 0, q < 0, d < 0, or sigma <= 0.
     */
    explicit ArimaSimulator(const ParametricConfig& cfg);

    /**
     * @brief Constructs in fitted (bootstrap) mode from an ArimaResult.
     *
     * @param result  Fitted model from ArimaAnalyzer::analyze().
     * @param seed    RNG seed for generation.
     * @throws ConfigException if result.fitted is empty.
     */
    explicit ArimaSimulator(const loki::arima::ArimaResult& result, uint64_t seed = 42);

    /**
     * @brief Generates a single realization of length n.
     * @param n Number of samples (after discarding burn-in).
     * @return  Vector of length n.
     * @throws ConfigException if n < 1.
     */
    [[nodiscard]] std::vector<double> generate(int n) const;

    /**
     * @brief Generates nSim independent realizations, each of length n.
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

    // Resolved model parameters (set by both constructors).
    std::vector<int>    m_arLags;
    std::vector<double> m_arCoeffs;
    std::vector<int>    m_maLags;
    std::vector<double> m_maCoeffs;
    double              m_intercept = 0.0;
    double              m_sigma     = 1.0;
    int                 m_d         = 0;
    uint64_t            m_seed      = 42;

    static constexpr int BURN_IN = 200;  ///< Samples discarded before returning.

    /**
     * @brief Core generation: ARMA(p,q) realization of length (n + BURN_IN),
     *        then discard the first BURN_IN samples.
     * @param n    Output length.
     * @param seed RNG seed for this specific realization.
     */
    [[nodiscard]] std::vector<double> _generateArma(int n, uint64_t seed) const;

    /**
     * @brief Apply d cumulative sums (integration) to an ARMA realization.
     * @param y Input series (modified in place).
     * @param d Differencing order.
     */
    static void _integrate(std::vector<double>& y, int d);
};

} // namespace loki::simulate
