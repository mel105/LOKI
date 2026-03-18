#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "loki/stats/descriptive.hpp"
#include "loki/core/exceptions.hpp"

#include <cmath>
#include <limits>
#include <vector>

// =============================================================================
// test_summarize.cpp
//
// Tests for summarize() and the NanPolicy behaviour wired through it.
// summarize() is an integration point: it calls nearly every other stat
// function internally, so these tests also serve as a smoke test for the
// full stats pipeline.
// =============================================================================

using namespace Catch::Matchers;
using loki::NanPolicy;

static const double NAN_V = std::numeric_limits<double>::quiet_NaN();

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::vector<double> iota(int n)
{
    std::vector<double> v(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) v[static_cast<std::size_t>(i)] = static_cast<double>(i + 1);
    return v;
}

// ---------------------------------------------------------------------------
// Basic correctness
// ---------------------------------------------------------------------------

TEST_CASE("summarize returns correct n for clean series", "[summarize]")
{
    auto s = loki::stats::summarize(iota(10), NanPolicy::SKIP, false);
    REQUIRE(s.n == 10);
    REQUIRE(s.nMissing == 0);
}

TEST_CASE("summarize min/max/range for {1..5}", "[summarize]")
{
    auto s = loki::stats::summarize(iota(5), NanPolicy::SKIP, false);

    REQUIRE_THAT(s.min,   WithinAbs(1.0, 1e-10));
    REQUIRE_THAT(s.max,   WithinAbs(5.0, 1e-10));
    REQUIRE_THAT(s.range, WithinAbs(4.0, 1e-10));
}

TEST_CASE("summarize mean and median for {1..5}", "[summarize]")
{
    auto s = loki::stats::summarize(iota(5), NanPolicy::SKIP, false);

    REQUIRE_THAT(s.mean,   WithinAbs(3.0, 1e-10));
    REQUIRE_THAT(s.median, WithinAbs(3.0, 1e-10));
}

TEST_CASE("summarize variance and stddev are consistent", "[summarize]")
{
    auto s = loki::stats::summarize(iota(10), NanPolicy::SKIP, false);

    REQUIRE_THAT(s.stddev, WithinAbs(std::sqrt(s.variance), 1e-10));
}

TEST_CASE("summarize quartiles satisfy Q1 <= median <= Q3", "[summarize]")
{
    auto s = loki::stats::summarize(iota(20), NanPolicy::SKIP, false);

    REQUIRE(s.q1 <= s.median);
    REQUIRE(s.median <= s.q3);
    REQUIRE_THAT(s.iqr, WithinAbs(s.q3 - s.q1, 1e-10));
}

// ---------------------------------------------------------------------------
// NaN handling
// ---------------------------------------------------------------------------

TEST_CASE("summarize counts NaN in nMissing regardless of policy", "[summarize][nan]")
{
    std::vector<double> v = {1.0, NAN_V, 3.0, NAN_V, 5.0};
    auto s = loki::stats::summarize(v, NanPolicy::SKIP, false);

    REQUIRE(s.nMissing == 2);
    REQUIRE(s.n == 3); // only valid values
}

TEST_CASE("summarize SKIP policy ignores NaN in computation", "[summarize][nan]")
{
    std::vector<double> v = {1.0, NAN_V, 3.0, NAN_V, 5.0};
    auto s = loki::stats::summarize(v, NanPolicy::SKIP, false);

    // Mean of {1, 3, 5} = 3.0
    REQUIRE_THAT(s.mean, WithinAbs(3.0, 1e-10));
}

TEST_CASE("summarize THROW policy throws when NaN present", "[summarize][nan]")
{
    std::vector<double> v = {1.0, NAN_V, 3.0};

    REQUIRE_THROWS_AS(
        loki::stats::summarize(v, NanPolicy::THROW, false),
        loki::DataException
    );
}

TEST_CASE("summarize throws DataException on empty input", "[summarize][nan]")
{
    REQUIRE_THROWS_AS(
        loki::stats::summarize({}),
        loki::DataException
    );
}

// ---------------------------------------------------------------------------
// Hurst exponent
// ---------------------------------------------------------------------------

TEST_CASE("hurstExp is NaN when computeHurst is false", "[summarize][hurst]")
{
    auto s = loki::stats::summarize(iota(30), NanPolicy::SKIP, /*computeHurst=*/false);
    REQUIRE(std::isnan(s.hurstExp));
}

TEST_CASE("hurstExp is NaN when series is shorter than 20", "[summarize][hurst]")
{
    // summarize does not throw for short series; it just leaves hurstExp as NaN
    auto s = loki::stats::summarize(iota(10), NanPolicy::SKIP, /*computeHurst=*/true);
    REQUIRE(std::isnan(s.hurstExp));
}

TEST_CASE("hurstExp is finite for series of length >= 20", "[summarize][hurst]")
{
    auto s = loki::stats::summarize(iota(50), NanPolicy::SKIP, /*computeHurst=*/true);
    REQUIRE_FALSE(std::isnan(s.hurstExp));
    REQUIRE(s.hurstExp > 0.0);
    REQUIRE(s.hurstExp < 1.0);
}

// ---------------------------------------------------------------------------
// formatSummary smoke test
// ---------------------------------------------------------------------------

TEST_CASE("formatSummary produces non-empty string", "[summarize][format]")
{
    auto s      = loki::stats::summarize(iota(10), NanPolicy::SKIP, false);
    auto output = loki::stats::formatSummary(s, "test_series");

    REQUIRE_FALSE(output.empty());
    // The label should appear somewhere in the output
    REQUIRE(output.find("test_series") != std::string::npos);
}
