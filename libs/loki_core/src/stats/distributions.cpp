#include <loki/stats/distributions.hpp>
#include <loki/core/exceptions.hpp>

#include <cmath>
#include <limits>
#include <stdexcept>

using namespace loki;

namespace {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Continued fraction expansion for the regularised incomplete beta function
// I_x(a, b) using Lentz's method.
// Converges for x < (a+1)/(a+b+2).
double incompleteBetaCf(double x, double a, double b)
{
    constexpr int    MAX_ITER = 200;
    constexpr double EPS      = 3.0e-7;
    constexpr double TINY     = 1.0e-30;

    const double qab = a + b;
    const double qap = a + 1.0;
    const double qam = a - 1.0;

    double c = 1.0;
    double d = 1.0 - qab * x / qap;
    if (std::fabs(d) < TINY) d = TINY;
    d        = 1.0 / d;
    double h = d;

    for (int m = 1; m <= MAX_ITER; ++m) {
        const double m2 = 2.0 * static_cast<double>(m);

        // Even step
        double aa = static_cast<double>(m) * (b - static_cast<double>(m)) * x
                    / ((qam + m2) * (a + m2));
        d = 1.0 + aa * d;
        if (std::fabs(d) < TINY) d = TINY;
        c = 1.0 + aa / c;
        if (std::fabs(c) < TINY) c = TINY;
        d  = 1.0 / d;
        h *= d * c;

        // Odd step
        aa = -(a + static_cast<double>(m)) * (qab + static_cast<double>(m)) * x
             / ((a + m2) * (qap + m2));
        d = 1.0 + aa * d;
        if (std::fabs(d) < TINY) d = TINY;
        c = 1.0 + aa / c;
        if (std::fabs(c) < TINY) c = TINY;
        d         = 1.0 / d;
        const double delta = d * c;
        h        *= delta;

        if (std::fabs(delta - 1.0) < EPS) break;
    }
    return h;
}

// Regularised incomplete beta function I_x(a, b).
double regularisedIncompleteBeta(double x, double a, double b)
{
    if (x < 0.0 || x > 1.0) {
        throw AlgorithmException("regularisedIncompleteBeta: x must be in [0, 1]");
    }
    if (x == 0.0) return 0.0;
    if (x == 1.0) return 1.0;

    const double lbeta = std::lgamma(a) + std::lgamma(b) - std::lgamma(a + b);
    const double front = std::exp(std::log(x) * a + std::log(1.0 - x) * b - lbeta);

    // Use the continued fraction directly or its reflection for better convergence.
    if (x < (a + 1.0) / (a + b + 2.0)) {
        return front * incompleteBetaCf(x, a, b) / a;
    } else {
        return 1.0 - front * incompleteBetaCf(1.0 - x, b, a) / b;
    }
}

// Regularised lower incomplete gamma function P(a, x) via series expansion.
// Accurate for x < a + 1.
double lowerIncompleteGammaSeries(double a, double x)
{
    constexpr int    MAX_ITER = 200;
    constexpr double EPS      = 3.0e-7;

    double ap  = a;
    double sum = 1.0 / a;
    double del = sum;

    for (int n = 0; n < MAX_ITER; ++n) {
        ap  += 1.0;
        del *= x / ap;
        sum += del;
        if (std::fabs(del) < std::fabs(sum) * EPS) break;
    }
    return sum * std::exp(-x + a * std::log(x) - std::lgamma(a));
}

// Regularised upper incomplete gamma function Q(a, x) via continued fraction.
// Accurate for x >= a + 1.
double upperIncompleteGammaCf(double a, double x)
{
    constexpr int    MAX_ITER = 200;
    constexpr double EPS      = 3.0e-7;
    constexpr double TINY     = 1.0e-30;

    double b = x + 1.0 - a;
    double c = 1.0 / TINY;
    double d = 1.0 / b;
    double h = d;

    for (int i = 1; i <= MAX_ITER; ++i) {
        const double fi = static_cast<double>(i);
        const double an = -fi * (fi - a);
        b += 2.0;
        d  = an * d + b;
        if (std::fabs(d) < TINY) d = TINY;
        c  = b + an / c;
        if (std::fabs(c) < TINY) c = TINY;
        d         = 1.0 / d;
        const double delta = d * c;
        h        *= delta;
        if (std::fabs(delta - 1.0) < EPS) break;
    }
    return std::exp(-x + a * std::log(x) - std::lgamma(a)) * h;
}

// Regularised lower incomplete gamma P(a, x).
double regularisedGammaP(double a, double x)
{
    if (x < 0.0) {
        throw AlgorithmException("regularisedGammaP: x must be >= 0");
    }
    if (x == 0.0) return 0.0;

    if (x < a + 1.0) {
        return lowerIncompleteGammaSeries(a, x);
    } else {
        return 1.0 - upperIncompleteGammaCf(a, x);
    }
}

// Rational approximation for the normal quantile (Beasley-Springer-Moro).
double normalQuantileImpl(double p)
{
    // Coefficients for the rational approximation.
    constexpr double A[] = {
        -3.969683028665376e+01,  2.209460984245205e+02,
        -2.759285104469687e+02,  1.383577518672690e+02,
        -3.066479806614716e+01,  2.506628277459239e+00
    };
    constexpr double B[] = {
        -5.447609879822406e+01,  1.615858368580409e+02,
        -1.556989798598866e+02,  6.680131188771972e+01,
        -1.328068155288572e+01
    };
    constexpr double C[] = {
        -7.784894002430293e-03, -3.223964580411365e-01,
        -2.400758277161838e+00, -2.549732539343734e+00,
         4.374664141464968e+00,  2.938163982698783e+00
    };
    constexpr double D[] = {
         7.784695709041462e-03,  3.224671290700398e-01,
         2.445134137142996e+00,  3.754408661907416e+00
    };

    constexpr double P_LOW  = 0.02425;
    constexpr double P_HIGH = 1.0 - P_LOW;

    double q{}, r{};

    if (p < P_LOW) {
        q = std::sqrt(-2.0 * std::log(p));
        return (((((C[0]*q+C[1])*q+C[2])*q+C[3])*q+C[4])*q+C[5]) /
               ((((D[0]*q+D[1])*q+D[2])*q+D[3])*q+1.0);
    }
    if (p <= P_HIGH) {
        q = p - 0.5;
        r = q * q;
        return (((((A[0]*r+A[1])*r+A[2])*r+A[3])*r+A[4])*r+A[5])*q /
               (((((B[0]*r+B[1])*r+B[2])*r+B[3])*r+B[4])*r+1.0);
    }
    // p > P_HIGH
    q = std::sqrt(-2.0 * std::log(1.0 - p));
    return -(((((C[0]*q+C[1])*q+C[2])*q+C[3])*q+C[4])*q+C[5]) /
             ((((D[0]*q+D[1])*q+D[2])*q+D[3])*q+1.0);
}

// Newton-Raphson inversion for tQuantile.
double tQuantileImpl(double p, double df)
{
    constexpr int    MAX_ITER = 50;
    constexpr double TOL      = 1.0e-10;

    // Initial guess via normal approximation.
    double x = loki::stats::normalQuantile(p);

    for (int i = 0; i < MAX_ITER; ++i) {
        const double fx  = loki::stats::tCdf(x, df) - p;
        // t-pdf: Gamma((df+1)/2) / (sqrt(df*pi) * Gamma(df/2)) * (1 + x^2/df)^(-(df+1)/2)
        const double lgn = std::lgamma((df + 1.0) / 2.0);
        const double lgd = std::lgamma(df / 2.0);
        const double pdf = std::exp(lgn - lgd - 0.5 * std::log(df * M_PI))
                           * std::pow(1.0 + x * x / df, -(df + 1.0) / 2.0);
        if (pdf < 1.0e-30) break;
        const double dx = fx / pdf;
        x -= dx;
        if (std::fabs(dx) < TOL) break;
    }
    return x;
}

// Newton-Raphson inversion for chi2Quantile.
double chi2QuantileImpl(double p, double df)
{
    constexpr int    MAX_ITER = 50;
    constexpr double TOL      = 1.0e-10;

    // Initial guess: Wilson-Hilferty approximation.
    const double h = 2.0 / (9.0 * df);
    const double z = loki::stats::normalQuantile(p);
    double x = df * std::pow(1.0 - h + z * std::sqrt(h), 3.0);
    if (x < 0.0) x = 0.0;

    for (int i = 0; i < MAX_ITER; ++i) {
        const double fx  = loki::stats::chi2Cdf(x, df) - p;
        // chi2-pdf: x^(df/2-1) * exp(-x/2) / (2^(df/2) * Gamma(df/2))
        const double pdf = std::exp((df / 2.0 - 1.0) * std::log(x) - x / 2.0
                                    - (df / 2.0) * std::log(2.0) - std::lgamma(df / 2.0));
        if (pdf < 1.0e-30) break;
        const double dx = fx / pdf;
        x -= dx;
        if (x < 0.0) x = 0.0;
        if (std::fabs(dx) < TOL) break;
    }
    return x;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

namespace loki::stats {

double normalCdf(double x)
{
    return 0.5 * std::erfc(-x / std::sqrt(2.0));
}

double normalQuantile(double p)
{
    if (p <= 0.0 || p >= 1.0) {
        throw AlgorithmException("normalQuantile: p must be in (0, 1)");
    }
    return normalQuantileImpl(p);
}

double tCdf(double t, double df)
{
    if (df <= 0.0) {
        throw AlgorithmException("tCdf: degrees of freedom must be > 0");
    }
    // Use incomplete beta: P(T <= t) = 1 - 0.5 * I_{df/(df+t^2)}(df/2, 1/2)  for t >= 0
    // and symmetry for t < 0.
    const double x = df / (df + t * t);
    const double ib = regularisedIncompleteBeta(x, df / 2.0, 0.5);
    if (t >= 0.0) {
        return 1.0 - 0.5 * ib;
    } else {
        return 0.5 * ib;
    }
}

double tQuantile(double p, double df)
{
    if (p <= 0.0 || p >= 1.0) {
        throw AlgorithmException("tQuantile: p must be in (0, 1)");
    }
    if (df <= 0.0) {
        throw AlgorithmException("tQuantile: degrees of freedom must be > 0");
    }
    return tQuantileImpl(p, df);
}

double chi2Cdf(double x, double df)
{
    if (x < 0.0) {
        throw AlgorithmException("chi2Cdf: x must be >= 0");
    }
    if (df <= 0.0) {
        throw AlgorithmException("chi2Cdf: degrees of freedom must be > 0");
    }
    if (x == 0.0) return 0.0;
    return regularisedGammaP(df / 2.0, x / 2.0);
}

double chi2Quantile(double p, double df)
{
    if (p <= 0.0 || p >= 1.0) {
        throw AlgorithmException("chi2Quantile: p must be in (0, 1)");
    }
    if (df <= 0.0) {
        throw AlgorithmException("chi2Quantile: degrees of freedom must be > 0");
    }
    return chi2QuantileImpl(p, df);
}

double fCdf(double x, double df1, double df2)
{
    if (x < 0.0) {
        throw AlgorithmException("fCdf: x must be >= 0");
    }
    if (df1 <= 0.0 || df2 <= 0.0) {
        throw AlgorithmException("fCdf: degrees of freedom must be > 0");
    }
    if (x == 0.0) return 0.0;
    // F-CDF via incomplete beta: I_{df1*x / (df1*x + df2)}(df1/2, df2/2)
    const double t = (df1 * x) / (df1 * x + df2);
    return regularisedIncompleteBeta(t, df1 / 2.0, df2 / 2.0);
}

} // namespace loki::stats
