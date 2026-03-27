#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <loki/regression/harmonicRegressor.hpp>
#include <loki/regression/trendEstimator.hpp>
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

// -----------------------------------------------------------------------------
//  Helpers
// -----------------------------------------------------------------------------

static RegressionConfig harmonicCfg(int K = 1, double period = 365.25)
{
    RegressionConfig cfg;
    cfg.harmonicTerms   = K;
    cfg.period          = period;
    cfg.confidenceLevel = 0.95;
    cfg.robust          = false;
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
//  HarmonicRegressor: pure annual signal y = 2*sin(2pi*t/T) + 3*cos(2pi*t/T)
// -----------------------------------------------------------------------------

TEST_CASE("HarmonicRegressor: recovers annual sine/cosine amplitudes", "[regression][harmonic]")
{
    const double T   = 365.25;
    const double amp_s = 2.0, amp_c = 3.0;
    const int    n   = 200;

    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) * T / static_cast<double>(n);
        mjds[i] = 50000.0 + t;
        vals[i] = amp_s * std::sin(2.0 * std::numbers::pi * t / T)
                + amp_c * std::cos(2.0 * std::numbers::pi * t / T);
    }

    HarmonicRegressor reg(harmonicCfg(1, T));
    const auto res = reg.fit(makeSeries(mjds, vals));

    // Coefficients: [a0, s1, c1]
    REQUIRE(res.coefficients.size() == 3);
    CHECK_THAT(res.coefficients[0], WithinAbs(0.0,   1e-6));  // mean = 0
    CHECK_THAT(res.coefficients[1], WithinAbs(amp_s, 1e-6));  // sin amplitude
    CHECK_THAT(res.coefficients[2], WithinAbs(amp_c, 1e-6));  // cos amplitude
    CHECK_THAT(res.rSquared,        WithinAbs(1.0,   1e-6));
}

// -----------------------------------------------------------------------------
//  HarmonicRegressor: amplitude() and phase() helpers
// -----------------------------------------------------------------------------

TEST_CASE("HarmonicRegressor: amplitude() and phase()", "[regression][harmonic]")
{
    const double T   = 365.25;
    const double amp = 5.0;
    const double phi = std::numbers::pi / 4.0;  // 45 degrees
    const int    n   = 300;

    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) * T / static_cast<double>(n);
        mjds[i] = 50000.0 + t;
        // y = amp * sin(omega*t + phi) = amp*(sin*cos(phi) + cos*sin(phi))
        vals[i] = amp * std::sin(2.0 * std::numbers::pi * t / T + phi);
    }

    HarmonicRegressor reg(harmonicCfg(1, T));
    reg.fit(makeSeries(mjds, vals));

    CHECK_THAT(reg.amplitude(1), WithinAbs(amp, 1e-4));
    // Phase check -- within 1e-4 rad
    CHECK_THAT(std::fabs(reg.phase(1) - phi),
               WithinAbs(0.0, 1e-3));
}

// -----------------------------------------------------------------------------
//  HarmonicRegressor: mean offset recovered
// -----------------------------------------------------------------------------

TEST_CASE("HarmonicRegressor: mean offset recovered", "[regression][harmonic]")
{
    const double T    = 365.25;
    const double mean = 7.5;
    const int    n    = 200;

    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) * T / static_cast<double>(n);
        mjds[i] = 50000.0 + t;
        vals[i] = mean + std::sin(2.0 * std::numbers::pi * t / T);
    }

    HarmonicRegressor reg(harmonicCfg(1, T));
    const auto res = reg.fit(makeSeries(mjds, vals));

    CHECK_THAT(res.coefficients[0], WithinAbs(mean, 1e-5));
}

// -----------------------------------------------------------------------------
//  HarmonicRegressor: too few observations throws
// -----------------------------------------------------------------------------

TEST_CASE("HarmonicRegressor: throws on too few observations", "[regression][harmonic]")
{
    // K=2 needs at least 6 observations (1 + 2*2 + 1)
    std::vector<double> mjds = {50000.0, 50001.0, 50002.0, 50003.0, 50004.0};
    std::vector<double> vals = {1.0, 2.0, 1.5, 0.5, 1.0};

    HarmonicRegressor reg(harmonicCfg(2, 365.25));
    CHECK_THROWS_AS(reg.fit(makeSeries(mjds, vals)), DataException);
}

// -----------------------------------------------------------------------------
//  HarmonicRegressor: predict() interval ordering
// -----------------------------------------------------------------------------

TEST_CASE("HarmonicRegressor: predict() interval ordering", "[regression][harmonic]")
{
    const double T = 365.25;
    const int    n = 200;

    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) * T / static_cast<double>(n);
        mjds[i] = 50000.0 + t;
        vals[i] = 1.0 + 2.0 * std::sin(2.0 * std::numbers::pi * t / T) +
                  0.1 * std::cos(4.0 * std::numbers::pi * t / T);
    }

    HarmonicRegressor reg(harmonicCfg(1, T));
    const auto res = reg.fit(makeSeries(mjds, vals));
    const auto pts = reg.predict({0.0, 91.0, 182.0, 273.0, 365.0});

    for (const auto& pt : pts) {
        CHECK(pt.confLow  <= pt.predicted + 1e-10);
        CHECK(pt.confHigh >= pt.predicted - 1e-10);
        CHECK(pt.predLow  <= pt.confLow   + 1e-10);
        CHECK(pt.predHigh >= pt.confHigh  - 1e-10);
    }
}

