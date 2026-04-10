#include "loki/spectral/spectralAnalyzer.hpp"

#include "loki/spectral/fftAnalyzer.hpp"
#include "loki/spectral/lombScargle.hpp"
#include "loki/spectral/peakDetector.hpp"
#include "loki/spectral/plotSpectral.hpp"
#include "loki/spectral/spectrogramAnalyzer.hpp"

#include "loki/core/exceptions.hpp"
#include "loki/core/logger.hpp"
#include "loki/timeseries/gapFiller.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>

using namespace loki;

namespace loki::spectral {

// -----------------------------------------------------------------------------
//  Construction
// -----------------------------------------------------------------------------

SpectralAnalyzer::SpectralAnalyzer(const loki::AppConfig& cfg)
    : m_cfg(cfg)
{}

// -----------------------------------------------------------------------------
//  Public: run
// -----------------------------------------------------------------------------

void SpectralAnalyzer::run(const loki::TimeSeries& ts, const std::string& datasetName)
{
    const loki::SpectralConfig& scfg = m_cfg.spectral;
    const std::string component      = ts.metadata().componentName;
    const std::string tag            = datasetName + " / " + component;

    LOKI_INFO("SpectralAnalyzer: starting  [" + tag + "]  n=" + std::to_string(ts.size()));

    if (ts.size() < 4) {
        throw DataException(
            "SpectralAnalyzer: series '" + tag + "' is too short (n="
            + std::to_string(ts.size()) + "); need at least 4 observations.");
    }

    // ---- 1. Gap-fill (in-place on a working copy) ---------------------------
    loki::TimeSeries working = ts;
    {
        loki::GapFiller::Config gfCfg;
        if      (scfg.gapFillStrategy == "linear")       gfCfg.strategy = loki::GapFiller::Strategy::LINEAR;
        else if (scfg.gapFillStrategy == "forward_fill") gfCfg.strategy = loki::GapFiller::Strategy::FORWARD_FILL;
        else if (scfg.gapFillStrategy == "mean")         gfCfg.strategy = loki::GapFiller::Strategy::MEAN;
        else if (scfg.gapFillStrategy == "spline")       gfCfg.strategy = loki::GapFiller::Strategy::SPLINE;
        else if (scfg.gapFillStrategy == "none")         gfCfg.strategy = loki::GapFiller::Strategy::NONE;
        else {
            LOKI_WARNING("SpectralAnalyzer: unknown gap_fill_strategy '"
                         + scfg.gapFillStrategy + "' -- using 'linear'.");
            gfCfg.strategy = loki::GapFiller::Strategy::LINEAR;
        }
        gfCfg.maxFillLength = static_cast<std::size_t>(
            std::max(0, scfg.gapFillMaxLength));

        loki::GapFiller gf(gfCfg);
        working = gf.fill(working);
        LOKI_INFO("SpectralAnalyzer: gap filling done  n=" + std::to_string(working.size()));
    }

    // ---- 2. Extract (times, values) -----------------------------------------
    std::vector<double> times, values;
    _extractTimesValues(working, times, values);

    if (static_cast<int>(times.size()) < 4) {
        throw DataException(
            "SpectralAnalyzer: fewer than 4 valid observations remain after "
            "gap filling for '" + tag + "'.");
    }

    // ---- 3. Method selection -------------------------------------------------
    std::string method = scfg.method;
    if (method == "auto") {
        method = _isUniform(times) ? "fft" : "lomb_scargle";
        LOKI_INFO("SpectralAnalyzer: auto-selected method '" + method + "'");
    } else {
        LOKI_INFO("SpectralAnalyzer: using method '" + method + "'");
        if (method == "fft" && !_isUniform(times)) {
            LOKI_WARNING(
                "SpectralAnalyzer: method='fft' requested but series does not "
                "appear uniformly sampled. Results may be inaccurate.");
        }
    }

    // ---- 4. Median step (needed for FFT) ------------------------------------
    double medianStep = 0.0;
    {
        std::vector<double> steps;
        steps.reserve(times.size() - 1);
        for (std::size_t i = 1; i < times.size(); ++i)
            steps.push_back(times[i] - times[i - 1]);
        std::sort(steps.begin(), steps.end());
        medianStep = steps[steps.size() / 2];
    }

    // ---- 5. Compute periodogram ---------------------------------------------
    SpectralResult result;

    if (method == "fft") {
        FftParams fftParams;
        fftParams.windowFunction = m_cfg.spectral.fft.windowFunction;
        fftParams.welch          = m_cfg.spectral.fft.welch;
        fftParams.welchSegments  = m_cfg.spectral.fft.welchSegments;
        fftParams.welchOverlap   = m_cfg.spectral.fft.welchOverlap;

        FftAnalyzer fftAnalyzer(fftParams, medianStep);
        result = fftAnalyzer.compute(values);
    } else {
        LombScargleParams lsParams;
        lsParams.oversampling = m_cfg.spectral.lombScargle.oversampling;
        lsParams.fastNfft     = m_cfg.spectral.lombScargle.fastNfft;
        lsParams.fapThreshold = m_cfg.spectral.lombScargle.fapThreshold;

        LombScargle ls(lsParams);
        result = ls.compute(times, values);
    }

    LOKI_INFO("SpectralAnalyzer: periodogram computed  nFreqs="
              + std::to_string(result.frequencies.size()));

    // ---- 6. Peak detection --------------------------------------------------
    PeakDetectorParams pkParams;
    pkParams.topN          = m_cfg.spectral.peaks.topN;
    pkParams.minPeriodDays = m_cfg.spectral.peaks.minPeriodDays;
    pkParams.maxPeriodDays = m_cfg.spectral.peaks.maxPeriodDays;
    pkParams.fapThreshold  = m_cfg.spectral.lombScargle.fapThreshold;

    PeakDetector peakDetector(pkParams);
    peakDetector.detect(result);

    LOKI_INFO("SpectralAnalyzer: " + std::to_string(result.peaks.size())
              + " peaks detected");
    for (const auto& pk : result.peaks) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2)
            << "  rank=" << pk.rank
            << "  period=" << pk.periodDays << " days"
            << "  power=" << std::setprecision(4) << pk.power;
        if (pk.fap >= 0.0)
            oss << "  FAP=" << std::scientific << std::setprecision(2) << pk.fap;
        LOKI_INFO(oss.str());
    }

    // ---- 7. Plots -----------------------------------------------------------
    PlotSpectral plotter(m_cfg);
    plotter.plot(result, ts, datasetName);

    // ---- 7b. Spectrogram (optional) -----------------------------------------
    if (m_cfg.plots.spectralSpectrogram && m_cfg.spectral.spectrogram.enabled) {
        try {
            SpectrogramParams sgParams;
            sgParams.windowLength   = m_cfg.spectral.spectrogram.windowLength;
            sgParams.overlap        = m_cfg.spectral.spectrogram.overlap;
            sgParams.focusPeriodMin = m_cfg.spectral.spectrogram.focusPeriodMin;
            sgParams.focusPeriodMax = m_cfg.spectral.spectrogram.focusPeriodMax;
            sgParams.windowFunction = m_cfg.spectral.fft.windowFunction;

            SpectrogramAnalyzer sgAnalyzer(sgParams, medianStep);
            const SpectrogramResult sgResult = sgAnalyzer.compute(times, values);
            plotter.plotSpectrogram(sgResult, ts, datasetName);
        } catch (const loki::LOKIException& ex) {
            LOKI_WARNING(std::string("SpectralAnalyzer: spectrogram failed: ")
                         + ex.what());
        }
    }

    // ---- 8. Protocol --------------------------------------------------------
    _writeProtocol(result, datasetName, component);

    LOKI_INFO("SpectralAnalyzer: finished [" + tag + "]");
}

