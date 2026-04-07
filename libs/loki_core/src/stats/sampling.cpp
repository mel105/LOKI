#include <loki/stats/sampling.hpp>

#include <loki/core/exceptions.hpp>

#include <cmath>
#include <string>

using namespace loki;
using namespace loki::stats;

// ---------------------------------------------------------------------------
//  Construction
// ---------------------------------------------------------------------------

Sampler::Sampler()
{
    std::random_device rd;
    m_rng.seed(rd());
}

Sampler::Sampler(uint64_t seed)
    : m_rng(seed)
{}

void Sampler::setSeed(uint64_t seed)
{
    m_rng.seed(seed);
}

// ---------------------------------------------------------------------------
//  Continuous distributions
// ---------------------------------------------------------------------------

double Sampler::normal(double mu, double sigma)
{
    if (sigma <= 0.0) {
        throw ConfigException(
            "Sampler::normal: sigma must be > 0, got " + std::to_string(sigma) + ".");
    }
    std::normal_distribution<double> dist(mu, sigma);
    return dist(m_rng);
}

double Sampler::uniform(double a, double b)
{
    if (b <= a) {
        throw ConfigException(
            "Sampler::uniform: b must be > a, got a=" + std::to_string(a)
            + " b=" + std::to_string(b) + ".");
    }
    std::uniform_real_distribution<double> dist(a, b);
    return dist(m_rng);
}

double Sampler::exponential(double lambda)
{
    if (lambda <= 0.0) {
        throw ConfigException(
            "Sampler::exponential: lambda must be > 0, got "
            + std::to_string(lambda) + ".");
    }
    std::exponential_distribution<double> dist(lambda);
    return dist(m_rng);
}

double Sampler::chi2(double df)
{
    if (df <= 0.0) {
        throw ConfigException(
            "Sampler::chi2: df must be > 0, got " + std::to_string(df) + ".");
    }
    // Chi2(df) = Gamma(df/2, 2).
    std::gamma_distribution<double> dist(df / 2.0, 2.0);
    return dist(m_rng);
}

double Sampler::studentT(double df)
{
    if (df <= 0.0) {
        throw ConfigException(
            "Sampler::studentT: df must be > 0, got " + std::to_string(df) + ".");
    }
    // t(df) = N(0,1) / sqrt(Chi2(df) / df).
    std::normal_distribution<double> normDist(0.0, 1.0);
    const double z = normDist(m_rng);
    const double c = chi2(df);
    return z / std::sqrt(c / df);
}

double Sampler::fDist(double df1, double df2)
{
    if (df1 <= 0.0) {
        throw ConfigException(
            "Sampler::fDist: df1 must be > 0, got " + std::to_string(df1) + ".");
    }
    if (df2 <= 0.0) {
        throw ConfigException(
            "Sampler::fDist: df2 must be > 0, got " + std::to_string(df2) + ".");
    }
    // F(df1, df2) = (Chi2(df1)/df1) / (Chi2(df2)/df2).
    const double c1 = chi2(df1);
    const double c2 = chi2(df2);
    return (c1 / df1) / (c2 / df2);
}

double Sampler::gamma(double shape, double scale)
{
    if (shape <= 0.0) {
        throw ConfigException(
            "Sampler::gamma: shape must be > 0, got " + std::to_string(shape) + ".");
    }
    if (scale <= 0.0) {
        throw ConfigException(
            "Sampler::gamma: scale must be > 0, got " + std::to_string(scale) + ".");
    }
    std::gamma_distribution<double> dist(shape, scale);
    return dist(m_rng);
}

double Sampler::beta(double alpha, double betaParam)
{
    if (alpha <= 0.0) {
        throw ConfigException(
            "Sampler::beta: alpha must be > 0, got " + std::to_string(alpha) + ".");
    }
    if (betaParam <= 0.0) {
        throw ConfigException(
            "Sampler::beta: betaParam must be > 0, got "
            + std::to_string(betaParam) + ".");
    }
    // Beta(a, b) = Gamma(a, 1) / (Gamma(a, 1) + Gamma(b, 1)).
    const double x = gamma(alpha, 1.0);
    const double y = gamma(betaParam, 1.0);
    return x / (x + y);
}

