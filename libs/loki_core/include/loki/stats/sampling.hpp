#pragma once

#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

namespace loki::stats {

/**
 * @brief Pseudo-random sampler for statistical distributions.
 *
 * Wraps std::mt19937 (Mersenne Twister) with a clean API for drawing samples
 * from the distributions most commonly needed in time series analysis and
 * Monte Carlo simulation.
 *
 * The generator state is owned by the Sampler instance. Each call to a
 * sampling method advances the internal state. Two Sampler instances
 * constructed with the same seed produce identical sequences.
 *
 * Thread safety: NOT thread-safe. Use one Sampler per thread.
 *
 * Design note: CDFs and quantile functions live in distributions.hpp.
 * This class is concerned only with random variate generation.
 *
 * Primary use cases in LOKI:
 *   - loki_core/stats/bootstrap.hpp  (resampling indices)
 *   - loki_simulate app              (synthetic time series generation)
 *   - Monte Carlo integration        (probability estimation)
 */
class Sampler {
public:

    // -----------------------------------------------------------------------
    //  Construction
    // -----------------------------------------------------------------------

    /**
     * @brief Constructs a Sampler seeded from std::random_device.
     *
     * Produces a non-deterministic sequence (hardware entropy where available).
     * Use the seeded constructor for reproducible results.
     */
    Sampler();

    /**
     * @brief Constructs a Sampler with a fixed seed.
     *
     * Two Sampler objects constructed with the same seed produce the same
     * sequence of variates, provided they call the same methods in the same order.
     *
     * @param seed Seed value for the Mersenne Twister.
     */
    explicit Sampler(uint64_t seed);

    // -----------------------------------------------------------------------
    //  Seed control
    // -----------------------------------------------------------------------

    /**
     * @brief Reseeds the internal generator.
     *
     * Resets the generator state completely. Subsequent draws are as if a
     * fresh Sampler(seed) had been constructed.
     *
     * @param seed New seed value.
     */
    void setSeed(uint64_t seed);

    /// @brief Returns a reference to the internal generator for use with std::distributions.
    /// Intended for internal loki::stats use only -- not part of the public API.
    std::mt19937_64& rng() noexcept { return m_rng; }

    // -----------------------------------------------------------------------
    //  Continuous distributions
    // -----------------------------------------------------------------------

    /**
     * @brief Draws one sample from N(mu, sigma^2).
     *
     * @param mu    Mean.
     * @param sigma Standard deviation (must be > 0).
     * @return      Sample from the normal distribution.
     * @throws ConfigException if sigma <= 0.
     */
    double normal(double mu = 0.0, double sigma = 1.0);

    /**
     * @brief Draws one sample from Uniform(a, b).
     *
     * @param a Lower bound (inclusive).
     * @param b Upper bound (exclusive). Must satisfy b > a.
     * @return  Sample in [a, b).
     * @throws ConfigException if b <= a.
     */
    double uniform(double a = 0.0, double b = 1.0);

    /**
     * @brief Draws one sample from Exponential(lambda).
     *
     * Mean of the distribution is 1/lambda.
     *
     * @param lambda Rate parameter (must be > 0).
     * @return       Sample from the exponential distribution.
     * @throws ConfigException if lambda <= 0.
     */
    double exponential(double lambda = 1.0);

    /**
     * @brief Draws one sample from Student's t(df).
     *
     * Generated as N(0,1) / sqrt(Chi2(df) / df).
     *
     * @param df Degrees of freedom (must be > 0).
     * @return   Sample from the t-distribution.
     * @throws ConfigException if df <= 0.
     */
    double studentT(double df);

    /**
     * @brief Draws one sample from Chi-squared(df).
     *
     * Generated as sum of df squared standard normals.
     * Uses std::gamma_distribution with shape=df/2, scale=2 for efficiency.
     *
     * @param df Degrees of freedom (must be > 0).
     * @return   Sample from the chi-squared distribution.
     * @throws ConfigException if df <= 0.
     */
    double chi2(double df);

    /**
     * @brief Draws one sample from F(df1, df2).
     *
     * Generated as (Chi2(df1)/df1) / (Chi2(df2)/df2).
     *
     * @param df1 Numerator degrees of freedom (must be > 0).
     * @param df2 Denominator degrees of freedom (must be > 0).
     * @return    Sample from the F-distribution.
     * @throws ConfigException if df1 <= 0 or df2 <= 0.
     */
    double fDist(double df1, double df2);

    /**
     * @brief Draws one sample from Beta(alpha, betaParam).
     *
     * Generated via the Gamma distribution relation:
     *   X ~ Gamma(alpha, 1), Y ~ Gamma(beta, 1) => X/(X+Y) ~ Beta(alpha, beta).
     *
     * @param alpha     Shape parameter alpha (must be > 0).
     * @param betaParam Shape parameter beta  (must be > 0).
     * @return          Sample in (0, 1).
     * @throws ConfigException if alpha <= 0 or betaParam <= 0.
     */
    double beta(double alpha, double betaParam);

