#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <loki/filter/movingAverageFilter.hpp>
#include <loki/filter/emaFilter.hpp>
#include <loki/filter/weightedMovingAverageFilter.hpp>
#include <loki/filter/kernelSmoother.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/timeseries/timeSeries.hpp>
#include <loki/timeseries/timeStamp.hpp>

#include <cmath>
#include <vector>

using namespace loki;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------

/**
 * Build a simple TimeSeries from a vector of values.
 * Timestamps are synthetic: MJD 0, 1, 2, ... (daily spacing).
 */
static TimeSeries makeSeries(const std::vector<double>& values)
{
    SeriesMetadata meta;
    meta.componentName = "test";
    TimeSeries ts{meta};
    for (std::size_t i = 0; i < values.size(); ++i) {
        ts.append({::TimeStamp::fromMjd(static_cast<double>(i)), values[i]});
    }
    return ts;
}

/** Extract filtered values from a FilterResult into a plain vector. */
static std::vector<double> filteredValues(const FilterResult& r)
{
    const auto& obs = r.filtered.observations();
    std::vector<double> out;
    out.reserve(obs.size());
    for (const auto& o : obs) {
        out.push_back(o.value);
    }
    return out;
}

/** Extract residual values from a FilterResult into a plain vector. */
static std::vector<double> residualValues(const FilterResult& r)
{
    const auto& obs = r.residuals.observations();
    std::vector<double> out;
    out.reserve(obs.size());
    for (const auto& o : obs) {
        out.push_back(o.value);
    }
    return out;
}

static constexpr double EPS = 1.0e-9;

// ===========================================================================
// MovingAverageFilter
// ===========================================================================

TEST_CASE("MovingAverageFilter: constant series stays constant", "[filter][ma]")
{
    // A constant series must be unchanged by any symmetric filter.
    const std::vector<double> vals(20, 5.0);
    MovingAverageFilter f{MovingAverageFilter::Config{.window = 5}};
    auto result = f.apply(makeSeries(vals));
    auto out = filteredValues(result);

    REQUIRE(out.size() == vals.size());
    for (std::size_t i = 0; i < out.size(); ++i) {
        REQUIRE_THAT(out[i], WithinAbs(5.0, EPS));
    }
}

TEST_CASE("MovingAverageFilter: output same length as input", "[filter][ma]")
{
    MovingAverageFilter f{MovingAverageFilter::Config{.window = 3}};
    auto result = f.apply(makeSeries({1.0, 2.0, 3.0, 4.0, 5.0}));
    REQUIRE(result.filtered.observations().size() == 5);
    REQUIRE(result.residuals.observations().size() == 5);
}

TEST_CASE("MovingAverageFilter: no NaN in output after edge fill", "[filter][ma]")
{
    MovingAverageFilter f{MovingAverageFilter::Config{.window = 5}};
    auto result = f.apply(makeSeries({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0}));
    for (const auto& o : result.filtered.observations()) {
        REQUIRE_FALSE(std::isnan(o.value));
    }
}

TEST_CASE("MovingAverageFilter: residuals = original - filtered", "[filter][ma]")
{
    const std::vector<double> vals = {1.0, 3.0, 2.0, 5.0, 4.0, 6.0, 3.0, 7.0};
    MovingAverageFilter f{MovingAverageFilter::Config{.window = 3}};
    auto ts = makeSeries(vals);
    auto result = f.apply(ts);
    auto filt = filteredValues(result);
    auto res  = residualValues(result);

    for (std::size_t i = 0; i < vals.size(); ++i) {
        REQUIRE_THAT(res[i], WithinAbs(vals[i] - filt[i], EPS));
    }
}

TEST_CASE("MovingAverageFilter: interior points computed correctly", "[filter][ma]")
{
    // window=3: interior y[i] = (x[i-1] + x[i] + x[i+1]) / 3
    const std::vector<double> vals = {1.0, 2.0, 3.0, 4.0, 5.0};
    MovingAverageFilter f{MovingAverageFilter::Config{.window = 3}};
    auto result = f.apply(makeSeries(vals));
    auto out = filteredValues(result);

    // Interior points: indices 1, 2, 3
    REQUIRE_THAT(out[1], WithinAbs((1.0 + 2.0 + 3.0) / 3.0, EPS));
    REQUIRE_THAT(out[2], WithinAbs((2.0 + 3.0 + 4.0) / 3.0, EPS));
    REQUIRE_THAT(out[3], WithinAbs((3.0 + 4.0 + 5.0) / 3.0, EPS));
}

TEST_CASE("MovingAverageFilter: name is correct", "[filter][ma]")
{
    MovingAverageFilter f{MovingAverageFilter::Config{.window = 3}};
    REQUIRE(f.name() == "MovingAverage");
}

