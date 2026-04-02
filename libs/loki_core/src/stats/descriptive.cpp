#include "loki/stats/descriptive.hpp"

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <numeric>
#include <sstream>

using namespace loki;

namespace {

// =============================================================================
// Internal helpers
// =============================================================================

inline bool isNan(double v) noexcept
{
    return std::isnan(v);
}

/**
 * @brief Applies NanPolicy to raw data and returns a clean working copy.
 *
 * THROW     - throws DataException if any NaN found.
 * SKIP      - removes NaN values; result may be shorter than input.
 * PROPAGATE - returns the original vector unchanged (NaN will spread).
 */
std::vector<double> applyNanPolicy(const std::vector<double>& x,
                                   loki::NanPolicy policy)
{
    std::size_t nanCount = 0;
    for (double v : x) { if (isNan(v)) { ++nanCount; } }

    if (nanCount == 0) { return x; }

    switch (policy) {
    case loki::NanPolicy::THROW:
        throw DataException(
            "Input contains " + std::to_string(nanCount) +
            " NaN value(s). Use NanPolicy::SKIP or NanPolicy::PROPAGATE "
            "to handle missing data.");

    case loki::NanPolicy::PROPAGATE:
        return x;

    case loki::NanPolicy::SKIP: {
        std::vector<double> clean;
        clean.reserve(x.size() - nanCount);
        for (double v : x) { if (!isNan(v)) { clean.push_back(v); } }
        return clean;
    }
    }
    return x; // unreachable
}

std::vector<double> sorted(std::vector<double> v)
{
    std::sort(v.begin(), v.end());
    return v;
}

std::size_t countNan(const std::vector<double>& x) noexcept
{
    std::size_t n = 0;
    for (double v : x) { if (isNan(v)) { ++n; } }
    return n;
}

void requireNonEmpty(const std::vector<double>& clean, const char* fn)
{
    if (clean.empty()) {
        throw DataException(
            std::string(fn) + ": no valid (non-NaN) observations remain.");
    }
}

void requireMinSize(const std::vector<double>& clean,
                    std::size_t minSize,
                    const char* fn)
{
    if (clean.size() < minSize) {
        throw DataException(
            std::string(fn) + ": requires at least " +
            std::to_string(minSize) + " valid observations, got " +
            std::to_string(clean.size()) + ".");
    }
}

double meanOf(const std::vector<double>& v) noexcept
{
    return std::accumulate(v.begin(), v.end(), 0.0) /
           static_cast<double>(v.size());
}

double medianOfSorted(const std::vector<double>& s) noexcept
{
    const std::size_t n = s.size();
    if (n % 2 == 1) { return s[n / 2]; }
    return (s[n / 2 - 1] + s[n / 2]) * 0.5;
}

// Type-7 quantile (R / NumPy default): linear interpolation.
double quantileOfSorted(const std::vector<double>& s, double p) noexcept
{
    if (s.size() == 1) { return s[0]; }
    const double h  = p * static_cast<double>(s.size() - 1);
    const auto   lo = static_cast<std::size_t>(std::floor(h));
    const auto   hi = static_cast<std::size_t>(std::ceil(h));
    return s[lo] + (h - static_cast<double>(lo)) * (s[hi] - s[lo]);
}

// Average-rank assignment with tie handling.
std::vector<double> rankData(const std::vector<double>& v)
{
    const std::size_t n = v.size();
    std::vector<std::size_t> idx(n);
    std::iota(idx.begin(), idx.end(), 0);
    std::sort(idx.begin(), idx.end(),
              [&](std::size_t a, std::size_t b){ return v[a] < v[b]; });

    std::vector<double> ranks(n);
    std::size_t i = 0;
    while (i < n) {
        std::size_t j = i;
        while (j < n && v[idx[j]] == v[idx[i]]) { ++j; }
        const double avgRank = static_cast<double>(i + j - 1) * 0.5 + 1.0;
        for (std::size_t k = i; k < j; ++k) { ranks[idx[k]] = avgRank; }
        i = j;
    }
    return ranks;
}

// Convenience: propagate NaN if any present in clean vector.
inline bool hasNan(const std::vector<double>& v) noexcept
{
    for (double x : v) { if (isNan(x)) { return true; } }
    return false;
}

} // anonymous namespace

