#include "loki/kalman/kalmanAnalyzer.hpp"

#include "loki/kalman/emEstimator.hpp"
#include "loki/kalman/kalmanFilter.hpp"
#include "loki/kalman/kalmanModel.hpp"
#include "loki/kalman/kalmanModelBuilder.hpp"
#include "loki/kalman/plotKalman.hpp"
#include "loki/kalman/rtsSmoother.hpp"

#include "loki/core/exceptions.hpp"
#include "loki/core/logger.hpp"
#include "loki/stats/descriptive.hpp"
#include "loki/timeseries/deseasonalizer.hpp"
#include "loki/timeseries/gapFiller.hpp"
#include "loki/timeseries/medianYearSeries.hpp"

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <vector>

using namespace loki;

namespace loki::kalman {

// Seconds per MJD day
static constexpr double SECONDS_PER_DAY = 86400.0;

// MJD step threshold below which data is considered 6h climatological
// (6h = 0.25 days; add a small tolerance)
static constexpr double DT_6H_MAX_DAYS = 0.30;

// -----------------------------------------------------------------------------
//  Construction
// -----------------------------------------------------------------------------

KalmanAnalyzer::KalmanAnalyzer(const AppConfig& cfg)
    : m_cfg(cfg)
{}

// -----------------------------------------------------------------------------
//  run -- public entry point
// -----------------------------------------------------------------------------

void KalmanAnalyzer::run(const TimeSeries& series, const std::string& datasetName)
{
    const std::string component = series.metadata().componentName;
    LOKI_INFO("KalmanAnalyzer: processing '" + component + "' from '" + datasetName + "'.");

    if (series.size() < 4) {
        throw SeriesTooShortException(
            "KalmanAnalyzer: series '" + component + "' has fewer than 4 observations.");
    }

    // 1. Gap filling
    const TimeSeries filled = fillGaps(series);

    // 2. Sampling interval
    const double dt = estimateDt(filled);
    LOKI_INFO("KalmanAnalyzer: estimated dt = " + std::to_string(dt) + " s.");

    // 3. Extract measurements
    std::vector<double> times;
    std::vector<double> measurements;
    extractMeasurements(filled, times, measurements);

    // Initialise x0 from first finite observation
    double x0 = 0.0;
    for (const double v : measurements) {
        if (std::isfinite(v)) { x0 = v; break; }
    }

    // 4. Noise estimation
    double q = 0.0, r = 0.0;
    int    emIter      = 0;
    bool   emConverged = false;
    estimateNoise(measurements, q, r, emIter, emConverged);
    LOKI_INFO("KalmanAnalyzer: Q=" + std::to_string(q) + "  R=" + std::to_string(r));

    // 5. Build model
    const std::string modelName = m_cfg.kalman.model;
    KalmanModel model;
    if (modelName == "local_level") {
        model = KalmanModelBuilder::localLevel(q, r, x0);
    } else if (modelName == "local_trend") {
        model = KalmanModelBuilder::localTrend(dt, q, r, x0);
    } else if (modelName == "constant_velocity") {
        model = KalmanModelBuilder::constantVelocity(dt, q, r, x0);
    } else {
        throw ConfigException(
            "KalmanAnalyzer: unknown model '" + modelName + "'. "
            "Valid: local_level, local_trend, constant_velocity.");
    }

    // 6. Forward pass
    KalmanFilter kf(model);
    const auto filterSteps = kf.run(measurements);
    const double ll = kf.logLikelihood(filterSteps);
    LOKI_INFO("KalmanAnalyzer: log-likelihood = " + std::to_string(ll));

    // 7. RTS smoother
    std::vector<SmootherStep> smootherSteps;
    const std::string smootherName = m_cfg.kalman.smoother;
    if (smootherName == "rts") {
        RtsSmoother rts(model);
        smootherSteps = rts.smooth(filterSteps);
        LOKI_INFO("KalmanAnalyzer: RTS smoother applied.");
    }

    // 8. Forecast
    std::vector<double> forecastTimes, forecastState, forecastStd;
    const int forecastSteps = m_cfg.kalman.forecast.steps;
    if (forecastSteps > 0 && !filterSteps.empty()) {
        const auto& lastStep = filterSteps.back();
        generateForecast(lastStep.xFilt, lastStep.PFilt, model,
                         times.back(), dt, forecastSteps,
                         forecastTimes, forecastState, forecastStd);
        LOKI_INFO("KalmanAnalyzer: generated " + std::to_string(forecastSteps)
                  + " forecast steps.");
    }

    // 9. Assemble result
    // Original (pre-gap-fill) values: re-extract from the raw series
    std::vector<double> originalTimes, originalValues;
    extractMeasurements(series, originalTimes, originalValues);

    // times from filled series; original values from raw series (aligned by index)
    const KalmanResult result = assembleResult(
        times, originalValues,
        filterSteps, smootherSteps,
        forecastTimes, forecastState, forecastStd,
        ll, q, r, emIter, emConverged,
        modelName, m_cfg.kalman.noise.estimation, smootherName);

    // 10. Plots
    PlotKalman plotter(m_cfg);
    plotter.plotAll(result, datasetName, component);

    // 11. Protocol
    writeProtocol(result, datasetName, component);

    LOKI_INFO("KalmanAnalyzer: finished '" + component + "'.");
}

// -----------------------------------------------------------------------------
//  fillGaps
// -----------------------------------------------------------------------------

TimeSeries KalmanAnalyzer::fillGaps(const TimeSeries& series) const
{
    GapFiller::Config gfCfg;
    gfCfg.maxFillLength = static_cast<std::size_t>(m_cfg.kalman.gapFillMaxLength);

    // Choose strategy
    const std::string& stratStr = m_cfg.kalman.gapFillStrategy;

    if (stratStr == "none") {
        gfCfg.strategy = GapFiller::Strategy::NONE;
        GapFiller gf(gfCfg);
        return gf.fill(series);
    }

    if (stratStr == "linear") {
        gfCfg.strategy = GapFiller::Strategy::LINEAR;
        GapFiller gf(gfCfg);
        return gf.fill(series);
    }

    if (stratStr == "median_year") {
        gfCfg.strategy = GapFiller::Strategy::MEDIAN_YEAR;
        GapFiller gf(gfCfg);
        // Build median year profile
        MedianYearSeries mys(series);
        return gf.fill(series, [&mys](const ::TimeStamp& ts) {
            return mys.valueAt(ts);
        });
    }

    if (stratStr == "auto") {
        // Auto: use MEDIAN_YEAR for 6h climatological data with long series,
        // otherwise LINEAR.
        const double dtDays = [&]() -> double {
            if (series.size() < 2) { return 1.0; }
            std::vector<double> diffs;
            diffs.reserve(series.size() - 1);
            for (std::size_t i = 1; i < series.size(); ++i) {
                diffs.push_back(series[i].time.mjd() - series[i-1].time.mjd());
            }
            std::nth_element(diffs.begin(), diffs.begin() + diffs.size() / 2, diffs.end());
            return diffs[diffs.size() / 2];
        }();

        const double spanYears = (series.size() > 1)
            ? (series[series.size()-1].time.mjd() - series[0].time.mjd()) / 365.25
            : 0.0;

        if (dtDays < DT_6H_MAX_DAYS && spanYears >= gfCfg.minSeriesYears) {
            gfCfg.strategy = GapFiller::Strategy::MEDIAN_YEAR;
            GapFiller gf(gfCfg);
            MedianYearSeries mys(series);
            return gf.fill(series, [&mys](const ::TimeStamp& ts) {
                return mys.valueAt(ts);
            });
        }

        gfCfg.strategy = GapFiller::Strategy::LINEAR;
        GapFiller gf(gfCfg);
        return gf.fill(series);
    }

    // Fallback
    LOKI_WARNING("KalmanAnalyzer: unknown gap_fill_strategy '" + stratStr
                 + "' -- using linear.");
    gfCfg.strategy = GapFiller::Strategy::LINEAR;
    GapFiller gf(gfCfg);
    return gf.fill(series);
}

// -----------------------------------------------------------------------------
//  estimateDt
// -----------------------------------------------------------------------------

double KalmanAnalyzer::estimateDt(const TimeSeries& series) const
{
    if (series.size() < 2) {
        throw DataException("KalmanAnalyzer::estimateDt: series has fewer than 2 observations.");
    }

    std::vector<double> diffs;
    diffs.reserve(series.size() - 1);
    for (std::size_t i = 1; i < series.size(); ++i) {
        const double d = series[i].time.mjd() - series[i-1].time.mjd();
        if (d > 0.0) { diffs.push_back(d); }
    }

    if (diffs.empty()) {
        throw AlgorithmException("KalmanAnalyzer::estimateDt: no positive time differences found.");
    }

    std::nth_element(diffs.begin(), diffs.begin() + diffs.size() / 2, diffs.end());
    const double medianDays = diffs[diffs.size() / 2];
    return medianDays * SECONDS_PER_DAY;
}

// -----------------------------------------------------------------------------
//  extractMeasurements
// -----------------------------------------------------------------------------

void KalmanAnalyzer::extractMeasurements(const TimeSeries&    series,
                                          std::vector<double>& times,
                                          std::vector<double>& values)
{
    const std::size_t n = series.size();
    times.resize(n);
    values.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        times[i]  = series[i].time.mjd();
        values[i] = series[i].value;  // NaN if missing
    }
}

