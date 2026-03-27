#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <loki/regression/linearRegressor.hpp>
#include <loki/regression/regressionResult.hpp>
#include <loki/timeseries/timeSeries.hpp>
#include <loki/timeseries/timeStamp.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/config.hpp>

#include <cmath>
#include <numbers>
#include <vector>

using namespace loki;
using namespace loki::regression;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

// -----------------------------------------------------------------------------
//  Helpers
// -----------------------------------------------------------------------------

static RegressionConfig defaultCfg()
{
    RegressionConfig cfg;
    cfg.confidenceLevel = 0.95;
    cfg.robust          = false;
    return cfg;
}

// Build a TimeSeries from parallel MJD and value vectors.
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
//  Basic fit -- perfect linear data y = 2 + 3*x (x in days from first obs)
// -----------------------------------------------------------------------------

TEST_CASE("LinearRegressor: perfect linear fit", "[regression][linear]")
{
    // x = 0, 1, 2, ..., 9 days from MJD 50000
    const int n = 10;
    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        mjds[i] = 50000.0 + static_cast<double>(i);
        vals[i] = 2.0 + 3.0 * static_cast<double>(i);  // a0=2, a1=3
    }

    LinearRegressor reg(defaultCfg());
    const RegressionResult res = reg.fit(makeSeries(mjds, vals));

    REQUIRE(res.coefficients.size() == 2);
    CHECK_THAT(res.coefficients[0], WithinAbs(2.0, 1e-8));  // intercept
    CHECK_THAT(res.coefficients[1], WithinAbs(3.0, 1e-8));  // slope
    CHECK_THAT(res.rSquared,        WithinAbs(1.0, 1e-8));
    CHECK_THAT(res.rSquaredAdj,     WithinAbs(1.0, 1e-8));
    CHECK(res.dof == n - 2);
    CHECK(res.converged);
    CHECK(res.modelName == "LinearRegressor");
    CHECK_THAT(res.tRef, WithinAbs(50000.0, 1e-10));
}

// -----------------------------------------------------------------------------
//  Residuals sum to zero for unweighted OLS with intercept term
// -----------------------------------------------------------------------------

TEST_CASE("LinearRegressor: residuals sum to zero", "[regression][linear]")
{
    const int n = 20;
    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        mjds[i] = 51000.0 + static_cast<double>(i);
        // Add some noise to make it non-trivial
        vals[i] = 1.5 + 0.7 * static_cast<double>(i) +
                  0.1 * std::sin(static_cast<double>(i));
    }

    LinearRegressor reg(defaultCfg());
    const RegressionResult res = reg.fit(makeSeries(mjds, vals));

    const double residualSum = res.residuals.sum();
    CHECK_THAT(residualSum, WithinAbs(0.0, 1e-8));
}

// -----------------------------------------------------------------------------
//  Fitted values match A * coefficients
// -----------------------------------------------------------------------------

TEST_CASE("LinearRegressor: fitted values consistent with coefficients", "[regression][linear]")
{
    const int n = 15;
    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        mjds[i] = 52000.0 + static_cast<double>(i);
        vals[i] = -1.0 + 0.5 * static_cast<double>(i);
    }

    LinearRegressor reg(defaultCfg());
    const RegressionResult res = reg.fit(makeSeries(mjds, vals));

    REQUIRE(static_cast<int>(res.fitted.size()) == n);
    const double a0 = res.coefficients[0];
    const double a1 = res.coefficients[1];

    for (int i = 0; i < n; ++i) {
        const double x        = mjds[i] - res.tRef;
        const double expected = a0 + a1 * x;
        CHECK_THAT(res.fitted[static_cast<std::size_t>(i)].value,
                   WithinAbs(expected, 1e-8));
    }
}

// -----------------------------------------------------------------------------
//  R^2 = 1 for perfect fit, R^2 < 1 for noisy data
// -----------------------------------------------------------------------------

