#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <loki/homogeneity/changePointDetector.hpp>
#include <loki/homogeneity/changePointResult.hpp>
#include <loki/core/exceptions.hpp>

#include <cmath>
#include <numeric>
#include <vector>

using namespace loki::homogeneity;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Generates a series with a known step shift at position 'splitAt'.
/// Values before split ~ 0, values after split ~ shift.
static std::vector<double> makeStepSeries(std::size_t n,
                                          std::size_t splitAt,
                                          double shift,
                                          double noise = 0.0)
{
    std::vector<double> z(n, 0.0);
    for (std::size_t i = splitAt; i < n; ++i) {
        z[i] = shift;
    }
    // Add deterministic "noise" if requested (sawtooth pattern).
    if (noise != 0.0) {
        for (std::size_t i = 0; i < n; ++i) {
            z[i] += noise * (static_cast<double>(i % 7) - 3.0);
        }
    }
    return z;
}

/// Generates a stationary series (all zeros + tiny sawtooth).
static std::vector<double> makeStationarySeries(std::size_t n)
{
    std::vector<double> z(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        z[i] = 0.01 * (static_cast<double>(i % 5) - 2.0);
    }
    return z;
}

// ---------------------------------------------------------------------------
// ChangePointResult default state
// ---------------------------------------------------------------------------

TEST_CASE("ChangePointResult default construction", "[changepoint][result]")
{
    ChangePointResult r;
    CHECK_FALSE(r.detected);
    CHECK(r.index == -1);
    CHECK_THAT(r.shift,         WithinAbs(0.0, 1e-12));
    CHECK_THAT(r.sigmaStar,     WithinAbs(1.0, 1e-12));
    CHECK(r.confIntervalLow  == -1);
    CHECK(r.confIntervalHigh == -1);
}

// ---------------------------------------------------------------------------
// ChangePointDetector -- construction
// ---------------------------------------------------------------------------

TEST_CASE("ChangePointDetector default config", "[changepoint][detector]")
{
    ChangePointDetector det;
    // Just verify construction does not throw.
    CHECK_NOTHROW(ChangePointDetector{});
}

TEST_CASE("ChangePointDetector custom config", "[changepoint][detector]")
{
    ChangePointDetector::Config cfg;
    cfg.significanceLevel = 0.01;
    cfg.acfDependenceLimit = 0.3;
    CHECK_NOTHROW(ChangePointDetector{cfg});
}

// ---------------------------------------------------------------------------
// Input validation
// ---------------------------------------------------------------------------

TEST_CASE("detect throws on empty segment (begin == end)", "[changepoint][detector][throws]")
{
    ChangePointDetector det;
    std::vector<double> z(100, 0.0);
    CHECK_THROWS_AS(det.detect(z, 10, 10), loki::DataException);
}

TEST_CASE("detect throws when segment is too short", "[changepoint][detector][throws]")
{
    ChangePointDetector det;
    std::vector<double> z(100, 0.0);
    // MIN_SEGMENT == 5, so length 4 must throw.
    CHECK_THROWS_AS(det.detect(z, 0, 4), loki::DataException);
}

TEST_CASE("detect throws when end exceeds series length", "[changepoint][detector][throws]")
{
    ChangePointDetector det;
    std::vector<double> z(50, 0.0);
    CHECK_THROWS_AS(det.detect(z, 0, 60), loki::DataException);
}

// ---------------------------------------------------------------------------
// Detection -- clear shift
// ---------------------------------------------------------------------------

TEST_CASE("detect finds obvious shift at midpoint", "[changepoint][detector]")
{
    // Series: 100 points, 0 for i < 50, shift=5 for i >= 50. No noise.
    const std::size_t n     = 100;
    const std::size_t split = 50;
    const double      shift = 5.0;

    const auto z = makeStepSeries(n, split, shift);

    ChangePointDetector det;  // alpha=0.05
    const auto result = det.detect(z, 0, n);

    CHECK(result.detected);
    // Allow +/-5 positions around true split.
    CHECK(result.index >= static_cast<int>(split) - 5);
    CHECK(result.index <= static_cast<int>(split) + 5);
    CHECK(result.shift > 0.0);
    CHECK(result.maxTk > result.criticalValue);
}