TEST_CASE("MovingAverageFilter: empty series throws DataException", "[filter][ma]")
{
    MovingAverageFilter f{MovingAverageFilter::Config{.window = 3}};
    REQUIRE_THROWS_AS(f.apply(makeSeries({})), DataException);
}

// ===========================================================================
// EmaFilter
// ===========================================================================

TEST_CASE("EmaFilter: constant series stays constant", "[filter][ema]")
{
    const std::vector<double> vals(15, 3.0);
    EmaFilter f{EmaFilter::Config{.alpha = 0.3}};
    auto result = f.apply(makeSeries(vals));
    auto out = filteredValues(result);

    REQUIRE(out.size() == vals.size());
    for (std::size_t i = 0; i < out.size(); ++i) {
        REQUIRE_THAT(out[i], WithinAbs(3.0, EPS));
    }
}

TEST_CASE("EmaFilter: output same length as input", "[filter][ema]")
{
    EmaFilter f{EmaFilter::Config{.alpha = 0.2}};
    auto result = f.apply(makeSeries({1.0, 2.0, 3.0, 4.0, 5.0}));
    REQUIRE(result.filtered.observations().size() == 5);
}

TEST_CASE("EmaFilter: no NaN in output", "[filter][ema]")
{
    EmaFilter f{EmaFilter::Config{.alpha = 0.5}};
    auto result = f.apply(makeSeries({1.0, 2.0, 3.0, 4.0, 5.0, 6.0}));
    for (const auto& o : result.filtered.observations()) {
        REQUIRE_FALSE(std::isnan(o.value));
    }
}

TEST_CASE("EmaFilter: alpha=1.0 returns copy of input", "[filter][ema]")
{
    // alpha=1 -> y[i] = x[i] exactly
    const std::vector<double> vals = {1.0, 5.0, 2.0, 8.0, 3.0};
    EmaFilter f{EmaFilter::Config{.alpha = 1.0}};
    auto result = f.apply(makeSeries(vals));
    auto out = filteredValues(result);

    for (std::size_t i = 0; i < vals.size(); ++i) {
        REQUIRE_THAT(out[i], WithinAbs(vals[i], EPS));
    }
}

TEST_CASE("EmaFilter: residuals = original - filtered", "[filter][ema]")
{
    const std::vector<double> vals = {2.0, 4.0, 6.0, 8.0, 10.0};
    EmaFilter f{EmaFilter::Config{.alpha = 0.4}};
    auto ts = makeSeries(vals);
    auto result = f.apply(ts);
    auto filt = filteredValues(result);
    auto res  = residualValues(result);

    for (std::size_t i = 0; i < vals.size(); ++i) {
        REQUIRE_THAT(res[i], WithinAbs(vals[i] - filt[i], EPS));
    }
}

TEST_CASE("EmaFilter: recurrence relation holds", "[filter][ema]")
{
    // y[0] = x[0], y[i] = alpha*x[i] + (1-alpha)*y[i-1]
    const std::vector<double> vals = {1.0, 3.0, 5.0, 7.0};
    const double alpha = 0.3;
    EmaFilter f{EmaFilter::Config{.alpha = alpha}};
    auto result = f.apply(makeSeries(vals));
    auto out = filteredValues(result);

    double expected = vals[0];
    REQUIRE_THAT(out[0], WithinAbs(expected, EPS));
    for (std::size_t i = 1; i < vals.size(); ++i) {
        expected = alpha * vals[i] + (1.0 - alpha) * expected;
        REQUIRE_THAT(out[i], WithinAbs(expected, EPS));
    }
}

TEST_CASE("EmaFilter: name is correct", "[filter][ema]")
{
    EmaFilter f{EmaFilter::Config{.alpha = 0.1}};
    REQUIRE(f.name() == "EMA");
}

TEST_CASE("EmaFilter: empty series throws DataException", "[filter][ema]")
{
    EmaFilter f{EmaFilter::Config{.alpha = 0.2}};
    REQUIRE_THROWS_AS(f.apply(makeSeries({})), DataException);
}

// ===========================================================================
// WeightedMovingAverageFilter
// ===========================================================================

TEST_CASE("WeightedMovingAverageFilter: constant series stays constant", "[filter][wma]")
{
    const std::vector<double> vals(20, 4.0);
    WeightedMovingAverageFilter f{WeightedMovingAverageFilter::Config{.weights = {1.0, 2.0, 1.0}}};
    auto result = f.apply(makeSeries(vals));
    auto out = filteredValues(result);

    REQUIRE(out.size() == vals.size());
    for (std::size_t i = 0; i < out.size(); ++i) {
        REQUIRE_THAT(out[i], WithinAbs(4.0, EPS));
    }
}

