#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <loki/regression/polynomialRegressor.hpp>
#include <loki/regression/regressionResult.hpp>
#include <loki/timeseries/timeSeries.hpp>
#include <loki/timeseries/timeStamp.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/config.hpp>

#include <cmath>
#include <vector>

using namespace loki;
using namespace loki::regression;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

// -----------------------------------------------------------------------------
//  Helpers
// -----------------------------------------------------------------------------

static RegressionConfig defaultCfg(int degree = 2, int cvFolds = 10)
{
    RegressionConfig cfg;
    cfg.polynomialDegree = degree;
    cfg.confidenceLevel  = 0.95;
    cfg.robust           = false;
    cfg.cvFolds          = cvFolds;
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
//  Perfect quadratic fit y = 1 + 2*x + 3*x^2
// -----------------------------------------------------------------------------

TEST_CASE("PolynomialRegressor: perfect degree-2 fit", "[regression][polynomial]")
{
    const int n = 15;
    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        const double x = static_cast<double>(i);
        mjds[i] = 50000.0 + x;
        vals[i] = 1.0 + 2.0 * x + 3.0 * x * x;
    }

    PolynomialRegressor reg(defaultCfg(2));
    const auto res = reg.fit(makeSeries(mjds, vals));

    REQUIRE(res.coefficients.size() == 3);
    CHECK_THAT(res.coefficients[0], WithinAbs(1.0, 1e-6));
    CHECK_THAT(res.coefficients[1], WithinAbs(2.0, 1e-6));
    CHECK_THAT(res.coefficients[2], WithinAbs(3.0, 1e-6));
    CHECK_THAT(res.rSquared,        WithinAbs(1.0, 1e-8));
    CHECK(res.dof == n - 3);
    CHECK(res.modelName == "PolynomialRegressor(degree=2)");
}

// -----------------------------------------------------------------------------
//  Degree-1 matches LinearRegressor behaviour
// -----------------------------------------------------------------------------

TEST_CASE("PolynomialRegressor: degree=1 matches linear fit", "[regression][polynomial]")
{
    const int n = 20;
    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        mjds[i] = 51000.0 + static_cast<double>(i);
        vals[i] = 5.0 - 0.3 * static_cast<double>(i) +
                  0.1 * std::sin(static_cast<double>(i));
    }

    PolynomialRegressor reg(defaultCfg(1));
    const auto res = reg.fit(makeSeries(mjds, vals));

    REQUIRE(res.coefficients.size() == 2);
    CHECK(res.dof == n - 2);
    CHECK_THAT(res.rSquared, WithinAbs(1.0, 0.5));  // not perfect due to noise
}

// -----------------------------------------------------------------------------
//  Higher degree reduces training error (R^2 increases with degree)
// -----------------------------------------------------------------------------

TEST_CASE("PolynomialRegressor: higher degree fits better", "[regression][polynomial]")
{
    // Data from degree-4 polynomial -- degree 2 should fit worse than degree 4.
    const int n = 30;
    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        const double x = static_cast<double>(i) * 0.1;
        mjds[i] = 50000.0 + x;
        vals[i] = 1.0 + x - 2.0*x*x + 0.5*x*x*x - 0.1*x*x*x*x;
    }
    const auto ts = makeSeries(mjds, vals);

    PolynomialRegressor reg2(defaultCfg(2));
    PolynomialRegressor reg4(defaultCfg(4));

    const auto res2 = reg2.fit(ts);
    const auto res4 = reg4.fit(ts);

    CHECK(res4.rSquared >= res2.rSquared - 1e-10);
    CHECK_THAT(res4.rSquared, WithinAbs(1.0, 1e-6));
}

// -----------------------------------------------------------------------------
//  NaN observations skipped
// -----------------------------------------------------------------------------

TEST_CASE("PolynomialRegressor: NaN observations skipped", "[regression][polynomial]")
{
    const int n = 20;
    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        const double x = static_cast<double>(i);
        mjds[i] = 50000.0 + x;
        vals[i] = 2.0 + x + x * x;
    }
    vals[5]  = std::numeric_limits<double>::quiet_NaN();
    vals[12] = std::numeric_limits<double>::quiet_NaN();

    PolynomialRegressor reg(defaultCfg(2));
    const auto res = reg.fit(makeSeries(mjds, vals));

    CHECK_THAT(res.coefficients[0], WithinAbs(2.0, 1e-4));
    CHECK_THAT(res.coefficients[1], WithinAbs(1.0, 1e-4));
    CHECK_THAT(res.coefficients[2], WithinAbs(1.0, 1e-4));
    CHECK(res.dof == 18 - 3);
}

// -----------------------------------------------------------------------------
//  Too few observations throws DataException
// -----------------------------------------------------------------------------

TEST_CASE("PolynomialRegressor: throws on too few observations", "[regression][polynomial]")
{
    // degree=3 needs at least 5 observations
    std::vector<double> mjds = {50000.0, 50001.0, 50002.0, 50003.0};
    std::vector<double> vals = {1.0, 2.0, 4.0, 8.0};

    PolynomialRegressor reg(defaultCfg(3));
    CHECK_THROWS_AS(reg.fit(makeSeries(mjds, vals)), DataException);
}

// -----------------------------------------------------------------------------
//  AIC and BIC finite
// -----------------------------------------------------------------------------

TEST_CASE("PolynomialRegressor: AIC and BIC are finite", "[regression][polynomial]")
{
    const int n = 20;
    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        const double x = static_cast<double>(i);
        mjds[i] = 50000.0 + x;
        vals[i] = 1.0 + 0.5*x - 0.02*x*x + 0.1*std::sin(x);
    }

    PolynomialRegressor reg(defaultCfg(2));
    const auto res = reg.fit(makeSeries(mjds, vals));

    CHECK(std::isfinite(res.aic));
    CHECK(std::isfinite(res.bic));
    CHECK(res.bic >= res.aic);
}

