#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <loki/stats/filter.hpp>
#include <loki/core/exceptions.hpp>

#include <cmath>
#include <limits>
#include <vector>

using Catch::Approx;
using loki::ConfigException;
using loki::DataException;

static constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool isNaN(double v) { return std::isnan(v); }

// ---------------------------------------------------------------------------
// movingAverage
// ---------------------------------------------------------------------------

TEST_CASE("movingAverage: basic correctness on constant series", "[filter][ma]")
{
    // MA of a constant series must return the same constant everywhere (except edges).
    const std::vector<double> x(10, 3.0);
    const auto out = loki::stats::movingAverage(x, 3);
    REQUIRE(out.size() == 10);
    CHECK(isNaN(out[0]));
    CHECK(isNaN(out[9]));
    for (std::size_t i = 1; i <= 8; ++i) {
        CHECK(out[i] == Approx(3.0));
    }
}

TEST_CASE("movingAverage: known values with window 3", "[filter][ma]")
{
    const std::vector<double> x{1.0, 2.0, 3.0, 4.0, 5.0};
    const auto out = loki::stats::movingAverage(x, 3);
    REQUIRE(out.size() == 5);
    CHECK(isNaN(out[0]));
    CHECK(out[1] == Approx(2.0));
    CHECK(out[2] == Approx(3.0));
    CHECK(out[3] == Approx(4.0));
    CHECK(isNaN(out[4]));
}

TEST_CASE("movingAverage: window 5 produces correct means", "[filter][ma]")
{
    const std::vector<double> x{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0};
    const auto out = loki::stats::movingAverage(x, 5);
    REQUIRE(out.size() == 7);
    CHECK(isNaN(out[0]));
    CHECK(isNaN(out[1]));
    CHECK(out[2] == Approx(3.0));
    CHECK(out[3] == Approx(4.0));
    CHECK(out[4] == Approx(5.0));
    CHECK(isNaN(out[5]));
    CHECK(isNaN(out[6]));
}

TEST_CASE("movingAverage: even windowSize is rounded up to next odd", "[filter][ma]")
{
    // Window 4 -> rounded to 5. Same result as window 5.
    const std::vector<double> x{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0};
    const auto out4 = loki::stats::movingAverage(x, 4);
    const auto out5 = loki::stats::movingAverage(x, 5);
    REQUIRE(out4.size() == out5.size());
    for (std::size_t i = 0; i < out4.size(); ++i) {
        if (!isNaN(out5[i])) {
            CHECK(out4[i] == Approx(out5[i]));
        } else {
            CHECK(isNaN(out4[i]));
        }
    }
}

TEST_CASE("movingAverage: windowSize < 3 throws ConfigException", "[filter][ma]")
{
    const std::vector<double> x{1.0, 2.0, 3.0};
    // windowSize=1 is odd and < 3 -> throws
    CHECK_THROWS_AS(loki::stats::movingAverage(x, 1), ConfigException);
    // windowSize=2 is even -> rounded up to 3 (valid) -> no throw
    CHECK_NOTHROW(loki::stats::movingAverage(x, 2));
    // windowSize=3 is valid -> no throw
    CHECK_NOTHROW(loki::stats::movingAverage(x, 3));
}

TEST_CASE("movingAverage: NaN input throws DataException", "[filter][ma]")
{
    const std::vector<double> x{1.0, NaN, 3.0};
    CHECK_THROWS_AS(loki::stats::movingAverage(x, 3), DataException);
}

TEST_CASE("movingAverage: series shorter than window returns all-NaN", "[filter][ma]")
{
    const std::vector<double> x{1.0, 2.0};
    const auto out = loki::stats::movingAverage(x, 5);
    REQUIRE(out.size() == 2);
    CHECK(isNaN(out[0]));
    CHECK(isNaN(out[1]));
}

TEST_CASE("movingAverage: empty input returns empty output", "[filter][ma]")
{
    const std::vector<double> x;
    const auto out = loki::stats::movingAverage(x, 3);
    CHECK(out.empty());
}

// ---------------------------------------------------------------------------
// exponentialMovingAverage
// ---------------------------------------------------------------------------

TEST_CASE("exponentialMovingAverage: alpha=1 returns copy of input", "[filter][ema]")
{
    const std::vector<double> x{1.0, 2.0, 3.0, 4.0};
    const auto out = loki::stats::exponentialMovingAverage(x, 1.0);
    REQUIRE(out.size() == 4);
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(out[i] == Approx(x[i]));
    }
}

TEST_CASE("exponentialMovingAverage: known recurrence alpha=0.5", "[filter][ema]")
{
    // y[0] = 1.0 (init)
    // y[1] = 0.5*2 + 0.5*1 = 1.5
    // y[2] = 0.5*3 + 0.5*1.5 = 2.25
    // y[3] = 0.5*4 + 0.5*2.25 = 3.125
    const std::vector<double> x{1.0, 2.0, 3.0, 4.0};
    const auto out = loki::stats::exponentialMovingAverage(x, 0.5);
    REQUIRE(out.size() == 4);
    CHECK(out[0] == Approx(1.0));
    CHECK(out[1] == Approx(1.5));
    CHECK(out[2] == Approx(2.25));
    CHECK(out[3] == Approx(3.125));
}

TEST_CASE("exponentialMovingAverage: constant series stays constant", "[filter][ema]")
{
    const std::vector<double> x(8, 5.0);
    const auto out = loki::stats::exponentialMovingAverage(x, 0.3);
    for (double v : out) {
        CHECK(v == Approx(5.0));
    }
}