// -----------------------------------------------------------------------------
//  estimateNoise
// -----------------------------------------------------------------------------

void KalmanAnalyzer::estimateNoise(const std::vector<double>& measurements,
                                    double&                    qOut,
                                    double&                    rOut,
                                    int&                       emIterations,
                                    bool&                      emConverged) const
{
    const std::string& method = m_cfg.kalman.noise.estimation;

    if (method == "manual") {
        qOut         = m_cfg.kalman.noise.Q;
        rOut         = m_cfg.kalman.noise.R;
        emIterations = 0;
        emConverged  = false;
        return;
    }

    if (method == "heuristic") {
        // R = variance of finite measurements, Q = R / smoothingFactor
        std::vector<double> valid;
        valid.reserve(measurements.size());
        for (const double v : measurements) {
            if (std::isfinite(v)) { valid.push_back(v); }
        }
        if (valid.size() < 2) {
            throw DataException(
                "KalmanAnalyzer: heuristic noise estimation requires at least 2 valid observations.");
        }
        const double mean = std::accumulate(valid.begin(), valid.end(), 0.0)
                            / static_cast<double>(valid.size());
        double var = 0.0;
        for (const double v : valid) { var += (v - mean) * (v - mean); }
        var /= static_cast<double>(valid.size() - 1);

        rOut         = std::max(var, 1.0e-12);
        qOut         = rOut / std::max(m_cfg.kalman.noise.smoothingFactor, 1.0);
        emIterations = 0;
        emConverged  = false;
        return;
    }

    if (method == "em") {
        // Build an initial model for EM to start from
        const double qInit = m_cfg.kalman.noise.QInit;
        const double rInit = m_cfg.kalman.noise.RInit;

        // Temporary model just to provide the right n for EmEstimator
        // The EM estimator needs F and H -- we must know the model name here.
        // We build a temporary model with the initial noise guess.
        const std::string modelName = m_cfg.kalman.model;
        KalmanModel tmpModel;
        if (modelName == "local_level") {
            tmpModel = KalmanModelBuilder::localLevel(qInit, rInit);
        } else if (modelName == "local_trend" || modelName == "constant_velocity") {
            // dt unknown here; use 1.0 -- EM only updates Q/R, not F
            tmpModel = (modelName == "local_trend")
                ? KalmanModelBuilder::localTrend(1.0, qInit, rInit)
                : KalmanModelBuilder::constantVelocity(1.0, qInit, rInit);
        } else {
            throw ConfigException(
                "KalmanAnalyzer::estimateNoise: unknown model '" + modelName + "'.");
        }

        EmEstimator em(tmpModel, m_cfg.kalman.noise.emMaxIter, m_cfg.kalman.noise.emTol);
        const auto [finalModel, emResult] = em.estimate(measurements);

        qOut         = emResult.estimatedQ;
        rOut         = emResult.estimatedR;
        emIterations = emResult.iterations;
        emConverged  = emResult.converged;

        if (!emConverged) {
            LOKI_WARNING("KalmanAnalyzer: EM did not converge in "
                         + std::to_string(m_cfg.kalman.noise.emMaxIter) + " iterations.");
        }
        return;
    }

    throw ConfigException(
        "KalmanAnalyzer: unknown noise estimation method '" + method + "'. "
        "Valid: manual, heuristic, em.");
}