TEST_CASE("LinearRegressor: R^2 values", "[regression][linear]")
{
    SECTION("perfect fit gives R^2 = 1") {
        const int n = 10;
        std::vector<double> mjds(n), vals(n);
        for (int i = 0; i < n; ++i) {
            mjds[i] = 50000.0 + static_cast<double>(i);
            vals[i] = 5.0 - 2.0 * static_cast<double>(i);
        }
        LinearRegressor reg(defaultCfg());
        const auto res = reg.fit(makeSeries(mjds, vals));
        CHECK_THAT(res.rSquared, WithinAbs(1.0, 1e-8));
    }

    SECTION("noisy data gives R^2 < 1") {
        const int n = 30;
        std::vector<double> mjds(n), vals(n);
        for (int i = 0; i < n; ++i) {
            mjds[i] = 50000.0 + static_cast<double>(i);
            // Pure noise -- no linear trend
            vals[i] = std::sin(static_cast<double>(i) * 1.3);
        }
        LinearRegressor reg(defaultCfg());
        const auto res = reg.fit(makeSeries(mjds, vals));
        CHECK(res.rSquared < 1.0);
        CHECK(res.rSquared >= 0.0);
    }
}

// -----------------------------------------------------------------------------
//  AIC < BIC for small n (standard property)
// -----------------------------------------------------------------------------

TEST_CASE("LinearRegressor: AIC and BIC are finite and positive", "[regression][linear]")
{
    const int n = 20;
    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        mjds[i] = 50000.0 + static_cast<double>(i);
        vals[i] = 1.0 + 0.1 * static_cast<double>(i) +
                  0.05 * std::cos(static_cast<double>(i));
    }
    LinearRegressor reg(defaultCfg());
    const auto res = reg.fit(makeSeries(mjds, vals));

    CHECK(std::isfinite(res.aic));
    CHECK(std::isfinite(res.bic));
    // BIC >= AIC for n >= e^2 ~ 7 (log(n)*k >= 2*k)
    CHECK(res.bic >= res.aic);
}

// -----------------------------------------------------------------------------
//  NaN observations are skipped
// -----------------------------------------------------------------------------

TEST_CASE("LinearRegressor: NaN observations skipped", "[regression][linear]")
{
    // Perfect line y = 1 + 2*x with two NaN values inserted
    const int n = 12;
    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        mjds[i] = 50000.0 + static_cast<double>(i);
        vals[i] = 1.0 + 2.0 * static_cast<double>(i);
    }
    vals[3] = std::numeric_limits<double>::quiet_NaN();
    vals[8] = std::numeric_limits<double>::quiet_NaN();

    LinearRegressor reg(defaultCfg());
    const RegressionResult res = reg.fit(makeSeries(mjds, vals));

    // Should still recover the true coefficients from remaining 10 points.
    CHECK_THAT(res.coefficients[0], WithinAbs(1.0, 1e-6));
    CHECK_THAT(res.coefficients[1], WithinAbs(2.0, 1e-6));
    CHECK(res.dof == 10 - 2);
    CHECK(static_cast<int>(res.fitted.size()) == 10);
}

// -----------------------------------------------------------------------------
//  tRef is MJD of first valid observation
// -----------------------------------------------------------------------------

TEST_CASE("LinearRegressor: tRef is MJD of first valid observation", "[regression][linear]")
{
    std::vector<double> mjds = {50000.0, 50001.0, 50002.0, 50003.0, 50004.0};
    std::vector<double> vals = {1.0, 2.0, 3.0, 4.0, 5.0};
    vals[0] = std::numeric_limits<double>::quiet_NaN();  // first is NaN

    LinearRegressor reg(defaultCfg());
    const auto res = reg.fit(makeSeries(mjds, vals));

    // tRef should be MJD of first valid obs = 50001.0
    CHECK_THAT(res.tRef, WithinAbs(50001.0, 1e-10));
}

// -----------------------------------------------------------------------------
//  Too few observations throws DataException
// -----------------------------------------------------------------------------