TEST_CASE("exponentialMovingAverage: alpha <= 0 throws ConfigException", "[filter][ema]")
{
    const std::vector<double> x{1.0, 2.0};
    CHECK_THROWS_AS(loki::stats::exponentialMovingAverage(x, 0.0),  ConfigException);
    CHECK_THROWS_AS(loki::stats::exponentialMovingAverage(x, -0.1), ConfigException);
}

TEST_CASE("exponentialMovingAverage: alpha > 1 throws ConfigException", "[filter][ema]")
{
    const std::vector<double> x{1.0, 2.0};
    CHECK_THROWS_AS(loki::stats::exponentialMovingAverage(x, 1.1), ConfigException);
}

TEST_CASE("exponentialMovingAverage: NaN input throws DataException", "[filter][ema]")
{
    const std::vector<double> x{1.0, NaN, 3.0};
    CHECK_THROWS_AS(loki::stats::exponentialMovingAverage(x, 0.5), DataException);
}

TEST_CASE("exponentialMovingAverage: empty input returns empty output", "[filter][ema]")
{
    const std::vector<double> x;
    const auto out = loki::stats::exponentialMovingAverage(x, 0.5);
    CHECK(out.empty());
}

// ---------------------------------------------------------------------------
// weightedMovingAverage
// ---------------------------------------------------------------------------

TEST_CASE("weightedMovingAverage: uniform weights equal simple MA", "[filter][wma]")
{
    const std::vector<double> x{1.0, 2.0, 3.0, 4.0, 5.0};
    const std::vector<double> w{1.0, 1.0, 1.0};
    const auto wma = loki::stats::weightedMovingAverage(x, w);
    const auto sma = loki::stats::movingAverage(x, 3);
    REQUIRE(wma.size() == sma.size());
    for (std::size_t i = 0; i < wma.size(); ++i) {
        if (!isNaN(sma[i])) {
            CHECK(wma[i] == Approx(sma[i]));
        } else {
            CHECK(isNaN(wma[i]));
        }
    }
}

TEST_CASE("weightedMovingAverage: triangular kernel {1,2,1}", "[filter][wma]")
{
    // Weights sum = 4. Centre element weighted double.
    // x = {1,2,3,4,5}
    // i=1: (1*1 + 2*2 + 1*3)/4 = 8/4 = 2.0
    // i=2: (1*2 + 2*3 + 1*4)/4 = 12/4 = 3.0
    // i=3: (1*3 + 2*4 + 1*5)/4 = 16/4 = 4.0
    const std::vector<double> x{1.0, 2.0, 3.0, 4.0, 5.0};
    const std::vector<double> w{1.0, 2.0, 1.0};
    const auto out = loki::stats::weightedMovingAverage(x, w);
    REQUIRE(out.size() == 5);
    CHECK(isNaN(out[0]));
    CHECK(out[1] == Approx(2.0));
    CHECK(out[2] == Approx(3.0));
    CHECK(out[3] == Approx(4.0));
    CHECK(isNaN(out[4]));
}

TEST_CASE("weightedMovingAverage: un-normalised weights give same result as normalised", "[filter][wma]")
{
    const std::vector<double> x{2.0, 4.0, 6.0, 4.0, 2.0};
    const std::vector<double> w1{1.0, 2.0, 1.0};
    const std::vector<double> w2{2.0, 4.0, 2.0}; // scale by 2
    const auto out1 = loki::stats::weightedMovingAverage(x, w1);
    const auto out2 = loki::stats::weightedMovingAverage(x, w2);
    for (std::size_t i = 0; i < out1.size(); ++i) {
        if (!isNaN(out1[i])) {
            CHECK(out1[i] == Approx(out2[i]));
        }
    }
}

TEST_CASE("weightedMovingAverage: even kernel size throws ConfigException", "[filter][wma]")
{
    const std::vector<double> x{1.0, 2.0, 3.0, 4.0};
    const std::vector<double> w{1.0, 2.0, 2.0, 1.0}; // size 4 (even)
    CHECK_THROWS_AS(loki::stats::weightedMovingAverage(x, w), ConfigException);
}

TEST_CASE("weightedMovingAverage: kernel size < 3 throws ConfigException", "[filter][wma]")
{
    const std::vector<double> x{1.0, 2.0, 3.0};
    const std::vector<double> w{1.0}; // size 1
    CHECK_THROWS_AS(loki::stats::weightedMovingAverage(x, w), ConfigException);
}

TEST_CASE("weightedMovingAverage: zero-sum weights throws DataException", "[filter][wma]")
{
    const std::vector<double> x{1.0, 2.0, 3.0};
    const std::vector<double> w{0.0, 0.0, 0.0};
    CHECK_THROWS_AS(loki::stats::weightedMovingAverage(x, w), DataException);
}

TEST_CASE("weightedMovingAverage: NaN input throws DataException", "[filter][wma]")
{
    const std::vector<double> x{1.0, NaN, 3.0};
    const std::vector<double> w{1.0, 2.0, 1.0};
    CHECK_THROWS_AS(loki::stats::weightedMovingAverage(x, w), DataException);
}

TEST_CASE("weightedMovingAverage: series shorter than kernel returns all-NaN", "[filter][wma]")
{
    const std::vector<double> x{1.0, 2.0};
    const std::vector<double> w{1.0, 2.0, 1.0};
    const auto out = loki::stats::weightedMovingAverage(x, w);
    REQUIRE(out.size() == 2);
    CHECK(isNaN(out[0]));
    CHECK(isNaN(out[1]));
}