// -----------------------------------------------------------------------------
//  generateForecast
// -----------------------------------------------------------------------------

void KalmanAnalyzer::generateForecast(const Eigen::VectorXd&  lastXFilt,
                                       const Eigen::MatrixXd&  lastPFilt,
                                       const KalmanModel&      model,
                                       double                  lastMjd,
                                       double                  dt,
                                       int                     steps,
                                       std::vector<double>&    forecastTimes,
                                       std::vector<double>&    forecastState,
                                       std::vector<double>&    forecastStd)
{
    forecastTimes.resize(steps);
    forecastState.resize(steps);
    forecastStd.resize(steps);

    Eigen::VectorXd x = lastXFilt;
    Eigen::MatrixXd P = lastPFilt;
    const double dtDays = dt / SECONDS_PER_DAY;

    for (int k = 0; k < steps; ++k) {
        x = model.F * x;
        P = model.F * P * model.F.transpose() + model.Q;

        forecastTimes[k] = lastMjd + static_cast<double>(k + 1) * dtDays;
        forecastState[k] = x(0);
        forecastStd[k]   = std::sqrt(std::max(P(0, 0), 0.0));
    }
}

// -----------------------------------------------------------------------------
//  assembleResult
// -----------------------------------------------------------------------------

KalmanResult KalmanAnalyzer::assembleResult(
    const std::vector<double>&        times,
    const std::vector<double>&        original,
    const std::vector<FilterStep>&    filterSteps,
    const std::vector<SmootherStep>&  smootherSteps,
    const std::vector<double>&        forecastTimes,
    const std::vector<double>&        forecastState,
    const std::vector<double>&        forecastStd,
    double                            logLikelihood,
    double                            q,
    double                            r,
    int                               emIterations,
    bool                              emConverged,
    const std::string&                modelName,
    const std::string&                noiseMethod,
    const std::string&                smoother)
{
    KalmanResult res;
    res.times    = times;
    res.original = original;

    const std::size_t n = filterSteps.size();
    res.filteredState.resize(n);
    res.filteredStd.resize(n);
    res.predictedState.resize(n);
    res.innovations.resize(n);
    res.innovationStd.resize(n);
    res.gains.resize(n);

    for (std::size_t i = 0; i < n; ++i) {
        const auto& s = filterSteps[i];
        res.filteredState[i]  = s.xFilt(0);
        res.filteredStd[i]    = std::sqrt(std::max(s.PFilt(0, 0), 0.0));
        res.predictedState[i] = s.xPred(0);
        res.innovations[i]    = s.innovation;
        res.innovationStd[i]  = s.hasObservation ? std::sqrt(std::max(s.innovationVar, 0.0))
                                                  : std::numeric_limits<double>::quiet_NaN();
        res.gains[i]          = s.kalmanGain0;
    }

    if (!smootherSteps.empty()) {
        res.smoothedState.resize(n);
        res.smoothedStd.resize(n);
        for (std::size_t i = 0; i < n; ++i) {
            res.smoothedState[i] = smootherSteps[i].xSmooth(0);
            res.smoothedStd[i]   = std::sqrt(std::max(smootherSteps[i].PSmooth(0, 0), 0.0));
        }
    }

    res.forecastTimes = forecastTimes;
    res.forecastState = forecastState;
    res.forecastStd   = forecastStd;

    res.logLikelihood = logLikelihood;
    res.estimatedQ    = q;
    res.estimatedR    = r;
    res.emIterations  = emIterations;
    res.emConverged   = emConverged;
    res.modelName     = modelName;
    res.noiseMethod   = noiseMethod;
    res.smoother      = smoother;

    return res;
}