TEST_CASE("WeightedMovingAverageFilter: no NaN in output after edge fill", "[filter][wma]")
{
    WeightedMovingAverageFilter f{WeightedMovingAverageFilter::Config{
        .weights = {1.0, 2.0, 3.0, 2.0, 1.0}}};
    auto result = f.apply(makeSeries({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0}));
    for (const auto& o : result.filtered.observations()) {
        REQUIRE_FALSE(std::isnan(o.value));
    }
}

TEST_CASE("WeightedMovingAverageFilter: residuals = original - filtered", "[filter][wma]")
{
    const std::vector<double> vals = {2.0, 4.0, 6.0, 8.0, 10.0, 8.0, 6.0, 4.0};
    WeightedMovingAverageFilter f{WeightedMovingAverageFilter::Config{
        .weights = {1.0, 2.0, 1.0}}};
    auto ts = makeSeries(vals);
    auto result = f.apply(ts);
    auto filt = filteredValues(result);
    auto res  = residualValues(result);

    for (std::size_t i = 0; i < vals.size(); ++i) {
        REQUIRE_THAT(res[i], WithinAbs(vals[i] - filt[i], EPS));
    }
}

TEST_CASE("WeightedMovingAverageFilter: uniform weights == simple MA", "[filter][wma]")
{
    // WMA with uniform weights {1,1,1} should match MA with window=3 on interior points.
    const std::vector<double> vals = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0};
    MovingAverageFilter ma{MovingAverageFilter::Config{.window = 3}};
    WeightedMovingAverageFilter wma{WeightedMovingAverageFilter::Config{
        .weights = {1.0, 1.0, 1.0}}};

    auto rMA  = filteredValues(ma.apply(makeSeries(vals)));
    auto rWMA = filteredValues(wma.apply(makeSeries(vals)));

    // Interior points only (edge fill strategies may differ slightly at boundaries)
    for (std::size_t i = 1; i + 1 < vals.size(); ++i) {
        REQUIRE_THAT(rWMA[i], WithinAbs(rMA[i], EPS));
    }
}

TEST_CASE("WeightedMovingAverageFilter: empty weights throws ConfigException", "[filter][wma]")
{
    REQUIRE_THROWS_AS(
        WeightedMovingAverageFilter{WeightedMovingAverageFilter::Config{.weights = {}}},
        ConfigException);
}

TEST_CASE("WeightedMovingAverageFilter: name is correct", "[filter][wma]")
{
    WeightedMovingAverageFilter f{WeightedMovingAverageFilter::Config{.weights = {1.0, 2.0, 1.0}}};
    REQUIRE(f.name() == "WeightedMovingAverage");
}

// ===========================================================================
// KernelSmoother
// ===========================================================================

TEST_CASE("KernelSmoother: constant series stays constant (Epanechnikov)", "[filter][kernel]")
{
    const std::vector<double> vals(30, 7.0);
    KernelSmoother f{KernelSmoother::Config{
        .bandwidth = 0.2,
        .kernel    = KernelSmoother::Kernel::EPANECHNIKOV}};
    auto result = f.apply(makeSeries(vals));
    auto out = filteredValues(result);

    REQUIRE(out.size() == vals.size());
    for (std::size_t i = 0; i < out.size(); ++i) {
        REQUIRE_THAT(out[i], WithinAbs(7.0, 1.0e-6));
    }
}

TEST_CASE("KernelSmoother: constant series stays constant (Gaussian)", "[filter][kernel]")
{
    const std::vector<double> vals(30, 2.5);
    KernelSmoother f{KernelSmoother::Config{
        .bandwidth      = 0.2,
        .kernel         = KernelSmoother::Kernel::GAUSSIAN,
        .gaussianCutoff = 3.0}};
    auto result = f.apply(makeSeries(vals));
    auto out = filteredValues(result);

    for (std::size_t i = 0; i < out.size(); ++i) {
        REQUIRE_THAT(out[i], WithinAbs(2.5, 1.0e-6));
    }
}

TEST_CASE("KernelSmoother: output same length as input", "[filter][kernel]")
{
    KernelSmoother f{KernelSmoother::Config{.bandwidth = 0.3}};
    auto result = f.apply(makeSeries({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0}));
    REQUIRE(result.filtered.observations().size() == 10);
    REQUIRE(result.residuals.observations().size() == 10);
}

