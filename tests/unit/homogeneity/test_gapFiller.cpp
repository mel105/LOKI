#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "loki/timeseries/gapFiller.hpp"
#include "loki/timeseries/timeSeries.hpp"
#include "loki/timeseries/timeStamp.hpp"
#include "loki/homogeneity/medianYearSeries.hpp"
#include "loki/core/exceptions.hpp"

#include <cmath>
#include <limits>

using namespace loki;
using namespace loki::homogeneity;

// =============================================================================
//  Helpers
// =============================================================================

namespace {

constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

// Builds a daily series starting at MJD 51544.0 (2000-01-01).
// values[i] == NaN inserts a NaN observation at that slot.
TimeSeries makeDailySeries(const std::vector<double>& values,
                           double startMjd = 51544.0)
{
    TimeSeries ts;
    for (std::size_t i = 0; i < values.size(); ++i) {
        const double mjd = startMjd + static_cast<double>(i);
        ts.append(TimeStamp::fromMjd(mjd), values[i]);
    }
    return ts;
}

// Builds a series with some rows entirely absent (time jumps).
// presentIndices: which positions (0-based) have observations.
// totalSlots: total expected length.
TimeSeries makeSparseDaily(const std::vector<std::size_t>& presentIndices,
                           std::size_t totalSlots,
                           double startMjd = 51544.0)
{
    TimeSeries ts;
    for (std::size_t idx : presentIndices) {
        if (idx < totalSlots) {
            const double mjd = startMjd + static_cast<double>(idx);
            ts.append(TimeStamp::fromMjd(mjd), static_cast<double>(idx) + 1.0);
        }
    }
    return ts;
}

bool isNaN(double v) { return v != v; }

} // anonymous namespace

// =============================================================================
//  Construction
// =============================================================================

TEST_CASE("GapFiller: default construction succeeds", "[gapfiller]")
{
    REQUIRE_NOTHROW(GapFiller{});
}

TEST_CASE("GapFiller: MEDIAN_YEAR construction succeeds (no longer blocked)", "[gapfiller]")
{
    GapFiller::Config cfg;
    cfg.strategy = GapFiller::Strategy::MEDIAN_YEAR;
    REQUIRE_NOTHROW(GapFiller{cfg});
}

TEST_CASE("GapFiller: MEDIAN_YEAR single-arg fill throws ConfigException", "[gapfiller]")
{
    GapFiller::Config cfg;
    cfg.strategy = GapFiller::Strategy::MEDIAN_YEAR;
    GapFiller gf{cfg};
    auto ts = makeDailySeries({1.0, 2.0, 3.0});
    REQUIRE_THROWS_AS(gf.fill(ts), ConfigException);
}

// =============================================================================
//  detectGaps -- preconditions
// =============================================================================

TEST_CASE("GapFiller::detectGaps: throws on unsorted series", "[gapfiller]")
{
    TimeSeries ts;
    ts.append(TimeStamp::fromMjd(51545.0), 1.0);
    ts.append(TimeStamp::fromMjd(51544.0), 2.0); // earlier than previous
    ts.append(TimeStamp::fromMjd(51546.0), 3.0);

    GapFiller gf;
    REQUIRE_THROWS_AS(gf.detectGaps(ts), AlgorithmException);
}

TEST_CASE("GapFiller::detectGaps: throws when fewer than 2 observations", "[gapfiller]")
{
    TimeSeries ts;
    ts.append(TimeStamp::fromMjd(51544.0), 1.0);

    GapFiller gf;
    REQUIRE_THROWS_AS(gf.detectGaps(ts), DataException);
}

// =============================================================================
//  detectGaps -- NaN gaps
// =============================================================================

TEST_CASE("GapFiller::detectGaps: no gaps in clean series", "[gapfiller]")
{
    auto ts = makeDailySeries({1.0, 2.0, 3.0, 4.0, 5.0});
    GapFiller gf;
    const auto gaps = gf.detectGaps(ts);
    REQUIRE(gaps.empty());
}

TEST_CASE("GapFiller::detectGaps: single NaN detected", "[gapfiller]")
{
    auto ts = makeDailySeries({1.0, NaN, 3.0});
    GapFiller gf;
    const auto gaps = gf.detectGaps(ts);
    REQUIRE(gaps.size() == 1);
    CHECK(gaps[0].startIndex == 1);
    CHECK(gaps[0].endIndex   == 1);
    CHECK(gaps[0].count      == 1);
    REQUIRE(gaps[0].startMjd.has_value());
}