// -----------------------------------------------------------------------------
//  HarmonicRegressor: amplitude() before fit() throws
// -----------------------------------------------------------------------------

TEST_CASE("HarmonicRegressor: amplitude() before fit() throws", "[regression][harmonic]")
{
    HarmonicRegressor reg(harmonicCfg(1, 365.25));
    CHECK_THROWS_AS(reg.amplitude(1), AlgorithmException);
}

// =============================================================================
//  TrendEstimator tests
// =============================================================================

// -----------------------------------------------------------------------------
//  TrendEstimator: recovers trend + seasonal jointly
// -----------------------------------------------------------------------------

TEST_CASE("TrendEstimator: recovers trend slope and seasonal amplitude", "[regression][trend]")
{
    const double T     = 365.25;
    const double slope = 0.01;   // units/day
    const double amp_s = 3.0;
    const double amp_c = 1.5;
    const int    n     = 400;

    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) * T / static_cast<double>(n) * 2.0;
        mjds[i] = 50000.0 + t;
        vals[i] = slope * t
                + amp_s * std::sin(2.0 * std::numbers::pi * t / T)
                + amp_c * std::cos(2.0 * std::numbers::pi * t / T);
    }

    TrendEstimator est(harmonicCfg(1, T));
    const auto res = est.fit(makeSeries(mjds, vals));

    CHECK_THAT(res.trendSlope,     WithinAbs(slope, 1e-5));
    CHECK_THAT(res.trendIntercept, WithinAbs(0.0,   1e-4));

    // Seasonal amplitudes from coefficients [a0, a1, s1, c1].
    CHECK_THAT(res.regression.coefficients[2], WithinAbs(amp_s, 1e-4));
    CHECK_THAT(res.regression.coefficients[3], WithinAbs(amp_c, 1e-4));
}

// -----------------------------------------------------------------------------
//  TrendEstimator: residuals sum to zero
// -----------------------------------------------------------------------------

TEST_CASE("TrendEstimator: residuals sum to zero", "[regression][trend]")
{
    const double T = 365.25;
    const int    n = 300;

    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) * T / static_cast<double>(n) * 2.0;
        mjds[i] = 50000.0 + t;
        vals[i] = 0.005 * t + 2.0 * std::sin(2.0 * std::numbers::pi * t / T);
    }

    TrendEstimator est(harmonicCfg(1, T));
    const auto res = est.fit(makeSeries(mjds, vals));

    // Residuals of the full fit must sum to zero (OLS with intercept).
    const auto& reg = res.regression;
    CHECK_THAT(reg.residuals.sum(), WithinAbs(0.0, 1e-6));
}

// -----------------------------------------------------------------------------
//  TrendEstimator: component sizes match
// -----------------------------------------------------------------------------

TEST_CASE("TrendEstimator: component TimeSeries sizes match", "[regression][trend]")
{
    const int n = 200;
    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        mjds[i] = 50000.0 + static_cast<double>(i);
        vals[i] = static_cast<double>(i) * 0.01 +
                  std::sin(2.0 * std::numbers::pi * static_cast<double>(i) / 365.25);
    }

    TrendEstimator est(harmonicCfg(1, 365.25));
    const auto res = est.fit(makeSeries(mjds, vals));

    CHECK(res.trend.size()     == static_cast<std::size_t>(n));
    CHECK(res.seasonal.size()  == static_cast<std::size_t>(n));
    CHECK(res.residuals.size() == static_cast<std::size_t>(n));
}

// -----------------------------------------------------------------------------
//  TrendEstimator: trend + seasonal + residual = original
// -----------------------------------------------------------------------------

TEST_CASE("TrendEstimator: components reconstruct original", "[regression][trend]")
{
    const int n = 200;
    std::vector<double> mjds(n), vals(n);
    for (int i = 0; i < n; ++i) {
        mjds[i] = 50000.0 + static_cast<double>(i);
        vals[i] = 1.0 + 0.005 * static_cast<double>(i) +
                  2.0 * std::sin(2.0 * std::numbers::pi * static_cast<double>(i) / 365.25) +
                  0.1 * std::cos(4.0 * std::numbers::pi * static_cast<double>(i) / 365.25);
    }
    const auto ts = makeSeries(mjds, vals);

    TrendEstimator est(harmonicCfg(2, 365.25));
    const auto res = est.fit(ts);

    for (std::size_t i = 0; i < static_cast<std::size_t>(n); ++i) {
        const double reconstructed = res.trend[i].value +
                                     res.seasonal[i].value +
                                     res.residuals[i].value;
        CHECK_THAT(reconstructed, WithinAbs(vals[i], 1e-6));
    }
}

// -----------------------------------------------------------------------------
//  TrendEstimator: too few observations throws
// -----------------------------------------------------------------------------

TEST_CASE("TrendEstimator: throws on too few observations", "[regression][trend]")
{
    // K=1 needs at least 2 + 2*1 + 1 = 5 observations
    std::vector<double> mjds = {50000.0, 50001.0, 50002.0, 50003.0};
    std::vector<double> vals = {1.0, 2.0, 1.5, 0.5};

    TrendEstimator est(harmonicCfg(1, 365.25));
    CHECK_THROWS_AS(est.fit(makeSeries(mjds, vals)), DataException);
}