// -----------------------------------------------------------------------------
//  predict() confidence interval contains prediction
// -----------------------------------------------------------------------------

TEST_CASE("PolynomialRegressor: predict() interval ordering", "[regression][polynomial]")
{
    const int n = 20;
    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        const double x = static_cast<double>(i);
        mjds[i] = 50000.0 + x;
        vals[i] = 1.0 + x - 0.1*x*x + 0.05*std::cos(x);
    }

    PolynomialRegressor reg(defaultCfg(2));
    const auto res = reg.fit(makeSeries(mjds, vals));

    const auto pts = reg.predict({0.0, 5.0, 10.0, 15.0});
    for (const auto& pt : pts) {
        CHECK(pt.confLow  <= pt.predicted + 1e-10);
        CHECK(pt.confHigh >= pt.predicted - 1e-10);
        CHECK(pt.predLow  <= pt.confLow   + 1e-10);
        CHECK(pt.predHigh >= pt.confHigh  - 1e-10);
    }
}

// -----------------------------------------------------------------------------
//  predict() before fit() throws
// -----------------------------------------------------------------------------

TEST_CASE("PolynomialRegressor: predict() before fit() throws", "[regression][polynomial]")
{
    PolynomialRegressor reg(defaultCfg(2));
    CHECK_THROWS_AS(reg.predict({1.0, 2.0}), AlgorithmException);
}

// -----------------------------------------------------------------------------
//  LOO-CV: perfect fit gives near-zero RMSE
// -----------------------------------------------------------------------------

TEST_CASE("PolynomialRegressor: LOO-CV RMSE near zero for perfect fit", "[regression][polynomial][cv]")
{
    const int n = 20;
    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        const double x = static_cast<double>(i);
        mjds[i] = 50000.0 + x;
        vals[i] = 1.0 + 2.0*x + 0.5*x*x;
    }

    PolynomialRegressor reg(defaultCfg(2));
    reg.fit(makeSeries(mjds, vals));
    const auto cv = reg.leaveOneOutCV();

    // LOO-CV RMSE should be very small for a perfect polynomial fit.
    CHECK(cv.rmse   < 1e-4);
    CHECK(cv.folds  == n);
    CHECK(std::isfinite(cv.bias));
}

// -----------------------------------------------------------------------------
//  LOO-CV: noisy data gives higher RMSE than training RMSE
// -----------------------------------------------------------------------------

TEST_CASE("PolynomialRegressor: LOO-CV RMSE > training RMSE for noisy data", "[regression][polynomial][cv]")
{
    const int n = 30;
    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        const double x = static_cast<double>(i) * 0.5;
        mjds[i] = 50000.0 + x;
        vals[i] = 1.0 + x - 0.1*x*x +
                  0.5 * std::sin(static_cast<double>(i) * 1.7);  // noise
    }

    PolynomialRegressor reg(defaultCfg(2));
    const auto res = reg.fit(makeSeries(mjds, vals));
    const auto cv  = reg.leaveOneOutCV();

    // Training RMSE = sigma0 * sqrt(dof/n) approximately.
    // LOO-CV RMSE should be >= training RMSE.
    const double trainRmse = res.sigma0 * std::sqrt(
        static_cast<double>(res.dof) / static_cast<double>(n));
    CHECK(cv.rmse >= trainRmse - 1e-6);
    CHECK(cv.folds == n);
}

// -----------------------------------------------------------------------------
//  k-fold CV: basic sanity
// -----------------------------------------------------------------------------

TEST_CASE("PolynomialRegressor: k-fold CV produces finite metrics", "[regression][polynomial][cv]")
{
    const int n = 50;
    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        const double x = static_cast<double>(i);
        mjds[i] = 50000.0 + x;
        vals[i] = 2.0 + 0.3*x - 0.01*x*x +
                  0.2 * std::cos(static_cast<double>(i) * 0.8);
    }

    PolynomialRegressor reg(defaultCfg(2, 5));
    reg.fit(makeSeries(mjds, vals));
    const auto cv = reg.kFoldCV();

    CHECK(std::isfinite(cv.rmse));
    CHECK(std::isfinite(cv.mae));
    CHECK(std::isfinite(cv.bias));
    CHECK(cv.rmse  >= 0.0);
    CHECK(cv.mae   >= 0.0);
    CHECK(cv.folds == 5);
}

// -----------------------------------------------------------------------------
//  k-fold CV: fold clamping warning (cvFolds > MAX_CV_FOLDS)
// -----------------------------------------------------------------------------

TEST_CASE("PolynomialRegressor: k-fold clamps oversized folds", "[regression][polynomial][cv]")
{
    const int n = 50;
    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        mjds[i] = 50000.0 + static_cast<double>(i);
        vals[i] = static_cast<double>(i) * 0.5;
    }

    // Request 200 folds -- should clamp to min(100, n/2) = 25.
    PolynomialRegressor reg(defaultCfg(1, 200));
    reg.fit(makeSeries(mjds, vals));
    const auto cv = reg.kFoldCV();

    CHECK(cv.folds <= 100);
    CHECK(cv.folds <= n / 2);
    CHECK(cv.folds >= 2);
}

// -----------------------------------------------------------------------------
//  LOO-CV before fit() throws
// -----------------------------------------------------------------------------

TEST_CASE("PolynomialRegressor: leaveOneOutCV() before fit() throws", "[regression][polynomial][cv]")
{
    PolynomialRegressor reg(defaultCfg(2));
    CHECK_THROWS_AS(reg.leaveOneOutCV(), AlgorithmException);
}