// -----------------------------------------------------------------------------
//  Private: _isUniform
// -----------------------------------------------------------------------------

bool SpectralAnalyzer::_isUniform(const std::vector<double>& times)
{
    if (times.size() < 2) return true;

    // Median step
    std::vector<double> steps;
    steps.reserve(times.size() - 1);
    for (std::size_t i = 1; i < times.size(); ++i)
        steps.push_back(times[i] - times[i - 1]);
    std::sort(steps.begin(), steps.end());
    const double medianStep = steps[steps.size() / 2];

    if (medianStep <= 0.0) return false;

    // Count steps that deviate more than 10 % from the median
    const double threshold = 1.1 * medianStep;
    int nIrregular = 0;
    for (const double s : steps) {
        if (s > threshold) ++nIrregular;
    }
    const double fraction =
        static_cast<double>(nIrregular) / static_cast<double>(steps.size());
    return fraction < 0.05;
}

// -----------------------------------------------------------------------------
//  Private: _extractTimesValues
// -----------------------------------------------------------------------------

void SpectralAnalyzer::_extractTimesValues(const loki::TimeSeries& ts,
                                            std::vector<double>& times,
                                            std::vector<double>& values)
{
    times.clear();
    values.clear();
    times.reserve(ts.size());
    values.reserve(ts.size());

    for (std::size_t i = 0; i < ts.size(); ++i) {
        const double v = ts[i].value;
        if (std::isnan(v)) continue;
        times.push_back(ts[i].time.mjd());
        values.push_back(v);
    }
}