// =============================================================================
// Public API
// =============================================================================

namespace loki::stats {

// -----------------------------------------------------------------------------
// Central tendency
// -----------------------------------------------------------------------------

double mean(const std::vector<double>& x, loki::NanPolicy policy)
{
    const auto clean = applyNanPolicy(x, policy);
    requireNonEmpty(clean, "mean");
    if (policy == loki::NanPolicy::PROPAGATE && hasNan(clean)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return meanOf(clean);
}

double median(const std::vector<double>& x, loki::NanPolicy policy)
{
    const auto clean = applyNanPolicy(x, policy);
    requireNonEmpty(clean, "median");
    if (policy == loki::NanPolicy::PROPAGATE && hasNan(clean)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return medianOfSorted(sorted(clean));
}

std::vector<double> mode(const std::vector<double>& x, loki::NanPolicy policy)
{
    const auto clean = applyNanPolicy(x, policy);
    requireNonEmpty(clean, "mode");
    if (policy == loki::NanPolicy::PROPAGATE && hasNan(clean)) {
        return { std::numeric_limits<double>::quiet_NaN() };
    }

    std::map<double, std::size_t> freq;
    for (double v : clean) { ++freq[v]; }

    std::size_t maxFreq = 0;
    for (const auto& [val, cnt] : freq) {
        if (cnt > maxFreq) { maxFreq = cnt; }
    }

    // All values distinct -- mode is not meaningful.
    if (maxFreq == 1) { return {}; }

    std::vector<double> modes;
    for (const auto& [val, cnt] : freq) {
        if (cnt == maxFreq) { modes.push_back(val); }
    }
    return modes;
}

double trimmedMean(const std::vector<double>& x,
                   double fraction,
                   loki::NanPolicy policy)
{
    if (fraction < 0.0 || fraction >= 0.5) {
        throw DataException(
            "trimmedMean: fraction must be in [0.0, 0.5), got " +
            std::to_string(fraction) + ".");
    }

    const auto clean = applyNanPolicy(x, policy);
    requireNonEmpty(clean, "trimmedMean");
    if (policy == loki::NanPolicy::PROPAGATE && hasNan(clean)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    auto s = sorted(clean);
    const auto n   = static_cast<std::size_t>(s.size());
    const auto cut = static_cast<std::size_t>(
        std::floor(fraction * static_cast<double>(n)));

    if (2 * cut >= n) {
        throw DataException(
            "trimmedMean: fraction too large -- no elements remain after trimming.");
    }

    const double sum = std::accumulate(
        s.begin() + static_cast<std::ptrdiff_t>(cut),
        s.end()   - static_cast<std::ptrdiff_t>(cut),
        0.0);
    return sum / static_cast<double>(n - 2 * cut);
}

// -----------------------------------------------------------------------------
// Dispersion
// -----------------------------------------------------------------------------

double variance(const std::vector<double>& x,
                bool population,
                loki::NanPolicy policy)
{
    const auto clean = applyNanPolicy(x, policy);
    requireMinSize(clean, population ? 1 : 2, "variance");
    if (policy == loki::NanPolicy::PROPAGATE && hasNan(clean)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const double m = meanOf(clean);
    double sumSq = 0.0;
    for (double v : clean) {
        const double d = v - m;
        sumSq += d * d;
    }
    const double denom = population
        ? static_cast<double>(clean.size())
        : static_cast<double>(clean.size() - 1);
    return sumSq / denom;
}

double stddev(const std::vector<double>& x,
              bool population,
              loki::NanPolicy policy)
{
    return std::sqrt(variance(x, population, policy));
}

double mad(const std::vector<double>& x, loki::NanPolicy policy)
{
    const auto clean = applyNanPolicy(x, policy);
    requireNonEmpty(clean, "mad");
    if (policy == loki::NanPolicy::PROPAGATE && hasNan(clean)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const double med = medianOfSorted(sorted(clean));
    std::vector<double> absdev;
    absdev.reserve(clean.size());
    for (double v : clean) { absdev.push_back(std::abs(v - med)); }
    return medianOfSorted(sorted(absdev));
}

double iqr(const std::vector<double>& x, loki::NanPolicy policy)
{
    const auto clean = applyNanPolicy(x, policy);
    requireNonEmpty(clean, "iqr");
    if (policy == loki::NanPolicy::PROPAGATE && hasNan(clean)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const auto s = sorted(clean);
    return quantileOfSorted(s, 0.75) - quantileOfSorted(s, 0.25);
}

double range(const std::vector<double>& x, loki::NanPolicy policy)
{
    const auto clean = applyNanPolicy(x, policy);
    requireNonEmpty(clean, "range");
    if (policy == loki::NanPolicy::PROPAGATE && hasNan(clean)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const auto [minIt, maxIt] = std::minmax_element(clean.begin(), clean.end());
    return *maxIt - *minIt;
}

double cv(const std::vector<double>& x, loki::NanPolicy policy)
{
    const auto clean = applyNanPolicy(x, policy);
    requireMinSize(clean, 2, "cv");
    if (policy == loki::NanPolicy::PROPAGATE && hasNan(clean)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const double m = meanOf(clean);
    if (std::abs(m) < std::numeric_limits<double>::epsilon()) {
        throw DataException(
            "cv: mean is zero or near-zero; coefficient of variation is undefined.");
    }
    const double sd = std::sqrt(variance(clean, false, loki::NanPolicy::SKIP));
    return sd / std::abs(m);
}

// -----------------------------------------------------------------------------
// Distribution shape
// -----------------------------------------------------------------------------

double skewness(const std::vector<double>& x, loki::NanPolicy policy)
{
    const auto clean = applyNanPolicy(x, policy);
    requireMinSize(clean, 3, "skewness");
    if (policy == loki::NanPolicy::PROPAGATE && hasNan(clean)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const double n = static_cast<double>(clean.size());
    const double m = meanOf(clean);
    const double s = std::sqrt(variance(clean, false, loki::NanPolicy::SKIP));

    if (s < std::numeric_limits<double>::epsilon()) { return 0.0; }

    double sum3 = 0.0;
    for (double v : clean) {
        const double z = (v - m) / s;
        sum3 += z * z * z;
    }
    return (n / ((n - 1.0) * (n - 2.0))) * sum3;
}

double kurtosis(const std::vector<double>& x, loki::NanPolicy policy)
{
    const auto clean = applyNanPolicy(x, policy);
    requireMinSize(clean, 4, "kurtosis");
    if (policy == loki::NanPolicy::PROPAGATE && hasNan(clean)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const double n = static_cast<double>(clean.size());
    const double m = meanOf(clean);
    const double s = std::sqrt(variance(clean, false, loki::NanPolicy::SKIP));

    if (s < std::numeric_limits<double>::epsilon()) { return 0.0; }

    double sum4 = 0.0;
    for (double v : clean) {
        const double z = (v - m) / s;
        sum4 += z * z * z * z;
    }
    return (n * (n + 1.0) / ((n - 1.0) * (n - 2.0) * (n - 3.0))) * sum4
           - 3.0 * (n - 1.0) * (n - 1.0) / ((n - 2.0) * (n - 3.0));
}

// -----------------------------------------------------------------------------
// Quantiles / order statistics
// -----------------------------------------------------------------------------

double quantile(const std::vector<double>& x, double p, loki::NanPolicy policy)
{
    if (p < 0.0 || p > 1.0) {
        throw DataException(
            "quantile: probability p must be in [0, 1], got " +
            std::to_string(p) + ".");
    }
    const auto clean = applyNanPolicy(x, policy);
    requireNonEmpty(clean, "quantile");
    if (policy == loki::NanPolicy::PROPAGATE && hasNan(clean)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return quantileOfSorted(sorted(clean), p);
}

std::array<double, 5> fiveNumberSummary(const std::vector<double>& x,
                                        loki::NanPolicy policy)
{
    const auto clean = applyNanPolicy(x, policy);
    requireNonEmpty(clean, "fiveNumberSummary");
    if (policy == loki::NanPolicy::PROPAGATE && hasNan(clean)) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return { nan, nan, nan, nan, nan };
    }
    const auto s = sorted(clean);
    return {
        s.front(),
        quantileOfSorted(s, 0.25),
        medianOfSorted(s),
        quantileOfSorted(s, 0.75),
        s.back()
    };
}

// -----------------------------------------------------------------------------
// Bivariate statistics
// -----------------------------------------------------------------------------

double covariance(const std::vector<double>& x,
                  const std::vector<double>& y,
                  loki::NanPolicy policy)
{
    if (x.size() != y.size()) {
        throw DataException(
            "covariance: x and y must have the same length (" +
            std::to_string(x.size()) + " vs " +
            std::to_string(y.size()) + ").");
    }

    std::vector<double> cx, cy;
    cx.reserve(x.size());
    cy.reserve(y.size());

    for (std::size_t i = 0; i < x.size(); ++i) {
        const bool xNan = isNan(x[i]);
        const bool yNan = isNan(y[i]);
        if (xNan || yNan) {
            if (policy == loki::NanPolicy::THROW) {
                throw DataException(
                    "covariance: NaN at index " + std::to_string(i) + ".");
            }
            if (policy == loki::NanPolicy::PROPAGATE) {
                return std::numeric_limits<double>::quiet_NaN();
            }
        } else {
            cx.push_back(x[i]);
            cy.push_back(y[i]);
        }
    }

    requireMinSize(cx, 2, "covariance");

    const double mx = meanOf(cx);
    const double my = meanOf(cy);
    double sum = 0.0;
    for (std::size_t i = 0; i < cx.size(); ++i) {
        sum += (cx[i] - mx) * (cy[i] - my);
    }
    return sum / static_cast<double>(cx.size() - 1);
}

double pearsonR(const std::vector<double>& x,
                const std::vector<double>& y,
                loki::NanPolicy policy)
{
    const double cov = covariance(x, y, policy);
    if (std::isnan(cov)) { return cov; }

    std::vector<double> cx, cy;
    cx.reserve(x.size());
    cy.reserve(y.size());
    for (std::size_t i = 0; i < x.size(); ++i) {
        if (!isNan(x[i]) && !isNan(y[i])) {
            cx.push_back(x[i]);
            cy.push_back(y[i]);
        }
    }

    const double sx = std::sqrt(variance(cx, false, loki::NanPolicy::SKIP));
    const double sy = std::sqrt(variance(cy, false, loki::NanPolicy::SKIP));

    if (sx < std::numeric_limits<double>::epsilon() ||
        sy < std::numeric_limits<double>::epsilon()) {
        throw DataException(
            "pearsonR: standard deviation is zero; correlation is undefined.");
    }
    return cov / (sx * sy);
}

double spearmanR(const std::vector<double>& x,
                 const std::vector<double>& y,
                 loki::NanPolicy policy)
{
    if (x.size() != y.size()) {
        throw DataException(
            "spearmanR: x and y must have the same length (" +
            std::to_string(x.size()) + " vs " +
            std::to_string(y.size()) + ").");
    }

    std::vector<double> cx, cy;
    cx.reserve(x.size());
    cy.reserve(y.size());
    for (std::size_t i = 0; i < x.size(); ++i) {
        if (isNan(x[i]) || isNan(y[i])) {
            if (policy == loki::NanPolicy::THROW) {
                throw DataException(
                    "spearmanR: NaN at index " + std::to_string(i) + ".");
            }
            if (policy == loki::NanPolicy::PROPAGATE) {
                return std::numeric_limits<double>::quiet_NaN();
            }
        } else {
            cx.push_back(x[i]);
            cy.push_back(y[i]);
        }
    }

    requireMinSize(cx, 2, "spearmanR");

    const std::vector<double> rx = rankData(cx);
    const std::vector<double> ry = rankData(cy);
    return pearsonR(rx, ry, loki::NanPolicy::THROW);
}

// -----------------------------------------------------------------------------
// Time series specific
// -----------------------------------------------------------------------------

double autocorrelation(const std::vector<double>& x,
                       int lag,
                       loki::NanPolicy policy)
{
    if (lag < 0) {
        throw DataException(
            "autocorrelation: lag must be non-negative, got " +
            std::to_string(lag) + ".");
    }

    const auto clean = applyNanPolicy(x, policy);
    requireNonEmpty(clean, "autocorrelation");
    if (policy == loki::NanPolicy::PROPAGATE && hasNan(clean)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    if (static_cast<std::size_t>(lag) >= clean.size()) {
        throw DataException(
            "autocorrelation: lag (" + std::to_string(lag) +
            ") must be less than series length (" +
            std::to_string(clean.size()) + ").");
    }

    if (lag == 0) { return 1.0; }

    const double m = meanOf(clean);
    const std::size_t n = clean.size();

    double denom = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double d = clean[i] - m;
        denom += d * d;
    }

    if (denom < std::numeric_limits<double>::epsilon()) { return 0.0; }

    double numer = 0.0;
    const auto k = static_cast<std::size_t>(lag);
    for (std::size_t i = 0; i < n - k; ++i) {
        numer += (clean[i] - m) * (clean[i + k] - m);
    }
    return numer / denom; // biased estimator
}

std::vector<double> acf(const std::vector<double>& x,
                        int maxLag,
                        loki::NanPolicy policy)
{
    if (maxLag < 0) {
        throw DataException(
            "acf: maxLag must be non-negative, got " +
            std::to_string(maxLag) + ".");
    }

    const auto clean = applyNanPolicy(x, policy);
    requireNonEmpty(clean, "acf");

    if (static_cast<std::size_t>(maxLag) >= clean.size()) {
        throw DataException(
            "acf: maxLag (" + std::to_string(maxLag) +
            ") must be less than series length (" +
            std::to_string(clean.size()) + ").");
    }

    std::vector<double> result;
    result.reserve(static_cast<std::size_t>(maxLag) + 1);
    for (int k = 0; k <= maxLag; ++k) {
        result.push_back(autocorrelation(clean, k, loki::NanPolicy::THROW));
    }
    return result;
}

double hurstExponent(const std::vector<double>& x, loki::NanPolicy policy)
{
    static constexpr std::size_t MIN_N = 20;

    const auto clean = applyNanPolicy(x, policy);
    requireMinSize(clean, MIN_N, "hurstExponent");
    if (policy == loki::NanPolicy::PROPAGATE && hasNan(clean)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const std::size_t n = clean.size();

    // Subseries lengths: powers of 2 from 4 up to n/2, plus n itself.
    std::vector<std::size_t> lengths;
    for (std::size_t len = 4; len <= n / 2; len *= 2) {
        lengths.push_back(len);
    }
    if (lengths.empty() || lengths.back() != n) {
        lengths.push_back(n);
    }

    std::vector<double> logLengths, logRS;
    logLengths.reserve(lengths.size());
    logRS.reserve(lengths.size());

    for (std::size_t len : lengths) {
        const std::size_t numSubs = n / len;
        double rsSum = 0.0;
        std::size_t rsCount = 0;

        for (std::size_t s = 0; s < numSubs; ++s) {
            const std::size_t start = s * len;
            const std::vector<double> sub(
                clean.begin() + static_cast<std::ptrdiff_t>(start),
                clean.begin() + static_cast<std::ptrdiff_t>(start + len));

            const double subMean = meanOf(sub);

            std::vector<double> dev(len);
            double cumDev = 0.0;
            for (std::size_t i = 0; i < len; ++i) {
                cumDev += sub[i] - subMean;
                dev[i] = cumDev;
            }

            const double devMin = *std::min_element(dev.begin(), dev.end());
            const double devMax = *std::max_element(dev.begin(), dev.end());
            const double R = devMax - devMin;

            double sumSq = 0.0;
            for (double v : sub) {
                const double d = v - subMean;
                sumSq += d * d;
            }
            const double S = std::sqrt(sumSq / static_cast<double>(len));

            if (S > std::numeric_limits<double>::epsilon()) {
                rsSum += R / S;
                ++rsCount;
            }
        }

        if (rsCount > 0) {
            const double meanRS = rsSum / static_cast<double>(rsCount);
            logLengths.push_back(std::log(static_cast<double>(len)));
            logRS.push_back(std::log(meanRS));
        }
    }

    if (logLengths.size() < 2) {
        throw DataException(
            "hurstExponent: insufficient data points for R/S regression.");
    }

    // OLS: slope of log(R/S) ~ H * log(n) + c gives H.
    const double xMean = meanOf(logLengths);
    const double yMean = meanOf(logRS);
    double ssxy = 0.0;
    double ssxx = 0.0;
    for (std::size_t i = 0; i < logLengths.size(); ++i) {
        const double dx = logLengths[i] - xMean;
        ssxy += dx * (logRS[i] - yMean);
        ssxx += dx * dx;
    }

    if (ssxx < std::numeric_limits<double>::epsilon()) {
        throw DataException(
            "hurstExponent: degenerate regression (zero variance in log-lengths).");
    }
    return ssxy / ssxx;
}

// -----------------------------------------------------------------------------


// =============================================================================
// New functions to append to descriptive.cpp (after hurstExponent, before summarize)
// =============================================================================

// -----------------------------------------------------------------------------
// pacf
// -----------------------------------------------------------------------------

std::vector<double> pacf(const std::vector<double>& x,
                         int maxLag,
                         loki::NanPolicy policy)
{
    if (maxLag < 1) {
        throw DataException(
            "pacf: maxLag must be >= 1, got " + std::to_string(maxLag) + ".");
    }

    const auto clean = applyNanPolicy(x, policy);
    requireNonEmpty(clean, "pacf");

    if (static_cast<std::size_t>(maxLag) >= clean.size()) {
        throw DataException(
            "pacf: maxLag (" + std::to_string(maxLag) +
            ") must be less than series length (" +
            std::to_string(clean.size()) + ").");
    }

    // Precompute ACF values r[0..maxLag]. r[0] = 1.0 by definition.
    const std::size_t p = static_cast<std::size_t>(maxLag);
    std::vector<double> r(p + 1);
    for (std::size_t k = 0; k <= p; ++k) {
        r[k] = autocorrelation(clean, static_cast<int>(k), loki::NanPolicy::THROW);
    }

    // Result vector: element 0 = 1.0, elements 1..maxLag filled below.
    std::vector<double> result(p + 1, 0.0);
    result[0] = 1.0;

    // For each order m from 1 to maxLag, solve the Yule-Walker system
    // R_m * phi_m = r_m, where R_m is the m x m Toeplitz ACF matrix
    // and r_m = [r[1], ..., r[m]]^T.
    // The PACF at lag m is phi_m[m-1] (the last component).
    //
    // We use Eigen's LDLT solver on the symmetric Toeplitz matrix.
    for (std::size_t m = 1; m <= p; ++m) {
        Eigen::MatrixXd R(static_cast<Eigen::Index>(m),
                          static_cast<Eigen::Index>(m));
        Eigen::VectorXd rhs(static_cast<Eigen::Index>(m));

        for (Eigen::Index i = 0; i < static_cast<Eigen::Index>(m); ++i) {
            rhs(i) = r[static_cast<std::size_t>(i) + 1];
            for (Eigen::Index j = 0; j < static_cast<Eigen::Index>(m); ++j) {
                const std::size_t diff =
                    static_cast<std::size_t>(std::abs(i - j));
                R(i, j) = r[diff];
            }
        }

        const Eigen::LDLT<Eigen::MatrixXd> ldlt(R);
        if (ldlt.info() != Eigen::Success) {
            throw AlgorithmException(
                "pacf: Yule-Walker system is numerically singular at lag " +
                std::to_string(m) + ".");
        }

        const Eigen::VectorXd phi = ldlt.solve(rhs);
        // The PACF at lag m is the last coefficient phi[m-1].
        result[m] = phi(static_cast<Eigen::Index>(m) - 1);
    }

    return result;
}

// -----------------------------------------------------------------------------
// diff
// -----------------------------------------------------------------------------

std::vector<double> diff(const std::vector<double>& x,
                         loki::NanPolicy policy)
{
    const auto clean = applyNanPolicy(x, policy);
    requireMinSize(clean, 2, "diff");
    if (policy == loki::NanPolicy::PROPAGATE && hasNan(clean)) {
        return std::vector<double>(clean.size() - 1,
                                   std::numeric_limits<double>::quiet_NaN());
    }

    std::vector<double> result;
    result.reserve(clean.size() - 1);
    for (std::size_t i = 1; i < clean.size(); ++i) {
        result.push_back(clean[i] - clean[i - 1]);
    }
    return result;
}

// -----------------------------------------------------------------------------
// laggedDiff
// -----------------------------------------------------------------------------

std::vector<double> laggedDiff(const std::vector<double>& x,
                                int s,
                                loki::NanPolicy policy)
{
    if (s < 1) {
        throw DataException(
            "laggedDiff: s must be >= 1, got " + std::to_string(s) + ".");
    }

    const auto clean = applyNanPolicy(x, policy);

    if (clean.size() <= static_cast<std::size_t>(s)) {
        throw DataException(
            "laggedDiff: series length (" + std::to_string(clean.size()) +
            ") must be greater than s (" + std::to_string(s) + ").");
    }

    if (policy == loki::NanPolicy::PROPAGATE && hasNan(clean)) {
        return std::vector<double>(clean.size() - static_cast<std::size_t>(s),
                                   std::numeric_limits<double>::quiet_NaN());
    }

    const std::size_t step = static_cast<std::size_t>(s);
    std::vector<double> result;
    result.reserve(clean.size() - step);
    for (std::size_t i = 0; i < clean.size() - step; ++i) {
        result.push_back(clean[i + step] - clean[i]);
    }
    return result;
}

// Summary
// -----------------------------------------------------------------------------

SummaryStats summarize(const std::vector<double>& x,
                       loki::NanPolicy policy,
                       bool computeHurst)
{
    if (x.empty()) {
        throw DataException("summarize: input vector is empty.");
    }

    const std::size_t nMissing = countNan(x);

    std::vector<double> clean;
    clean.reserve(x.size() - nMissing);
    for (double v : x) { if (!isNan(v)) { clean.push_back(v); } }

    if (clean.empty()) {
        throw DataException("summarize: all values are NaN.");
    }

    // If caller chose THROW, honour it before we strip NaNs ourselves.
    if (policy == loki::NanPolicy::THROW && nMissing > 0) {
        throw DataException(
            "summarize: input contains " + std::to_string(nMissing) +
            " NaN value(s) and NanPolicy::THROW is active.");
    }

    const auto s = sorted(clean);
    const std::size_t n = clean.size();

    SummaryStats st{};
    st.n        = n;
    st.nMissing = nMissing;
    st.min      = s.front();
    st.max      = s.back();
    st.range    = st.max - st.min;
    st.mean     = meanOf(clean);
    st.median   = medianOfSorted(s);
    st.q1       = quantileOfSorted(s, 0.25);
    st.q3       = quantileOfSorted(s, 0.75);
    st.iqr      = st.q3 - st.q1;

    const double nan = std::numeric_limits<double>::quiet_NaN();

    if (n >= 2) {
        double sumSq = 0.0;
        for (double v : clean) {
            const double d = v - st.mean;
            sumSq += d * d;
        }
        st.variance = sumSq / static_cast<double>(n - 1);
        st.stddev   = std::sqrt(st.variance);
    } else {
        st.variance = nan;
        st.stddev   = nan;
    }

    {
        std::vector<double> absdev;
        absdev.reserve(n);
        for (double v : clean) { absdev.push_back(std::abs(v - st.median)); }
        st.mad = medianOfSorted(sorted(absdev));
    }

    st.cv = (std::abs(st.mean) > std::numeric_limits<double>::epsilon() && n >= 2)
        ? st.stddev / std::abs(st.mean)
        : nan;

    if (n >= 3 && !std::isnan(st.stddev) &&
        st.stddev > std::numeric_limits<double>::epsilon()) {
        const double dn = static_cast<double>(n);
        double sum3 = 0.0;
        for (double v : clean) {
            const double z = (v - st.mean) / st.stddev;
            sum3 += z * z * z;
        }
        st.skewness = (dn / ((dn - 1.0) * (dn - 2.0))) * sum3;
    } else {
        st.skewness = nan;
    }

    if (n >= 4 && !std::isnan(st.stddev) &&
        st.stddev > std::numeric_limits<double>::epsilon()) {
        const double dn = static_cast<double>(n);
        double sum4 = 0.0;
        for (double v : clean) {
            const double z = (v - st.mean) / st.stddev;
            sum4 += z * z * z * z;
        }
        st.kurtosis =
            (dn * (dn + 1.0) / ((dn - 1.0) * (dn - 2.0) * (dn - 3.0))) * sum4
            - 3.0 * (dn - 1.0) * (dn - 1.0) / ((dn - 2.0) * (dn - 3.0));
    } else {
        st.kurtosis = nan;
    }

    if (computeHurst && n >= 20) {
        try {
            st.hurstExp = hurstExponent(clean, loki::NanPolicy::THROW);
        } catch (const DataException&) {
            st.hurstExp = nan;
        }
    } else {
        st.hurstExp = nan;
    }

    return st;
}

std::string formatSummary(const SummaryStats& s, const std::string& label)
{
    auto fmt = [](double v) -> std::string {
        if (std::isnan(v)) { return "       NaN"; }
        std::ostringstream oss;
        oss << std::setw(10) << std::fixed << std::setprecision(4) << v;
        return oss.str();
    };

    const std::string hdr =
        label.empty() ? "Descriptive Statistics"
                      : "Descriptive Statistics: " + label;

    std::ostringstream out;
    out << hdr << "\n";
    out << std::string(hdr.size(), '-') << "\n";
    out << "  n           : " << s.n        << "\n";
    out << "  missing     : " << s.nMissing << "\n";
    out << "  min         : " << fmt(s.min)      << "\n";
    out << "  max         : " << fmt(s.max)      << "\n";
    out << "  range       : " << fmt(s.range)    << "\n";
    out << "  mean        : " << fmt(s.mean)     << "\n";
    out << "  median      : " << fmt(s.median)   << "\n";
    out << "  Q1          : " << fmt(s.q1)       << "\n";
    out << "  Q3          : " << fmt(s.q3)       << "\n";
    out << "  IQR         : " << fmt(s.iqr)      << "\n";
    out << "  variance    : " << fmt(s.variance) << "\n";
    out << "  std dev     : " << fmt(s.stddev)   << "\n";
    out << "  MAD         : " << fmt(s.mad)      << "\n";
    out << "  CV          : " << fmt(s.cv)       << "\n";
    out << "  skewness    : " << fmt(s.skewness) << "\n";
    out << "  kurtosis    : " << fmt(s.kurtosis) << "\n";
    out << "  Hurst exp.  : " << fmt(s.hurstExp) << "\n";
    return out.str();
}

} // namespace loki::stats