#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <loki/outlier/outlierCleaner.hpp>
#include <loki/outlier/iqrDetector.hpp>
#include <loki/outlier/madDetector.hpp>
#include <loki/outlier/zScoreDetector.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/timeseries/timeSeries.hpp>
#include <loki/timeseries/timeStamp.hpp>

#include <cmath>
#include <limits>
#include <vector>

using namespace loki;
using namespace loki::outlier;
using Catch::Matchers::WithinAbs;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Build a TimeSeries of n daily observations starting at MJD 51544 (J2000.0).
// Values are taken from the provided vector.
static TimeSeries buildSeries(const std::vector<double>& values)
{
    SeriesMetadata meta;
    meta.stationId     = "TEST";
    meta.componentName = "temp";
    meta.unit          = "C";

    TimeSeries ts(meta);
    ts.reserve(values.size());

    // Use simple MJD offsets -- TimeStamp from MJD
    const double baseMjd = 51544.0;
    for (std::size_t i = 0; i < values.size(); ++i) {
        TimeStamp t = TimeStamp::fromMjd(baseMjd + static_cast<double>(i));
        ts.append(t, values[i]);
    }
    return ts;
}

static std::vector<double> cleanValues()
{
    return { -0.5, 0.3, -0.2, 0.8, -0.6, 0.1, 0.4, -0.3, 0.7, -0.1,
              0.2, -0.4,  0.6, 0.0, -0.7,  0.5, 0.3, -0.5,  0.1,  0.4 };
}