TEST_CASE("GapFiller::detectGaps: run of NaNs counted correctly", "[gapfiller]")
{
    auto ts = makeDailySeries({1.0, NaN, NaN, NaN, 5.0});
    GapFiller gf;
    const auto gaps = gf.detectGaps(ts);
    REQUIRE(gaps.size() == 1);
    CHECK(gaps[0].count == 3);
}

TEST_CASE("GapFiller::detectGaps: two separate NaN runs produce two gaps", "[gapfiller]")
{
    auto ts = makeDailySeries({1.0, NaN, 3.0, NaN, NaN, 6.0});
    GapFiller gf;
    const auto gaps = gf.detectGaps(ts);
    REQUIRE(gaps.size() == 2);
    CHECK(gaps[0].count == 1);
    CHECK(gaps[1].count == 2);
}

TEST_CASE("GapFiller::detectGaps: leading NaN detected", "[gapfiller]")
{
    auto ts = makeDailySeries({NaN, NaN, 3.0, 4.0});
    GapFiller gf;
    const auto gaps = gf.detectGaps(ts);
    REQUIRE(gaps.size() == 1);
    CHECK(gaps[0].startIndex == 0);
    CHECK(gaps[0].count      == 2);
}

TEST_CASE("GapFiller::detectGaps: trailing NaN detected", "[gapfiller]")
{
    auto ts = makeDailySeries({1.0, 2.0, NaN, NaN});
    GapFiller gf;
    const auto gaps = gf.detectGaps(ts);
    REQUIRE(gaps.size() == 1);
    CHECK(gaps[0].endIndex == 3);
    CHECK(gaps[0].count    == 2);
}

// =============================================================================
//  detectGaps -- absent-row (time-jump) gaps
// =============================================================================

TEST_CASE("GapFiller::detectGaps: detects absent rows via time jump", "[gapfiller]")
{
    // Daily series: indices 0,1,2 present; then jump to index 5 (3 rows absent).
    TimeSeries ts;
    for (int i = 0; i <= 2; ++i)
        ts.append(TimeStamp::fromMjd(51544.0 + i), static_cast<double>(i));
    for (int i = 5; i <= 8; ++i)
        ts.append(TimeStamp::fromMjd(51544.0 + i), static_cast<double>(i));

    GapFiller gf;
    const auto gaps = gf.detectGaps(ts);

    // Exactly one gap, with 2 absent rows (indices 3 and 4).
    REQUIRE(gaps.size() == 1);
    CHECK(gaps[0].count == 2);
}

// =============================================================================
//  fill -- LINEAR
// =============================================================================

TEST_CASE("GapFiller::fill LINEAR: single interior NaN interpolated", "[gapfiller]")
{
    // 1.0, NaN, 3.0 -> 1.0, 2.0, 3.0
    auto ts = makeDailySeries({1.0, NaN, 3.0});
    GapFiller::Config cfg;
    cfg.strategy = GapFiller::Strategy::LINEAR;
    GapFiller gf{cfg};
    const auto result = gf.fill(ts);

    REQUIRE(result.size() == 3);
    CHECK_THAT(result[1].value, Catch::Matchers::WithinAbs(2.0, 1e-9));
}

TEST_CASE("GapFiller::fill LINEAR: run of NaNs interpolated", "[gapfiller]")
{
    // 0.0, NaN, NaN, NaN, 4.0 -> 0.0, 1.0, 2.0, 3.0, 4.0
    auto ts = makeDailySeries({0.0, NaN, NaN, NaN, 4.0});
    GapFiller::Config cfg;
    cfg.strategy = GapFiller::Strategy::LINEAR;
    GapFiller gf{cfg};
    const auto result = gf.fill(ts);

    REQUIRE(result.size() == 5);
    CHECK_THAT(result[1].value, Catch::Matchers::WithinAbs(1.0, 1e-9));
    CHECK_THAT(result[2].value, Catch::Matchers::WithinAbs(2.0, 1e-9));
    CHECK_THAT(result[3].value, Catch::Matchers::WithinAbs(3.0, 1e-9));
}

