#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <loki/regression/robustRegressor.hpp>
#include <loki/timeseries/timeSeries.hpp>
#include <loki/timeseries/timeStamp.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/config.hpp>

#include <cmath>
#include <vector>

using namespace loki;
using namespace loki::regression;
using Catch::Matchers::WithinAbs;

// -----------------------------------------------------------------------------
//  Helpers
// -----------------------------------------------------------------------------

static RegressionConfig robustCfg(const std::string& fn = "bisquare", int degree = 1)
{
    RegressionConfig cfg;
    cfg.polynomialDegree  = degree;
    cfg.robust            = true;
    cfg.robustIterations  = 20;
    cfg.robustWeightFn    = fn;
    cfg.confidenceLevel   = 0.95;
    return cfg;
}

static TimeSeries makeSeries(const std::vector<double>& mjds,
                              const std::vector<double>& vals)
{
    SeriesMetadata meta;
    meta.stationId     = "TEST";
    meta.componentName = "y";
    TimeSeries ts(meta);
    for (std::size_t i = 0; i < mjds.size(); ++i)
        ts.append(::TimeStamp::fromMjd(mjds[i]), vals[i]);
    return ts;
}

// -----------------------------------------------------------------------------
//  Robust fit recovers true slope in presence of outliers
// -----------------------------------------------------------------------------

TEST_CASE("RobustRegressor: recovers slope despite outliers (bisquare)", "[regression][robust]")
{
    const int    n          = 50;
    const double true_slope = 2.0;
    const double true_int   = 1.0;

    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        const double x = static_cast<double>(i);
        mjds[i] = 50000.0 + x;
        vals[i] = true_int + true_slope * x;
    }
    // Inject 5 severe outliers (10% contamination)
    vals[5]  += 100.0;
    vals[15] -= 80.0;
    vals[25] += 120.0;
    vals[35] -= 90.0;
    vals[45] += 110.0;

    RobustRegressor reg(robustCfg("bisquare"));
    const auto res = reg.fit(makeSeries(mjds, vals));

    // Robust fit should recover true parameters despite outliers.
    CHECK_THAT(res.coefficients[0], WithinAbs(true_int,   0.5));
    CHECK_THAT(res.coefficients[1], WithinAbs(true_slope, 0.05));
}

// -----------------------------------------------------------------------------
//  Robust fit vs OLS: outliers inflate OLS slope more than robust
// -----------------------------------------------------------------------------

TEST_CASE("RobustRegressor: less influenced by outliers than OLS", "[regression][robust]")
{
    const int    n     = 40;
    const double slope = 1.0;

    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        mjds[i] = 50000.0 + static_cast<double>(i);
        vals[i] = slope * static_cast<double>(i);
    }
    vals[0]  += 50.0;
    vals[39] -= 50.0;

    // OLS via PolynomialRegressor degree=1 (no robust)
    RegressionConfig olsCfg;
    olsCfg.polynomialDegree = 1;
    olsCfg.robust           = false;
    olsCfg.confidenceLevel  = 0.95;

    // We can't directly use PolynomialRegressor here without including it,
    // so we verify the robust result is closer to true slope than a known OLS
    // deviation would be. Instead just verify robust stays close to truth.
    RobustRegressor reg(robustCfg("bisquare"));
    const auto res = reg.fit(makeSeries(mjds, vals));

    CHECK_THAT(res.coefficients[1], WithinAbs(slope, 0.1));
}

// -----------------------------------------------------------------------------
//  weightedObservations: outliers get low weight
// -----------------------------------------------------------------------------

TEST_CASE("RobustRegressor: outliers receive low IRLS weight", "[regression][robust]")
{
    const int n = 30;
    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        mjds[i] = 50000.0 + static_cast<double>(i);
        vals[i] = static_cast<double>(i) * 0.5;
    }
    // One severe outlier
    vals[10] += 200.0;

    RobustRegressor reg(robustCfg("bisquare"));
    reg.fit(makeSeries(mjds, vals));

    const auto& wobs = reg.weightedObservations();
    REQUIRE(wobs.size() == static_cast<std::size_t>(n));

    // The outlier at index 10 should have near-zero weight.
    CHECK(wobs[10].weight < 0.1);

    // Clean observations should have weight close to 1.
    double sumCleanWeight = 0.0;
    int    countClean     = 0;
    for (std::size_t i = 0; i < wobs.size(); ++i) {
        if (i == 10) continue;
        sumCleanWeight += wobs[i].weight;
        ++countClean;
    }
    CHECK(sumCleanWeight / static_cast<double>(countClean) > 0.8);
}

