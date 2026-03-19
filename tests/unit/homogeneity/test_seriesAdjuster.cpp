#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <loki/homogeneity/seriesAdjuster.hpp>
#include <loki/homogeneity/changePointResult.hpp>
#include <loki/timeseries/timeSeries.hpp>
#include <loki/timeseries/timeStamp.hpp>
#include <loki/core/exceptions.hpp>

#include <cmath>
#include <vector>

using Catch::Approx;
using loki::TimeSeries;
using loki::SeriesMetadata;
// TimeStamp is in global namespace (not loki::)
using loki::homogeneity::ChangePoint;
using loki::homogeneity::SeriesAdjuster;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * Builds a daily TimeSeries starting at startMjd with the given values.
 * Optional series name (default "test").
 */
static TimeSeries makeSeries(double                     startMjd,
                              const std::vector<double>& vals,
                              const std::string&         name = "test")
{
    TimeSeries ts;
    SeriesMetadata meta;
    meta.componentName = name;
    ts.setMetadata(meta);
    for (std::size_t i = 0; i < vals.size(); ++i) {
        TimeStamp t = TimeStamp::fromMjd(startMjd + static_cast<double>(i));
        ts.append(t, vals[i]);
    }
    return ts;
}

/**
 * Builds a ChangePoint with the given globalIndex and shift.
 * mjd and pValue are set to plausible defaults.
 */
static ChangePoint makeCP(std::size_t globalIndex, double shift,
                           double mjd = 0.0, double pValue = 0.01)
{
    return ChangePoint{globalIndex, mjd, shift, pValue};
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("SeriesAdjuster: empty changePoints returns copy of original", "[seriesadjuster]")
{
    const std::vector<double> vals{1.0, 2.0, 3.0, 4.0, 5.0};
    const auto ts = makeSeries(51544.0, vals);

    SeriesAdjuster adjuster;
    const auto result = adjuster.adjust(ts, {});

    REQUIRE(result.size() == vals.size());
    for (std::size_t i = 0; i < vals.size(); ++i) {
        CHECK(result[i].value == Approx(vals[i]));
    }
}

TEST_CASE("SeriesAdjuster: empty changePoints still appends _adj suffix", "[seriesadjuster]")
{
    const auto ts = makeSeries(51544.0, {1.0, 2.0, 3.0}, "pressure");

    SeriesAdjuster adjuster;
    const auto result = adjuster.adjust(ts, {});

    CHECK(result.metadata().componentName == "pressure_adj");
}

TEST_CASE("SeriesAdjuster: single change point - left segment shifted, right unchanged", "[seriesadjuster]")
{
    // Series: [0, 1, 2, 3, 4] with change point at index 3, shift = +2.0
    // Reference = right segment [3, 4]
    // Left segment [0, 1, 2] should each be decreased by 2.0
    const std::vector<double> vals{10.0, 11.0, 12.0, 13.0, 14.0};
    const auto ts = makeSeries(51544.0, vals);

    const std::vector<ChangePoint> cps{makeCP(3, 2.0)};

    SeriesAdjuster adjuster;
    const auto result = adjuster.adjust(ts, cps);

    REQUIRE(result.size() == vals.size());

    // Left of change point: corrected by -2.0
    CHECK(result[0].value == Approx(8.0));
    CHECK(result[1].value == Approx(9.0));
    CHECK(result[2].value == Approx(10.0));

    // Change point and to the right: reference, no correction
    CHECK(result[3].value == Approx(13.0));
    CHECK(result[4].value == Approx(14.0));
}

TEST_CASE("SeriesAdjuster: two change points - cumulative shifts applied correctly", "[seriesadjuster]")
{
    // Segments: [0,1] | [2,3] | [4,5] (reference)
    // CP at index 2, shift = 1.0
    // CP at index 4, shift = 2.0
    // Segment 0 ([0,1]): correction = 1.0 + 2.0 = 3.0
    // Segment 1 ([2,3]): correction = 2.0
    // Segment 2 ([4,5]): reference, correction = 0.0
    const std::vector<double> vals{10.0, 10.0, 11.0, 11.0, 12.0, 12.0};
    const auto ts = makeSeries(51544.0, vals);

    const std::vector<ChangePoint> cps{makeCP(2, 1.0), makeCP(4, 2.0)};

    SeriesAdjuster adjuster;
    const auto result = adjuster.adjust(ts, cps);

    REQUIRE(result.size() == vals.size());

    CHECK(result[0].value == Approx(7.0));   // 10 - 3
    CHECK(result[1].value == Approx(7.0));   // 10 - 3
    CHECK(result[2].value == Approx(9.0));   // 11 - 2
    CHECK(result[3].value == Approx(9.0));   // 11 - 2
    CHECK(result[4].value == Approx(12.0));  // reference
    CHECK(result[5].value == Approx(12.0));  // reference
}

TEST_CASE("SeriesAdjuster: unordered change points give same result as ordered", "[seriesadjuster]")
{
    const std::vector<double> vals{10.0, 10.0, 11.0, 11.0, 12.0, 12.0};
    const auto ts = makeSeries(51544.0, vals);

    SeriesAdjuster adjuster;

    const auto ordered   = adjuster.adjust(ts, {makeCP(2, 1.0), makeCP(4, 2.0)});
    const auto unordered = adjuster.adjust(ts, {makeCP(4, 2.0), makeCP(2, 1.0)});

    REQUIRE(ordered.size() == unordered.size());
    for (std::size_t i = 0; i < ordered.size(); ++i) {
        CHECK(ordered[i].value == Approx(unordered[i].value));
    }
}

TEST_CASE("SeriesAdjuster: change point at index 0 - entire series is reference", "[seriesadjuster]")
{
    // If the change point is at index 0, no point has globalIndex < 0,
    // so the correction for every point is shift for CPs with index <= i,
    // meaning index 0 is in the reference segment (correction = 0 for all).
    const std::vector<double> vals{5.0, 6.0, 7.0};
    const auto ts = makeSeries(51544.0, vals);

    const std::vector<ChangePoint> cps{makeCP(0, 3.0)};

    SeriesAdjuster adjuster;
    const auto result = adjuster.adjust(ts, cps);

    // The change point is at index 0: it and everything after is the reference.
    // No points lie strictly left of index 0, so no correction is applied.
    CHECK(result[0].value == Approx(5.0));
    CHECK(result[1].value == Approx(6.0));
    CHECK(result[2].value == Approx(7.0));
}

TEST_CASE("SeriesAdjuster: change point at last index - all but last point corrected", "[seriesadjuster]")
{
    // CP at index 4 (last), shift = 1.0
    // Points [0..3] get correction 1.0; point [4] is reference.
    const std::vector<double> vals{10.0, 10.0, 10.0, 10.0, 10.0};
    const auto ts = makeSeries(51544.0, vals);

    const std::vector<ChangePoint> cps{makeCP(4, 1.0)};

    SeriesAdjuster adjuster;
    const auto result = adjuster.adjust(ts, cps);

    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(result[i].value == Approx(9.0));
    }
    CHECK(result[4].value == Approx(10.0));
}