static std::vector<double> valuesWithOutliers()
{
    auto v = cleanValues();
    v[4]  = 25.0;
    v[14] = -20.0;
    return v;
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TEST_CASE("OutlierCleaner: valid config constructs successfully", "[OutlierCleaner]")
{
    IqrDetector d;
    OutlierCleaner::Config cfg;
    cfg.fillStrategy = GapFiller::Strategy::LINEAR;
    REQUIRE_NOTHROW(OutlierCleaner(cfg, d));
}

TEST_CASE("OutlierCleaner: MEDIAN_YEAR strategy throws ConfigException", "[OutlierCleaner]")
{
    IqrDetector d;
    OutlierCleaner::Config cfg;
    cfg.fillStrategy = GapFiller::Strategy::MEDIAN_YEAR;
    REQUIRE_THROWS_AS(OutlierCleaner(cfg, d), loki::ConfigException);
}

TEST_CASE("OutlierCleaner: NONE strategy throws ConfigException", "[OutlierCleaner]")
{
    IqrDetector d;
    OutlierCleaner::Config cfg;
    cfg.fillStrategy = GapFiller::Strategy::NONE;
    REQUIRE_THROWS_AS(OutlierCleaner(cfg, d), loki::ConfigException);
}

// ---------------------------------------------------------------------------
// No-outlier case
// ---------------------------------------------------------------------------

TEST_CASE("OutlierCleaner: clean series is returned unchanged", "[OutlierCleaner]")
{
    auto ts = buildSeries(cleanValues());
    IqrDetector d(1.5);
    OutlierCleaner::Config cfg;
    OutlierCleaner cleaner(cfg, d);

    auto result = cleaner.clean(ts);

    REQUIRE(result.detection.nOutliers == 0);
    REQUIRE(result.cleaned.size() == ts.size());

    // Values must be unchanged
    for (std::size_t i = 0; i < ts.size(); ++i) {
        REQUIRE_THAT(result.cleaned[i].value,
                     WithinAbs(ts[i].value, 1e-12));
    }
}

// ---------------------------------------------------------------------------
// Basic outlier replacement
// ---------------------------------------------------------------------------

TEST_CASE("OutlierCleaner: detects and replaces two outliers", "[OutlierCleaner]")
{
    auto ts = buildSeries(valuesWithOutliers());
    ZScoreDetector d(3.0);
    OutlierCleaner::Config cfg;
    OutlierCleaner cleaner(cfg, d);

    auto result = cleaner.clean(ts);

    REQUIRE(result.detection.nOutliers == 2);

    // Outlier positions must have flag = 2 in cleaned series
    bool found4  = false;
    bool found14 = false;
    for (const auto& pt : result.detection.points) {
        REQUIRE(pt.flag          == 2);
        REQUIRE(!std::isnan(pt.replacedValue));
        if (pt.index == 4)  found4  = true;
        if (pt.index == 14) found14 = true;
    }
    REQUIRE(found4);
    REQUIRE(found14);
}

TEST_CASE("OutlierCleaner: replaced values are finite", "[OutlierCleaner]")
{
    auto ts = buildSeries(valuesWithOutliers());
    ZScoreDetector d(3.0);
    OutlierCleaner cleaner({}, d);

    auto result = cleaner.clean(ts);

    for (std::size_t i = 0; i < result.cleaned.size(); ++i) {
        REQUIRE(!std::isnan(result.cleaned[i].value));
    }
}

TEST_CASE("OutlierCleaner: non-outlier values are preserved", "[OutlierCleaner]")
{
    auto ts = buildSeries(valuesWithOutliers());
    ZScoreDetector d(3.0);
    OutlierCleaner cleaner({}, d);

    auto result = cleaner.clean(ts);

    // Build set of outlier indices
    std::vector<bool> isOutlier(ts.size(), false);
    for (const auto& pt : result.detection.points) {
        isOutlier[pt.index] = true;
    }

    for (std::size_t i = 0; i < ts.size(); ++i) {
        if (!isOutlier[i]) {
            REQUIRE_THAT(result.cleaned[i].value,
                         WithinAbs(ts[i].value, 1e-12));
        }
    }
}

// ---------------------------------------------------------------------------
// Seasonal overload
// ---------------------------------------------------------------------------

TEST_CASE("OutlierCleaner: clean with seasonal component reconstructs correctly", "[OutlierCleaner]")
{
    // Seasonal = constant offset of 10.0
    auto raw = cleanValues();
    const double offset = 10.0;
    std::vector<double> seasonal(raw.size(), offset);
    std::vector<double> combined(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) {
        combined[i] = raw[i] + offset;
    }

    auto ts = buildSeries(combined);
    IqrDetector d(1.5);
    OutlierCleaner cleaner({}, d);

    auto result = cleaner.clean(ts, seasonal);

    REQUIRE(result.detection.nOutliers == 0);

    // Values should be close to original combined values
    for (std::size_t i = 0; i < ts.size(); ++i) {
        REQUIRE_THAT(result.cleaned[i].value,
                     WithinAbs(combined[i], 1e-10));
    }
}

TEST_CASE("OutlierCleaner: outlier in seasonally-adjusted series is detected and fixed", "[OutlierCleaner]")
{
    auto raw = cleanValues();
    const double offset = 5.0;
    std::vector<double> seasonal(raw.size(), offset);

    // Inject outlier in the raw residuals
    raw[7] = 30.0;

    std::vector<double> combined(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) {
        combined[i] = raw[i] + offset;
    }

    auto ts = buildSeries(combined);
    ZScoreDetector d(3.0);
    OutlierCleaner cleaner({}, d);

    auto result = cleaner.clean(ts, seasonal);

    REQUIRE(result.detection.nOutliers >= 1);

    bool found = false;
    for (const auto& pt : result.detection.points) {
        if (pt.index == 7) found = true;
    }
    REQUIRE(found);

    // Reconstructed value at position 7 should not be 30 + 5 = 35
    REQUIRE(result.cleaned[7].value < 20.0);
}

TEST_CASE("OutlierCleaner: seasonal size mismatch throws DataException", "[OutlierCleaner]")
{
    auto ts = buildSeries(cleanValues());
    IqrDetector d;
    OutlierCleaner cleaner({}, d);

    std::vector<double> wrongSeasonal(5, 0.0);  // wrong size
    REQUIRE_THROWS_AS(cleaner.clean(ts, wrongSeasonal), loki::DataException);
}

// ---------------------------------------------------------------------------
// Residuals in result
// ---------------------------------------------------------------------------

TEST_CASE("OutlierCleaner: residuals series has same length as input", "[OutlierCleaner]")
{
    auto ts = buildSeries(valuesWithOutliers());
    ZScoreDetector d(3.0);
    OutlierCleaner cleaner({}, d);

    auto result = cleaner.clean(ts);

    REQUIRE(result.residuals.size() == ts.size());
}

TEST_CASE("OutlierCleaner: residuals without seasonal equal original values", "[OutlierCleaner]")
{
    auto ts = buildSeries(cleanValues());
    IqrDetector d;
    OutlierCleaner cleaner({}, d);

    auto result = cleaner.clean(ts);

    for (std::size_t i = 0; i < ts.size(); ++i) {
        REQUIRE_THAT(result.residuals[i].value,
                     WithinAbs(ts[i].value, 1e-12));
    }
}

// ---------------------------------------------------------------------------
// Metadata
// ---------------------------------------------------------------------------

TEST_CASE("OutlierCleaner: cleaned series name contains _cleaned suffix", "[OutlierCleaner]")
{
    auto ts = buildSeries(cleanValues());
    IqrDetector d;
    OutlierCleaner cleaner({}, d);

    auto result = cleaner.clean(ts);

    const std::string& name = result.cleaned.metadata().componentName;
    REQUIRE(name.find("_cleaned") != std::string::npos);
}

TEST_CASE("OutlierCleaner: residuals series name contains _residuals suffix", "[OutlierCleaner]")
{
    auto ts = buildSeries(cleanValues());
    IqrDetector d;
    OutlierCleaner cleaner({}, d);

    auto result = cleaner.clean(ts);

    const std::string& name = result.residuals.metadata().componentName;
    REQUIRE(name.find("_residuals") != std::string::npos);
}