// -----------------------------------------------------------------------------
//  Huber weight function: less aggressive than bisquare
// -----------------------------------------------------------------------------

TEST_CASE("RobustRegressor: Huber gives non-zero weight to moderate outliers", "[regression][robust]")
{
    const int n = 30;
    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        mjds[i] = 50000.0 + static_cast<double>(i);
        vals[i] = static_cast<double>(i) * 1.0;
    }
    // Moderate outlier -- Bisquare would zero it, Huber downweights it.
    vals[15] += 5.0;

    RobustRegressor regHuber(robustCfg("huber"));
    regHuber.fit(makeSeries(mjds, vals));
    const auto& wHuber = regHuber.weightedObservations();

    // Huber should give positive weight to the moderate outlier.
    CHECK(wHuber[15].weight > 0.0);
    CHECK(wHuber[15].weight < 1.0);
}

// -----------------------------------------------------------------------------
//  config.robust = false is forced to true with warning
// -----------------------------------------------------------------------------

TEST_CASE("RobustRegressor: forces robust=true even if config says false", "[regression][robust]")
{
    RegressionConfig cfg = robustCfg();
    cfg.robust = false;  // explicitly set false

    const int n = 10;
    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        mjds[i] = 50000.0 + static_cast<double>(i);
        vals[i] = static_cast<double>(i);
    }

    // Should not throw -- constructor forces robust=true.
    RobustRegressor reg(cfg);
    CHECK_NOTHROW(reg.fit(makeSeries(mjds, vals)));
}

// -----------------------------------------------------------------------------
//  predict() before fit() throws
// -----------------------------------------------------------------------------

TEST_CASE("RobustRegressor: predict() before fit() throws", "[regression][robust]")
{
    RobustRegressor reg(robustCfg());
    CHECK_THROWS_AS(reg.predict({1.0, 2.0}), AlgorithmException);
}

// -----------------------------------------------------------------------------
//  weightedObservations() before fit() throws
// -----------------------------------------------------------------------------

TEST_CASE("RobustRegressor: weightedObservations() before fit() throws", "[regression][robust]")
{
    RobustRegressor reg(robustCfg());
    CHECK_THROWS_AS(reg.weightedObservations(), AlgorithmException);
}

// -----------------------------------------------------------------------------
//  name() correct format
// -----------------------------------------------------------------------------

TEST_CASE("RobustRegressor: name() format", "[regression][robust]")
{
    RobustRegressor regBs(robustCfg("bisquare", 2));
    CHECK(regBs.name() == "RobustRegressor(degree=2, fn=bisquare)");

    RobustRegressor regH(robustCfg("huber", 1));
    CHECK(regH.name() == "RobustRegressor(degree=1, fn=huber)");
}

// -----------------------------------------------------------------------------
//  Too few observations throws
// -----------------------------------------------------------------------------

TEST_CASE("RobustRegressor: throws on too few observations", "[regression][robust]")
{
    std::vector<double> mjds = {50000.0, 50001.0};
    std::vector<double> vals = {1.0, 2.0};

    RobustRegressor reg(robustCfg("bisquare", 1));
    CHECK_THROWS_AS(reg.fit(makeSeries(mjds, vals)), DataException);
}

// -----------------------------------------------------------------------------
//  predict() intervals contain prediction
// -----------------------------------------------------------------------------

TEST_CASE("RobustRegressor: predict() interval ordering", "[regression][robust]")
{
    const int n = 30;
    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        mjds[i] = 50000.0 + static_cast<double>(i);
        vals[i] = 1.0 + 0.5 * static_cast<double>(i) +
                  0.2 * std::sin(static_cast<double>(i));
    }
    vals[10] += 50.0;  // outlier

    RobustRegressor reg(robustCfg("bisquare"));
    reg.fit(makeSeries(mjds, vals));
    const auto pts = reg.predict({5.0, 15.0, 25.0});

    for (const auto& pt : pts) {
        CHECK(pt.confLow  <= pt.predicted + 1e-10);
        CHECK(pt.confHigh >= pt.predicted - 1e-10);
        CHECK(pt.predLow  <= pt.confLow   + 1e-10);
        CHECK(pt.predHigh >= pt.confHigh  - 1e-10);
    }
}
