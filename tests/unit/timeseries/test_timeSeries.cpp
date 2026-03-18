#include <catch2/catch_test_macros.hpp>

#include "loki/timeseries/timeSeries.hpp"
#include "loki/timeseries/timeStamp.hpp"
#include "loki/core/exceptions.hpp"

#include <cmath>

// =============================================================================
// test_timeSeries.cpp
//
// Tests for TimeSeries, Observation, and the isValid() free function.
// Verifies that:
//   - append / size / empty work correctly
//   - at() throws on out-of-bounds, operator[] does not
//   - sorted-flag tracking is correct
//   - sortByTime() sorts an out-of-order series
//   - indexOf() and atTime() perform correct binary search
//   - slice(size_t, size_t) returns the right half-open range
//   - slice(TimeStamp, TimeStamp) returns the right inclusive range
//   - metadata is stored and retrieved unchanged
//   - isValid() detects NaN values
// =============================================================================

// Convenience: build a sorted series with n observations spaced 1 day apart
static loki::TimeSeries makeSorted(int n)
{
    loki::TimeSeries ts;
    for (int i = 0; i < n; ++i) {
        ts.append(TimeStamp(2020, 1, 1 + i), static_cast<double>(i));
    }
    return ts;
}

// -----------------------------------------------------------------------------
// isValid free function
// -----------------------------------------------------------------------------

TEST_CASE("isValid returns true for a finite value", "[timeseries][observation]")
{
    loki::Observation obs;
    obs.value = 3.14;
    REQUIRE(loki::isValid(obs));
}

TEST_CASE("isValid returns false for NaN", "[timeseries][observation]")
{
    loki::Observation obs;
    obs.value = std::numeric_limits<double>::quiet_NaN();
    REQUIRE_FALSE(loki::isValid(obs));
}

// -----------------------------------------------------------------------------
// Construction and population
// -----------------------------------------------------------------------------

TEST_CASE("Default-constructed TimeSeries is empty", "[timeseries][construction]")
{
    loki::TimeSeries ts;
    REQUIRE(ts.empty());
    REQUIRE(ts.size() == 0);
}

TEST_CASE("append increases size", "[timeseries][append]")
{
    loki::TimeSeries ts;
    ts.append(TimeStamp(2020, 1, 1), 1.0);
    ts.append(TimeStamp(2020, 1, 2), 2.0);

    REQUIRE(ts.size() == 2);
    REQUIRE_FALSE(ts.empty());
}

TEST_CASE("append stores value and timestamp correctly", "[timeseries][append]")
{
    loki::TimeSeries ts;
    TimeStamp t(2021, 6, 15, 12, 0, 0.0);
    ts.append(t, 42.5, 3);

    const auto& obs = ts[0];
    REQUIRE(obs.value == 42.5);
    REQUIRE(obs.flag  == 3);
    REQUIRE(obs.time  == t);
}

// -----------------------------------------------------------------------------
// Element access
// -----------------------------------------------------------------------------

TEST_CASE("at() throws DataException on out-of-bounds index", "[timeseries][access]")
{
    loki::TimeSeries ts = makeSorted(3);

    REQUIRE_NOTHROW(ts.at(2));
    REQUIRE_THROWS_AS(ts.at(3), loki::DataException);
}

TEST_CASE("operator[] returns correct element", "[timeseries][access]")
{
    loki::TimeSeries ts = makeSorted(5);

    REQUIRE(ts[0].value == 0.0);
    REQUIRE(ts[4].value == 4.0);
}

// -----------------------------------------------------------------------------
// Sorted-flag and sortByTime
// -----------------------------------------------------------------------------

TEST_CASE("Series appended in order is considered sorted", "[timeseries][sorted]")
{
    loki::TimeSeries ts = makeSorted(5);
    REQUIRE(ts.isSorted());
}

TEST_CASE("Out-of-order append clears sorted flag", "[timeseries][sorted]")
{
    loki::TimeSeries ts;
    ts.append(TimeStamp(2020, 1, 2), 2.0);
    ts.append(TimeStamp(2020, 1, 1), 1.0); // earlier than previous

    REQUIRE_FALSE(ts.isSorted());
}

TEST_CASE("sortByTime reorders and sets sorted flag", "[timeseries][sorted]")
{
    loki::TimeSeries ts;
    ts.append(TimeStamp(2020, 1, 3), 3.0);
    ts.append(TimeStamp(2020, 1, 1), 1.0);
    ts.append(TimeStamp(2020, 1, 2), 2.0);

    REQUIRE_FALSE(ts.isSorted());
    ts.sortByTime();

    REQUIRE(ts.isSorted());
    REQUIRE(ts[0].value == 1.0);
    REQUIRE(ts[1].value == 2.0);
    REQUIRE(ts[2].value == 3.0);
}