TEST_CASE("SeriesAdjuster: out-of-range globalIndex throws AlgorithmException", "[seriesadjuster]")
{
    const auto ts = makeSeries(51544.0, {1.0, 2.0, 3.0});

    SeriesAdjuster adjuster;

    CHECK_THROWS_AS(adjuster.adjust(ts, {makeCP(3, 1.0)}),  loki::AlgorithmException);
    CHECK_THROWS_AS(adjuster.adjust(ts, {makeCP(99, 1.0)}), loki::AlgorithmException);
}

TEST_CASE("SeriesAdjuster: metadata name gets _adj suffix", "[seriesadjuster]")
{
    const auto ts = makeSeries(51544.0, {1.0, 2.0, 3.0, 4.0}, "iwv");

    SeriesAdjuster adjuster;
    const auto result = adjuster.adjust(ts, {makeCP(2, 0.5)});

    CHECK(result.metadata().componentName == "iwv_adj");
}

TEST_CASE("SeriesAdjuster: timestamps and flags are preserved unchanged", "[seriesadjuster]")
{
    TimeSeries ts;
    SeriesMetadata meta;
    meta.componentName = "gnss";
    ts.setMetadata(meta);

    TimeStamp t0 = TimeStamp::fromMjd(51544.0);
    TimeStamp t1 = TimeStamp::fromMjd(51545.0);
    TimeStamp t2 = TimeStamp::fromMjd(51546.0);

    ts.append(t0, 1.0, 0);
    ts.append(t1, 2.0, 1);
    ts.append(t2, 3.0, 2);

    SeriesAdjuster adjuster;
    const auto result = adjuster.adjust(ts, {makeCP(2, 0.5)});

    CHECK(result[0].time.mjd() == Approx(t0.mjd()));
    CHECK(result[1].time.mjd() == Approx(t1.mjd()));
    CHECK(result[2].time.mjd() == Approx(t2.mjd()));

    CHECK(result[0].flag == 0);
    CHECK(result[1].flag == 1);
    CHECK(result[2].flag == 2);
}

TEST_CASE("SeriesAdjuster: negative shift correctly raises values in left segment", "[seriesadjuster]")
{
    // Shift = -3.0: correction = -3.0, so adjusted = original - (-3.0) = original + 3.0
    const std::vector<double> vals{10.0, 10.0, 13.0, 13.0};
    const auto ts = makeSeries(51544.0, vals);

    SeriesAdjuster adjuster;
    const auto result = adjuster.adjust(ts, {makeCP(2, -3.0)});

    CHECK(result[0].value == Approx(13.0));
    CHECK(result[1].value == Approx(13.0));
    CHECK(result[2].value == Approx(13.0));
    CHECK(result[3].value == Approx(13.0));
}