TEST_CASE("GapFiller::fill LINEAR: leading NaN filled by bfill", "[gapfiller]")
{
    auto ts = makeDailySeries({NaN, NaN, 3.0, 4.0});
    GapFiller::Config cfg;
    cfg.strategy = GapFiller::Strategy::LINEAR;
    GapFiller gf{cfg};
    const auto result = gf.fill(ts);

    CHECK_THAT(result[0].value, Catch::Matchers::WithinAbs(3.0, 1e-9));
    CHECK_THAT(result[1].value, Catch::Matchers::WithinAbs(3.0, 1e-9));
}

TEST_CASE("GapFiller::fill LINEAR: trailing NaN filled by ffill", "[gapfiller]")
{
    auto ts = makeDailySeries({1.0, 2.0, NaN, NaN});
    GapFiller::Config cfg;
    cfg.strategy = GapFiller::Strategy::LINEAR;
    GapFiller gf{cfg};
    const auto result = gf.fill(ts);

    CHECK_THAT(result[2].value, Catch::Matchers::WithinAbs(2.0, 1e-9));
    CHECK_THAT(result[3].value, Catch::Matchers::WithinAbs(2.0, 1e-9));
}

TEST_CASE("GapFiller::fill LINEAR: gap exceeding maxFillLength left as NaN", "[gapfiller]")
{
    // Gap of 4 NaNs; maxFillLength = 2 -> left unfilled.
    auto ts = makeDailySeries({1.0, NaN, NaN, NaN, NaN, 6.0});
    GapFiller::Config cfg;
    cfg.strategy       = GapFiller::Strategy::LINEAR;
    cfg.maxFillLength  = 2;
    GapFiller gf{cfg};
    const auto result = gf.fill(ts);

    CHECK(isNaN(result[1].value));
    CHECK(isNaN(result[2].value));
    CHECK(isNaN(result[3].value));
    CHECK(isNaN(result[4].value));
}

TEST_CASE("GapFiller::fill LINEAR: absent rows inserted and interpolated", "[gapfiller]")
{
    // Days 0,1,2 and 5,6 present; days 3,4 absent.
    // Values: 0,1,2,...,5,6
    TimeSeries ts;
    for (int i = 0; i <= 2; ++i)
        ts.append(TimeStamp::fromMjd(51544.0 + i), static_cast<double>(i));
    for (int i = 5; i <= 6; ++i)
        ts.append(TimeStamp::fromMjd(51544.0 + i), static_cast<double>(i));

    GapFiller::Config cfg;
    cfg.strategy = GapFiller::Strategy::LINEAR;
    GapFiller gf{cfg};
    const auto result = gf.fill(ts);

    // Result should have 7 observations (days 0-6).
    REQUIRE(result.size() == 7);
    // Day 3: interpolated between 2 and 5 -> 3.0
    CHECK_THAT(result[3].value, Catch::Matchers::WithinAbs(3.0, 1e-6));
    CHECK_THAT(result[4].value, Catch::Matchers::WithinAbs(4.0, 1e-6));
}

// =============================================================================
//  fill -- FORWARD_FILL
// =============================================================================

TEST_CASE("GapFiller::fill FORWARD_FILL: interior NaN replaced by last known", "[gapfiller]")
{
    auto ts = makeDailySeries({1.0, NaN, NaN, 4.0});
    GapFiller::Config cfg;
    cfg.strategy = GapFiller::Strategy::FORWARD_FILL;
    GapFiller gf{cfg};
    const auto result = gf.fill(ts);

    CHECK_THAT(result[1].value, Catch::Matchers::WithinAbs(1.0, 1e-9));
    CHECK_THAT(result[2].value, Catch::Matchers::WithinAbs(1.0, 1e-9));
    CHECK_THAT(result[3].value, Catch::Matchers::WithinAbs(4.0, 1e-9));
}

TEST_CASE("GapFiller::fill FORWARD_FILL: leading NaN filled by bfill", "[gapfiller]")
{
    auto ts = makeDailySeries({NaN, NaN, 3.0, 4.0});
    GapFiller::Config cfg;
    cfg.strategy = GapFiller::Strategy::FORWARD_FILL;
    GapFiller gf{cfg};
    const auto result = gf.fill(ts);

    CHECK_THAT(result[0].value, Catch::Matchers::WithinAbs(3.0, 1e-9));
    CHECK_THAT(result[1].value, Catch::Matchers::WithinAbs(3.0, 1e-9));
}

