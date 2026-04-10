#include "loki/evt/gev.hpp"
#include "loki/core/exceptions.hpp"
#include "loki/math/nelderMead.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

using namespace loki;

namespace loki::evt {

// -----------------------------------------------------------------------------
//  Constants
// -----------------------------------------------------------------------------

static constexpr double SIGMA_MIN = 1.0e-9;
static constexpr double PENALTY   = 1.0e12;

// -----------------------------------------------------------------------------
//  cdf
// -----------------------------------------------------------------------------

double Gev::cdf(double x, double xi, double sigma, double mu)
{
    if (sigma <= 0.0) return 0.0;

    if (std::abs(xi) < 1.0e-10) {
        // Gumbel limit.
        return std::exp(-std::exp(-(x - mu) / sigma));
    }

    const double inner = 1.0 + xi * (x - mu) / sigma;
    if (inner <= 0.0) return (xi > 0.0) ? 0.0 : 1.0;
    return std::exp(-std::pow(inner, -1.0 / xi));
}

// -----------------------------------------------------------------------------
//  quantile
// -----------------------------------------------------------------------------

double Gev::quantile(double p, double xi, double sigma, double mu)
{
    if (p <= 0.0 || p >= 1.0) return std::numeric_limits<double>::quiet_NaN();
    if (sigma <= 0.0) return mu;

    if (std::abs(xi) < 1.0e-10) {
        // Gumbel.
        return mu - sigma * std::log(-std::log(p));
    }
    return mu + sigma / xi * (std::pow(-std::log(p), -xi) - 1.0);
}

// -----------------------------------------------------------------------------
//  logLik
// -----------------------------------------------------------------------------

double Gev::logLik(const std::vector<double>& maxima,
                    double xi, double sigma, double mu)
{
    const double NEG_INF = -std::numeric_limits<double>::infinity();

    if (sigma <= SIGMA_MIN) return NEG_INF;

    const int n = static_cast<int>(maxima.size());
    if (n == 0) return NEG_INF;

    double ll = -static_cast<double>(n) * std::log(sigma);

    if (std::abs(xi) < 1.0e-10) {
        // Gumbel.
        for (const double x : maxima) {
            const double z = (x - mu) / sigma;
            ll += -z - std::exp(-z);
        }
    } else {
        const double a = -(1.0 / xi + 1.0);
        for (const double x : maxima) {
            const double inner = 1.0 + xi * (x - mu) / sigma;
            if (inner <= 0.0) return NEG_INF;
            const double logInner = std::log(inner);
            ll += a * logInner - std::exp(-logInner / xi);
        }
    }
    return ll;
}

// -----------------------------------------------------------------------------
//  fit
// -----------------------------------------------------------------------------

GevFitResult Gev::fit(const std::vector<double>& maxima)
{
    if (static_cast<int>(maxima.size()) < 3)
        throw DataException("Gev::fit: need at least 3 block maxima.");

    const int n = static_cast<int>(maxima.size());

    // Initial estimates: method of moments / L-moments approximation.
    double sumX = 0.0, sumX2 = 0.0;
    double xMin =  std::numeric_limits<double>::infinity();
    double xMax = -std::numeric_limits<double>::infinity();
    for (const double x : maxima) {
        sumX  += x;
        sumX2 += x * x;
        if (x < xMin) xMin = x;
        if (x > xMax) xMax = x;
    }
    const double meanX = sumX / static_cast<double>(n);
    const double varX  = sumX2 / static_cast<double>(n) - meanX * meanX;
    const double stdX  = (varX > 0.0) ? std::sqrt(varX) : 1.0;

    // Gumbel starting point: mu0 ~ mean - 0.5772*sigma0, sigma0 ~ stdX * sqrt(6)/pi
    const double sigma0 = stdX * std::sqrt(6.0) / std::numbers::pi;
    const double mu0    = meanX - 0.5772156649 * sigma0;

    // Objective: minimise -logLik.
    // Parameters: p[0] = log(sigma), p[1] = xi, p[2] = mu.
    auto objective = [&](const std::vector<double>& p) -> double {
        const double s  = std::exp(p[0]);
        const double xi = p[1];
        const double mu = p[2];

        const double ll = logLik(maxima, xi, s, mu);
        if (!std::isfinite(ll)) return PENALTY;
        return -ll;
    };

    const std::vector<double> x0 = {std::log(sigma0 > SIGMA_MIN ? sigma0 : 1.0),
                                     0.1,
                                     mu0};
    const auto res = loki::math::nelderMead(objective, x0, 3000, 1.0e-9);

    GevFitResult r;
    r.sigma        = std::exp(res.params[0]);
    r.xi           = res.params[1];
    r.mu           = res.params[2];
    r.logLik       = logLik(maxima, r.xi, r.sigma, r.mu);
    r.nBlockMaxima = n;
    r.converged    = res.converged;
    return r;
}

// -----------------------------------------------------------------------------
//  returnLevel
// -----------------------------------------------------------------------------

double Gev::returnLevel(double T, double xi, double sigma, double mu)
{
    // P(X <= z_T)^(1/block) = 1 - 1/T  =>  p = exp(-1/T) for one-block probability.
    // Standard formula: z_T = quantile(1 - 1/T)
    const double p = 1.0 - 1.0 / T;
    if (p <= 0.0) return mu;
    return quantile(p, xi, sigma, mu);
}

} // namespace loki::evt
