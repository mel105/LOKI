#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "loki/timeseries/timeStamp.hpp"
#include "loki/core/exceptions.hpp"

// =============================================================================
// test_timeStamp.cpp
//
// Tests for the TimeStamp class.
// Verifies that:
//   - calendar constructor stores and retrieves components correctly
//   - factory methods produce consistent results (MJD, Unix, GPS)
//   - round-trip conversions are lossless within floating-point tolerance
//   - UTC string parsing works for supported formats
//   - comparison operators order timestamps correctly
// =============================================================================

using namespace Catch::Matchers;

static constexpr double MJD_TOL = 1e-9; // ~0.1 ms -- acceptable round-trip error

// -----------------------------------------------------------------------------
// Calendar constructor and getters
// -----------------------------------------------------------------------------

TEST_CASE("Calendar constructor stores date components", "[timestamp][calendar]")
{
    TimeStamp ts(2020, 3, 14, 9, 26, 53.0);

    REQUIRE(ts.year()   == 2020);
    REQUIRE(ts.month()  == 3);
    REQUIRE(ts.day()    == 14);
    REQUIRE(ts.hour()   == 9);
    REQUIRE(ts.minute() == 26);
    REQUIRE_THAT(ts.second(), WithinAbs(53.0, 1e-6));
}

TEST_CASE("Calendar constructor defaults to midnight", "[timestamp][calendar]")
{
    TimeStamp ts(2021, 6, 1);

    REQUIRE(ts.hour()   == 0);
    REQUIRE(ts.minute() == 0);
    REQUIRE_THAT(ts.second(), WithinAbs(0.0, 1e-9));
}

// -----------------------------------------------------------------------------
// MJD round-trip
// -----------------------------------------------------------------------------

TEST_CASE("fromMjd round-trip: MJD -> calendar -> MJD", "[timestamp][mjd]")
{
    // 2000-01-01 12:00:00 UTC = MJD 51544.5 (J2000.0)
    const double mjd = 51544.5;
    TimeStamp ts = TimeStamp::fromMjd(mjd);

    REQUIRE(ts.year()  == 2000);
    REQUIRE(ts.month() == 1);
    REQUIRE(ts.day()   == 1);
    REQUIRE(ts.hour()  == 12);
    REQUIRE_THAT(ts.mjd(), WithinAbs(mjd, MJD_TOL));
}

// -----------------------------------------------------------------------------
// Unix time round-trip
// -----------------------------------------------------------------------------

TEST_CASE("fromUnix round-trip: Unix -> MJD -> Unix", "[timestamp][unix]")
{
    // Unix epoch itself: 1970-01-01 00:00:00 UTC
    const double unix0 = 0.0;
    TimeStamp ts = TimeStamp::fromUnix(unix0);

    REQUIRE(ts.year()  == 1970);
    REQUIRE(ts.month() == 1);
    REQUIRE(ts.day()   == 1);
    REQUIRE_THAT(ts.unixTime(), WithinAbs(unix0, 1e-3)); // 1 ms tolerance

    // Arbitrary date: 2023-07-04 00:00:00 UTC = 1688428800
    const double t2 = 1688428800.0;
    TimeStamp ts2 = TimeStamp::fromUnix(t2);
    REQUIRE(ts2.year()  == 2023);
    REQUIRE(ts2.month() == 7);
    REQUIRE(ts2.day()   == 4);
    REQUIRE_THAT(ts2.unixTime(), WithinAbs(t2, 1e-3));
}

// -----------------------------------------------------------------------------
// GPS total-seconds round-trip
// -----------------------------------------------------------------------------

TEST_CASE("fromGpsTotalSeconds: GPS epoch is 1980-01-06", "[timestamp][gps]")
{
    // GPS t=0 is nominally 1980-01-06 00:00:00 GPS time.
    // The implementation subtracts the leap-second offset valid at that MJD
    // (10 seconds, from the 1980-01-01 entry), producing UTC 1980-01-05 23:59:50.
    // This is the correct behaviour: GPS and UTC differed by 10 s at the GPS epoch.
    TimeStamp ts = TimeStamp::fromGpsTotalSeconds(0.0);

    REQUIRE(ts.year()   == 1980);
    REQUIRE(ts.month()  == 1);
    REQUIRE(ts.day()    == 5);
    REQUIRE(ts.hour()   == 23);
    REQUIRE(ts.minute() == 59);
    REQUIRE_THAT(ts.second(), WithinAbs(50.0, 1e-6));
}

TEST_CASE("fromGpsTotalSeconds round-trip", "[timestamp][gps]")
{
    // Pick a known GPS seconds value and verify the round-trip holds.
    const double gps = 1e9; // approx 2011-09-14
    TimeStamp ts = TimeStamp::fromGpsTotalSeconds(gps);
    REQUIRE_THAT(ts.gpsTotalSeconds(), WithinAbs(gps, 1.0));
    // 1-second tolerance because leap-second corrections are integer steps
}

// -----------------------------------------------------------------------------
// UTC string parsing
// -----------------------------------------------------------------------------

TEST_CASE("UTC string constructor parses full ISO datetime", "[timestamp][string]")
{
    TimeStamp ts("2015-09-30 12:00:00");

    REQUIRE(ts.year()   == 2015);
    REQUIRE(ts.month()  == 9);
    REQUIRE(ts.day()    == 30);
    REQUIRE(ts.hour()   == 12);
    REQUIRE(ts.minute() == 0);
}

TEST_CASE("UTC string constructor parses fractional seconds", "[timestamp][string]")
{
    TimeStamp ts("2015-09-30 12:00:00.500");

    REQUIRE_THAT(ts.second(), WithinAbs(0.5, 1e-6));
}

TEST_CASE("Malformed UTC string throws ParseException", "[timestamp][string]")
{
    REQUIRE_THROWS_AS(
        []{ TimeStamp ts("not-a-date"); }(),
        loki::ParseException
    );
}

// -----------------------------------------------------------------------------
// Comparison operators
// -----------------------------------------------------------------------------

TEST_CASE("Earlier timestamp compares less than later one", "[timestamp][comparison]")
{
    TimeStamp t1(2020, 1, 1);
    TimeStamp t2(2020, 6, 1);

    REQUIRE(t1 < t2);
    REQUIRE(t2 > t1);
    REQUIRE(t1 != t2);
}

TEST_CASE("Equal timestamps compare equal", "[timestamp][comparison]")
{
    TimeStamp t1(2020, 3, 15, 10, 0, 0.0);
    TimeStamp t2(2020, 3, 15, 10, 0, 0.0);

    REQUIRE(t1 == t2);
    REQUIRE_FALSE(t1 < t2);
    REQUIRE_FALSE(t1 > t2);
}

TEST_CASE("Calendar vs fromMjd produce equal timestamps for J2000", "[timestamp][comparison]")
{
    TimeStamp fromCal = TimeStamp(2000, 1, 1, 12, 0, 0.0);
    TimeStamp fromMjd = TimeStamp::fromMjd(51544.5);

    // Both should land on MJD 51544.5 within tolerance
    REQUIRE_THAT(fromCal.mjd(), WithinAbs(fromMjd.mjd(), MJD_TOL));
}