// =============================================================================
//  fill -- MEAN
// =============================================================================

TEST_CASE("GapFiller::fill MEAN: NaN replaced by series mean", "[gapfiller]")
{
    // mean of {1,3,5} = 3.0
    auto ts = makeDailySeries({1.0, NaN, 3.0, NaN, 5.0});
    GapFiller::Config cfg;
    cfg.strategy = GapFiller::Strategy::MEAN;
    GapFiller gf{cfg};
    const auto result = gf.fill(ts);

    CHECK_THAT(result[1].value, Catch::Matchers::WithinAbs(3.0, 1e-9));
    CHECK_THAT(result[3].value, Catch::Matchers::WithinAbs(3.0, 1e-9));
}

// =============================================================================
//  fill -- NONE
// =============================================================================

TEST_CASE("GapFiller::fill NONE: series returned unchanged", "[gapfiller]")
{
    auto ts = makeDailySeries({1.0, NaN, 3.0});
    GapFiller::Config cfg;
    cfg.strategy = GapFiller::Strategy::NONE;
    GapFiller gf{cfg};
    const auto result = gf.fill(ts);

    REQUIRE(result.size() == 3);
    CHECK(isNaN(result[1].value));
}

// =============================================================================
//  fill -- edge cases
// =============================================================================

TEST_CASE("GapFiller::fill: all-NaN series throws DataException", "[gapfiller]")
{
    auto ts = makeDailySeries({NaN, NaN, NaN});
    GapFiller gf;
    REQUIRE_THROWS_AS(gf.fill(ts), DataException);
}

TEST_CASE("GapFiller::fill: no gaps returns identical values", "[gapfiller]")
{
    auto ts = makeDailySeries({1.0, 2.0, 3.0, 4.0});
    GapFiller gf;
    const auto result = gf.fill(ts);

    REQUIRE(result.size() == ts.size());
    for (std::size_t i = 0; i < ts.size(); ++i) {
        CHECK_THAT(result[i].value, Catch::Matchers::WithinAbs(ts[i].value, 1e-12));
    }
}

TEST_CASE("GapFiller::fill: metadata preserved after fill", "[gapfiller]")
{
    SeriesMetadata meta;
    meta.stationId     = "GRAZ";
    meta.componentName = "dN";
    meta.unit          = "mm";

    TimeSeries ts(meta);
    ts.append(TimeStamp::fromMjd(51544.0), 1.0);
    ts.append(TimeStamp::fromMjd(51545.0), NaN);
    ts.append(TimeStamp::fromMjd(51546.0), 3.0);

    GapFiller gf;
    const auto result = gf.fill(ts);

    CHECK(result.metadata().stationId     == "GRAZ");
    CHECK(result.metadata().componentName == "dN");
    CHECK(result.metadata().unit          == "mm");
}

TEST_CASE("GapFiller::fill: unsorted series throws AlgorithmException", "[gapfiller]")
{
    TimeSeries ts;
    ts.append(TimeStamp::fromMjd(51545.0), 1.0);
    ts.append(TimeStamp::fromMjd(51544.0), 2.0);
    ts.append(TimeStamp::fromMjd(51546.0), 3.0);

    GapFiller gf;
    REQUIRE_THROWS_AS(gf.fill(ts), AlgorithmException);
}

// =============================================================================
//  fill -- MEDIAN_YEAR via ProfileLookup
// =============================================================================

namespace {

// Builds a daily constant series spanning numYears years starting at startYear.
TimeSeries makeDailyConstantForMYS(int startYear, int numYears, double v)
{
    TimeSeries ts;
    TimeStamp start(startYear, 1, 1);
    const double endMjd = start.mjd() + static_cast<double>(numYears) * 365.25;
    double mjd = start.mjd();
    while (mjd < endMjd) {
        ts.append(TimeStamp::fromMjd(mjd), v);
        mjd += 1.0;
    }
    return ts;
}

} // anonymous namespace

