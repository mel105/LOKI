#include "loki/evt/gpd.hpp"
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

static constexpr double XI_MIN      = -0.5 + 1.0e-6; ///< Minimum allowable xi.
static constexpr double SIGMA_MIN   =  1.0e-9;        ///< Minimum allowable sigma.
static constexpr double PENALTY     =  1.0e12;        ///< Barrier penalty for constraint violations.
static constexpr int    PWM_MIN_N   =  50;            ///< Below this n, use PWM fallback.

// -----------------------------------------------------------------------------
//  cdf
// -----------------------------------------------------------------------------

double Gpd::cdf(double x, double xi, double sigma)
{
    if (sigma <= 0.0 || x < 0.0) return 1.0;
    if (std::abs(xi) < 1.0e-10) {
        // Exponential limit.
        return 1.0 - std::exp(-x / sigma);
    }
    const double inner = 1.0 + xi * x / sigma;
    if (inner <= 0.0) return 1.0;
    return 1.0 - std::pow(inner, -1.0 / xi);
}

// -----------------------------------------------------------------------------
//  quantile
// -----------------------------------------------------------------------------

double Gpd::quantile(double p, double xi, double sigma)
{
    if (p <= 0.0) return 0.0;
    if (p >= 1.0) return std::numeric_limits<double>::infinity();
    if (sigma <= 0.0) return 0.0;

    if (std::abs(xi) < 1.0e-10) {
        return -sigma * std::log(1.0 - p);
    }
    return sigma / xi * (std::pow(1.0 - p, -xi) - 1.0);
}

// -----------------------------------------------------------------------------
//  logLik
// -----------------------------------------------------------------------------

double Gpd::logLik(const std::vector<double>& exc, double xi, double sigma)
{
    const double NEG_INF = -std::numeric_limits<double>::infinity();

    if (sigma <= SIGMA_MIN || xi <= XI_MIN - 1.0e-6) return NEG_INF;

    const int n = static_cast<int>(exc.size());
    if (n == 0) return NEG_INF;

    double ll = -static_cast<double>(n) * std::log(sigma);

    if (std::abs(xi) < 1.0e-10) {
        // Exponential: logLik = -n*log(sigma) - sum(exc)/sigma
        double s = 0.0;
        for (const double e : exc) {
            if (e < 0.0) return NEG_INF;
            s += e;
        }
        ll -= s / sigma;
    } else {
        const double a = -(1.0 / xi + 1.0);
        for (const double e : exc) {
            if (e < 0.0) return NEG_INF;
            const double inner = 1.0 + xi * e / sigma;
            if (inner <= 0.0) return NEG_INF;
            ll += a * std::log(inner);
        }
    }
    return ll;
}

// -----------------------------------------------------------------------------
//  _fitPwm (Probability-Weighted Moments, fallback for small n)
// -----------------------------------------------------------------------------

GpdFitResult Gpd::_fitPwm(const std::vector<double>& exc, double threshold)
{
    const int n = static_cast<int>(exc.size());
    std::vector<double> sorted = exc;
    std::sort(sorted.begin(), sorted.end());

    // PWM estimates: b0 = mean(x), b1 = sum((i-1)/(n-1) * x_i) / n
    double b0 = 0.0;
    double b1 = 0.0;
    for (int i = 0; i < n; ++i) {
        b0 += sorted[i];
        b1 += (static_cast<double>(i) / static_cast<double>(n - 1)) * sorted[i];
    }
    b0 /= static_cast<double>(n);
    b1 /= static_cast<double>(n);

    // GPD PWM formulas (Hosking & Wallis 1987):
    //   xi    = 2 - b0 / (b0 - 2*b1)
    //   sigma = 2 * b0 * b1 / (b0 - 2*b1)
    const double denom = b0 - 2.0 * b1;

    GpdFitResult r;
    r.threshold    = threshold;
    r.nExceedances = n;
    r.converged    = false;  // PWM path, not MLE

    if (std::abs(denom) < 1.0e-12) {
        // Degenerate: assume exponential.
        r.xi    = 0.0;
        r.sigma = b0;
    } else {
        r.xi    = 2.0 - b0 / denom;
        r.sigma = 2.0 * b0 * b1 / denom;
    }

    if (r.sigma <= SIGMA_MIN) r.sigma = b0 > 0.0 ? b0 : 1.0;
    r.logLik = logLik(exc, r.xi, r.sigma);
    return r;
}

// -----------------------------------------------------------------------------
//  _fitMle
// -----------------------------------------------------------------------------

GpdFitResult Gpd::_fitMle(const std::vector<double>& exc,
                            double threshold,
                            double sigmaInit)
{
    const int n = static_cast<int>(exc.size());

    // Initial parameters: xi = 0 (exponential), sigma = mean(exc).
    const double meanExc = std::accumulate(exc.begin(), exc.end(), 0.0)
                           / static_cast<double>(n);
    const double s0 = (sigmaInit > SIGMA_MIN) ? sigmaInit : meanExc;

    // Transform: use unconstrained variable for sigma (log) and raw xi.
    // Objective: minimise -logLik with barrier on xi < XI_MIN.
    auto objective = [&](const std::vector<double>& p) -> double {
        const double s = std::exp(p[0]);  // sigma = exp(log_sigma) > 0 always
        const double x = p[1];           // xi unconstrained in search space

        // Barrier for xi <= XI_MIN.
        if (x <= XI_MIN) return PENALTY + (XI_MIN - x) * PENALTY;

        const double ll = logLik(exc, x, s);
        if (!std::isfinite(ll)) return PENALTY;
        return -ll;
    };

    const std::vector<double> x0 = {std::log(s0), 0.0};
    const auto res = loki::math::nelderMead(objective, x0, 2000, 1.0e-9);

    GpdFitResult r;
    r.threshold    = threshold;
    r.nExceedances = n;
    r.sigma        = std::exp(res.params[0]);
    r.xi           = res.params[1];
    r.logLik       = logLik(exc, r.xi, r.sigma);
    r.converged    = res.converged;
    return r;
}

// -----------------------------------------------------------------------------
//  fit
// -----------------------------------------------------------------------------

GpdFitResult Gpd::fit(const std::vector<double>& exc,
                       double threshold,
                       double sigmaInit)
{
    if (exc.empty())
        throw DataException("Gpd::fit: exceedances vector is empty.");

    if (static_cast<int>(exc.size()) < PWM_MIN_N) {
        return _fitPwm(exc, threshold);
    }
    return _fitMle(exc, threshold, sigmaInit);
}

// -----------------------------------------------------------------------------
//  returnLevel
// -----------------------------------------------------------------------------

double Gpd::returnLevel(double T, double lambda,
                         double threshold, double xi, double sigma)
{
    // P(X > z_T) = 1/T  =>  z_T = threshold + GPD_quantile(1 - 1/(T*lambda))
    const double p = 1.0 - 1.0 / (T * lambda);
    if (p <= 0.0) return threshold;
    return threshold + quantile(p, xi, sigma);
}

} // namespace loki::evt
