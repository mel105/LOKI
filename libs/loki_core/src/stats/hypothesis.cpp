#include <loki/stats/hypothesis.hpp>
#include <loki/stats/distributions.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

using namespace loki;

namespace {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Filter NaN values according to policy, return clean vector.
std::vector<double> prepareData(
    const std::vector<double>& data,
    NanPolicy                  policy,
    const std::string&         caller,
    std::size_t                minN)
{
    std::vector<double> clean;
    clean.reserve(data.size());

    for (double v : data) {
        if (std::isnan(v)) {
            if (policy == NanPolicy::THROW) {
                throw MissingValueException(caller + ": NaN encountered with NanPolicy::THROW");
            }
            if (policy == NanPolicy::PROPAGATE) {
                // Return sentinel to signal propagation to caller.
                return {};
            }
            // NanPolicy::SKIP -- just omit
        } else {
            clean.push_back(v);
        }
    }

    if (clean.size() < minN) {
        throw DataException(
            caller + ": insufficient valid observations (" +
            std::to_string(clean.size()) + " < " + std::to_string(minN) + ")");
    }
    return clean;
}

// Sample mean of a clean (NaN-free) vector.
double sampleMean(const std::vector<double>& v)
{
    return std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(v.size());
}

// Sample variance (unbiased, ddof=1) of a clean vector.
double sampleVariance(const std::vector<double>& v, double mean)
{
    const double n = static_cast<double>(v.size());
    double ss = 0.0;
    for (double x : v) {
        const double d = x - mean;
        ss += d * d;
    }
    return ss / (n - 1.0);
}

// ---------------------------------------------------------------------------
// Shapiro-Wilk coefficients (Royston 1992 approximation)
// Valid for n in [3, 5000].
// ---------------------------------------------------------------------------

// Polynomial evaluation helper (Horner's method).
double polyval(const double* c, int deg, double x)
{
    double result = c[0];
    for (int i = 1; i <= deg; ++i) {
        result = result * x + c[i];
    }
    return result;
}

// Compute Shapiro-Wilk W statistic and its normalised z-score
// using the Royston (1992) algorithm.
// Returns {W, z} where z ~ N(0,1) under H0 for transformed W.
std::pair<double, double> shapiroWilkWZ(const std::vector<double>& sorted)
{
    const int n = static_cast<int>(sorted.size());

    // Compute expected normal order statistics (approximation).
    std::vector<double> m(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        // Approximation: use normal quantile of (i+1 - 3/8) / (n + 1/4).
        const double p = (static_cast<double>(i + 1) - 0.375) /
                         (static_cast<double>(n) + 0.25);
        m[static_cast<std::size_t>(i)] = loki::stats::normalQuantile(std::clamp(p, 1e-10, 1.0 - 1e-10));
    }

    // Compute c = m / ||m||
    double m2 = 0.0;
    for (double mi : m) m2 += mi * mi;
    const double mnorm = std::sqrt(m2);

    // Royston polynomial coefficients for a_n (leading weight).
    // Approximation valid over [3, 5000].
    const double fn = static_cast<double>(n);
    const double logn = std::log(fn);

    // Gamma polynomial (Royston 1992, Table 1 -- simplified approximation).
    // Coefficients for polynomial in log(n) giving gamma = a[n-1] * ||m||.
    static const double GAMMA_C[] = { -2.706056, 4.434685, -2.071190,
                                       -0.147981,  0.221157,  0.0 };
    const double gamma = polyval(GAMMA_C, 5, logn);

    // u = 1/sqrt(n), used in coefficient approximation.
    const double u = 1.0 / std::sqrt(fn);

    // Approximation for a[n-1] (last weight coefficient).
    static const double A1_C[] = { -3.582633, 5.682633, -1.752461,
                                    -0.293762,  0.042981,  0.0 };
    static const double A2_C[] = { -1.2725,    1.052157, -0.895,
                                     0.0,        0.0,       0.0 };

    const double a_last = polyval(A1_C, 5, u) / mnorm;
    const double a_prev = polyval(A2_C, 2, u) / mnorm;

    // Build weight vector a (symmetric).
    std::vector<double> a(static_cast<std::size_t>(n), 0.0);
    a[static_cast<std::size_t>(n) - 1] =  a_last;
    a[0]     = -a_last;
    if (n >= 4) {
        a[static_cast<std::size_t>(n) - 2] =  a_prev;
        a[1]     = -a_prev;
    }
    // Remaining weights approximated from m / ||m|| with scaling.
    const double scale = std::sqrt(1.0 - 2.0 * a_last * a_last -
                                   (n >= 4 ? 2.0 * a_prev * a_prev : 0.0));
    double mrem2 = 0.0;
    const int start = (n >= 4) ? 2 : 1;
    const int end   = n / 2;
    for (int i = start; i < end; ++i) mrem2 += m[static_cast<std::size_t>(i)] * m[static_cast<std::size_t>(i)];
    const double mremNorm = (mrem2 > 0.0) ? std::sqrt(mrem2) : 1.0;

    for (int i = start; i < end; ++i) {
        a[static_cast<std::size_t>(n) - 1 - static_cast<std::size_t>(i)] =  scale * m[static_cast<std::size_t>(n) - 1 - static_cast<std::size_t>(i)] / (mremNorm * mnorm);
        a[static_cast<std::size_t>(i)] = -a[static_cast<std::size_t>(n) - 1 - static_cast<std::size_t>(i)];
    }

    // W = (sum a_i * x_(i))^2 / sum (x_i - mean)^2
    double aw = 0.0;
    for (int i = 0; i < n; ++i) aw += a[static_cast<std::size_t>(i)] * sorted[static_cast<std::size_t>(i)];

    const double mean = sampleMean(sorted);
    double ss = 0.0;
    for (double x : sorted) { const double d = x - mean; ss += d * d; }

    const double W = (ss > 0.0) ? (aw * aw) / ss : 1.0;

    // Transform W to normality (Royston 1992).
    // z = (log(1 - W) - mu_y) / sigma_y
    // mu_y and sigma_y approximated by polynomials in log(n).
    double mu{}, sigma{};

    if (n <= 11) {
        static const double MU_C[] = { 0.544,  -0.39978, 0.025054, -0.6714e-3 };
        static const double SI_C[] = { 1.3822, -0.77857,  0.062767, -0.0020322 };
        mu    = polyval(MU_C, 3, fn);
        sigma = std::exp(polyval(SI_C, 3, fn));
    } else {
        (void)gamma;
        // Royston 1992 large-n approximation.
        mu    = std::exp(0.0038915 * std::pow(logn, 3.0)
                         - 0.083751 * logn * logn
                         - 0.31082  * logn - 1.5861);
        sigma = std::exp(0.0030302 * std::pow(logn, 2.0)
                         - 0.082676 * logn - 0.4803);
    }

    const double y  = std::log(1.0 - W);
    const double z  = (y - mu) / sigma;

    return {W, z};
}

// ---------------------------------------------------------------------------
// Kolmogorov distribution p-value (one-sided, asymptotic).
// P(D_n > d) ~ 2 * sum_{k=1}^{inf} (-1)^{k+1} exp(-2 k^2 lambda^2)
// where lambda = sqrt(n) * d.
// ---------------------------------------------------------------------------
double kolmogorovPValue(double lambda)
{
    constexpr int MAX_K = 100;
    double sum = 0.0;
    for (int k = 1; k <= MAX_K; ++k) {
        const double term = std::exp(-2.0 * static_cast<double>(k * k) * lambda * lambda);
        sum += (k % 2 == 0 ? -1.0 : 1.0) * term;
        if (std::fabs(term) < 1.0e-12) break;
    }
    return std::clamp(2.0 * sum, 0.0, 1.0);
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

namespace loki::stats {

HypothesisResult jarqueBera(
    const std::vector<double>& data,
    double                     alpha,
    NanPolicy                  policy)
{
    const auto v = prepareData(data, policy, "jarqueBera", 8);
    if (v.empty()) {
        // NanPolicy::PROPAGATE -- return neutral result.
        return {std::numeric_limits<double>::quiet_NaN(),
                std::numeric_limits<double>::quiet_NaN(),
                false, "jarque-bera"};
    }

    const double n    = static_cast<double>(v.size());
    const double mean = sampleMean(v);
    const double var  = sampleVariance(v, mean);
    const double sd   = std::sqrt(var);

    double s3 = 0.0, s4 = 0.0;
    for (double x : v) {
        const double d = (x - mean) / sd;
        s3 += d * d * d;
        s4 += d * d * d * d;
    }
    const double skewness = s3 / n;
    const double kurtosis = s4 / n - 3.0;  // excess kurtosis

    // JB = n/6 * (S^2 + K^2/4)
    const double jb     = (n / 6.0) * (skewness * skewness + kurtosis * kurtosis / 4.0);
    // Under H0, JB ~ chi2(2)
    const double pValue = 1.0 - chi2Cdf(jb, 2.0);

    return {jb, pValue, pValue < alpha, "jarque-bera"};
}

HypothesisResult shapiroWilk(
    const std::vector<double>& data,
    double                     alpha,
    NanPolicy                  policy)
{
    const auto v = prepareData(data, policy, "shapiroWilk", 3);
    if (v.empty()) {
        return {std::numeric_limits<double>::quiet_NaN(),
                std::numeric_limits<double>::quiet_NaN(),
                false, "shapiro-wilk"};
    }

    constexpr std::size_t SW_MAX_N = 5000;

    if (v.size() > SW_MAX_N) {
        LOKI_WARNING("shapiroWilk: n=" + std::to_string(v.size()) +
                     " exceeds Shapiro-Wilk limit (5000). Falling back to Jarque-Bera.");
        auto result      = jarqueBera(data, alpha, policy);
        result.testName  = "shapiro-wilk (fallback: jarque-bera)";
        return result;
    }

    // Sort a copy.
    std::vector<double> sorted = v;
    std::sort(sorted.begin(), sorted.end());

    const auto [W, z] = shapiroWilkWZ(sorted);

    // p-value: P(Z > z) = 1 - normalCdf(z) under H0 (upper tail).
    const double pValue = 1.0 - normalCdf(z);

    return {W, pValue, pValue < alpha, "shapiro-wilk"};
}

HypothesisResult kolmogorovSmirnov(
    const std::vector<double>& data,
    double                     alpha,
    NanPolicy                  policy)
{
    const auto v = prepareData(data, policy, "kolmogorovSmirnov", 5);
    if (v.empty()) {
        return {std::numeric_limits<double>::quiet_NaN(),
                std::numeric_limits<double>::quiet_NaN(),
                false, "kolmogorov-smirnov"};
    }

    const double n    = static_cast<double>(v.size());
    const double mean = sampleMean(v);
    const double sd   = std::sqrt(sampleVariance(v, mean));

    std::vector<double> sorted = v;
    std::sort(sorted.begin(), sorted.end());

    // Compute D = max |F_n(x) - F(x)| where F is N(mean, sd^2).
    double D = 0.0;
    for (std::size_t i = 0; i < sorted.size(); ++i) {
        const double fi     = static_cast<double>(i + 1) / n;
        const double fi_prev = static_cast<double>(i) / n;
        const double Fx    = normalCdf((sorted[i] - mean) / sd);
        D = std::max(D, std::fabs(fi - Fx));
        D = std::max(D, std::fabs(fi_prev - Fx));
    }

    // Asymptotic p-value via Kolmogorov distribution.
    const double lambda = (std::sqrt(n) + 0.12 + 0.11 / std::sqrt(n)) * D;
    const double pValue = kolmogorovPValue(lambda);

    return {D, pValue, pValue < alpha, "kolmogorov-smirnov"};
}

HypothesisResult runsTest(
    const std::vector<double>& data,
    double                     alpha,
    NanPolicy                  policy)
{
    const auto v = prepareData(data, policy, "runsTest", 10);
    if (v.empty()) {
        return {std::numeric_limits<double>::quiet_NaN(),
                std::numeric_limits<double>::quiet_NaN(),
                false, "runs-test"};
    }

    // Split at median.
    std::vector<double> sorted = v;
    std::sort(sorted.begin(), sorted.end());
    const double med = sorted[sorted.size() / 2];

    // Count runs of above/below median (skip ties).
    std::vector<int> signs;
    signs.reserve(v.size());
    for (double x : v) {
        if (x > med)       signs.push_back(1);
        else if (x < med)  signs.push_back(-1);
        // ties omitted
    }

    const double n1 = static_cast<double>(
        std::count(signs.begin(), signs.end(),  1));
    const double n2 = static_cast<double>(
        std::count(signs.begin(), signs.end(), -1));

    if (n1 < 1.0 || n2 < 1.0) {
        throw DataException("runsTest: all values on one side of median -- cannot perform test");
    }

    // Count runs.
    int runs = 1;
    for (std::size_t i = 1; i < signs.size(); ++i) {
        if (signs[i] != signs[i - 1]) ++runs;
    }

    const double R    = static_cast<double>(runs);
    const double n    = n1 + n2;
    const double muR  = (2.0 * n1 * n2) / n + 1.0;
    const double varR = (2.0 * n1 * n2 * (2.0 * n1 * n2 - n)) /
                        (n * n * (n - 1.0));

    if (varR <= 0.0) {
        throw AlgorithmException("runsTest: variance of runs is zero");
    }

    // Normal approximation with continuity correction.
    const double z      = (R - muR) / std::sqrt(varR);
    // Two-tailed p-value.
    const double pValue = 2.0 * (1.0 - normalCdf(std::fabs(z)));

    return {z, pValue, pValue < alpha, "runs-test"};
}

double durbinWatson(
    const std::vector<double>& residuals,
    NanPolicy                  policy)
{
    const auto v = prepareData(residuals, policy, "durbinWatson", 2);
    if (v.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    double num = 0.0;
    double den = 0.0;

    for (std::size_t i = 1; i < v.size(); ++i) {
        const double diff = v[i] - v[i - 1];
        num += diff * diff;
    }
    for (double e : v) {
        den += e * e;
    }

    if (den < std::numeric_limits<double>::epsilon()) {
        throw AlgorithmException("durbinWatson: sum of squared residuals is zero");
    }

    return num / den;
}

HypothesisResult ljungBox(
    const std::vector<double>& data,
    int                        maxLag,
    double                     alpha,
    NanPolicy                  policy)
{
    if (maxLag < 1) {
        throw DataException(
            "ljungBox: maxLag must be >= 1, got " + std::to_string(maxLag) + ".");
    }

    const auto v = prepareData(data, policy, "ljungBox",
                               static_cast<std::size_t>(maxLag) + 2);
    if (v.empty()) {
        return {std::numeric_limits<double>::quiet_NaN(),
                std::numeric_limits<double>::quiet_NaN(),
                false, "ljung-box"};
    }

    const double n = static_cast<double>(v.size());

    // Compute biased ACF estimates rho[1..maxLag].
    const double mu = sampleMean(v);
    double denom = 0.0;
    for (double xi : v) {
        const double d = xi - mu;
        denom += d * d;
    }

    if (denom < std::numeric_limits<double>::epsilon()) {
        throw AlgorithmException(
            "ljungBox: series has zero variance -- test is undefined.");
    }

    // Q = n*(n+2) * sum_{k=1}^{maxLag} rho_k^2 / (n-k)
    double Q = 0.0;
    for (int k = 1; k <= maxLag; ++k) {
        const auto kk = static_cast<std::size_t>(k);
        double numer = 0.0;
        for (std::size_t i = 0; i < v.size() - kk; ++i) {
            numer += (v[i] - mu) * (v[i + kk] - mu);
        }
        const double rho = numer / denom; // biased ACF at lag k
        Q += (rho * rho) / (n - static_cast<double>(k));
    }
    Q *= n * (n + 2.0);

    // Under H0, Q ~ chi2(maxLag).
    const double pValue = 1.0 - chi2Cdf(Q, static_cast<double>(maxLag));

    return {Q, pValue, pValue < alpha, "ljung-box"};
}

} // namespace loki::stats