TEST_CASE("GapFiller::fill MEDIAN_YEAR: single NaN filled from profile", "[gapfiller]")
{
    // Build a 6-year constant series at value 5.0, then punch a hole.
    auto fullSeries = makeDailyConstantForMYS(2000, 6, 5.0);

    // Create a shorter series with a NaN on Jan 15 2003.
    TimeSeries ts;
    TimeStamp start(2000, 1, 1);
    TimeStamp target(2003, 1, 15);
    const double endMjd = TimeStamp(2006, 1, 1).mjd();
    double mjd = start.mjd();
    while (mjd < endMjd) {
        const double val = (std::abs(mjd - target.mjd()) < 0.5)
                           ? std::numeric_limits<double>::quiet_NaN()
                           : 5.0;
        ts.append(TimeStamp::fromMjd(mjd), val);
        mjd += 1.0;
    }

    // Build profile from full series (no NaNs).
    MedianYearSeries mys{fullSeries};

    GapFiller::Config cfg;
    cfg.strategy = GapFiller::Strategy::MEDIAN_YEAR;
    GapFiller gf{cfg};

    const auto result = gf.fill(ts, [&mys](const TimeStamp& t) {
        return mys.valueAt(t);
    });

    // Find the filled position.
    std::size_t filledIdx = result.size(); // sentinel
    for (std::size_t i = 0; i < result.size(); ++i) {
        if (std::abs(result[i].time.mjd() - target.mjd()) < 0.5) {
            filledIdx = i;
            break;
        }
    }
    REQUIRE(filledIdx < result.size());
    CHECK_THAT(result[filledIdx].value, Catch::Matchers::WithinAbs(5.0, 1e-6));
}

TEST_CASE("GapFiller::fill MEDIAN_YEAR: profile NaN slot left unfilled", "[gapfiller]")
{
    // Build a series where Jan 1 is always NaN -> profile slot 0 = NaN.
    TimeSeries profileSource;
    for (int y = 2000; y < 2006; ++y) {
        TimeStamp jan1(y, 1, 1);
        const double endMjd = TimeStamp(y + 1, 1, 1).mjd();
        double mjd = jan1.mjd() + 1.0; // skip Jan 1
        while (mjd < endMjd) {
            profileSource.append(TimeStamp::fromMjd(mjd), 3.0);
            mjd += 1.0;
        }
    }

    // Series to fill: has a NaN on Jan 1 2003.
    TimeSeries ts;
    {
        double mjd = TimeStamp(2000, 1, 1).mjd();
        const double endMjd = TimeStamp(2006, 1, 1).mjd();
        while (mjd < endMjd) {
            const double val =
                (std::abs(mjd - TimeStamp(2003, 1, 1).mjd()) < 0.5)
                ? std::numeric_limits<double>::quiet_NaN()
                : 3.0;
            ts.append(TimeStamp::fromMjd(mjd), val);
            mjd += 1.0;
        }
    }

    MedianYearSeries::Config mCfg;
    mCfg.minYears = 5;
    MedianYearSeries mys{profileSource, mCfg};

    GapFiller::Config cfg;
    cfg.strategy = GapFiller::Strategy::MEDIAN_YEAR;
    GapFiller gf{cfg};

    // Should not throw; the NaN slot is left as NaN with a warning.
    TimeSeries result;
    REQUIRE_NOTHROW(result = gf.fill(ts, [&mys](const TimeStamp& t) {
        return mys.valueAt(t);
    }));

    // The Jan 1 2003 slot should still be NaN.
    for (std::size_t i = 0; i < result.size(); ++i) {
        if (std::abs(result[i].time.mjd() - TimeStamp(2003, 1, 1).mjd()) < 0.5) {
            CHECK(result[i].value != result[i].value); // NaN
            break;
        }
    }
}

TEST_CASE("GapFiller::fill MEDIAN_YEAR: two-arg overload with LINEAR strategy ignores lookup", "[gapfiller]")
{
    // If strategy != MEDIAN_YEAR, the lookup must be ignored.
    auto ts = makeDailySeries({1.0, std::numeric_limits<double>::quiet_NaN(), 3.0});

    GapFiller::Config cfg;
    cfg.strategy = GapFiller::Strategy::LINEAR;
    GapFiller gf{cfg};

    bool lookupCalled = false;
    const auto result = gf.fill(ts, [&](const TimeStamp&) {
        lookupCalled = true;
        return 99.0;
    });

    CHECK_FALSE(lookupCalled);
    CHECK_THAT(result[1].value, Catch::Matchers::WithinAbs(2.0, 1e-9));
}
