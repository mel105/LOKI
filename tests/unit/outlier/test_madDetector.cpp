#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <loki/outlier/madDetector.hpp>
#include <loki/outlier/outlierResult.hpp>
#include <loki/core/exceptions.hpp>

#include <cmath>
#include <limits>
#include <vector>

using namespace loki::outlier;
using Catch::Matchers::WithinAbs;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::vector<double> normalSeries()
{
    return { -0.5, 0.3, -0.2, 0.8, -0.6, 0.1, 0.4, -0.3, 0.7, -0.1,
              0.2, -0.4,  0.6, 0.0, -0.7,  0.5, 0.3, -0.5,  0.1,  0.4 };
}

static std::vector<double> seriesWithOutliers()
{
    auto v = normalSeries();
    v[2]  = 18.0;
    v[17] = -15.0;
    return v;
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TEST_CASE("MadDetector: default multiplier is 3.0", "[MadDetector]")
{
    MadDetector d;
    REQUIRE_THAT(d.multiplier(), WithinAbs(3.0, 1e-12));
}

TEST_CASE("MadDetector: custom multiplier stored correctly", "[MadDetector]")
{
    MadDetector d(2.5);
    REQUIRE_THAT(d.multiplier(), WithinAbs(2.5, 1e-12));
}

TEST_CASE("MadDetector: non-positive multiplier throws AlgorithmException", "[MadDetector]")
{
    REQUIRE_THROWS_AS(MadDetector(0.0),  loki::AlgorithmException);
    REQUIRE_THROWS_AS(MadDetector(-2.0), loki::AlgorithmException);
}

// ---------------------------------------------------------------------------
// Basic detection
// ---------------------------------------------------------------------------

TEST_CASE("MadDetector: no outliers in clean series", "[MadDetector]")
{
    MadDetector d;
    auto result = d.detect(normalSeries());

    REQUIRE(result.nOutliers == 0);
    REQUIRE(result.points.empty());
    REQUIRE(result.method == "MAD");
    REQUIRE(result.n == 20);
}

TEST_CASE("MadDetector: detects known outliers", "[MadDetector]")
{
    MadDetector d(3.0);
    auto result = d.detect(seriesWithOutliers());

    REQUIRE(result.nOutliers == 2);

    bool found2  = false;
    bool found17 = false;
    for (const auto& pt : result.points) {
        if (pt.index == 2)  { found2  = true; REQUIRE(pt.originalValue ==  18.0); }
        if (pt.index == 17) { found17 = true; REQUIRE(pt.originalValue == -15.0); }
        REQUIRE(pt.flag == 1);
        REQUIRE(std::isnan(pt.replacedValue));
    }
    REQUIRE(found2);
    REQUIRE(found17);
}

TEST_CASE("MadDetector: result fields populated correctly", "[MadDetector]")
{
    MadDetector d;
    auto result = d.detect(seriesWithOutliers());

    REQUIRE(result.method   == "MAD");
    REQUIRE(result.n        == 20);
    REQUIRE(result.scale    > 0.0);
    REQUIRE_THAT(result.threshold, WithinAbs(3.0, 1e-12));
}

TEST_CASE("MadDetector: score sign reflects direction of deviation", "[MadDetector]")
{
    MadDetector d(3.0);
    auto result = d.detect(seriesWithOutliers());

    for (const auto& pt : result.points) {
        if (pt.originalValue > 0.0) REQUIRE(pt.score > 0.0);
        if (pt.originalValue < 0.0) REQUIRE(pt.score < 0.0);
    }
}

// ---------------------------------------------------------------------------
// MAD-specific: robustness to clustered outliers
// ---------------------------------------------------------------------------

TEST_CASE("MadDetector: robust when multiple outliers present", "[MadDetector]")
{
    // Series with 4 large outliers -- MAD location/scale stay stable
    std::vector<double> v = normalSeries();
    v[0] = 50.0;
    v[1] = 52.0;
    v[2] = 49.0;
    v[3] = 51.0;

    MadDetector d(3.0);
    auto result = d.detect(v);

    // All 4 injected values should be flagged
    REQUIRE(result.nOutliers >= 4);
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

TEST_CASE("MadDetector: all-identical series returns empty result", "[MadDetector]")
{
    std::vector<double> flat(10, 3.0);
    MadDetector d;
    auto result = d.detect(flat);

    REQUIRE(result.nOutliers == 0);
    REQUIRE(result.scale == 0.0);
}

TEST_CASE("MadDetector: series too short throws SeriesTooShortException", "[MadDetector]")
{
    MadDetector d;
    REQUIRE_THROWS_AS(d.detect({1.0, 2.0, 3.0}), loki::SeriesTooShortException);
}

TEST_CASE("MadDetector: NaN in series throws MissingValueException", "[MadDetector]")
{
    auto v = normalSeries();
    v[9] = std::numeric_limits<double>::quiet_NaN();
    MadDetector d;
    REQUIRE_THROWS_AS(d.detect(v), loki::MissingValueException);
}

TEST_CASE("MadDetector: stricter multiplier flags more points", "[MadDetector]")
{
    MadDetector loose(5.0);
    MadDetector strict(1.0);
    auto v = seriesWithOutliers();

    REQUIRE(strict.detect(v).nOutliers >= loose.detect(v).nOutliers);
}