// -----------------------------------------------------------------------------
//  writeProtocol
// -----------------------------------------------------------------------------

void KalmanAnalyzer::writeProtocol(const KalmanResult& result,
                                    const std::string&  datasetName,
                                    const std::string&  componentName) const
{
    const std::string fname =
        "kalman_" + datasetName + "_" + componentName + "_protocol.txt";
    const std::filesystem::path outPath = m_cfg.protocolsDir / fname;

    std::ofstream ofs(outPath);
    if (!ofs.is_open()) {
        LOKI_WARNING("KalmanAnalyzer: cannot write protocol to '" + outPath.string() + "'.");
        return;
    }

    // Count observed epochs
    int nObs = 0;
    for (const double v : result.original) {
        if (std::isfinite(v)) { ++nObs; }
    }

    ofs << std::fixed << std::setprecision(6);
    ofs << "=====================================\n";
    ofs << " LOKI Kalman Filter Protocol\n";
    ofs << "=====================================\n";
    ofs << "Dataset   : " << datasetName   << "\n";
    ofs << "Component : " << componentName << "\n";
    ofs << "-------------------------------------\n";
    ofs << "Model     : " << result.modelName   << "\n";
    ofs << "Noise est.: " << result.noiseMethod  << "\n";
    ofs << "Smoother  : " << result.smoother     << "\n";
    ofs << "-------------------------------------\n";
    ofs << "N total   : " << result.times.size() << "\n";
    ofs << "N observed: " << nObs                << "\n";
    ofs << "Estimated Q: " << result.estimatedQ  << "\n";
    ofs << "Estimated R: " << result.estimatedR  << "\n";
    ofs << "Log-likelihood: " << result.logLikelihood << "\n";

    if (result.noiseMethod == "em") {
        ofs << "EM iterations: " << result.emIterations << "\n";
        ofs << "EM converged : " << (result.emConverged ? "yes" : "no") << "\n";
    }

    if (!result.forecastState.empty()) {
        ofs << "-------------------------------------\n";
        ofs << "Forecast steps: " << result.forecastState.size() << "\n";
        ofs << "Forecast start MJD: " << result.forecastTimes.front() << "\n";
        ofs << "Forecast end MJD  : " << result.forecastTimes.back()  << "\n";
        ofs << "Final forecast value: " << result.forecastState.back() << "\n";
        ofs << "Final forecast std  : " << result.forecastStd.back()   << "\n";
    }

    ofs << "=====================================\n";
    ofs.close();

    LOKI_INFO("KalmanAnalyzer: protocol -> " + outPath.filename().string());
}

} // namespace loki::kalman
