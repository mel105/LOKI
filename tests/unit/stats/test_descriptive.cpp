#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "loki/stats/descriptive.hpp"
#include "loki/core/exceptions.hpp"

#include <cmath>
#include <limits>
#include <vector>

// =============================================================================
// test_descriptive.cpp
//
// Tests for free functions in namespace loki::stats.
// Ground-truth values computed with Python/NumPy for the same input.
// Floating-point comparisons use WithinAbs or WithinRel tolerances.
// =============================================================================

using namespace Catch::Matchers;
using loki::NanPolicy;

// ---------------------------------------------------------------------------
// Shared test data
// ---------------------------------------------------------------------------

// Simple 5-element series: {1, 2, 3, 4, 5}
static const std::vector<double> V5 = {1.0, 2.0, 3.0, 4.0, 5.0};

// Symmetric series centred on 0: {-2, -1, 0, 1, 2}
static const std::vector<double> VSYM = {-2.0, -1.0, 0.0, 1.0, 2.0};

// Series with one NaN
static const std::vector<double> VNAN = {1.0, 2.0,
    std::numeric_limits<double>::quiet_NaN(), 4.0, 5.0};

// -----------------------------------------------------------------------------
// mean
// -----------------------------------------------------------------------------

TEST_CASE("mean of {1,2,3,4,5} is 3.0", "[stats][mean]")
{
    REQUIRE_THAT(loki::stats::mean(V5), WithinAbs(3.0, 1e-10));
}

TEST_CASE("mean throws DataException on empty vector", "[stats][mean]")
{
    REQUIRE_THROWS_AS(
        loki::stats::mean({}),
        loki::DataException
    );
}

TEST_CASE("mean with NaN and NanPolicy::THROW throws DataException", "[stats][mean][nan]")
{
    REQUIRE_THROWS_AS(
        loki::stats::mean(VNAN, NanPolicy::THROW),
        loki::DataException
    );
}

TEST_CASE("mean with NaN and NanPolicy::SKIP skips NaN", "[stats][mean][nan]")
{
    // Valid values: 1, 2, 4, 5 -> mean = 3.0
    REQUIRE_THAT(
        loki::stats::mean(VNAN, NanPolicy::SKIP),
        WithinAbs(3.0, 1e-10)
    );
}

// -----------------------------------------------------------------------------
// median
// -----------------------------------------------------------------------------

TEST_CASE("median of {1,2,3,4,5} is 3.0", "[stats][median]")
{
    REQUIRE_THAT(loki::stats::median(V5), WithinAbs(3.0, 1e-10));
}

TEST_CASE("median of even-length series is average of two middle values", "[stats][median]")
{
    std::vector<double> v = {1.0, 2.0, 3.0, 4.0};
    REQUIRE_THAT(loki::stats::median(v), WithinAbs(2.5, 1e-10));
}

TEST_CASE("median does not require pre-sorted input", "[stats][median]")
{
    std::vector<double> unsorted = {5.0, 1.0, 3.0, 2.0, 4.0};
    REQUIRE_THAT(loki::stats::median(unsorted), WithinAbs(3.0, 1e-10));
}

// -----------------------------------------------------------------------------
// variance and stddev
// -----------------------------------------------------------------------------

TEST_CASE("sample variance of {1,2,3,4,5} is 2.5", "[stats][variance]")
{
    // sum of squared deviations = 10, n-1 = 4 => 2.5
    REQUIRE_THAT(loki::stats::variance(V5), WithinAbs(2.5, 1e-10));
}

TEST_CASE("population variance of {1,2,3,4,5} is 2.0", "[stats][variance]")
{
    // sum of squared deviations = 10, n = 5 => 2.0
    REQUIRE_THAT(loki::stats::variance(V5, /*population=*/true), WithinAbs(2.0, 1e-10));
}

TEST_CASE("stddev is square root of variance", "[stats][stddev]")
{
    const double var = loki::stats::variance(V5);
    const double sd  = loki::stats::stddev(V5);
    REQUIRE_THAT(sd, WithinAbs(std::sqrt(var), 1e-10));
}

TEST_CASE("variance throws DataException for single-element series", "[stats][variance]")
{
    REQUIRE_THROWS_AS(
        loki::stats::variance({42.0}),
        loki::DataException
    );
}

// -----------------------------------------------------------------------------
// range and IQR
// -----------------------------------------------------------------------------

TEST_CASE("range of {1,2,3,4,5} is 4.0", "[stats][range]")
{
    REQUIRE_THAT(loki::stats::range(V5), WithinAbs(4.0, 1e-10));
}

TEST_CASE("iqr of {1,2,3,4,5} matches NumPy result", "[stats][iqr]")
{
    // NumPy np.percentile([1,2,3,4,5], [25,75]) = [1.5, 4.5] => IQR = 3.0
    // (type-7 linear interpolation)
    REQUIRE_THAT(loki::stats::iqr(V5), WithinAbs(3.0, 1e-6));
}

