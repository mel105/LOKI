#include "loki/evt/evtAnalyzer.hpp"
#include "loki/evt/gpd.hpp"
#include "loki/evt/gev.hpp"
#include "loki/evt/thresholdSelector.hpp"
#include "loki/evt/plotEvt.hpp"
#include "loki/core/exceptions.hpp"
#include "loki/core/logger.hpp"
#include "loki/timeseries/gapFiller.hpp"
#include "loki/timeseries/deseasonalizer.hpp"
#include "loki/timeseries/medianYearSeries.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <numbers>
#include <numeric>
#include <random>
#include <sstream>

using namespace loki;

namespace loki::evt {

// =============================================================================
//  Constants
// =============================================================================

static constexpr double SECONDS_PER_HOUR = 3600.0;
static constexpr double HOURS_PER_DAY    = 24.0;
static constexpr double DAYS_PER_YEAR    = 365.25;

// chi2(0.95) / 2 = 1.92073 (1 d.o.f.) used for profile likelihood CI.
static constexpr double PROFILE_LIK_DROP = 1.92073;

// =============================================================================
//  Constructor
// =============================================================================

EvtAnalyzer::EvtAnalyzer(const AppConfig& cfg)
    : m_cfg(cfg)
{}

// =============================================================================
//  run
// =============================================================================

void EvtAnalyzer::run(const TimeSeries& series, const std::string& datasetName)
{
    const EvtConfig& ec = m_cfg.evt;
    const std::string comp = series.metadata().componentName;

    LOKI_INFO("EVT: starting analysis for '" + comp + "' dataset='" + datasetName + "'.");

    // ---- 1. Gap filling -----------------------------------------------------
    GapFiller::Config gfc;
    const std::string& s = ec.gapFillStrategy;
    if      (s == "linear")      { gfc.strategy = GapFiller::Strategy::LINEAR; }
    else if (s == "median_year") { gfc.strategy = GapFiller::Strategy::MEDIAN_YEAR; }
    else if (s == "spline")      { gfc.strategy = GapFiller::Strategy::SPLINE; }
    else if (s == "none")        { gfc.strategy = GapFiller::Strategy::NONE; }
    else {
        LOKI_WARNING("EVT: unknown gap_fill_strategy '" + s + "' -- using linear.");
        gfc.strategy = GapFiller::Strategy::LINEAR;
    }
    gfc.maxFillLength = ec.gapFillMaxLength;

    GapFiller filler(gfc);
    const TimeSeries filled = filler.fill(series);

    // ---- 2. Deseasonalization (optional) ------------------------------------
    TimeSeries analysis = filled;
    bool deseasApplied = false;

    if (ec.deseasonalization.enabled
        && ec.deseasonalization.strategy != "none")
    {
        const std::string& ds = ec.deseasonalization.strategy;
        Deseasonalizer::Strategy strat = Deseasonalizer::Strategy::NONE;

        if (ds == "moving_average") {
            strat = Deseasonalizer::Strategy::MOVING_AVERAGE;
        } else if (ds == "median_year") {
            strat = Deseasonalizer::Strategy::MEDIAN_YEAR;
        } else {
            LOKI_WARNING("EVT: unknown deseasonalization strategy '" + ds
                         + "' -- skipping deseasonalization.");
        }

        if (strat != Deseasonalizer::Strategy::NONE) {
            try {
                Deseasonalizer::Config dc(strat, ec.deseasonalization.maWindowSize);
                Deseasonalizer deseas(dc);

                std::function<double(const ::TimeStamp&)> profileLookup = nullptr;

                if (strat == Deseasonalizer::Strategy::MEDIAN_YEAR) {
                    MedianYearSeries::Config mysCfg;
                    mysCfg.minYears = ec.deseasonalization.medianYearMinYears;
                    MedianYearSeries mys(filled, mysCfg);
                    profileLookup = [mys](const ::TimeStamp& t) {
                        return mys.valueAt(t);
                    };
                }

                const auto result = deseas.deseasonalize(filled, profileLookup);
                analysis     = result.series;
                deseasApplied = true;

                LOKI_INFO("EVT: deseasonalization applied (strategy=" + ds
                          + ") -- EVT will analyse residuals.");
            } catch (const LOKIException& ex) {
                LOKI_WARNING(std::string("EVT: deseasonalization failed: ")
                             + ex.what() + " -- using original series.");
            }
        }
    }

    // ---- 3. Extract valid values --------------------------------------------
    const std::vector<double> data = _extractValid(analysis);
    if (static_cast<int>(data.size()) < 20) {
        throw DataException(
            "EVT: series '" + comp + "' has only " + std::to_string(data.size())
            + " valid observations after gap filling -- too few for EVT.");
    }

    const double dtHours = _medianDtHours(filled);

    LOKI_INFO("EVT: n_valid=" + std::to_string(data.size())
              + "  dt=" + std::to_string(dtHours) + "h  method=" + ec.method
              + (deseasApplied ? "  [deseasonalized]" : ""));

    // ---- 3. Run analysis ----------------------------------------------------
    std::vector<EvtResult> results;

    if (ec.method == "pot" || ec.method == "both") {
        try {
            EvtResult r = _runPot(data, dtHours);
            results.push_back(std::move(r));
        } catch (const LOKIException& ex) {
            LOKI_WARNING(std::string("EVT: POT analysis failed: ") + ex.what());
        }
    }

    if (ec.method == "block_maxima" || ec.method == "both") {
        try {
            EvtResult r = _runBlockMaxima(data);
            results.push_back(std::move(r));
        } catch (const LOKIException& ex) {
            LOKI_WARNING(std::string("EVT: block maxima analysis failed: ") + ex.what());
        }
    }

    if (results.empty()) {
        throw AlgorithmException(
            "EVT: all analysis methods failed for series '" + comp + "'.");
    }

    // ---- 4. Output ----------------------------------------------------------
    PlotEvt plotter(m_cfg);

    for (const EvtResult& r : results) {
        _writeProtocol(r, filled, datasetName);
        _writeCsv(r, datasetName, comp);
        plotter.plot(r, datasetName, comp);
    }

    LOKI_INFO("EVT: finished analysis for '" + comp + "'.");
}

// =============================================================================
//  _runPot
// =============================================================================

EvtResult EvtAnalyzer::_runPot(const std::vector<double>& data,
                                 double dtHours) const
{
    const EvtConfig& ec = m_cfg.evt;

    // Threshold selection.
    ThresholdSelector::Result thr;
    if (ec.threshold.autoSelect) {
        thr = ThresholdSelector::autoSelect(data,
                                             ec.threshold.nCandidates,
                                             ec.threshold.minExceedances);
    } else {
        thr = ThresholdSelector::manual(data,
                                         ec.threshold.value,
                                         ec.threshold.minExceedances);
    }

    const double u = thr.selected;

    // Collect exceedances.
    std::vector<double> exc;
    for (const double v : data)
        if (v > u) exc.push_back(v - u);

    if (exc.empty())
        throw DataException("EVT POT: no exceedances above threshold "
                            + std::to_string(u) + ".");

    LOKI_INFO("EVT POT: threshold=" + std::to_string(u)
              + "  n_exc=" + std::to_string(exc.size()));

    // Exceedance rate (lambda): exceedances per time_unit.
    // Total observation time in time_unit = n_obs * dt.
    const double nObs = static_cast<double>(data.size());
    double dtUnit = 1.0; // dt in time_unit units
    const std::string& tu = ec.timeUnit;
    if      (tu == "seconds") { dtUnit = dtHours * SECONDS_PER_HOUR; }
    else if (tu == "minutes") { dtUnit = dtHours * 60.0; }
    else if (tu == "hours")   { dtUnit = dtHours; }
    else if (tu == "days")    { dtUnit = dtHours / HOURS_PER_DAY; }
    else if (tu == "years")   { dtUnit = dtHours / (HOURS_PER_DAY * DAYS_PER_YEAR); }

    const double totalTime = nObs * dtUnit;
    const double lambda    = static_cast<double>(exc.size()) / totalTime;

    // GPD fit.
    GpdFitResult gpd = Gpd::fit(exc, u);
    gpd.lambda = lambda;

    if (!gpd.converged) {
        LOKI_WARNING("EVT POT: GPD MLE did not converge (PWM fallback used).");
    }

    // Return levels with CI.
    std::vector<ReturnLevelCI> rls = _returnLevelsGpd(exc, gpd);

    // GoF.
    const GoFResult gof = _gofGpd(exc, gpd);

    // Assemble result.
    EvtResult res;
    res.method               = "pot";
    res.gpd                  = gpd;
    res.returnLevels         = std::move(rls);
    res.gof                  = gof;
    res.exceedances          = exc;
    res.thresholdCandidates  = thr.candidates;
    res.meanExcessValues     = thr.meanExcess;
    res.sigmaStability       = thr.sigmaStability;
    res.xiStability          = thr.xiStability;
    return res;
}

// =============================================================================
//  _runBlockMaxima
// =============================================================================

EvtResult EvtAnalyzer::_runBlockMaxima(const std::vector<double>& data) const
{
    const EvtConfig& ec = m_cfg.evt;
    const int blockSize = ec.blockMaxima.blockSize;

    if (blockSize < 2)
        throw ConfigException("EVT: block_maxima.block_size must be >= 2.");

    const int n = static_cast<int>(data.size());
    const int nBlocks = n / blockSize;

    if (nBlocks < 3)
        throw DataException(
            "EVT: only " + std::to_string(nBlocks)
            + " complete blocks of size " + std::to_string(blockSize)
            + " -- need at least 3 for block maxima analysis.");

    std::vector<double> maxima;
    maxima.reserve(static_cast<std::size_t>(nBlocks));

    for (int b = 0; b < nBlocks; ++b) {
        const int start = b * blockSize;
        const int end   = start + blockSize;
        double mx = data[static_cast<std::size_t>(start)];
        for (int i = start + 1; i < end; ++i)
            if (data[static_cast<std::size_t>(i)] > mx)
                mx = data[static_cast<std::size_t>(i)];
        maxima.push_back(mx);
    }

    LOKI_INFO("EVT block maxima: n_blocks=" + std::to_string(nBlocks)
              + "  block_size=" + std::to_string(blockSize));

    GevFitResult gev = Gev::fit(maxima);
    if (!gev.converged) {
        LOKI_WARNING("EVT: GEV MLE did not converge.");
    }

    std::vector<ReturnLevelCI> rls = _returnLevelsGev(maxima, gev);
    const GoFResult gof = _gofGev(maxima, gev);

    EvtResult res;
    res.method       = "block_maxima";
    res.gev          = gev;
    res.returnLevels = std::move(rls);
    res.gof          = gof;
    res.blockMaxima  = maxima;
    return res;
}

// =============================================================================
//  _returnLevelsGpd
// =============================================================================

std::vector<ReturnLevelCI> EvtAnalyzer::_returnLevelsGpd(
    const std::vector<double>& exc,
    const GpdFitResult&        gpd) const
{
    const EvtConfig& ec = m_cfg.evt;
    const double level  = ec.confidenceLevel;

    std::vector<ReturnLevelCI> result;
    result.reserve(ec.returnPeriods.size());

    for (const double T : ec.returnPeriods) {
        ReturnLevelCI rl;
        rl.period   = T;
        rl.estimate = Gpd::returnLevel(T, gpd.lambda, gpd.threshold, gpd.xi, gpd.sigma);

        rl.lower = rl.estimate;
        rl.upper = rl.estimate;

        if (ec.ci.enabled && !exc.empty()) {
            const std::string& cm = ec.ci.method;
            if (cm == "profile_likelihood") {
                _profileLikCiGpd(exc, gpd, T, level, rl.lower, rl.upper);
            } else if (cm == "bootstrap") {
                _bootstrapCiGpd(exc, gpd, T, level, rl.lower, rl.upper);
            } else {
                // "delta" or unknown
                _deltaCiGpd(exc, gpd, T, level, rl.lower, rl.upper);
            }
        }

        result.push_back(rl);
    }
    return result;
}

// =============================================================================
//  _returnLevelsGev
// =============================================================================

std::vector<ReturnLevelCI> EvtAnalyzer::_returnLevelsGev(
    const std::vector<double>& maxima,
    const GevFitResult&        gev) const
{
    const EvtConfig& ec = m_cfg.evt;

    std::vector<ReturnLevelCI> result;
    result.reserve(ec.returnPeriods.size());

    for (const double T : ec.returnPeriods) {
        ReturnLevelCI rl;
        rl.period   = T;
        rl.estimate = Gev::returnLevel(T, gev.xi, gev.sigma, gev.mu);

        // Bootstrap CI for GEV.
        rl.lower = rl.estimate;
        rl.upper = rl.estimate;

        if (ec.ci.enabled && !maxima.empty() && ec.ci.method == "bootstrap") {
            const int B     = ec.ci.nBootstrap;
            const double alpha = 1.0 - ec.confidenceLevel;
            const int n     = static_cast<int>(maxima.size());

            std::mt19937_64 rng(42);
            std::uniform_int_distribution<int> dist(0, n - 1);

            std::vector<double> samples;
            samples.reserve(static_cast<std::size_t>(B));

            for (int b = 0; b < B; ++b) {
                std::vector<double> boot;
                boot.reserve(static_cast<std::size_t>(n));
                for (int i = 0; i < n; ++i)
                    boot.push_back(maxima[static_cast<std::size_t>(dist(rng))]);
                try {
                    const GevFitResult gr = Gev::fit(boot);
                    samples.push_back(Gev::returnLevel(T, gr.xi, gr.sigma, gr.mu));
                } catch (...) {}
            }

            if (!samples.empty()) {
                std::sort(samples.begin(), samples.end());
                const std::size_t lo = static_cast<std::size_t>(
                    std::floor(alpha / 2.0 * static_cast<double>(samples.size())));
                const std::size_t hi = static_cast<std::size_t>(
                    std::ceil((1.0 - alpha / 2.0) * static_cast<double>(samples.size())));
                rl.lower = samples[std::min(lo, samples.size() - 1)];
                rl.upper = samples[std::min(hi, samples.size() - 1)];
            }
        }

        result.push_back(rl);
    }
    return result;
}

// =============================================================================
//  CI: profile likelihood (GPD)
// =============================================================================

void EvtAnalyzer::_profileLikCiGpd(const std::vector<double>& exc,
                                     const GpdFitResult&        gpd,
                                     double T, double level,
                                     double& lower, double& upper) const
{
    // Maximum log-likelihood at fitted parameters.
    const double llMax = Gpd::logLik(exc, gpd.xi, gpd.sigma);

    // Profile likelihood drop threshold.
    const double drop = 0.5 * (level > 0.0 ? -std::log(1.0 - level) * 2.0 : PROFILE_LIK_DROP);
    // Standard: chi2(1, level) / 2.  For 0.95: drop = 1.92073.
    // We use the exact chi2 inverse approximation below.
    // chi2(1, p) = 2 * InvGamma(0.5, p) -- approximate with the known value.
    const double chi2 = [&]() -> double {
        // Closed-form approximation for chi2(1) quantile (Wilson-Hilferty).
        // For p in [0.9, 0.999] this is accurate enough.
        if (level <= 0.0 || level >= 1.0) return PROFILE_LIK_DROP * 2.0;
        // Use erfinv: chi2(1, p) = (erfinv(p))^2 * 2
        // Approximation of erfinv via Newton iteration starting from a guess.
        double x = 1.0;
        for (int it = 0; it < 20; ++it) {
            const double ex = std::erf(x / std::sqrt(2.0));
            const double fx = ex - level;
            const double dfx = std::sqrt(2.0 / std::numbers::pi) * std::exp(-0.5 * x * x);
            if (std::abs(dfx) < 1.0e-15) break;
            x -= fx / dfx;
        }
        return x * x;
    }();
    const double llThreshold = llMax - 0.5 * chi2;

    // Point estimate.
    const double zEst = Gpd::returnLevel(T, gpd.lambda, gpd.threshold, gpd.xi, gpd.sigma);

    // Profile function: for a fixed return level z, find max logLik(xi, sigma)
    // subject to returnLevel(T, lambda, threshold, xi, sigma) == z.
    // We reparameterise: fix z, express sigma as a function of xi from the
    // return level equation, then maximise over xi.
    auto profileLogLik = [&](double z) -> double {
        // sigma(xi) from returnLevel equation:
        //   z = threshold + sigma/xi * ((T*lambda)^xi - 1)
        //   sigma = xi*(z - threshold) / ((T*lambda)^xi - 1)  for xi != 0
        //   sigma = (z - threshold) / log(T*lambda)            for xi == 0
        const double excess = z - gpd.threshold;
        if (excess <= 0.0) return -std::numeric_limits<double>::infinity();

        auto sigmaFromXi = [&](double xi) -> double {
            if (std::abs(xi) < 1.0e-8) {
                const double denom = std::log(T * gpd.lambda);
                return (denom > 0.0) ? excess / denom : -1.0;
            }
            const double denom = std::pow(T * gpd.lambda, xi) - 1.0;
            return (std::abs(denom) > 1.0e-12) ? xi * excess / denom : -1.0;
        };

        // Maximise logLik over xi in [-0.4, 2] for this fixed z.
        double bestLL = -std::numeric_limits<double>::infinity();
        // Grid search + refine.
        const int nGrid = 40;
        double bestXi = 0.0;
        for (int i = 0; i <= nGrid; ++i) {
            const double xi = -0.4 + static_cast<double>(i) * 2.4 / static_cast<double>(nGrid);
            const double sg = sigmaFromXi(xi);
            if (sg <= 0.0) continue;
            const double ll = Gpd::logLik(exc, xi, sg);
            if (ll > bestLL) { bestLL = ll; bestXi = xi; }
        }
        // Nelder-Mead refinement in 1D.
        auto obj1d = [&](const std::vector<double>& p) -> double {
            const double xi = p[0];
            const double sg = sigmaFromXi(xi);
            if (sg <= 0.0) return 1.0e12;
            const double ll = Gpd::logLik(exc, xi, sg);
            return std::isfinite(ll) ? -ll : 1.0e12;
        };
        const auto nm = loki::math::nelderMead(obj1d, {bestXi}, 500, 1.0e-9);
        const double xi2  = nm.params[0];
        const double sg2  = sigmaFromXi(xi2);
        if (sg2 > 0.0) {
            const double ll2 = Gpd::logLik(exc, xi2, sg2);
            if (ll2 > bestLL) bestLL = ll2;
        }
        return bestLL;
    };

    // Profile CI: find z where profileLogLik(z) == llThreshold.
    // Lower bound: search below zEst.
    const double searchRange = std::max(std::abs(zEst) * 5.0, 10.0);

    auto diffLower = [&](double z) -> double {
        return profileLogLik(z) - llThreshold;
    };
    auto diffUpper = [&](double z) -> double {
        return profileLogLik(z) - llThreshold;
    };

    lower = zEst;
    upper = zEst;

    try {
        lower = _brentRoot(diffLower,
                           zEst - searchRange, zEst - 1.0e-6);
    } catch (...) {
        LOKI_WARNING("EVT: profile likelihood lower bound search failed for T="
                     + std::to_string(T));
    }
    try {
        upper = _brentRoot(diffUpper,
                           zEst + 1.0e-6, zEst + searchRange);
    } catch (...) {
        LOKI_WARNING("EVT: profile likelihood upper bound search failed for T="
                     + std::to_string(T));
    }
}

// =============================================================================
//  CI: bootstrap (GPD)
// =============================================================================

void EvtAnalyzer::_bootstrapCiGpd(const std::vector<double>& exc,
                                    const GpdFitResult&        gpd,
                                    double T, double level,
                                    double& lower, double& upper) const
{
    const EvtConfig& ec = m_cfg.evt;
    const int maxN = ec.ci.maxExceedancesBootstrap;
    const int B    = ec.ci.nBootstrap;
    const double alpha = 1.0 - level;

    // Subsample if necessary.
    std::vector<double> pool = exc;
    if (static_cast<int>(pool.size()) > maxN) {
        std::mt19937_64 rng(1234);
        std::shuffle(pool.begin(), pool.end(), rng);
        pool.resize(static_cast<std::size_t>(maxN));
    }

    const int n = static_cast<int>(pool.size());
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<int> dist(0, n - 1);

    std::vector<double> samples;
    samples.reserve(static_cast<std::size_t>(B));

    for (int b = 0; b < B; ++b) {
        std::vector<double> boot;
        boot.reserve(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i)
            boot.push_back(pool[static_cast<std::size_t>(dist(rng))]);
        try {
            const GpdFitResult gr = Gpd::fit(boot, gpd.threshold);
            const double rl = Gpd::returnLevel(T, gpd.lambda,
                                                gpd.threshold, gr.xi, gr.sigma);
            if (std::isfinite(rl)) samples.push_back(rl);
        } catch (...) {}
    }

    if (samples.empty()) {
        lower = upper = Gpd::returnLevel(T, gpd.lambda, gpd.threshold, gpd.xi, gpd.sigma);
        return;
    }

    std::sort(samples.begin(), samples.end());
    const int ns = static_cast<int>(samples.size());
    const auto lo = static_cast<int>(std::floor(alpha / 2.0 * static_cast<double>(ns)));
    const auto hi = static_cast<int>(std::ceil((1.0 - alpha / 2.0) * static_cast<double>(ns)));
    lower = samples[static_cast<std::size_t>(std::max(0, lo))];
    upper = samples[static_cast<std::size_t>(std::min(ns - 1, hi))];
}

// =============================================================================
//  CI: delta method (GPD)
// =============================================================================

void EvtAnalyzer::_deltaCiGpd(const std::vector<double>& exc,
                                const GpdFitResult&        gpd,
                                double T, double level,
                                double& lower, double& upper) const
{
    // Numerical gradient of returnLevel w.r.t. (xi, sigma).
    const double dxi   = 1.0e-5;
    const double dsig  = 1.0e-5;

    const double z0 = Gpd::returnLevel(T, gpd.lambda, gpd.threshold, gpd.xi, gpd.sigma);

    const double dz_dxi  = (Gpd::returnLevel(T, gpd.lambda, gpd.threshold, gpd.xi + dxi, gpd.sigma)
                           - Gpd::returnLevel(T, gpd.lambda, gpd.threshold, gpd.xi - dxi, gpd.sigma))
                           / (2.0 * dxi);
    const double dz_dsig = (Gpd::returnLevel(T, gpd.lambda, gpd.threshold, gpd.xi, gpd.sigma + dsig)
                            - Gpd::returnLevel(T, gpd.lambda, gpd.threshold, gpd.xi, gpd.sigma - dsig))
                            / (2.0 * dsig);

    // Approximate covariance: inverse Fisher information (diagonal approximation).
    // Var(xi)    ~ 1/(n * I_xi), where I_xi = E[-d^2 logL / d_xi^2].
    // Use numerical second derivative.
    const int n = static_cast<int>(exc.size());
    const double ll0 = Gpd::logLik(exc, gpd.xi,       gpd.sigma);
    const double llp = Gpd::logLik(exc, gpd.xi + dxi,  gpd.sigma);
    const double llm = Gpd::logLik(exc, gpd.xi - dxi,  gpd.sigma);
    const double d2xi  = -(llp - 2.0 * ll0 + llm) / (dxi * dxi);

    const double llsp = Gpd::logLik(exc, gpd.xi, gpd.sigma + dsig);
    const double llsm = Gpd::logLik(exc, gpd.xi, gpd.sigma - dsig);
    const double d2sig = -(llsp - 2.0 * ll0 + llsm) / (dsig * dsig);

    const double varXi  = (d2xi  > 1.0e-12) ? 1.0 / d2xi  : 1.0;
    const double varSig = (d2sig > 1.0e-12) ? 1.0 / d2sig : 1.0;

    const double varZ = dz_dxi * dz_dxi * varXi
                       + dz_dsig * dz_dsig * varSig;
    const double seZ  = (varZ > 0.0) ? std::sqrt(varZ / static_cast<double>(n)) : 0.0;

    // z-quantile for two-sided CI.
    double zq = 1.96; // approx for 0.95 level
    if (level > 0.0 && level < 1.0) {
        // Refine: erfinv approximation.
        double x = 1.96;
        for (int it = 0; it < 20; ++it) {
            const double ex = std::erf(x / std::sqrt(2.0));
            const double fx = ex - level;
            const double dfx = std::sqrt(2.0 / std::numbers::pi) * std::exp(-0.5 * x * x);
            if (std::abs(dfx) < 1.0e-15) break;
            x -= fx / dfx;
        }
        zq = x;
    }

    lower = z0 - zq * seZ;
    upper = z0 + zq * seZ;
}

// =============================================================================
//  GoF tests
// =============================================================================

GoFResult EvtAnalyzer::_gofGpd(const std::vector<double>& exc,
                                 const GpdFitResult& gpd) const
{
    GoFResult gof;
    if (exc.empty()) return gof;

    std::vector<double> sorted = exc;
    std::sort(sorted.begin(), sorted.end());
    const int n = static_cast<int>(sorted.size());

    // KS statistic: max |F_empirical(x) - F_gpd(x)|.
    double ksMax = 0.0;
    double adSum = 0.0;
    for (int i = 0; i < n; ++i) {
        const double fi  = static_cast<double>(i + 1) / static_cast<double>(n);
        const double fim = static_cast<double>(i) / static_cast<double>(n);
        const double fc  = Gpd::cdf(sorted[static_cast<std::size_t>(i)], gpd.xi, gpd.sigma);

        ksMax = std::max(ksMax, std::abs(fi - fc));
        ksMax = std::max(ksMax, std::abs(fim - fc));

        // Anderson-Darling: uses ordered CDF values.
        const double u = fc;
        if (u > 1.0e-10 && u < 1.0 - 1.0e-10) {
            const double u2 = Gpd::cdf(sorted[static_cast<std::size_t>(n - 1 - i)],
                                        gpd.xi, gpd.sigma);
            const int j = i + 1;
            adSum += static_cast<double>(2 * j - 1)
                   * (std::log(u) + std::log(1.0 - u2));
        }
    }

    gof.ksStatistic = ksMax;
    // KS p-value (approximation for n > 35): Kolmogorov distribution.
    const double ksz = ksMax * (std::sqrt(static_cast<double>(n))
                                 + 0.12 + 0.11 / std::sqrt(static_cast<double>(n)));
    // P(D < d) ~ 1 - 2*sum_{k=1}^{inf} (-1)^{k+1} exp(-2*k^2*z^2)
    // Approximate with first term for z > 0.5.
    gof.ksPvalue = (ksz > 0.0)
        ? std::exp(-2.0 * ksz * ksz)
        : 1.0;
    gof.ksPvalue = std::min(1.0, std::max(0.0, 2.0 * gof.ksPvalue));

    // Anderson-Darling statistic.
    const double an = static_cast<double>(n);
    gof.adStatistic = -an - adSum / an;
    // A^2 p-value: Marsaglia & Marsaglia (2004) approximation.
    const double z = gof.adStatistic;
    if (z < 0.2)       { gof.adPvalue = 1.0 - std::exp(-13.436 + 101.14 * z - 223.73 * z * z); }
    else if (z < 0.34) { gof.adPvalue = 1.0 - std::exp(-8.318 + 42.796 * z - 59.938 * z * z); }
    else if (z < 0.6)  { gof.adPvalue = std::exp(0.9177 - 4.279 * z - 1.38 * z * z); }
    else if (z < 10.0) { gof.adPvalue = std::exp(1.2937 - 5.709 * z + 0.0186 * z * z); }
    else               { gof.adPvalue = 0.0; }

    return gof;
}

GoFResult EvtAnalyzer::_gofGev(const std::vector<double>& maxima,
                                 const GevFitResult& gev) const
{
    GoFResult gof;
    if (maxima.empty()) return gof;

    std::vector<double> sorted = maxima;
    std::sort(sorted.begin(), sorted.end());
    const int n = static_cast<int>(sorted.size());

    double ksMax = 0.0;
    double adSum = 0.0;

    for (int i = 0; i < n; ++i) {
        const double fi  = static_cast<double>(i + 1) / static_cast<double>(n);
        const double fim = static_cast<double>(i) / static_cast<double>(n);
        const double fc  = Gev::cdf(sorted[static_cast<std::size_t>(i)],
                                     gev.xi, gev.sigma, gev.mu);

        ksMax = std::max(ksMax, std::abs(fi - fc));
        ksMax = std::max(ksMax, std::abs(fim - fc));

        const double u = fc;
        if (u > 1.0e-10 && u < 1.0 - 1.0e-10) {
            const double u2 = Gev::cdf(sorted[static_cast<std::size_t>(n - 1 - i)],
                                        gev.xi, gev.sigma, gev.mu);
            const int j = i + 1;
            adSum += static_cast<double>(2 * j - 1)
                   * (std::log(u) + std::log(1.0 - u2));
        }
    }

    gof.ksStatistic = ksMax;
    const double ksz = ksMax * (std::sqrt(static_cast<double>(n))
                                 + 0.12 + 0.11 / std::sqrt(static_cast<double>(n)));
    gof.ksPvalue = std::min(1.0, std::max(0.0,
        2.0 * std::exp(-2.0 * ksz * ksz)));

    const double an = static_cast<double>(n);
    gof.adStatistic = -an - adSum / an;
    const double z  = gof.adStatistic;
    if (z < 0.2)       { gof.adPvalue = 1.0 - std::exp(-13.436 + 101.14 * z - 223.73 * z * z); }
    else if (z < 0.34) { gof.adPvalue = 1.0 - std::exp(-8.318 + 42.796 * z - 59.938 * z * z); }
    else if (z < 0.6)  { gof.adPvalue = std::exp(0.9177 - 4.279 * z - 1.38 * z * z); }
    else if (z < 10.0) { gof.adPvalue = std::exp(1.2937 - 5.709 * z + 0.0186 * z * z); }
    else               { gof.adPvalue = 0.0; }

    return gof;
}

// =============================================================================
//  Protocol output
// =============================================================================

void EvtAnalyzer::_writeProtocol(const EvtResult& r,
                                   const TimeSeries& ts,
                                   const std::string& datasetName) const
{
    const std::string comp = ts.metadata().componentName;
    const std::string fname = m_cfg.protocolsDir.string()
        + "/evt_" + datasetName + "_" + comp + "_protocol.txt";

    std::ofstream ofs(fname);
    if (!ofs.is_open()) {
        LOKI_WARNING("EVT: cannot write protocol to '" + fname + "'.");
        return;
    }

    ofs << std::fixed << std::setprecision(6);
    ofs << "============================================================\n";
    ofs << "  LOKI EVT PROTOCOL\n";
    ofs << "  Dataset:   " << datasetName << "\n";
    ofs << "  Component: " << comp << "\n";
    ofs << "  Method:    " << r.method << "\n";
    ofs << "============================================================\n\n";

    if (m_cfg.evt.deseasonalization.enabled
        && m_cfg.evt.deseasonalization.strategy != "none")
    {
        ofs << "NOTE: EVT was applied to deseasonalized residuals\n";
        ofs << "      (strategy: " << m_cfg.evt.deseasonalization.strategy << ").\n";
        ofs << "      Return levels are in units of residuals,\n";
        ofs << "      not original series values.\n\n";
    }

    if (r.method == "pot") {
        const GpdFitResult& g = r.gpd;
        ofs << "--- GPD Fit (Peaks-Over-Threshold) ---\n";
        ofs << "  Threshold:      " << g.threshold    << "\n";
        ofs << "  Exceedances:    " << g.nExceedances  << "\n";
        ofs << "  Lambda (rate):  " << g.lambda        << "  (per "
            << m_cfg.evt.timeUnit << ")\n";
        ofs << "  xi (shape):     " << g.xi            << "\n";
        ofs << "  sigma (scale):  " << g.sigma         << "\n";
        ofs << "  Log-likelihood: " << g.logLik        << "\n";
        ofs << "  Converged:      " << (g.converged ? "yes" : "no (PWM fallback)") << "\n\n";
    } else {
        const GevFitResult& g = r.gev;
        ofs << "--- GEV Fit (Block Maxima) ---\n";
        ofs << "  Block maxima:   " << g.nBlockMaxima  << "\n";
        ofs << "  xi (shape):     " << g.xi            << "\n";
        ofs << "  sigma (scale):  " << g.sigma         << "\n";
        ofs << "  mu (location):  " << g.mu            << "\n";
        ofs << "  Log-likelihood: " << g.logLik        << "\n";
        ofs << "  Converged:      " << (g.converged ? "yes" : "no") << "\n\n";
    }

    ofs << "--- Goodness of Fit ---\n";
    ofs << "  Anderson-Darling: A^2 = " << r.gof.adStatistic
        << "  p = " << r.gof.adPvalue << "\n";
    ofs << "  Kolmogorov-Smirnov: D = " << r.gof.ksStatistic
        << "  p = " << r.gof.ksPvalue << "\n\n";

    ofs << "--- Return Levels (" << m_cfg.evt.ci.method << " CI, "
        << static_cast<int>(m_cfg.evt.confidenceLevel * 100.0) << "%) ---\n";
    ofs << std::setw(20) << "Return Period"
        << std::setw(16) << "Estimate"
        << std::setw(16) << "Lower"
        << std::setw(16) << "Upper" << "\n";
    ofs << std::string(68, '-') << "\n";
    for (const auto& rl : r.returnLevels) {
        ofs << std::setw(20) << rl.period
            << std::setw(16) << rl.estimate
            << std::setw(16) << rl.lower
            << std::setw(16) << rl.upper << "\n";
    }
    ofs << "\n";

    LOKI_INFO("EVT: protocol written to '" + fname + "'.");
}

// =============================================================================
//  CSV output
// =============================================================================

void EvtAnalyzer::_writeCsv(const EvtResult& r,
                               const std::string& datasetName,
                               const std::string& component) const
{
    const std::string fname = m_cfg.csvDir.string()
        + "/evt_" + datasetName + "_" + component + "_return_levels.csv";

    std::ofstream ofs(fname);
    if (!ofs.is_open()) {
        LOKI_WARNING("EVT: cannot write CSV to '" + fname + "'.");
        return;
    }

    ofs << std::fixed << std::setprecision(6);
    ofs << "return_period;estimate;lower_ci;upper_ci\n";
    for (const auto& rl : r.returnLevels) {
        ofs << rl.period   << ";"
            << rl.estimate << ";"
            << rl.lower    << ";"
            << rl.upper    << "\n";
    }

    LOKI_INFO("EVT: return levels CSV written to '" + fname + "'.");
}

// =============================================================================
//  Helpers
// =============================================================================

std::vector<double> EvtAnalyzer::_extractValid(const TimeSeries& ts)
{
    std::vector<double> v;
    v.reserve(ts.size());
    for (std::size_t i = 0; i < ts.size(); ++i)
        if (isValid(ts[i])) v.push_back(ts[i].value);
    return v;
}

double EvtAnalyzer::_medianDtHours(const TimeSeries& ts)
{
    if (ts.size() < 2) return 1.0;

    std::vector<double> dts;
    dts.reserve(ts.size() - 1);
    for (std::size_t i = 1; i < ts.size(); ++i) {
        const double dt = (ts[i].time.mjd() - ts[i - 1].time.mjd()) * 24.0;
        if (dt > 0.0) dts.push_back(dt);
    }
    if (dts.empty()) return 1.0;

    std::sort(dts.begin(), dts.end());
    const std::size_t mid = dts.size() / 2;
    return (dts.size() % 2 == 0)
        ? 0.5 * (dts[mid - 1] + dts[mid])
        : dts[mid];
}

double EvtAnalyzer::_brentRoot(std::function<double(double)> f,
                                double a, double b,
                                double tol,
                                int maxIter)
{
    double fa = f(a);
    double fb = f(b);

    // If both sides have the same sign, return the endpoint with smaller |f|.
    if (fa * fb > 0.0) {
        return (std::abs(fa) < std::abs(fb)) ? a : b;
    }

    if (std::abs(fa) < std::abs(fb)) { std::swap(a, b); std::swap(fa, fb); }

    double c = a, fc = fa;
    bool mflag = true;
    double s = 0.0, d = 0.0;

    for (int i = 0; i < maxIter; ++i) {
        if (std::abs(b - a) < tol) break;

        if (fa != fc && fb != fc) {
            // Inverse quadratic interpolation.
            s = a * fb * fc / ((fa - fb) * (fa - fc))
              + b * fa * fc / ((fb - fa) * (fb - fc))
              + c * fa * fb / ((fc - fa) * (fc - fb));
        } else {
            // Secant method.
            s = b - fb * (b - a) / (fb - fa);
        }

        const bool cond1 = (s < (3.0 * a + b) / 4.0) || (s > b);
        const bool cond2 = mflag && (std::abs(s - b) >= std::abs(b - c) / 2.0);
        const bool cond3 = !mflag && (std::abs(s - b) >= std::abs(c - d) / 2.0);
        const bool cond4 = mflag && (std::abs(b - c) < tol);
        const bool cond5 = !mflag && (std::abs(c - d) < tol);

        if (cond1 || cond2 || cond3 || cond4 || cond5) {
            s = (a + b) / 2.0;
            mflag = true;
        } else {
            mflag = false;
        }

        const double fs = f(s);
        d = c; c = b; fc = fb;

        if (fa * fs < 0.0) { b = s; fb = fs; }
        else               { a = s; fa = fs; }

        if (std::abs(fa) < std::abs(fb)) { std::swap(a, b); std::swap(fa, fb); }
    }
    return b;
}

} // namespace loki::evt
