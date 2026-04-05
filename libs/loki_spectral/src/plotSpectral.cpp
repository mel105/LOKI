#include "loki/spectral/plotSpectral.hpp"

#include "loki/core/exceptions.hpp"
#include "loki/core/logger.hpp"
#include "loki/io/gnuplot.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

using namespace loki;

namespace loki::spectral {

// -----------------------------------------------------------------------------
//  Helpers
// -----------------------------------------------------------------------------

std::string PlotSpectral::fwdSlash(const std::filesystem::path& p)
{
    std::string s = p.string();
    for (char& c : s) { if (c == '\\') c = '/'; }
    return s;
}

// -----------------------------------------------------------------------------
//  Construction
// -----------------------------------------------------------------------------

PlotSpectral::PlotSpectral(const loki::AppConfig& cfg)
    : m_cfg(cfg)
{}

// -----------------------------------------------------------------------------
//  Public: plot
// -----------------------------------------------------------------------------

void PlotSpectral::plot(const SpectralResult& result,
                         const loki::TimeSeries& ts,
                         const std::string& datasetName) const
{
    if (m_cfg.plots.spectralPsd) {
        try { _plotPsd(result, ts, datasetName); }
        catch (const loki::LOKIException& ex) {
            LOKI_WARNING(std::string("PlotSpectral: PSD plot failed: ") + ex.what());
        }
    }
    if (m_cfg.plots.spectralAmplitude) {
        try { _plotAmplitude(result, ts, datasetName); }
        catch (const loki::LOKIException& ex) {
            LOKI_WARNING(std::string("PlotSpectral: amplitude plot failed: ") + ex.what());
        }
    }
    if (m_cfg.plots.spectralPhase) {
        try { _plotPhase(result, ts, datasetName); }
        catch (const loki::LOKIException& ex) {
            LOKI_WARNING(std::string("PlotSpectral: phase plot failed: ") + ex.what());
        }
    }
}

// -----------------------------------------------------------------------------
//  Public: plotSpectrogram
// -----------------------------------------------------------------------------

void PlotSpectral::plotSpectrogram(const SpectrogramResult& sgResult,
                                    const loki::TimeSeries& ts,
                                    const std::string& datasetName) const
{
    if (sgResult.times.empty() || sgResult.frequencies.empty()) {
        LOKI_WARNING("PlotSpectral: spectrogram result is empty -- skipping plot.");
        return;
    }

    const std::string component = ts.metadata().componentName;
    const std::string fmt       = m_cfg.plots.outputFormat;

    const std::filesystem::path outFile =
        m_cfg.imgDir / ("spectral_" + datasetName + "_" + component
                        + "_spectrogram." + fmt);

    // Spectrogram uses a tmp file because the nonuniform matrix format
    // is too large to send efficiently via pipe for many frames.
    const std::filesystem::path tmpFile =
        m_cfg.imgDir / (".tmp_sg_" + datasetName + "_" + component + ".dat");

    const int nFrames = static_cast<int>(sgResult.times.size());
    const int nBins   = static_cast<int>(sgResult.frequencies.size());

    // Skip DC bin (freq = 0)
    int binFirst = 0;
    while (binFirst < nBins &&
           sgResult.frequencies[static_cast<std::size_t>(binFirst)] <= 0.0)
        ++binFirst;

    if (nBins - binFirst <= 0 || nFrames <= 0) {
        LOKI_WARNING("PlotSpectral: no usable frequency bins for spectrogram.");
        return;
    }

    // ---- Axis ranges --------------------------------------------------------
    const loki::SpectralSpectrogramConfig& sgCfg = m_cfg.spectral.spectrogram;

    const double mjdMin = sgResult.times.front();
    const double mjdMax = sgResult.times.back();

    const double fMax    = sgResult.frequencies[static_cast<std::size_t>(nBins - 1)];
    const double fMinNdc = sgResult.frequencies[static_cast<std::size_t>(binFirst)];
    double periodMin = (fMax    > 0.0) ? 1.0 / fMax    : 0.25;
    double periodMax = (fMinNdc > 0.0) ? 1.0 / fMinNdc : 9999.0;

    if (sgCfg.focusPeriodMin > 0.0) {
        periodMin = sgCfg.focusPeriodMin;
    } else {
        periodMin = std::max(periodMin, 1.0);
    }
    if (sgCfg.focusPeriodMax > 0.0) {
        periodMax = sgCfg.focusPeriodMax;
    } else {
        const double halfSpan = (mjdMax - mjdMin) * 0.5;
        const double cap = std::min(halfSpan, 730.0);
        if (cap > periodMin) periodMax = cap;
    }
    if (periodMax <= periodMin) periodMax = periodMin * 100.0;

    // ---- Focus bin range ----------------------------------------------------
    int kMin = binFirst;
    int kMax = nBins - 1;

    if (sgCfg.focusPeriodMax > 0.0) {
        const double fLow = 1.0 / sgCfg.focusPeriodMax;
        for (int k = binFirst; k < nBins; ++k) {
            if (sgResult.frequencies[static_cast<std::size_t>(k)] >= fLow) {
                kMin = k; break;
            }
        }
    }
    if (sgCfg.focusPeriodMin > 0.0) {
        const double fHigh = 1.0 / sgCfg.focusPeriodMin;
        for (int k = nBins - 1; k >= binFirst; --k) {
            if (sgResult.frequencies[static_cast<std::size_t>(k)] <= fHigh) {
                kMax = k; break;
            }
        }
    }
    if (kMax < kMin) { kMin = binFirst; kMax = nBins - 1; }

    // ---- Power range --------------------------------------------------------
    double pwrMax = 0.0;
    double pwrMin = std::numeric_limits<double>::max();
    for (int i = 0; i < nFrames; ++i)
        for (int k = kMin; k <= kMax; ++k) {
            const double p = sgResult.power[static_cast<std::size_t>(i)]
                                           [static_cast<std::size_t>(k)];
            if (p > pwrMax) pwrMax = p;
            if (p > 0.0 && p < pwrMin) pwrMin = p;
        }
    if (pwrMax <= 0.0) {
        LOKI_WARNING("PlotSpectral: all spectrogram power values are zero.");
        return;
    }
    const double cbMin = std::max(pwrMin, pwrMax * 1.0e-4);
    const double cbMax = pwrMax;

    // ---- Write matrix tmp file (nonuniform matrix format) -------------------
    {
        std::ofstream ofs(tmpFile);
        if (!ofs.is_open())
            throw IoException("PlotSpectral: cannot write spectrogram temp file.");

        ofs << nFrames;
        for (int i = 0; i < nFrames; ++i)
            ofs << " " << std::fixed << std::setprecision(3)
                << sgResult.times[static_cast<std::size_t>(i)];
        ofs << "\n";

        for (int k = kMax; k >= kMin; --k) {
            const double freq   = sgResult.frequencies[static_cast<std::size_t>(k)];
            const double period = (freq > 0.0) ? 1.0 / freq : 0.0;
            ofs << std::fixed << std::setprecision(4) << period;
            for (int i = 0; i < nFrames; ++i) {
                const double p = sgResult.power[static_cast<std::size_t>(i)]
                                               [static_cast<std::size_t>(k)];
                ofs << " " << std::scientific << std::setprecision(6) << p;
            }
            ofs << "\n";
        }
        ofs.flush();
        ofs.close();
    }

    // ---- Title --------------------------------------------------------------
    std::ostringstream titleStream;
    titleStream << "Spectrogram -- " << datasetName << " / " << component;
    if (sgCfg.focusPeriodMin > 0.0 || sgCfg.focusPeriodMax > 0.0) {
        titleStream << std::fixed << std::setprecision(1)
                    << "  [" << periodMin << " -- " << periodMax << " days]";
    }

    // ---- Gnuplot ------------------------------------------------------------
    {
        const std::string terminal =
            fmt == "eps"
            ? "postscript eps enhanced color font 'Sans,12'"
            : "pngcairo noenhanced font 'Sans,12' size 1400,600";

        const std::string cbRangeStr =
            "[" + std::to_string(cbMin) + ":" + std::to_string(cbMax) + "]";

        Gnuplot gp;
        gp("set terminal " + terminal);
        gp("set output '" + fwdSlash(outFile) + "'");
        gp("set title '" + titleStream.str() + "'");
        gp("set xlabel 'Time (MJD)'");
        gp("set ylabel 'Period (days)'");
        gp("set xrange [" + std::to_string(mjdMin) + ":"
                          + std::to_string(mjdMax) + "]");
        gp("set yrange [" + std::to_string(periodMax) + ":"
                          + std::to_string(periodMin) + "]");
        gp("set palette defined (0 '#313695', 2 '#4575b4', 4 '#74add1', "
           "5 '#ffffbf', 7 '#f46d43', 9 '#d73027', 10 '#a50026')");
        gp("set cblabel 'Power (PSD)'");
        gp("set logscale cb");
        gp("set cbrange " + cbRangeStr);
        gp("unset key");
        gp("plot '" + fwdSlash(tmpFile) + "' nonuniform matrix with image notitle");
        gp("set output");
    }

    std::error_code ec;
    std::filesystem::remove(tmpFile, ec);

    LOKI_INFO("PlotSpectral: spectrogram plot -> " + outFile.string());
}

// -----------------------------------------------------------------------------
//  Private: _plotPsd
// -----------------------------------------------------------------------------

void PlotSpectral::_plotPsd(const SpectralResult& result,
                              const loki::TimeSeries& ts,
                              const std::string& datasetName) const
{
    if (result.frequencies.empty()) return;

    const std::string component = ts.metadata().componentName;
    const std::string fmt       = m_cfg.plots.outputFormat;

    const std::filesystem::path outFile =
        m_cfg.imgDir / ("spectral_" + datasetName + "_" + component + "_psd." + fmt);

    const bool hasPeaks = m_cfg.plots.spectralPeaks && !result.peaks.empty();
    const double maxRaw = result.power.empty() ? 1.0
        : *std::max_element(result.power.begin(), result.power.end());

    // Build inline data before opening pipe
    std::ostringstream psdData;
    for (std::size_t k = 0; k < result.frequencies.size(); ++k) {
        const double freq = result.frequencies[k];
        if (freq <= 0.0) continue;
        psdData << std::scientific << std::setprecision(8)
                << (1.0 / freq) << " " << result.power[k] << "\n";
    }

    std::ostringstream peakData;
    if (hasPeaks) {
        for (const auto& pk : result.peaks) {
            peakData << std::fixed << std::setprecision(4) << pk.periodDays << " "
                     << std::scientific << std::setprecision(6) << (pk.power * maxRaw)
                     << " " << pk.rank << "\n";
        }
    }

    const std::string terminal =
        fmt == "eps" ? "postscript eps enhanced color font 'Sans,12'"
                     : "pngcairo noenhanced font 'Sans,12' size 1200,600";

    const bool isLs = (result.method == "lomb_scargle");
    const std::string ylabel = isLs ? "Normalised Lomb-Scargle power"
                                    : "Power spectral density";

    std::ostringstream title;
    title << "Spectral analysis -- " << datasetName
          << " / " << result.method << "  n=" << result.nObs;

    Gnuplot gp;
    gp("set terminal " + terminal);
    gp("set output '" + fwdSlash(outFile) + "'");
    gp("set title '" + title.str() + "'");
    gp("set xlabel 'Period (days)'");
    gp("set ylabel '" + ylabel + "'");
    gp("set logscale x");
    gp("set logscale y");
    gp("set grid xtics ytics lt 0 lw 1 lc rgb '#cccccc'");
    gp("set key top right");

    if (!hasPeaks) {
        gp("plot '-' u 1:2 w l lw 1.5 lc rgb '#2166ac' title 'PSD'");
        gp(psdData.str() + "e");
    } else {
        gp("plot '-' u 1:2 w l lw 1.5 lc rgb '#2166ac' title 'PSD', "
           "'-' u 1:2 w impulses lw 2 lc rgb '#d73027' title 'Peaks', "
           "'-' u 1:2:3 with labels offset char 0.5,0.5 font 'Sans,9' "
           "tc rgb '#d73027' notitle");
        gp(psdData.str() + "e");
        gp(peakData.str() + "e");
        gp(peakData.str() + "e");
    }
    gp("set output");

    LOKI_INFO("PlotSpectral: PSD plot -> " + outFile.string());
}

// -----------------------------------------------------------------------------
//  Private: _plotAmplitude
// -----------------------------------------------------------------------------

void PlotSpectral::_plotAmplitude(const SpectralResult& result,
                                   const loki::TimeSeries& ts,
                                   const std::string& datasetName) const
{
    if (result.amplitudes.empty() || result.frequencies.empty()) {
        LOKI_WARNING("PlotSpectral: amplitude spectrum not available -- skipping.");
        return;
    }

    const std::string component = ts.metadata().componentName;
    const std::string fmt       = m_cfg.plots.outputFormat;
    const std::string unit      = ts.metadata().unit.empty()
                                  ? "signal units" : ts.metadata().unit;

    const std::filesystem::path outFile =
        m_cfg.imgDir / ("spectral_" + datasetName + "_" + component + "_amplitude." + fmt);

    // Build inline data before opening pipe
    std::ostringstream data;
    for (std::size_t k = 0; k < result.frequencies.size(); ++k) {
        const double freq = result.frequencies[k];
        if (freq <= 0.0) continue;
        data << std::scientific << std::setprecision(8)
             << (1.0 / freq) << " " << result.amplitudes[k] << "\n";
    }

    const std::string terminal =
        fmt == "eps" ? "postscript eps enhanced color font 'Sans,12'"
                     : "pngcairo noenhanced font 'Sans,12' size 1200,600";

    std::ostringstream title;
    title << "Amplitude spectrum -- " << datasetName
          << " / " << result.method << "  n=" << result.nObs;

    Gnuplot gp;
    gp("set terminal " + terminal);
    gp("set output '" + fwdSlash(outFile) + "'");
    gp("set title '" + title.str() + "'");
    gp("set xlabel 'Period (days)'");
    gp("set ylabel 'Amplitude (" + unit + ")'");
    gp("set logscale x");
    gp("set grid xtics ytics lt 0 lw 1 lc rgb '#cccccc'");
    gp("set key top right");
    gp("plot '-' u 1:2 w l lw 1.5 lc rgb '#1a9850' title 'Amplitude'");
    gp(data.str() + "e");
    gp("set output");

    LOKI_INFO("PlotSpectral: amplitude plot -> " + outFile.string());
}

// -----------------------------------------------------------------------------
//  Private: _plotPhase
// -----------------------------------------------------------------------------

void PlotSpectral::_plotPhase(const SpectralResult& result,
                               const loki::TimeSeries& ts,
                               const std::string& datasetName) const
{
    if (result.phases.empty()) {
        LOKI_WARNING("PlotSpectral: phase spectrum not available "
                     "(only computed for FFT method) -- skipping.");
        return;
    }

    const std::string component = ts.metadata().componentName;
    const std::string fmt       = m_cfg.plots.outputFormat;

    const std::filesystem::path outFile =
        m_cfg.imgDir / ("spectral_" + datasetName + "_" + component + "_phase." + fmt);

    const double maxAmp = result.amplitudes.empty() ? 1.0
        : *std::max_element(result.amplitudes.begin(), result.amplitudes.end());
    const double ampThreshold = maxAmp * 0.01;

    // Build inline data before opening pipe
    std::ostringstream data;
    for (std::size_t k = 0; k < result.frequencies.size(); ++k) {
        const double freq = result.frequencies[k];
        if (freq <= 0.0) continue;
        const double amp = result.amplitudes.empty() ? 1.0 : result.amplitudes[k];
        if (amp < ampThreshold) continue;
        data << std::scientific << std::setprecision(8)
             << (1.0 / freq) << " " << result.phases[k] << "\n";
    }

    const std::string terminal =
        fmt == "eps" ? "postscript eps enhanced color font 'Sans,12'"
                     : "pngcairo noenhanced font 'Sans,12' size 1200,600";

    std::ostringstream title;
    title << "Phase spectrum -- " << datasetName
          << " / " << result.method << "  n=" << result.nObs;

    Gnuplot gp;
    gp("set terminal " + terminal);
    gp("set output '" + fwdSlash(outFile) + "'");
    gp("set title '" + title.str() + "'");
    gp("set xlabel 'Period (days)'");
    gp("set ylabel 'Phase (radians)'");
    gp("set logscale x");
    gp("set yrange [-3.14159:3.14159]");
    gp("set ytics (-3.14159, -1.5708, 0, 1.5708, 3.14159)");
    gp("set grid xtics ytics lt 0 lw 1 lc rgb '#cccccc'");
    gp("set key top right");
    gp("plot '-' u 1:2 w p pt 7 ps 0.4 lc rgb '#e08214' title 'Phase'");
    gp(data.str() + "e");
    gp("set output");

    LOKI_INFO("PlotSpectral: phase plot -> " + outFile.string());
}

} // namespace loki::spectral