// -----------------------------------------------------------------------------
// skewness and kurtosis
// -----------------------------------------------------------------------------

TEST_CASE("skewness of symmetric series is 0", "[stats][skewness]")
{
    // {-2,-1,0,1,2} is perfectly symmetric
    REQUIRE_THAT(loki::stats::skewness(VSYM), WithinAbs(0.0, 1e-10));
}

TEST_CASE("kurtosis of {1,2,3,4,5} is negative (platykurtic)", "[stats][kurtosis]")
{
    // Uniform-ish distribution has thinner tails than normal -> negative excess kurtosis
    const double k = loki::stats::kurtosis(V5);
    REQUIRE(k < 0.0);
}

TEST_CASE("skewness throws DataException for fewer than 3 elements", "[stats][skewness]")
{
    REQUIRE_THROWS_AS(
        loki::stats::skewness({1.0, 2.0}),
        loki::DataException
    );
}

// -----------------------------------------------------------------------------
// quantile
// -----------------------------------------------------------------------------

TEST_CASE("quantile(0.0) returns minimum", "[stats][quantile]")
{
    REQUIRE_THAT(loki::stats::quantile(V5, 0.0), WithinAbs(1.0, 1e-10));
}

TEST_CASE("quantile(1.0) returns maximum", "[stats][quantile]")
{
    REQUIRE_THAT(loki::stats::quantile(V5, 1.0), WithinAbs(5.0, 1e-10));
}

TEST_CASE("quantile(0.5) equals median", "[stats][quantile]")
{
    REQUIRE_THAT(
        loki::stats::quantile(V5, 0.5),
        WithinAbs(loki::stats::median(V5), 1e-10)
    );
}

TEST_CASE("quantile throws DataException for p outside [0,1]", "[stats][quantile]")
{
    REQUIRE_THROWS_AS(loki::stats::quantile(V5, -0.1), loki::DataException);
    REQUIRE_THROWS_AS(loki::stats::quantile(V5,  1.1), loki::DataException);
}

// -----------------------------------------------------------------------------
// Pearson correlation
// -----------------------------------------------------------------------------

TEST_CASE("pearsonR of perfectly correlated series is 1.0", "[stats][pearsonR]")
{
    std::vector<double> x = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> y = {2.0, 4.0, 6.0, 8.0, 10.0}; // y = 2x

    REQUIRE_THAT(loki::stats::pearsonR(x, y), WithinAbs(1.0, 1e-10));
}

TEST_CASE("pearsonR of perfectly anti-correlated series is -1.0", "[stats][pearsonR]")
{
    std::vector<double> x = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> y = {5.0, 4.0, 3.0, 2.0, 1.0}; // y = 6 - x

    REQUIRE_THAT(loki::stats::pearsonR(x, y), WithinAbs(-1.0, 1e-10));
}

TEST_CASE("pearsonR throws DataException when sizes differ", "[stats][pearsonR]")
{
    REQUIRE_THROWS_AS(
        loki::stats::pearsonR({1.0, 2.0}, {1.0, 2.0, 3.0}),
        loki::DataException
    );
}

// -----------------------------------------------------------------------------
// autocorrelation
// -----------------------------------------------------------------------------

TEST_CASE("autocorrelation at lag 0 is always 1.0", "[stats][acf]")
{
    REQUIRE_THAT(loki::stats::autocorrelation(V5, 0), WithinAbs(1.0, 1e-10));
}

TEST_CASE("acf returns vector of length maxLag+1", "[stats][acf]")
{
    auto result = loki::stats::acf(V5, 3);
    REQUIRE(result.size() == 4);       // lags 0,1,2,3
    REQUIRE_THAT(result[0], WithinAbs(1.0, 1e-10)); // lag 0 == 1
}

// -----------------------------------------------------------------------------
// hurstExponent
// -----------------------------------------------------------------------------

TEST_CASE("hurstExponent throws DataException for series shorter than 20", "[stats][hurst]")
{
    std::vector<double> short_series(15, 1.0);
    REQUIRE_THROWS_AS(
        loki::stats::hurstExponent(short_series),
        loki::DataException
    );
}

TEST_CASE("hurstExponent returns value in (0, 1) for typical series", "[stats][hurst]")
{
    // 50 linearly increasing values -- should be persistent (H > 0.5)
    std::vector<double> trend(50);
    for (int i = 0; i < 50; ++i) trend[i] = static_cast<double>(i);

    const double h = loki::stats::hurstExponent(trend, NanPolicy::SKIP);
    REQUIRE(h > 0.0);
    REQUIRE(h < 1.0);
}