// -----------------------------------------------------------------------------
//  Private: _writeProtocol
// -----------------------------------------------------------------------------

void SpectralAnalyzer::_writeProtocol(const SpectralResult& result,
                                       const std::string& datasetName,
                                       const std::string& component) const
{
    const std::filesystem::path outDir = m_cfg.protocolsDir;
    std::filesystem::create_directories(outDir);

    const std::string fname =
        "spectral_" + datasetName + "_" + component + "_protocol.txt";
    const std::filesystem::path outPath = outDir / fname;

    std::ofstream ofs(outPath);
    if (!ofs.is_open()) {
        throw IoException(
            "SpectralAnalyzer: cannot write protocol to '"
            + outPath.string() + "'.");
    }

    // Derived span in years
    const double spanYears = result.spanDays / 365.25;

    ofs << "==========================================================\n"
        << " LOKI Spectral Analysis Protocol\n"
        << "==========================================================\n"
        << " Dataset    : " << datasetName << "\n"
        << " Component  : " << component  << "\n"
        << " Method     : " << result.method << "\n"
        << " N          : " << result.nObs << " observations\n"
        << std::fixed << std::setprecision(2)
        << " Span       : " << spanYears << " years ("
                            << result.spanDays << " days)\n"
        << " Median step: " << result.samplingStepDays << " days\n"
        << "----------------------------------------------------------\n";

    if (result.peaks.empty()) {
        ofs << " No significant peaks detected.\n";
    } else {
        ofs << " Top " << result.peaks.size() << " dominant period(s):\n\n";
        ofs << std::left
            << std::setw(6)  << " Rank"
            << std::setw(16) << "  Period (days)"
            << std::setw(14) << "  Freq (cpd)"
            << std::setw(10) << "  Power"
            << "  FAP\n";
        ofs << " " << std::string(58, '-') << "\n";

        for (const auto& pk : result.peaks) {
            ofs << std::right
                << std::setw(5)  << pk.rank
                << "  " << std::fixed << std::setprecision(2)
                << std::setw(13) << pk.periodDays
                << "  " << std::setprecision(6)
                << std::setw(12) << pk.freqCpd
                << "  " << std::setprecision(4)
                << std::setw(8)  << pk.power
                << "  ";
            if (pk.fap < 0.0) {
                ofs << "N/A";
            } else if (pk.fap < 0.001) {
                ofs << "< 0.001";
            } else {
                ofs << std::fixed << std::setprecision(3) << pk.fap;
            }
            ofs << "\n";
        }
    }

    ofs << "==========================================================\n";
    ofs.close();

    LOKI_INFO("SpectralAnalyzer: protocol written -> " + outPath.string());
}

} // namespace loki::spectral