TEST_CASE("LinearRegressor: throws on too few observations", "[regression][linear]")
{
    // Only 2 valid points -- not enough for 2 params + 1
    std::vector<double> mjds = {50000.0, 50001.0};
    std::vector<double> vals = {1.0, 2.0};

    LinearRegressor reg(defaultCfg());
    CHECK_THROWS_AS(reg.fit(makeSeries(mjds, vals)), DataException);
}

// -----------------------------------------------------------------------------
//  predict() -- point estimates match fit on training x
// -----------------------------------------------------------------------------

TEST_CASE("LinearRegressor: predict() matches fitted values on training data", "[regression][linear]")
{
    const int n = 10;
    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        mjds[i] = 50000.0 + static_cast<double>(i);
        vals[i] = 3.0 + 1.5 * static_cast<double>(i);
    }

    LinearRegressor reg(defaultCfg());
    const auto res = reg.fit(makeSeries(mjds, vals));

    // predict at training x values
    std::vector<double> xNew;
    for (int i = 0; i < n; ++i)
        xNew.push_back(mjds[i] - res.tRef);

    const auto pts = reg.predict(xNew);
    REQUIRE(static_cast<int>(pts.size()) == n);

    for (int i = 0; i < n; ++i) {
        CHECK_THAT(pts[i].predicted,
                   WithinAbs(res.fitted[static_cast<std::size_t>(i)].value, 1e-8));
    }
}

// -----------------------------------------------------------------------------
//  predict() -- confidence interval contains predicted value
// -----------------------------------------------------------------------------

TEST_CASE("LinearRegressor: predict() confidence interval contains prediction", "[regression][linear]")
{
    const int n = 20;
    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        mjds[i] = 50000.0 + static_cast<double>(i);
        vals[i] = 2.0 + 0.5 * static_cast<double>(i) +
                  0.1 * std::sin(static_cast<double>(i));
    }

    LinearRegressor reg(defaultCfg());
    const auto res = reg.fit(makeSeries(mjds, vals));

    std::vector<double> xNew = {5.0, 10.0, 15.0, 20.0};  // some x values
    const auto pts = reg.predict(xNew);

    for (const auto& pt : pts) {
        CHECK(pt.confLow  <= pt.predicted + 1e-10);
        CHECK(pt.confHigh >= pt.predicted - 1e-10);
        CHECK(pt.predLow  <= pt.confLow  + 1e-10);
        CHECK(pt.predHigh >= pt.confHigh - 1e-10);
    }
}

// -----------------------------------------------------------------------------
//  predict() before fit() throws AlgorithmException
// -----------------------------------------------------------------------------

TEST_CASE("LinearRegressor: predict() before fit() throws", "[regression][linear]")
{
    LinearRegressor reg(defaultCfg());
    CHECK_THROWS_AS(reg.predict({1.0, 2.0}), AlgorithmException);
}

// -----------------------------------------------------------------------------
//  Sub-day (ms-resolution) data -- tRef precision
// -----------------------------------------------------------------------------

TEST_CASE("LinearRegressor: ms-resolution data handled correctly", "[regression][linear]")
{
    // Simulate ms data: 10 points spaced 0.001 days (~86 seconds) apart
    const int n = 10;
    const double dtDays = 0.001;
    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        mjds[i] = 59000.0 + static_cast<double>(i) * dtDays;
        vals[i] = 0.5 + 100.0 * static_cast<double>(i) * dtDays;  // slope=100/day
    }

    LinearRegressor reg(defaultCfg());
    const RegressionResult res = reg.fit(makeSeries(mjds, vals));

    CHECK_THAT(res.tRef,            WithinAbs(59000.0, 1e-10));
    CHECK_THAT(res.coefficients[0], WithinAbs(0.5,     1e-6));
    CHECK_THAT(res.coefficients[1], WithinAbs(100.0,   1e-3));
    CHECK_THAT(res.rSquared,        WithinAbs(1.0,     1e-6));
}

// -----------------------------------------------------------------------------
//  name() returns correct string
// -----------------------------------------------------------------------------

TEST_CASE("LinearRegressor: name()", "[regression][linear]")
{
    LinearRegressor reg(defaultCfg());
    CHECK(reg.name() == "LinearRegressor");
}
