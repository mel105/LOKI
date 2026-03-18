#include <loki/stats/filter.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <string_view>

using namespace loki;

namespace {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

/**
 * @brief Checks that x contains no NaN values.
 * @throws loki::DataException if any NaN is found.
 */
void checkNoNaN(const std::vector<double>& x, std::string_view callerName)
{
    for (std::size_t i = 0; i < x.size(); ++i) {
        if (std::isnan(x[i])) {
            throw DataException(
                std::string(callerName) +
                ": input contains NaN at index " + std::to_string(i) +
                ". Run GapFiller before applying filters.");
        }
    }
}

/**
 * @brief Ensures windowSize is odd and >= 3.
 * Even values are rounded up; a warning is logged.
 * @throws loki::ConfigException if windowSize < 3 after rounding.
 */
int validateWindowSize(int windowSize, std::string_view callerName)
{
    if (windowSize % 2 == 0) {
        LOKI_WARNING(std::string(callerName) +
                     ": windowSize " + std::to_string(windowSize) +
                     " is even; rounded up to " + std::to_string(windowSize + 1));
        windowSize += 1;
    }
    if (windowSize < 3) {
        throw ConfigException(
            std::string(callerName) +
            ": windowSize must be >= 3, got " + std::to_string(windowSize));
    }
    return windowSize;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// loki::stats implementations
// ---------------------------------------------------------------------------

namespace loki::stats {

std::vector<double> movingAverage(const std::vector<double>& x, int windowSize)
{
    checkNoNaN(x, "movingAverage");
    windowSize = validateWindowSize(windowSize, "movingAverage");

    const std::size_t n    = x.size();
    const int         half = windowSize / 2;
    std::vector<double> out(n, NaN);

    if (static_cast<int>(n) < windowSize) {
        LOKI_WARNING("movingAverage: series length " + std::to_string(n) +
                     " is smaller than windowSize " + std::to_string(windowSize) +
                     "; returning all-NaN output.");
        return out;
    }

    // Build prefix sums for O(1) window queries.
    std::vector<double> prefix(n + 1, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        prefix[i + 1] = prefix[i] + x[i];
    }

    for (std::size_t i = static_cast<std::size_t>(half);
         i + static_cast<std::size_t>(half) < n;
         ++i)
    {
        const std::size_t lo = i - static_cast<std::size_t>(half);
        const std::size_t hi = i + static_cast<std::size_t>(half) + 1; // exclusive
        out[i] = (prefix[hi] - prefix[lo]) / static_cast<double>(windowSize);
    }

    return out;
}

// ---------------------------------------------------------------------------

std::vector<double> exponentialMovingAverage(const std::vector<double>& x,
                                             double alpha)
{
    checkNoNaN(x, "exponentialMovingAverage");

    if (alpha <= 0.0 || alpha > 1.0) {
        throw ConfigException(
            "exponentialMovingAverage: alpha must be in (0, 1], got " +
            std::to_string(alpha));
    }

    const std::size_t   n = x.size();
    std::vector<double> out(n, NaN);

    if (n == 0) {
        return out;
    }

    // alpha == 1 is a trivial copy.
    if (alpha == 1.0) {
        return x;
    }

    const double oneMinusAlpha = 1.0 - alpha;
    double       prev          = NaN;
    bool         initialised   = false;

    for (std::size_t i = 0; i < n; ++i) {
        if (std::isnan(x[i])) {
            // NaN input: output NaN; accumulator keeps last valid value.
            out[i] = NaN;
            continue;
        }
        if (!initialised) {
            prev        = x[i];
            initialised = true;
        } else {
            prev = alpha * x[i] + oneMinusAlpha * prev;
        }
        out[i] = prev;
    }

    return out;
}

// ---------------------------------------------------------------------------

std::vector<double> weightedMovingAverage(const std::vector<double>& x,
                                          const std::vector<double>& weights)
{
    checkNoNaN(x, "weightedMovingAverage");

    const int wSize = static_cast<int>(weights.size());

    if (wSize < 3) {
        throw ConfigException(
            "weightedMovingAverage: weights.size() must be >= 3, got " +
            std::to_string(wSize));
    }
    if (wSize % 2 == 0) {
        throw ConfigException(
            "weightedMovingAverage: weights.size() must be odd, got " +
            std::to_string(wSize));
    }

    const double weightSum = std::accumulate(weights.begin(), weights.end(), 0.0);
    if (std::abs(weightSum) < std::numeric_limits<double>::epsilon() * static_cast<double>(wSize)) {
        throw DataException("weightedMovingAverage: weights sum to zero.");
    }

    const std::size_t   n    = x.size();
    const int           half = wSize / 2;
    std::vector<double> out(n, NaN);

    if (static_cast<int>(n) < wSize) {
        LOKI_WARNING("weightedMovingAverage: series length " + std::to_string(n) +
                     " is smaller than kernel size " + std::to_string(wSize) +
                     "; returning all-NaN output.");
        return out;
    }

    for (std::size_t i = static_cast<std::size_t>(half);
         i + static_cast<std::size_t>(half) < n;
         ++i)
    {
        double sum = 0.0;
        for (int k = -half; k <= half; ++k) {
            sum += weights[static_cast<std::size_t>(k + half)] *
                   x[i + static_cast<std::size_t>(k)];
        }
        out[i] = sum / weightSum;
    }

    return out;
}

} // namespace loki::stats