// -----------------------------------------------------------------------------
// indexOf and atTime
// -----------------------------------------------------------------------------

TEST_CASE("indexOf finds existing timestamp", "[timeseries][search]")
{
    loki::TimeSeries ts = makeSorted(5);
    TimeStamp target(2020, 1, 3); // index 2

    auto idx = ts.indexOf(target);
    REQUIRE(idx.has_value());
    REQUIRE(*idx == 2);
}

TEST_CASE("indexOf returns nullopt for timestamp not in series", "[timeseries][search]")
{
    loki::TimeSeries ts = makeSorted(5);
    TimeStamp target(2025, 1, 1); // way outside range

    auto idx = ts.indexOf(target);
    REQUIRE_FALSE(idx.has_value());
}

TEST_CASE("indexOf throws AlgorithmException on unsorted series", "[timeseries][search]")
{
    loki::TimeSeries ts;
    ts.append(TimeStamp(2020, 1, 2), 2.0);
    ts.append(TimeStamp(2020, 1, 1), 1.0);

    REQUIRE_THROWS_AS(
        ts.indexOf(TimeStamp(2020, 1, 1)),
        loki::AlgorithmException
    );
}

TEST_CASE("atTime returns correct observation", "[timeseries][search]")
{
    loki::TimeSeries ts = makeSorted(5);
    TimeStamp target(2020, 1, 4); // index 3, value 3.0

    const auto& obs = ts.atTime(target);
    REQUIRE(obs.value == 3.0);
}

TEST_CASE("atTime throws DataException when no observation in tolerance", "[timeseries][search]")
{
    loki::TimeSeries ts = makeSorted(3);
    TimeStamp way_off(2030, 1, 1);

    REQUIRE_THROWS_AS(ts.atTime(way_off), loki::DataException);
}

// -----------------------------------------------------------------------------
// slice by index
// -----------------------------------------------------------------------------

TEST_CASE("slice(from, to) returns half-open range", "[timeseries][slice]")
{
    loki::TimeSeries ts = makeSorted(6); // values 0..5

    loki::TimeSeries sub = ts.slice(std::size_t(1), std::size_t(4));
    REQUIRE(sub.size() == 3);
    REQUIRE(sub[0].value == 1.0);
    REQUIRE(sub[2].value == 3.0);
}

TEST_CASE("slice(from, to) with from==to returns empty series", "[timeseries][slice]")
{
    loki::TimeSeries ts = makeSorted(4);
    loki::TimeSeries sub = ts.slice(std::size_t(2), std::size_t(2));
    REQUIRE(sub.empty());
}

TEST_CASE("slice(from, to) throws when from > to", "[timeseries][slice]")
{
    loki::TimeSeries ts = makeSorted(4);
    REQUIRE_THROWS_AS(
        ts.slice(std::size_t(3), std::size_t(1)),
        loki::DataException
    );
}

// -----------------------------------------------------------------------------
// slice by TimeStamp
// -----------------------------------------------------------------------------

TEST_CASE("slice(TimeStamp, TimeStamp) returns inclusive range", "[timeseries][slice]")
{
    loki::TimeSeries ts = makeSorted(6); // 2020-01-01 to 2020-01-06

    TimeStamp from(2020, 1, 2);
    TimeStamp to(2020, 1, 4);

    loki::TimeSeries sub = ts.slice(from, to);
    REQUIRE(sub.size() == 3); // indices 1,2,3
    REQUIRE(sub[0].value == 1.0);
    REQUIRE(sub[2].value == 3.0);
}

// -----------------------------------------------------------------------------
// Metadata
// -----------------------------------------------------------------------------

TEST_CASE("Metadata set via constructor is retrievable", "[timeseries][metadata]")
{
    loki::SeriesMetadata m{"GRAZ", "dN", "mm", "North component"};
    loki::TimeSeries ts(m);

    REQUIRE(ts.metadata().stationId     == "GRAZ");
    REQUIRE(ts.metadata().componentName == "dN");
    REQUIRE(ts.metadata().unit          == "mm");
}

TEST_CASE("setMetadata replaces metadata", "[timeseries][metadata]")
{
    loki::TimeSeries ts;
    loki::SeriesMetadata m{"NEW", "dE", "mm", ""};
    ts.setMetadata(m);

    REQUIRE(ts.metadata().stationId == "NEW");
}
