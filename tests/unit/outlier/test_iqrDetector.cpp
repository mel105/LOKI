#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <loki/outlier/iqrDetector.hpp>
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
    // 20 values centred near 0, no outliers
    return { -0.5, 0.3, -0.2, 0.8, -0.6, 0.1, 0.4, -0.3, 0.7, -0.1,
              0.2, -0.4,  0.6, 0.0, -0.7,  0.5, 0.3, -0.5,  0.1,  0.4 };
}

static std::vector<double> seriesWithOutliers()
{
    // Same base + two clear outliers
    auto v = normalSeries();
    v[3]  = 15.0;   // large positive outlier
    v[14] = -12.0;  // large negative outlier
    return v;
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TEST_CASE("IqrDetector: default multiplier is 1.5", "[IqrDetector]")
{
    IqrDetector d;
    REQUIRE_THAT(d.multiplier(), WithinAbs(1.5, 1e-12));
}

TEST_CASE("IqrDetector: custom multiplier stored correctly", "[IqrDetector]")
{
    IqrDetector d(3.0);
    REQUIRE_THAT(d.multiplier(), WithinAbs(3.0, 1e-12));
}

TEST_CASE("IqrDetector: non-positive multiplier throws AlgorithmException", "[IqrDetector]")
{
    REQUIRE_THROWS_AS(IqrDetector(0.0),  loki::AlgorithmException);
    REQUIRE_THROWS_AS(IqrDetector(-1.0), loki::AlgorithmException);
}

// ---------------------------------------------------------------------------
// Basic detection
// ---------------------------------------------------------------------------

TEST_CASE("IqrDetector: no outliers in clean series", "[IqrDetector]")
{
    IqrDetector d;
    auto result = d.detect(normalSeries());

    REQUIRE(result.nOutliers == 0);
    REQUIRE(result.points.empty());
    REQUIRE(result.method == "IQR");
    REQUIRE(result.n == 20);
}

TEST_CASE("IqrDetector: detects known outliers", "[IqrDetector]")
{
    IqrDetector d(1.5);
    auto result = d.detect(seriesWithOutliers());

    REQUIRE(result.nOutliers == 2);

    // Find by index
    bool found3  = false;
    bool found14 = false;
    for (const auto& pt : result.points) {
        if (pt.index == 3)  { found3  = true; REQUIRE(pt.originalValue ==  15.0); }
        if (pt.index == 14) { found14 = true; REQUIRE(pt.originalValue == -12.0); }
        REQUIRE(pt.flag == 1);
        REQUIRE(std::isnan(pt.replacedValue));
    }
    REQUIRE(found3);
    REQUIRE(found14);
}

TEST_CASE("IqrDetector: result fields populated correctly", "[IqrDetector]")
{
    IqrDetector d;
    auto result = d.detect(seriesWithOutliers());

    REQUIRE(result.method   == "IQR");
    REQUIRE(result.n        == 20);
    REQUIRE(result.scale    > 0.0);
    REQUIRE_THAT(result.threshold, WithinAbs(1.5, 1e-12));
}

TEST_CASE("IqrDetector: score sign reflects direction of deviation", "[IqrDetector]")
{
    IqrDetector d(1.5);
    auto result = d.detect(seriesWithOutliers());

    for (const auto& pt : result.points) {
        if (pt.originalValue > 0.0) REQUIRE(pt.score > 0.0);
        if (pt.originalValue < 0.0) REQUIRE(pt.score < 0.0);
    }
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

TEST_CASE("IqrDetector: all-identical series returns empty result", "[IqrDetector]")
{
    std::vector<double> flat(10, 5.0);
    IqrDetector d;
    auto result = d.detect(flat);

    REQUIRE(result.nOutliers == 0);
    REQUIRE(result.scale == 0.0);
}

TEST_CASE("IqrDetector: series too short throws SeriesTooShortException", "[IqrDetector]")
{
    IqrDetector d;
    REQUIRE_THROWS_AS(d.detect({1.0, 2.0, 3.0}), loki::SeriesTooShortException);
}

TEST_CASE("IqrDetector: NaN in series throws MissingValueException", "[IqrDetector]")
{
    auto v = normalSeries();
    v[5] = std::numeric_limits<double>::quiet_NaN();
    IqrDetector d;
    REQUIRE_THROWS_AS(d.detect(v), loki::MissingValueException);
}

TEST_CASE("IqrDetector: strict multiplier flags more points", "[IqrDetector]")
{
    IqrDetector loose(3.0);
    IqrDetector strict(0.5);
    auto v = seriesWithOutliers();

    auto rLoose  = loose.detect(v);
    auto rStrict = strict.detect(v);

    REQUIRE(rStrict.nOutliers >= rLoose.nOutliers);
}