TEST_CASE("KernelSmoother: no NaN in output", "[filter][kernel]")
{
    for (auto k : {KernelSmoother::Kernel::EPANECHNIKOV,
                   KernelSmoother::Kernel::GAUSSIAN,
                   KernelSmoother::Kernel::UNIFORM,
                   KernelSmoother::Kernel::TRIANGULAR}) {
        KernelSmoother f{KernelSmoother::Config{.bandwidth = 0.2, .kernel = k}};
        auto result = f.apply(makeSeries({1.0, 3.0, 2.0, 5.0, 4.0, 6.0, 3.0, 7.0, 5.0, 8.0}));
        for (const auto& o : result.filtered.observations()) {
            REQUIRE_FALSE(std::isnan(o.value));
        }
    }
}

TEST_CASE("KernelSmoother: residuals = original - filtered", "[filter][kernel]")
{
    const std::vector<double> vals = {1.0, 3.0, 2.0, 5.0, 4.0, 6.0, 3.0, 7.0, 5.0, 8.0};
    KernelSmoother f{KernelSmoother::Config{.bandwidth = 0.3}};
    auto ts = makeSeries(vals);
    auto result = f.apply(ts);
    auto filt = filteredValues(result);
    auto res  = residualValues(result);

    for (std::size_t i = 0; i < vals.size(); ++i) {
        REQUIRE_THAT(res[i], WithinAbs(vals[i] - filt[i], EPS));
    }
}

TEST_CASE("KernelSmoother: smoother output bounded by input range", "[filter][kernel]")
{
    // A kernel smoother is a weighted average -- output must lie within [min, max] of input.
    const std::vector<double> vals = {1.0, 5.0, 2.0, 8.0, 3.0, 7.0, 4.0, 6.0, 9.0, 2.0};
    const double vMin = *std::min_element(vals.begin(), vals.end());
    const double vMax = *std::max_element(vals.begin(), vals.end());

    KernelSmoother f{KernelSmoother::Config{.bandwidth = 0.4}};
    auto result = f.apply(makeSeries(vals));

    for (const auto& o : result.filtered.observations()) {
        REQUIRE(o.value >= vMin - EPS);
        REQUIRE(o.value <= vMax + EPS);
    }
}

TEST_CASE("KernelSmoother: large bandwidth produces very smooth output", "[filter][kernel]")
{
    // With bandwidth close to 1.0, the smoother approaches the global mean.
    const std::vector<double> vals = {1.0, 9.0, 2.0, 8.0, 3.0, 7.0, 4.0, 6.0, 5.0, 5.0};
    double mean = 0.0;
    for (double v : vals) mean += v;
    mean /= static_cast<double>(vals.size());

    KernelSmoother f{KernelSmoother::Config{
        .bandwidth = 0.99,
        .kernel    = KernelSmoother::Kernel::EPANECHNIKOV}};
    auto result = f.apply(makeSeries(vals));
    auto out = filteredValues(result);

    // All output values should be close to the global mean.
    for (std::size_t i = 0; i < out.size(); ++i) {
        REQUIRE_THAT(out[i], WithinAbs(mean, 0.5));
    }
}

TEST_CASE("KernelSmoother: invalid bandwidth throws ConfigException", "[filter][kernel]")
{
    REQUIRE_THROWS_AS(
        KernelSmoother{KernelSmoother::Config{.bandwidth = 0.0}},
        ConfigException);
    REQUIRE_THROWS_AS(
        KernelSmoother{KernelSmoother::Config{.bandwidth = 1.0}},
        ConfigException);
    REQUIRE_THROWS_AS(
        KernelSmoother{KernelSmoother::Config{.bandwidth = -0.1}},
        ConfigException);
}

TEST_CASE("KernelSmoother: empty series throws DataException", "[filter][kernel]")
{
    KernelSmoother f{KernelSmoother::Config{.bandwidth = 0.2}};
    REQUIRE_THROWS_AS(f.apply(makeSeries({})), DataException);
}

TEST_CASE("KernelSmoother: name contains kernel type", "[filter][kernel]")
{
    REQUIRE(KernelSmoother{KernelSmoother::Config{
        .kernel = KernelSmoother::Kernel::EPANECHNIKOV}}.name() == "KernelSmoother(Epanechnikov)");
    REQUIRE(KernelSmoother{KernelSmoother::Config{
        .kernel = KernelSmoother::Kernel::GAUSSIAN}}.name()     == "KernelSmoother(Gaussian)");
    REQUIRE(KernelSmoother{KernelSmoother::Config{
        .kernel = KernelSmoother::Kernel::UNIFORM}}.name()      == "KernelSmoother(Uniform)");
    REQUIRE(KernelSmoother{KernelSmoother::Config{
        .kernel = KernelSmoother::Kernel::TRIANGULAR}}.name()   == "KernelSmoother(Triangular)");
}