double Sampler::laplace(double mu, double scale)
{
    if (scale <= 0.0) {
        throw ConfigException(
            "Sampler::laplace: scale must be > 0, got " + std::to_string(scale) + ".");
    }
    // Inverse CDF method: U ~ Uniform(-0.5, 0.5), X = mu - scale*sign(U)*ln(1-2|U|).
    std::uniform_real_distribution<double> dist(-0.5, 0.5);
    const double u = dist(m_rng);
    // Avoid log(0) at the boundary (probability zero, but guard anyway).
    const double absU = std::abs(u);
    if (absU >= 0.5) {
        return mu;
    }
    const double sign = (u >= 0.0) ? 1.0 : -1.0;
    return mu - scale * sign * std::log(1.0 - 2.0 * absU);
}

// ---------------------------------------------------------------------------
//  Discrete distributions
// ---------------------------------------------------------------------------

int Sampler::poisson(double lambda)
{
    if (lambda <= 0.0) {
        throw ConfigException(
            "Sampler::poisson: lambda must be > 0, got " + std::to_string(lambda) + ".");
    }
    std::poisson_distribution<int> dist(lambda);
    return dist(m_rng);
}

int Sampler::binomial(int n, double p)
{
    if (n < 1) {
        throw ConfigException(
            "Sampler::binomial: n must be >= 1, got " + std::to_string(n) + ".");
    }
    if (p < 0.0 || p > 1.0) {
        throw ConfigException(
            "Sampler::binomial: p must be in [0, 1], got " + std::to_string(p) + ".");
    }
    std::binomial_distribution<int> dist(n, p);
    return dist(m_rng);
}

// ---------------------------------------------------------------------------
//  Vector sampling
// ---------------------------------------------------------------------------

std::vector<double> Sampler::normalVector(int n, double mu, double sigma)
{
    if (n < 1) {
        throw ConfigException(
            "Sampler::normalVector: n must be >= 1, got " + std::to_string(n) + ".");
    }
    if (sigma <= 0.0) {
        throw ConfigException(
            "Sampler::normalVector: sigma must be > 0, got "
            + std::to_string(sigma) + ".");
    }
    std::normal_distribution<double> dist(mu, sigma);
    std::vector<double> result;
    result.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        result.push_back(dist(m_rng));
    }
    return result;
}

std::vector<double> Sampler::uniformVector(int n, double a, double b)
{
    if (n < 1) {
        throw ConfigException(
            "Sampler::uniformVector: n must be >= 1, got " + std::to_string(n) + ".");
    }
    if (b <= a) {
        throw ConfigException(
            "Sampler::uniformVector: b must be > a, got a=" + std::to_string(a)
            + " b=" + std::to_string(b) + ".");
    }
    std::uniform_real_distribution<double> dist(a, b);
    std::vector<double> result;
    result.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        result.push_back(dist(m_rng));
    }
    return result;
}

// ---------------------------------------------------------------------------
//  Bootstrap index generation
// ---------------------------------------------------------------------------

std::vector<std::size_t> Sampler::bootstrapIndices(std::size_t n)
{
    if (n < 2) {
        throw ConfigException(
            "Sampler::bootstrapIndices: n must be >= 2, got "
            + std::to_string(n) + ".");
    }
    std::uniform_int_distribution<std::size_t> dist(0, n - 1);
    std::vector<std::size_t> indices;
    indices.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        indices.push_back(dist(m_rng));
    }
    return indices;
}

std::vector<std::size_t> Sampler::blockBootstrapIndices(
    std::size_t n,
    std::size_t blockLength)
{
    if (n < 2) {
        throw ConfigException(
            "Sampler::blockBootstrapIndices: n must be >= 2, got "
            + std::to_string(n) + ".");
    }
    if (blockLength < 1 || blockLength > n) {
        throw ConfigException(
            "Sampler::blockBootstrapIndices: blockLength must be in [1, n], got "
            + std::to_string(blockLength) + " with n=" + std::to_string(n) + ".");
    }

    // Number of valid block start positions: any index in [0, n - blockLength].
    const std::size_t maxStart = n - blockLength;
    std::uniform_int_distribution<std::size_t> dist(0, maxStart);

    std::vector<std::size_t> indices;
    indices.reserve(n);

    while (indices.size() < n) {
        const std::size_t start = dist(m_rng);
        for (std::size_t j = 0; j < blockLength && indices.size() < n; ++j) {
            indices.push_back(start + j);
        }
    }

    return indices;
}