    /**
     * @brief Draws one sample from Gamma(shape, scale).
     *
     * Parameterisation: mean = shape * scale, variance = shape * scale^2.
     * Uses std::gamma_distribution directly.
     *
     * @param shape Shape parameter k (must be > 0).
     * @param scale Scale parameter theta (must be > 0). Default 1.0.
     * @return      Sample from the gamma distribution.
     * @throws ConfigException if shape <= 0 or scale <= 0.
     */
    double gamma(double shape, double scale = 1.0);

    /**
     * @brief Draws one sample from Laplace(mu, scale).
     *
     * PDF: f(x) = (1/(2*scale)) * exp(-|x-mu|/scale).
     * Generated via the inverse CDF method using two exponentials.
     * Mean = mu, variance = 2*scale^2.
     *
     * @param mu    Location parameter.
     * @param scale Scale parameter (must be > 0).
     * @return      Sample from the Laplace distribution.
     * @throws ConfigException if scale <= 0.
     */
    double laplace(double mu = 0.0, double scale = 1.0);

    // -----------------------------------------------------------------------
    //  Discrete distributions
    // -----------------------------------------------------------------------

    /**
     * @brief Draws one sample from Poisson(lambda).
     *
     * @param lambda Mean rate (must be > 0).
     * @return       Non-negative integer sample.
     * @throws ConfigException if lambda <= 0.
     */
    int poisson(double lambda);

    /**
     * @brief Draws one sample from Binomial(n, p).
     *
     * @param n Number of trials (must be >= 1).
     * @param p Success probability per trial (must be in [0, 1]).
     * @return  Number of successes in [0, n].
     * @throws ConfigException if n < 1 or p not in [0, 1].
     */
    int binomial(int n, double p);

    // -----------------------------------------------------------------------
    //  Vector sampling
    // -----------------------------------------------------------------------

    /**
     * @brief Draws n independent samples from N(mu, sigma^2).
     *
     * @param n     Number of samples (must be >= 1).
     * @param mu    Mean.
     * @param sigma Standard deviation (must be > 0).
     * @return      Vector of n samples.
     * @throws ConfigException if sigma <= 0 or n < 1.
     */
    std::vector<double> normalVector(int n, double mu = 0.0, double sigma = 1.0);

    /**
     * @brief Draws n independent samples from Uniform(a, b).
     *
     * @param n Number of samples (must be >= 1).
     * @param a Lower bound.
     * @param b Upper bound (must be > a).
     * @return  Vector of n samples.
     * @throws ConfigException if b <= a or n < 1.
     */
    std::vector<double> uniformVector(int n, double a = 0.0, double b = 1.0);

    // -----------------------------------------------------------------------
    //  Bootstrap index generation
    // -----------------------------------------------------------------------

    /**
     * @brief Generates n iid bootstrap indices drawn with replacement from [0, n).
     *
     * Standard (iid) bootstrap. Each index is drawn independently and uniformly
     * from {0, 1, ..., n-1}.
     *
     * WARNING: iid bootstrap is invalid for autocorrelated time series.
     * Use blockBootstrapIndices() for climatological or GNSS data.
     *
     * @param n Sample size (number of observations in the original series).
     * @return  Vector of n indices, each in [0, n).
     * @throws ConfigException if n < 2.
     */
    std::vector<std::size_t> bootstrapIndices(std::size_t n);

    /**
     * @brief Generates block bootstrap indices drawn with replacement from [0, n).
     *
     * Divides the series into consecutive blocks of length blockLength and
     * resamples whole blocks with replacement. The result is a flat vector of
     * indices of length n (last partial block truncated to fit exactly).
     *
     * Required for autocorrelated time series (climatological, GNSS, sensor data)
     * where iid bootstrap destroys the dependence structure.
     *
     * Block length selection guidance:
     *   - Set blockLength >= effective decorrelation length of the series.
     *   - Rule of thumb: lag at which ACF first drops below 1.96/sqrt(n).
     *   - For 6-hourly IWV data: blockLength ~ 40-120 (10-30 days).
     *
     * @param n           Number of observations in the original series.
     * @param blockLength Length of each block (must satisfy 1 <= blockLength <= n).
     * @return            Vector of n indices (truncated to exactly n).
     * @throws ConfigException if n < 2 or blockLength < 1 or blockLength > n.
     */
    std::vector<std::size_t> blockBootstrapIndices(std::size_t n, std::size_t blockLength);

private:

    std::mt19937_64 m_rng;  ///< 64-bit Mersenne Twister generator.
};

} // namespace loki::stats
