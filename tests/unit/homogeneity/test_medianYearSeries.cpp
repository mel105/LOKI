#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "loki/homogeneity/medianYearSeries.hpp"
#include "loki/timeseries/timeSeries.hpp"
#include "loki/timeseries/timeStamp.hpp"
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

/**
 * Builds a daily series spanning the given number of years.
 * value(i) = base + i (simple ramp so each slot gets distinct values).
 * startYear: first year of the series.
 */
TimeSeries makeDailyYears(int startYear, int numYears, double base = 0.0)
{
    TimeSeries ts;
    double idx = 0.0;
    for (int y = startYear; y < startYear + numYears; ++y) {
        // Determine whether this is a leap year.
        bool leap = (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
        int days = leap ? 366 : 365;
        for (int d = 0; d < days; ++d) {
            // MJD of Jan 1 of year y (Gregorian).
            // Use TimeStamp calendar constructor to get MJD.
            TimeStamp jan1(y, 1, 1);
            const double mjd = jan1.mjd() + static_cast<double>(d);
            ts.append(TimeStamp::fromMjd(mjd), base + idx);
            idx += 1.0;
        }
    }
    return ts;
}

/**
 * Builds a 6-hourly series spanning numYears years with constant value v.
 */
TimeSeries make6HourlyConstant(int startYear, int numYears, double v)
{
    TimeSeries ts;
    const double step = 6.0 / 24.0; // 6 hours in days
    TimeStamp start(startYear, 1, 1);
    double mjd = start.mjd();
    // Total slots: 365.25 * 4 per year (approx), iterate by time.
    const double endMjd = mjd + static_cast<double>(numYears) * 365.25;
    while (mjd < endMjd) {
        ts.append(TimeStamp::fromMjd(mjd), v);
        mjd += step;
    }
    return ts;
}

/**
 * Builds a daily series with constant value v across all slots.
 */
TimeSeries makeDailyConstant(int startYear, int numYears, double v)
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

bool isNaN(double v) { return v != v; }

} // anonymous namespace

// =============================================================================
//  Construction -- preconditions
// =============================================================================

TEST_CASE("MedianYearSeries: throws on unsorted series", "[medianyear]")
{
    TimeSeries ts;
    ts.append(TimeStamp(2000, 1, 2), 1.0);
    ts.append(TimeStamp(2000, 1, 1), 2.0);
    ts.append(TimeStamp(2000, 1, 3), 3.0);
    REQUIRE_THROWS_AS(MedianYearSeries{ts}, AlgorithmException);
}

TEST_CASE("MedianYearSeries: throws on fewer than 2 observations", "[medianyear]")
{
    TimeSeries ts;
    ts.append(TimeStamp(2000, 1, 1), 1.0);
    REQUIRE_THROWS_AS(MedianYearSeries{ts}, DataException);
}

TEST_CASE("MedianYearSeries: throws ConfigException for sub-hourly resolution", "[medianyear]")
{
    // 1-minute resolution = 1/1440 days, finer than 1/24.
    TimeSeries ts;
    const double step = 1.0 / 1440.0;
    for (int i = 0; i < 100; ++i) {
        ts.append(TimeStamp::fromMjd(51544.0 + i * step), static_cast<double>(i));
    }
    REQUIRE_THROWS_AS(MedianYearSeries{ts}, ConfigException);
}

TEST_CASE("MedianYearSeries: default construction succeeds for daily data", "[medianyear]")
{
    auto ts = makeDailyConstant(2000, 6, 42.0);
    REQUIRE_NOTHROW(MedianYearSeries{ts});
}

// =============================================================================
//  profileSize and slotsPerDay
// =============================================================================

TEST_CASE("MedianYearSeries: daily series -> slotsPerDay = 1", "[medianyear]")
{
    auto ts = makeDailyConstant(2000, 6, 1.0);
    MedianYearSeries mys{ts};
    CHECK(mys.slotsPerDay() == 1);
    CHECK(mys.profileSize() == 366);
}

TEST_CASE("MedianYearSeries: 6-hourly series -> slotsPerDay = 4", "[medianyear]")
{
    auto ts = make6HourlyConstant(2000, 6, 1.0);
    MedianYearSeries mys{ts};
    CHECK(mys.slotsPerDay() == 4);
    CHECK(mys.profileSize() == static_cast<std::size_t>(366 * 4));
}

// =============================================================================
//  Median computation -- constant series
// =============================================================================

TEST_CASE("MedianYearSeries: constant daily series -> all slots return same value", "[medianyear]")
{
    auto ts = makeDailyConstant(2000, 6, 7.5);
    MedianYearSeries mys{ts};

    // Jan 1 of any year should map to slot 0.
    const double v = mys.valueAt(TimeStamp(2003, 1, 1));
    CHECK_THAT(v, Catch::Matchers::WithinAbs(7.5, 1e-9));

    // Mid-year slot.
    const double v2 = mys.valueAt(TimeStamp(2003, 7, 15));
    CHECK_THAT(v2, Catch::Matchers::WithinAbs(7.5, 1e-9));
}

