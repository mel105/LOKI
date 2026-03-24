#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <loki/outlier/zScoreDetector.hpp>
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
    v[6]  = 20.0;
    v[13] = -18.0;
    return v;
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TEST_CASE("ZScoreDetector: default threshold is 3.0", "[ZScoreDetector]")
{
    ZScoreDetector d;
    REQUIRE_THAT(d.zThreshold(), WithinAbs(3.0, 1e-12));
}

TEST_CASE("ZScoreDetector: custom threshold stored correctly", "[ZScoreDetector]")
{
    ZScoreDetector d(2.0);
    REQUIRE_THAT(d.zThreshold(), WithinAbs(2.0, 1e-12));
}

TEST_CASE("ZScoreDetector: non-positive threshold throws AlgorithmException", "[ZScoreDetector]")
{
    REQUIRE_THROWS_AS(ZScoreDetector(0.0),  loki::AlgorithmException);
    REQUIRE_THROWS_AS(ZScoreDetector(-3.0), loki::AlgorithmException);
}

// ---------------------------------------------------------------------------
// Basic detection
// ---------------------------------------------------------------------------

TEST_CASE("ZScoreDetector: no outliers in clean series", "[ZScoreDetector]")
{
    ZScoreDetector d;
    auto result = d.detect(normalSeries());

    REQUIRE(result.nOutliers == 0);
    REQUIRE(result.points.empty());
    REQUIRE(result.method == "Z-score");
    REQUIRE(result.n == 20);
}

TEST_CASE("ZScoreDetector: detects known outliers", "[ZScoreDetector]")
{
    ZScoreDetector d(3.0);
    auto result = d.detect(seriesWithOutliers());

    REQUIRE(result.nOutliers == 2);

    bool found6  = false;
    bool found13 = false;
    for (const auto& pt : result.points) {
        if (pt.index == 6)  { found6  = true; REQUIRE(pt.originalValue ==  20.0); }
        if (pt.index == 13) { found13 = true; REQUIRE(pt.originalValue == -18.0); }
        REQUIRE(pt.flag == 1);
        REQUIRE(std::isnan(pt.replacedValue));
    }
    REQUIRE(found6);
    REQUIRE(found13);
}

TEST_CASE("ZScoreDetector: result fields populated correctly", "[ZScoreDetector]")
{
    ZScoreDetector d;
    auto result = d.detect(seriesWithOutliers());

    REQUIRE(result.method == "Z-score");
    REQUIRE(result.n      == 20);
    REQUIRE(result.scale  > 0.0);
    REQUIRE_THAT(result.threshold, WithinAbs(3.0, 1e-12));
}

TEST_CASE("ZScoreDetector: score sign reflects direction of deviation", "[ZScoreDetector]")
{
    ZScoreDetector d(3.0);
    auto result = d.detect(seriesWithOutliers());

    for (const auto& pt : result.points) {
        if (pt.originalValue > 0.0) REQUIRE(pt.score > 0.0);
        if (pt.originalValue < 0.0) REQUIRE(pt.score < 0.0);
    }
}

TEST_CASE("ZScoreDetector: score magnitude is plausible", "[ZScoreDetector]")
{
    // With outliers at +-20 in a series with std dev ~few units,
    // z-score should be well above 3
    ZScoreDetector d(3.0);
    auto result = d.detect(seriesWithOutliers());

    for (const auto& pt : result.points) {
        REQUIRE(std::abs(pt.score) > 3.0);
    }
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

TEST_CASE("ZScoreDetector: all-identical series returns empty result", "[ZScoreDetector]")
{
    std::vector<double> flat(10, 7.0);
    ZScoreDetector d;
    auto result = d.detect(flat);

    REQUIRE(result.nOutliers == 0);
    REQUIRE(result.scale == 0.0);
}

TEST_CASE("ZScoreDetector: series too short throws SeriesTooShortException", "[ZScoreDetector]")
{
    ZScoreDetector d;
    REQUIRE_THROWS_AS(d.detect({1.0, 2.0}), loki::SeriesTooShortException);
}

TEST_CASE("ZScoreDetector: minimum series length is 3", "[ZScoreDetector]")
{
    // Exactly 3 points -- must not throw
    ZScoreDetector d;
    REQUIRE_NOTHROW(d.detect({1.0, 2.0, 3.0}));
}

TEST_CASE("ZScoreDetector: NaN in series throws MissingValueException", "[ZScoreDetector]")
{
    auto v = normalSeries();
    v[0] = std::numeric_limits<double>::quiet_NaN();
    ZScoreDetector d;
    REQUIRE_THROWS_AS(d.detect(v), loki::MissingValueException);
}

TEST_CASE("ZScoreDetector: stricter threshold flags more points", "[ZScoreDetector]")
{
    ZScoreDetector loose(5.0);
    ZScoreDetector strict(1.0);
    auto v = seriesWithOutliers();

    REQUIRE(strict.detect(v).nOutliers >= loose.detect(v).nOutliers);
}
