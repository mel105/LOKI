#include <loki/simulate/simulateAnalyzer.hpp>

#include <loki/simulate/arimaSimulator.hpp>
#include <loki/simulate/kalmanSimulator.hpp>
#include <loki/simulate/plotSimulate.hpp>

#include <loki/arima/arimaAnalyzer.hpp>
#include <loki/kalman/kalmanModelBuilder.hpp>

#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/timeseries/gapFiller.hpp>
#include <loki/stats/bootstrap.hpp>
#include <loki/stats/sampling.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <random>
#include <sstream>
#include <string>

using namespace loki;

namespace loki::simulate {

// -----------------------------------------------------------------------------
//  Constructor
// -----------------------------------------------------------------------------

SimulateAnalyzer::SimulateAnalyzer(const AppConfig& cfg)
    : m_cfg(cfg)
{}

// -----------------------------------------------------------------------------
//  Public: runSynthetic
// -----------------------------------------------------------------------------

void SimulateAnalyzer::runSynthetic(const std::string& datasetName)
{
    const SimulateConfig& sc = m_cfg.simulate;

    LOKI_INFO("SimulateAnalyzer: synthetic mode, model=" + sc.model
              + ", n=" + std::to_string(sc.n)
              + ", nSim=" + std::to_string(sc.nSimulations));

    SimulationResult result;
    if (sc.model == "arima" || sc.model == "ar") {
        result = _runArimaSynthetic(datasetName);
    } else if (sc.model == "kalman") {
        result = _runKalmanSynthetic(datasetName);
    } else {
        throw ConfigException(
            "SimulateAnalyzer: unknown model '" + sc.model
            + "'. Valid: arima, ar, kalman.");
    }

    _computeEnvelope(result);
    _computeSummaryStats(result);
    _writeProtocol(result);
    _writeCsv(result);

    PlotSimulate plotter(m_cfg);
    plotter.plotAll(result);
}

// -----------------------------------------------------------------------------
//  Public: run (bootstrap)
// -----------------------------------------------------------------------------

void SimulateAnalyzer::run(const TimeSeries& series, const std::string& datasetName)
{
    const SimulateConfig& sc = m_cfg.simulate;

    LOKI_INFO("SimulateAnalyzer: bootstrap mode, model=" + sc.model
              + ", component=" + series.metadata().componentName);

    SimulationResult result;
    if (sc.model == "arima" || sc.model == "ar") {
        result = _runArimaBootstrap(series, datasetName);
    } else if (sc.model == "kalman") {
        result = _runKalmanBootstrap(series, datasetName);
    } else {
        throw ConfigException(
            "SimulateAnalyzer: unknown model '" + sc.model
            + "'. Valid: arima, ar, kalman.");
    }

    _computeEnvelope(result);
    _computeSummaryStats(result);
    _writeProtocol(result);
    _writeCsv(result);

    PlotSimulate plotter(m_cfg);
    plotter.plotAll(result);
}

// -----------------------------------------------------------------------------
//  Private: _runArimaSynthetic
// -----------------------------------------------------------------------------

SimulationResult SimulateAnalyzer::_runArimaSynthetic(const std::string& datasetName) const
{
    const SimulateConfig& sc = m_cfg.simulate;

    ArimaSimulator::ParametricConfig simCfg;
    simCfg.p     = sc.arima.p;
    simCfg.d     = sc.arima.d;
    simCfg.q     = sc.arima.q;
    simCfg.sigma = sc.arima.sigma;
    simCfg.seed  = sc.seed;

    ArimaSimulator sim(simCfg);
    auto simulations = sim.generateBatch(sc.n, sc.nSimulations);

    _injectAnomalies(simulations, sc.seed + 1000000ULL);

    SimulationResult result;
    result.datasetName   = datasetName;
    result.componentName = "synthetic";
    result.mode          = "synthetic";
    result.model         = sc.model;
    result.n             = sc.n;
    result.nSimulations  = sc.nSimulations;
    result.simulations   = std::move(simulations);

    return result;
}

// -----------------------------------------------------------------------------
//  Private: _runKalmanSynthetic
// -----------------------------------------------------------------------------

SimulationResult SimulateAnalyzer::_runKalmanSynthetic(const std::string& datasetName) const
{
    const SimulateConfig& sc = m_cfg.simulate;

    KalmanSimulator::Config simCfg;
    simCfg.model = sc.kalman.model;
    simCfg.Q     = sc.kalman.Q;
    simCfg.R     = sc.kalman.R;
    simCfg.dt    = 1.0;  // Default 1s; meaningful dt comes from real data in bootstrap mode.
    simCfg.seed  = sc.seed;

    KalmanSimulator sim(simCfg);
    auto simulations = sim.generateBatch(sc.n, sc.nSimulations);

    _injectAnomalies(simulations, sc.seed + 1000000ULL);

    SimulationResult result;
    result.datasetName   = datasetName;
    result.componentName = "synthetic";
    result.mode          = "synthetic";
    result.model         = sc.model;
    result.n             = sc.n;
    result.nSimulations  = sc.nSimulations;
    result.simulations   = std::move(simulations);

    return result;
}

// -----------------------------------------------------------------------------
//  Private: _runArimaBootstrap
// -----------------------------------------------------------------------------

SimulationResult SimulateAnalyzer::_runArimaBootstrap(
    const TimeSeries& series, const std::string& datasetName) const
{
    const SimulateConfig& sc = m_cfg.simulate;

    // Step 1: gap fill.
    TimeSeries filled = _fillGaps(series);
    std::vector<double> values = _extractValues(filled);

    if (values.size() < 10) {
        throw SeriesTooShortException(
            "SimulateAnalyzer: series '" + series.metadata().componentName
            + "' has fewer than 10 valid observations after gap filling.");
    }

    // Step 2: fit ARIMA.
    loki::arima::ArimaAnalyzer arimaAnalyzer(m_cfg.arima);
    const loki::arima::ArimaResult fitResult = arimaAnalyzer.analyze(values);

    LOKI_INFO("SimulateAnalyzer: ARIMA(" + std::to_string(fitResult.order.p)
              + "," + std::to_string(fitResult.order.d)
              + "," + std::to_string(fitResult.order.q)
              + ") fitted, sigma2=" + std::to_string(fitResult.sigma2));

    // Step 3: generate B replicas from fitted model.
    ArimaSimulator sim(fitResult, sc.seed);
    const int n = static_cast<int>(values.size());
    auto simulations = sim.generateBatch(n, sc.nSimulations);

    _injectAnomalies(simulations, sc.seed + 1000000ULL);

    // Step 4: bootstrap CIs for key parameters.
    std::vector<ParameterCI> paramCIs;

    // Bootstrap CI for mean of the series.
    {
        loki::stats::Sampler sampler(sc.seed + 2000000ULL);
        loki::stats::BootstrapConfig bCfg;
        bCfg.nResamples  = sc.nSimulations;
        bCfg.alpha       = 1.0 - sc.confidenceLevel;
        bCfg.blockLength = 0;  // Auto block length from ACF.

        loki::stats::StatFn meanFn = [](const std::vector<double>& v) {
            if (v.empty()) return 0.0;
            return std::accumulate(v.begin(), v.end(), 0.0)
                   / static_cast<double>(v.size());
        };

        loki::stats::BootstrapResult ci;
        if (sc.bootstrapMethod == "percentile") {
            ci = loki::stats::percentileCI(values, meanFn, sampler, bCfg);
        } else if (sc.bootstrapMethod == "bca") {
            ci = loki::stats::bcaCI(values, meanFn, sampler, bCfg);
        } else {
            // Default: block bootstrap (appropriate for autocorrelated series).
            ci = loki::stats::blockCI(values, meanFn, sampler, bCfg);
        }

        ParameterCI pci;
        pci.name     = "mean";
        pci.estimate = ci.estimate;
        pci.lower    = ci.lower;
        pci.upper    = ci.upper;
        pci.bias     = ci.bias;
        pci.se       = ci.se;
        paramCIs.push_back(pci);
    }

    // CI for sigma2 (innovation variance).
    {
        loki::stats::Sampler sampler(sc.seed + 3000000ULL);
        loki::stats::BootstrapConfig bCfg;
        bCfg.nResamples  = sc.nSimulations;
        bCfg.alpha       = 1.0 - sc.confidenceLevel;
        bCfg.blockLength = 0;

        loki::stats::StatFn varFn = [](const std::vector<double>& v) {
            if (v.size() < 2) return 0.0;
            const double m = std::accumulate(v.begin(), v.end(), 0.0)
                             / static_cast<double>(v.size());
            double s = 0.0;
            for (double x : v) s += (x - m) * (x - m);
            return s / static_cast<double>(v.size() - 1);
        };

        loki::stats::BootstrapResult ci;
        if (sc.bootstrapMethod == "percentile") {
            ci = loki::stats::percentileCI(values, varFn, sampler, bCfg);
        } else if (sc.bootstrapMethod == "bca") {
            ci = loki::stats::bcaCI(values, varFn, sampler, bCfg);
        } else {
            ci = loki::stats::blockCI(values, varFn, sampler, bCfg);
        }

        ParameterCI pci;
        pci.name     = "sigma2";
        pci.estimate = ci.estimate;
        pci.lower    = ci.lower;
        pci.upper    = ci.upper;
        pci.bias     = ci.bias;
        pci.se       = ci.se;
        paramCIs.push_back(pci);
    }

    // Add AR coefficient CIs (per lag).
    for (std::size_t i = 0; i < fitResult.arCoeffs.size(); ++i) {
        ParameterCI pci;
        pci.name     = "ar[" + std::to_string(fitResult.arLags[i]) + "]";
        pci.estimate = fitResult.arCoeffs[i];
        // CIs for individual AR coefficients require refitting on each bootstrap
        // replica -- deferred to a future extension; report estimate only.
        pci.lower = pci.estimate;
        pci.upper = pci.estimate;
        pci.bias  = 0.0;
        pci.se    = 0.0;
        paramCIs.push_back(pci);
    }

    SimulationResult result;
    result.datasetName   = datasetName;
    result.componentName = series.metadata().componentName;
    result.mode          = "bootstrap";
    result.model         = sc.model;
    result.n             = n;
    result.nSimulations  = sc.nSimulations;
    result.original      = values;
    result.simulations   = std::move(simulations);
    result.parameterCIs  = std::move(paramCIs);

    return result;
}

// -----------------------------------------------------------------------------
//  Private: _runKalmanBootstrap
// -----------------------------------------------------------------------------

SimulationResult SimulateAnalyzer::_runKalmanBootstrap(
    const TimeSeries& series, const std::string& datasetName) const
{
    const SimulateConfig& sc = m_cfg.simulate;

    // Step 1: gap fill.
    TimeSeries filled = _fillGaps(series);
    std::vector<double> values = _extractValues(filled);

    if (values.size() < 10) {
        throw SeriesTooShortException(
            "SimulateAnalyzer: series '" + series.metadata().componentName
            + "' has fewer than 10 valid observations after gap filling.");
    }

    // Estimate dt from series.
    double dt = 1.0;
    if (filled.size() >= 2) {
        std::vector<double> diffs;
        diffs.reserve(filled.size() - 1);
        for (std::size_t i = 1; i < filled.size(); ++i) {
            const double d = (filled[i].time.mjd() - filled[i - 1].time.mjd()) * 86400.0;
            if (d > 0.0) diffs.push_back(d);
        }
        if (!diffs.empty()) {
            std::sort(diffs.begin(), diffs.end());
            dt = diffs[diffs.size() / 2];
        }
    }

    // Step 2: use configured Q and R (manual mode for bootstrap -- EM would require
    // full KalmanFilter pass which belongs in loki_kalman, not loki_simulate).
    KalmanSimulator::Config simCfg;
    simCfg.model = sc.kalman.model;
    simCfg.Q     = sc.kalman.Q;
    simCfg.R     = sc.kalman.R;
    simCfg.dt    = dt;
    simCfg.seed  = sc.seed;

    KalmanSimulator sim(simCfg);
    const int n = static_cast<int>(values.size());
    auto simulations = sim.generateBatch(n, sc.nSimulations);

    _injectAnomalies(simulations, sc.seed + 1000000ULL);

    // Step 3: bootstrap CI for mean and variance.
    std::vector<ParameterCI> paramCIs;

    loki::stats::Sampler sampler(sc.seed + 2000000ULL);
    loki::stats::BootstrapConfig bCfg;
    bCfg.nResamples  = sc.nSimulations;
    bCfg.alpha       = 1.0 - sc.confidenceLevel;
    bCfg.blockLength = 0;

    loki::stats::StatFn meanFn = [](const std::vector<double>& v) {
        if (v.empty()) return 0.0;
        return std::accumulate(v.begin(), v.end(), 0.0)
               / static_cast<double>(v.size());
    };

    loki::stats::BootstrapResult ci;
    if (sc.bootstrapMethod == "percentile") {
        ci = loki::stats::percentileCI(values, meanFn, sampler, bCfg);
    } else if (sc.bootstrapMethod == "bca") {
        ci = loki::stats::bcaCI(values, meanFn, sampler, bCfg);
    } else {
        ci = loki::stats::blockCI(values, meanFn, sampler, bCfg);
    }

    {
        ParameterCI pci;
        pci.name     = "mean";
        pci.estimate = ci.estimate;
        pci.lower    = ci.lower;
        pci.upper    = ci.upper;
        pci.bias     = ci.bias;
        pci.se       = ci.se;
        paramCIs.push_back(pci);
    }
    {
        ParameterCI pci;
        pci.name     = "Q";
        pci.estimate = sc.kalman.Q;
        pci.lower    = sc.kalman.Q;
        pci.upper    = sc.kalman.Q;
        pci.bias     = 0.0;
        pci.se       = 0.0;
        paramCIs.push_back(pci);
    }
    {
        ParameterCI pci;
        pci.name     = "R";
        pci.estimate = sc.kalman.R;
        pci.lower    = sc.kalman.R;
        pci.upper    = sc.kalman.R;
        pci.bias     = 0.0;
        pci.se       = 0.0;
        paramCIs.push_back(pci);
    }

    SimulationResult result;
    result.datasetName   = datasetName;
    result.componentName = series.metadata().componentName;
    result.mode          = "bootstrap";
    result.model         = sc.model;
    result.n             = n;
    result.nSimulations  = sc.nSimulations;
    result.original      = values;
    result.simulations   = std::move(simulations);
    result.parameterCIs  = std::move(paramCIs);

    return result;
}

// -----------------------------------------------------------------------------
//  Private: _injectAnomalies
// -----------------------------------------------------------------------------

void SimulateAnalyzer::_injectAnomalies(
    std::vector<std::vector<double>>& simulations,
    uint64_t baseSeed) const
{
    const SimulateConfig& sc = m_cfg.simulate;

    if (!sc.injectOutliers.enabled
        && !sc.injectGaps.enabled
        && !sc.injectShifts.enabled)
    {
        return;
    }

    for (std::size_t s = 0; s < simulations.size(); ++s) {
        std::vector<double>& y = simulations[s];
        const std::size_t n = y.size();
        if (n == 0) continue;

        std::mt19937_64 rng(baseSeed + s);
        std::uniform_int_distribution<std::size_t> idxDist(0, n - 1);
        std::normal_distribution<double> norm(0.0, 1.0);

        // ---- inject outliers ------------------------------------------------
        if (sc.injectOutliers.enabled) {
            const std::size_t nOut = static_cast<std::size_t>(
                std::max(1.0, std::round(sc.injectOutliers.fraction
                                         * static_cast<double>(n))));
            for (std::size_t k = 0; k < nOut; ++k) {
                const std::size_t idx = idxDist(rng);
                y[idx] += sc.injectOutliers.magnitude
                          * (norm(rng) > 0.0 ? 1.0 : -1.0);
            }
        }

        // ---- inject gaps (set to 0 as placeholder -- NaN not used in sim) ---
        if (sc.injectGaps.enabled && sc.injectGaps.nGaps > 0) {
            std::uniform_int_distribution<int> gapLen(1, sc.injectGaps.maxLength);
            for (int g = 0; g < sc.injectGaps.nGaps; ++g) {
                const std::size_t start = idxDist(rng);
                const int len = gapLen(rng);
                for (int k = 0; k < len; ++k) {
                    const std::size_t idx = std::min(start + static_cast<std::size_t>(k),
                                                     n - 1);
                    y[idx] = 0.0;  // Zero-fill for synthetic gaps.
                }
            }
        }

        // ---- inject mean shifts ---------------------------------------------
        if (sc.injectShifts.enabled && sc.injectShifts.nShifts > 0) {
            for (int g = 0; g < sc.injectShifts.nShifts; ++g) {
                const std::size_t start = idxDist(rng);
                const double mag = sc.injectShifts.magnitude
                                   * (norm(rng) > 0.0 ? 1.0 : -1.0);
                for (std::size_t k = start; k < n; ++k) {
                    y[k] += mag;
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------
//  Private: _computeEnvelope
// -----------------------------------------------------------------------------

void SimulateAnalyzer::_computeEnvelope(SimulationResult& result)
{
    if (result.simulations.empty()) return;
    const std::size_t n    = result.simulations[0].size();
    const std::size_t nSim = result.simulations.size();

    result.env05.resize(n);
    result.env25.resize(n);
    result.env50.resize(n);
    result.env75.resize(n);
    result.env95.resize(n);

    std::vector<double> col(nSim);

    auto quantile = [](std::vector<double>& v, double p) -> double {
        if (v.empty()) return 0.0;
        const double idx = p * static_cast<double>(v.size() - 1);
        const std::size_t lo = static_cast<std::size_t>(idx);
        const std::size_t hi = lo + 1;
        if (hi >= v.size()) return v.back();
        return v[lo] + (idx - static_cast<double>(lo)) * (v[hi] - v[lo]);
    };

    for (std::size_t t = 0; t < n; ++t) {
        for (std::size_t s = 0; s < nSim; ++s) {
            col[s] = result.simulations[s][t];
        }
        std::sort(col.begin(), col.end());
        result.env05[t] = quantile(col, 0.05);
        result.env25[t] = quantile(col, 0.25);
        result.env50[t] = quantile(col, 0.50);
        result.env75[t] = quantile(col, 0.75);
        result.env95[t] = quantile(col, 0.95);
    }
}

// -----------------------------------------------------------------------------
//  Private: _computeSummaryStats
// -----------------------------------------------------------------------------

void SimulateAnalyzer::_computeSummaryStats(SimulationResult& result)
{
    if (result.simulations.empty()) {
        result.simMeanMean = result.simMeanStd = 0.0;
        result.simStdMean  = result.simStdStd  = 0.0;
        return;
    }

    const std::size_t nSim = result.simulations.size();
    std::vector<double> means(nSim), stds(nSim);

    for (std::size_t s = 0; s < nSim; ++s) {
        const auto& v = result.simulations[s];
        if (v.empty()) { means[s] = 0.0; stds[s] = 0.0; continue; }
        const double m = std::accumulate(v.begin(), v.end(), 0.0)
                         / static_cast<double>(v.size());
        means[s] = m;
        double sq = 0.0;
        for (double x : v) sq += (x - m) * (x - m);
        stds[s] = (v.size() > 1)
            ? std::sqrt(sq / static_cast<double>(v.size() - 1))
            : 0.0;
    }

    auto statPair = [](const std::vector<double>& v, double& mean_out, double& std_out) {
        if (v.empty()) { mean_out = 0.0; std_out = 0.0; return; }
        mean_out = std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(v.size());
        double sq = 0.0;
        for (double x : v) sq += (x - mean_out) * (x - mean_out);
        std_out = (v.size() > 1)
            ? std::sqrt(sq / static_cast<double>(v.size() - 1))
            : 0.0;
    };

    statPair(means, result.simMeanMean, result.simMeanStd);
    statPair(stds,  result.simStdMean,  result.simStdStd);
}

// -----------------------------------------------------------------------------
//  Private: _writeProtocol
// -----------------------------------------------------------------------------

void SimulateAnalyzer::_writeProtocol(const SimulationResult& result) const
{
    const std::string fname =
        "simulate_" + result.datasetName + "_" + result.componentName + "_protocol.txt";
    const std::filesystem::path outPath = m_cfg.protocolsDir / fname;

    std::ofstream ofs(outPath);
    if (!ofs.is_open()) {
        LOKI_WARNING("SimulateAnalyzer: cannot write protocol to '"
                     + outPath.string() + "'.");
        return;
    }

    auto line = [&](int w = 72) {
        ofs << std::string(static_cast<std::size_t>(w), '-') << "\n";
    };

    ofs << "LOKI SIMULATE PROTOCOL\n";
    line();
    ofs << "Dataset   : " << result.datasetName   << "\n";
    ofs << "Component : " << result.componentName << "\n";
    ofs << "Mode      : " << result.mode          << "\n";
    ofs << "Model     : " << result.model         << "\n";
    ofs << "n         : " << result.n             << "\n";
    ofs << "nSim      : " << result.nSimulations  << "\n";
    line();

    ofs << "\nSIMULATION SUMMARY\n";
    line();
    ofs << std::fixed << std::setprecision(6);
    ofs << "Mean of sim means : " << result.simMeanMean << "\n";
    ofs << "Std  of sim means : " << result.simMeanStd  << "\n";
    ofs << "Mean of sim stds  : " << result.simStdMean  << "\n";
    ofs << "Std  of sim stds  : " << result.simStdStd   << "\n";
    line();

    if (!result.parameterCIs.empty()) {
        ofs << "\nBOOTSTRAP CONFIDENCE INTERVALS\n";
        line();
        ofs << std::left
            << std::setw(16) << "Parameter"
            << std::setw(14) << "Estimate"
            << std::setw(14) << "Lower"
            << std::setw(14) << "Upper"
            << std::setw(12) << "Bias"
            << std::setw(12) << "SE"
            << "\n";
        line();
        for (const auto& pci : result.parameterCIs) {
            ofs << std::setw(16) << pci.name
                << std::setw(14) << pci.estimate
                << std::setw(14) << pci.lower
                << std::setw(14) << pci.upper
                << std::setw(12) << pci.bias
                << std::setw(12) << pci.se
                << "\n";
        }
        line();
    }

    ofs << "\nENVELOPE (first 5 and last 5 time steps)\n";
    line();
    ofs << std::setw(8) << "t"
        << std::setw(12) << "p05"
        << std::setw(12) << "p25"
        << std::setw(12) << "p50"
        << std::setw(12) << "p75"
        << std::setw(12) << "p95"
        << "\n";
    line();

    const std::size_t n = result.env50.size();
    auto printRow = [&](std::size_t t) {
        ofs << std::setw(8)  << t
            << std::setw(12) << result.env05[t]
            << std::setw(12) << result.env25[t]
            << std::setw(12) << result.env50[t]
            << std::setw(12) << result.env75[t]
            << std::setw(12) << result.env95[t]
            << "\n";
    };
    const std::size_t headTail = std::min(n, std::size_t{5});
    for (std::size_t t = 0; t < headTail; ++t) printRow(t);
    if (n > 10) ofs << "...\n";
    const std::size_t tailStart = (n > 5) ? n - 5 : headTail;
    for (std::size_t t = tailStart; t < n; ++t) printRow(t);

    ofs << "\n[END OF PROTOCOL]\n";
    LOKI_INFO("SimulateAnalyzer: protocol written to '" + outPath.string() + "'.");
}

// -----------------------------------------------------------------------------
//  Private: _writeCsv
// -----------------------------------------------------------------------------

void SimulateAnalyzer::_writeCsv(const SimulationResult& result) const
{
    const std::string fname =
        "simulate_" + result.datasetName + "_" + result.componentName + "_envelope.csv";
    const std::filesystem::path outPath = m_cfg.csvDir / fname;

    std::ofstream ofs(outPath);
    if (!ofs.is_open()) {
        LOKI_WARNING("SimulateAnalyzer: cannot write CSV to '"
                     + outPath.string() + "'.");
        return;
    }

    // Envelope CSV.
    ofs << "t;p05;p25;p50;p75;p95";
    if (!result.original.empty()) ofs << ";original";
    ofs << "\n";

    const std::size_t n = result.env50.size();
    for (std::size_t t = 0; t < n; ++t) {
        ofs << t << ";"
            << result.env05[t] << ";"
            << result.env25[t] << ";"
            << result.env50[t] << ";"
            << result.env75[t] << ";"
            << result.env95[t];
        if (!result.original.empty()) {
            ofs << ";" << (t < result.original.size() ? result.original[t] : 0.0);
        }
        ofs << "\n";
    }

    LOKI_INFO("SimulateAnalyzer: envelope CSV written to '" + outPath.string() + "'.");

    // Full simulation matrix only for small nSim.
    if (result.nSimulations <= 50) {
        const std::string fname2 =
            "simulate_" + result.datasetName + "_" + result.componentName + "_simulations.csv";
        const std::filesystem::path outPath2 = m_cfg.csvDir / fname2;
        std::ofstream ofs2(outPath2);
        if (ofs2.is_open()) {
            // Header: t; sim_0; sim_1; ...
            ofs2 << "t";
            for (int s = 0; s < result.nSimulations; ++s) {
                ofs2 << ";sim_" << s;
            }
            ofs2 << "\n";
            for (std::size_t t = 0; t < n; ++t) {
                ofs2 << t;
                for (const auto& sim : result.simulations) {
                    ofs2 << ";" << (t < sim.size() ? sim[t] : 0.0);
                }
                ofs2 << "\n";
            }
            LOKI_INFO("SimulateAnalyzer: simulation matrix CSV written to '"
                      + outPath2.string() + "'.");
        }
    }
}

// -----------------------------------------------------------------------------
//  Private: _fillGaps
// -----------------------------------------------------------------------------

TimeSeries SimulateAnalyzer::_fillGaps(const TimeSeries& series) const
{
    const SimulateConfig& sc = m_cfg.simulate;

    GapFiller::Config gfc;
    gfc.maxFillLength = sc.gapFillMaxLength;

    const std::string s = sc.gapFillStrategy;
    if      (s == "linear")       gfc.strategy = GapFiller::Strategy::LINEAR;
    else if (s == "forward_fill") gfc.strategy = GapFiller::Strategy::FORWARD_FILL;
    else if (s == "median_year")  gfc.strategy = GapFiller::Strategy::MEDIAN_YEAR;
    else if (s == "spline")       gfc.strategy = GapFiller::Strategy::SPLINE;
    else if (s == "none")         return series;
    else {
        LOKI_WARNING("SimulateAnalyzer: unknown gap fill strategy '" + s
                     + "' -- using LINEAR.");
        gfc.strategy = GapFiller::Strategy::LINEAR;
    }

    GapFiller filler(gfc);
    return filler.fill(series);
}

// -----------------------------------------------------------------------------
//  Private: _extractValues
// -----------------------------------------------------------------------------

std::vector<double> SimulateAnalyzer::_extractValues(const TimeSeries& series)
{
    std::vector<double> vals;
    vals.reserve(series.size());
    for (std::size_t i = 0; i < series.size(); ++i) {
        if (isValid(series[i])) {
            vals.push_back(series[i].value);
        }
    }
    return vals;
}

} // namespace loki::simulate