TEST_CASE("detect works on sub-segment [begin, end)", "[changepoint][detector]")
{
    // Embed a shift in the middle of a longer series; only test the middle slice.
    const std::size_t n = 200;
    std::vector<double> z(n, 0.0);
    // Shift from index 80 to 120 within [50, 150).
    for (std::size_t i = 80; i < 150; ++i) { z[i] = 4.0; }

    ChangePointDetector det;
    const auto result = det.detect(z, 50, 150);

    CHECK(result.detected);
    // result.index is relative to begin=50, so global index = 50 + result.index.
    const int globalIdx = 50 + result.index;
    CHECK(globalIdx >= 75);
    CHECK(globalIdx <= 85);
}

TEST_CASE("detect result fields are consistent when shift found", "[changepoint][detector]")
{
    const auto z = makeStepSeries(120, 60, 3.0);
    ChangePointDetector det;
    const auto r = det.detect(z, 0, 120);

    REQUIRE(r.detected);
    CHECK(r.index >= 0);
    CHECK(r.maxTk > 0.0);
    CHECK(r.criticalValue > 0.0);
    CHECK(r.sigmaStar > 0.0);
    CHECK(r.pValue >= 0.0);
    CHECK(r.pValue <= 1.0);
    CHECK_THAT(r.shift, WithinAbs(r.meanAfter - r.meanBefore, 1e-10));
}

// ---------------------------------------------------------------------------
// Detection -- stationary series
// ---------------------------------------------------------------------------

TEST_CASE("detect returns not-detected on stationary series", "[changepoint][detector]")
{
    const auto z = makeStationarySeries(200);
    ChangePointDetector det;
    const auto result = det.detect(z, 0, 200);

    CHECK_FALSE(result.detected);
    CHECK(result.index == -1);
    CHECK(result.maxTk <= result.criticalValue);
}

// ---------------------------------------------------------------------------
// Critical value -- monotonicity
// ---------------------------------------------------------------------------

TEST_CASE("critical value decreases as alpha increases", "[changepoint][detector]")
{
    // Larger alpha -> lower bar -> smaller critical value.
    const auto z = makeStepSeries(100, 50, 5.0);
    ChangePointDetector detStrict({0.01});
    ChangePointDetector detLoose ({0.10});

    const auto rStrict = detStrict.detect(z, 0, 100);
    const auto rLoose  = detLoose .detect(z, 0, 100);

    CHECK(rStrict.criticalValue > rLoose.criticalValue);
}

// ---------------------------------------------------------------------------
// sigmaStar
// ---------------------------------------------------------------------------

TEST_CASE("sigmaStar is positive", "[changepoint][detector]")
{
    const auto z = makeStepSeries(100, 50, 5.0);
    ChangePointDetector det;
    const auto r = det.detect(z, 0, 100);
    CHECK(r.sigmaStar > 0.0);
}

// ---------------------------------------------------------------------------
// Confidence interval
// ---------------------------------------------------------------------------

TEST_CASE("confidence interval straddles the detected change point", "[changepoint][detector]")
{
    const std::size_t n     = 120;
    const std::size_t split = 60;
    const auto z = makeStepSeries(n, split, 6.0);

    // Use alpha=0.05 (conf level 0.95 is in the hardcoded table).
    ChangePointDetector det({0.05});
    const auto r = det.detect(z, 0, n);

    REQUIRE(r.detected);
    REQUIRE(r.confIntervalLow  != -1);
    REQUIRE(r.confIntervalHigh != -1);

    CHECK(r.confIntervalLow  <= r.index);
    CHECK(r.confIntervalHigh >= r.index);
}

TEST_CASE("confidence interval not available for untabled alpha", "[changepoint][detector]")
{
    const auto z = makeStepSeries(120, 60, 6.0);
    // alpha=0.07 is not in the table -> CI should be -1.
    ChangePointDetector det({0.07});
    const auto r = det.detect(z, 0, 120);

    if (r.detected) {
        CHECK(r.confIntervalLow  == -1);
        CHECK(r.confIntervalHigh == -1);
    }
}

// ---------------------------------------------------------------------------
// p-value
// ---------------------------------------------------------------------------

TEST_CASE("p-value is small for strong shift", "[changepoint][detector]")
{
    const auto z = makeStepSeries(200, 100, 10.0);
    ChangePointDetector det;
    const auto r = det.detect(z, 0, 200);

    REQUIRE(r.detected);
    CHECK(r.pValue < 0.05);
}

TEST_CASE("p-value is in [0, 1]", "[changepoint][detector]")
{
    const auto z = makeStepSeries(100, 50, 3.0);
    ChangePointDetector det;
    const auto r = det.detect(z, 0, 100);
    CHECK(r.pValue >= 0.0);
    CHECK(r.pValue <= 1.0);
}