TEST_CASE("MedianYearSeries: constant 6-hourly series -> all slots return same value", "[medianyear]")
{
    auto ts = make6HourlyConstant(2000, 6, 3.14);
    MedianYearSeries mys{ts};

    const double v = mys.valueAt(TimeStamp(2003, 3, 15, 6, 0, 0.0));
    CHECK_THAT(v, Catch::Matchers::WithinAbs(3.14, 1e-9));
}

// =============================================================================
//  NaN slots -- under-populated
// =============================================================================

TEST_CASE("MedianYearSeries: slots with fewer than minYears values return NaN", "[medianyear]")
{
    // Build a 6-year daily series but leave Jan 1 missing in all years.
    TimeSeries ts;
    for (int y = 2000; y < 2006; ++y) {
        TimeStamp jan1(y, 1, 1);
        const double endMjd = TimeStamp(y + 1, 1, 1).mjd();
        double mjd = jan1.mjd() + 1.0; // skip Jan 1
        while (mjd < endMjd) {
            ts.append(TimeStamp::fromMjd(mjd), 1.0);
            mjd += 1.0;
        }
    }

    MedianYearSeries::Config cfg;
    cfg.minYears = 5;
    MedianYearSeries mys{ts, cfg};

    // Jan 1 slot should be NaN (0 values).
    const double v = mys.valueAt(TimeStamp(2003, 1, 1));
    CHECK(isNaN(v));

    // Jan 2 slot should be valid.
    const double v2 = mys.valueAt(TimeStamp(2003, 1, 2));
    CHECK_FALSE(isNaN(v2));
}

// =============================================================================
//  valueAt -- slot mapping
// =============================================================================

TEST_CASE("MedianYearSeries: same DOY in different years maps to same slot", "[medianyear]")
{
    auto ts = makeDailyConstant(2000, 10, 5.0);
    MedianYearSeries mys{ts};

    const double v2001 = mys.valueAt(TimeStamp(2001, 6, 15));
    const double v2005 = mys.valueAt(TimeStamp(2005, 6, 15));
    const double v2009 = mys.valueAt(TimeStamp(2009, 6, 15));

    CHECK_THAT(v2001, Catch::Matchers::WithinAbs(v2005, 1e-9));
    CHECK_THAT(v2001, Catch::Matchers::WithinAbs(v2009, 1e-9));
}

TEST_CASE("MedianYearSeries: different DOYs map to different slots", "[medianyear]")
{
    // Build a series where each DOY has a unique constant value across years.
    // Value for day d (0-based) in year y = d (same value each year per DOY).
    TimeSeries ts;
    for (int y = 2000; y < 2010; ++y) {
        bool leap = (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
        int days = leap ? 366 : 365;
        TimeStamp jan1(y, 1, 1);
        for (int d = 0; d < days; ++d) {
            ts.append(TimeStamp::fromMjd(jan1.mjd() + d),
                      static_cast<double>(d));
        }
    }

    MedianYearSeries mys{ts};

    // DOY 1 (Jan 1) -> median of {0, 0, ..., 0} = 0.
    CHECK_THAT(mys.valueAt(TimeStamp(2005, 1, 1)),
               Catch::Matchers::WithinAbs(0.0, 1e-9));

    // DOY 2 (Jan 2) -> median of {1, 1, ..., 1} = 1.
    CHECK_THAT(mys.valueAt(TimeStamp(2005, 1, 2)),
               Catch::Matchers::WithinAbs(1.0, 1e-9));

    // DOY 100 -> median of {99, 99, ..., 99} = 99.
    // Find date for DOY 100 in a non-leap year: Apr 10.
    CHECK_THAT(mys.valueAt(TimeStamp(2005, 4, 10)),
               Catch::Matchers::WithinAbs(99.0, 1e-9));
}

// =============================================================================
//  stepDays
// =============================================================================

TEST_CASE("MedianYearSeries: stepDays is approximately 1.0 for daily series", "[medianyear]")
{
    auto ts = makeDailyConstant(2000, 6, 1.0);
    MedianYearSeries mys{ts};
    CHECK_THAT(mys.stepDays(), Catch::Matchers::WithinAbs(1.0, 0.01));
}

TEST_CASE("MedianYearSeries: stepDays is approximately 0.25 for 6-hourly series", "[medianyear]")
{
    auto ts = make6HourlyConstant(2000, 6, 1.0);
    MedianYearSeries mys{ts};
    CHECK_THAT(mys.stepDays(), Catch::Matchers::WithinAbs(0.25, 0.01));
}

// =============================================================================
//  Short series warning (no throw)
// =============================================================================

TEST_CASE("MedianYearSeries: series shorter than minYears logs warning but does not throw", "[medianyear]")
{
    // 3 years of data, minYears = 5 -> warning only.
    auto ts = makeDailyConstant(2000, 3, 2.0);
    MedianYearSeries::Config cfg;
    cfg.minYears = 5;
    // Should not throw.
    REQUIRE_NOTHROW(MedianYearSeries{ts, cfg});
}
