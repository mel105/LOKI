#include "loki/kalman/plotKalman.hpp"

#include "loki/core/exceptions.hpp"
#include "loki/core/logger.hpp"
#include "loki/io/gnuplot.hpp"
#include "loki/io/plot.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <sstream>
#include <string>

using namespace loki;

namespace loki::kalman {

// -----------------------------------------------------------------------------
//  Construction
// -----------------------------------------------------------------------------

PlotKalman::PlotKalman(const AppConfig& cfg)
    : m_cfg(cfg)
{}

// -----------------------------------------------------------------------------
//  plotAll
// -----------------------------------------------------------------------------

void PlotKalman::plotAll(const KalmanResult& result,
                         const std::string&  dataset,
                         const std::string&  component) const
{
    if (m_cfg.plots.kalmanOverlay) {
        try { plotOverlay(result, dataset, component); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotKalman: overlay plot failed: " + std::string(ex.what()));
        }
    }
    if (m_cfg.plots.kalmanInnovations) {
        try { plotInnovations(result, dataset, component); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotKalman: innovations plot failed: " + std::string(ex.what()));
        }
    }
    if (m_cfg.plots.kalmanGain) {
        try { plotGain(result, dataset, component); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotKalman: gain plot failed: " + std::string(ex.what()));
        }
    }
    if (m_cfg.plots.kalmanUncertainty) {
        try { plotUncertainty(result, dataset, component); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotKalman: uncertainty plot failed: " + std::string(ex.what()));
        }
    }
    if (m_cfg.plots.kalmanForecast && !result.forecastState.empty()) {
        try { plotForecast(result, dataset, component); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotKalman: forecast plot failed: " + std::string(ex.what()));
        }
    }
    if (m_cfg.plots.kalmanDiagnostics) {
        try { plotDiagnostics(result, dataset, component); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotKalman: diagnostics plot failed: " + std::string(ex.what()));
        }
    }
}

// -----------------------------------------------------------------------------
//  plotOverlay
// -----------------------------------------------------------------------------

void PlotKalman::plotOverlay(const KalmanResult& result,
                             const std::string&  dataset,
                             const std::string&  component) const
{
    const std::size_t n = result.times.size();
    if (n == 0) { return; }

    const auto outPath = outputPath(dataset, component, "overlay");
    Gnuplot gp;

    // Explicit xrange so filledcurves does not shrink the visible domain
    const double tMin = result.times.front();
    const double tMax = result.times.back();

    gp(terminalCmd(outPath, 1400, 600));
    gp("set xlabel 'MJD'");
    gp("set ylabel '" + component + "'");
    gp("set title 'Kalman filter -- " + component + " (" + result.modelName + ")'");
    gp("set key top left");
    gp("set grid");
    gp("set xrange [" + std::to_string(tMin) + ":" + std::to_string(tMax) + "]");

    const bool hasSmooth = !result.smoothedState.empty();

    // Single data block: col1=t, col2=orig, col3=filt, col4=lo, col5=hi, [col6=smooth]
    std::ostringstream dat;
    for (std::size_t i = 0; i < n; ++i) {
        const double filt = result.filteredState[i];
        const double sig  = result.filteredStd[i];

        auto fmt = [](double v) -> std::string {
            return std::isfinite(v) ? std::to_string(v) : "NaN";
        };

        dat << fmt(result.times[i])
            << " " << fmt(result.original[i])
            << " " << fmt(filt)
            << " " << fmt(filt - 2.0 * sig)
            << " " << fmt(filt + 2.0 * sig);

        if (hasSmooth) {
            dat << " " << fmt(result.smoothedState[i]);
        }
        dat << "\n";
    }
    dat << "e\n";

    const std::string block = dat.str();

    if (hasSmooth) {
        gp("plot '-' u 1:4:5 w filledcurves lc rgb '#ccddff' fs transparent solid 0.4 notitle, "
           "'-' u 1:2 w p pt 7 ps 0.3 lc rgb '#aaaaaa' title 'original', "
           "'-' u 1:3 w l lw 1.5 lc rgb '#0055cc' title 'filtered', "
           "'-' u 1:6 w l lw 2.0 lc rgb '#cc2200' title 'smoothed (RTS)'");
        gp(block); gp(block); gp(block); gp(block);
    } else {
        gp("plot '-' u 1:4:5 w filledcurves lc rgb '#ccddff' fs transparent solid 0.4 notitle, "
           "'-' u 1:2 w p pt 7 ps 0.3 lc rgb '#aaaaaa' title 'original', "
           "'-' u 1:3 w l lw 2.0 lc rgb '#0055cc' title 'filtered'");
        gp(block); gp(block); gp(block);
    }

    LOKI_INFO("PlotKalman: overlay -> " + outPath.filename().string());
}

// -----------------------------------------------------------------------------
//  plotInnovations
// -----------------------------------------------------------------------------

void PlotKalman::plotInnovations(const KalmanResult& result,
                                 const std::string&  dataset,
                                 const std::string&  component) const
{
    const std::size_t n = result.times.size();
    if (n == 0) { return; }

    // Compute steady-state sigma from the second half of the series
    // (avoids filter transient at the start)
    std::vector<double> sigmas;
    sigmas.reserve(n);
    for (std::size_t i = n / 2; i < n; ++i) {
        if (std::isfinite(result.innovationStd[i]) && result.innovationStd[i] > 0.0) {
            sigmas.push_back(result.innovationStd[i]);
        }
    }

    double steadySigma = 0.0;
    if (!sigmas.empty()) {
        std::nth_element(sigmas.begin(),
                         sigmas.begin() + static_cast<std::ptrdiff_t>(sigmas.size() / 2),
                         sigmas.end());
        steadySigma = sigmas[sigmas.size() / 2];
    }
    const double band = 2.0 * steadySigma;

    const auto outPath = outputPath(dataset, component, "innovations");
    Gnuplot gp;

    const double tMin = result.times.front();
    const double tMax = result.times.back();

    gp(terminalCmd(outPath, 1400, 500));
    gp("set xlabel 'MJD'");
    gp("set ylabel 'Innovation'");
    gp("set title 'Innovations (one-step-ahead residuals) -- " + component + "'");
    gp("set key top left");
    gp("set grid");
    gp("set xrange [" + std::to_string(tMin) + ":" + std::to_string(tMax) + "]");

    // Draw steady-state +/-2sigma as horizontal arrows (gnuplot horizontal lines)
    if (band > 0.0) {
        gp("set arrow 1 from " + std::to_string(tMin) + "," + std::to_string(band)
           + " to " + std::to_string(tMax) + "," + std::to_string(band)
           + " nohead lc rgb '#cc2200' dt 2 lw 1.5");
        gp("set arrow 2 from " + std::to_string(tMin) + "," + std::to_string(-band)
           + " to " + std::to_string(tMax) + "," + std::to_string(-band)
           + " nohead lc rgb '#cc2200' dt 2 lw 1.5");
        gp("set label 1 '+/-2sigma (steady-state)' at graph 0.01,0.95 tc rgb '#cc2200'");
    }

    // Build inline data: time, innovation only
    std::ostringstream dat;
    for (std::size_t i = 0; i < n; ++i) {
        if (!std::isfinite(result.innovations[i])) { continue; }
        dat << result.times[i] << " " << result.innovations[i] << "\n";
    }
    dat << "e\n";

    gp("plot '-' u 1:2 w l lw 0.8 lc rgb '#0055cc' title 'innovation'");
    gp(dat.str());

    LOKI_INFO("PlotKalman: innovations -> " + outPath.filename().string());
}

// -----------------------------------------------------------------------------
//  plotGain
// -----------------------------------------------------------------------------

void PlotKalman::plotGain(const KalmanResult& result,
                          const std::string&  dataset,
                          const std::string&  component) const
{
    const std::size_t n = result.times.size();
    if (n == 0) { return; }

    const auto outPath = outputPath(dataset, component, "gain");
    Gnuplot gp;

    gp(terminalCmd(outPath, 1400, 500));
    gp("set xlabel 'MJD'");
    gp("set ylabel 'Kalman gain K[0]'");
    gp("set title 'Kalman gain -- " + component + "'");
    gp("set grid");

    std::ostringstream dat;
    for (std::size_t i = 0; i < n; ++i) {
        if (!std::isfinite(result.gains[i])) { continue; }
        dat << result.times[i] << " " << result.gains[i] << "\n";
    }
    dat << "e\n";

    gp("plot '-' u 1:2 w l lw 1.5 lc rgb '#0055cc' notitle");
    gp(dat.str());

    LOKI_INFO("PlotKalman: gain -> " + outPath.filename().string());
}

// -----------------------------------------------------------------------------
//  plotUncertainty
// -----------------------------------------------------------------------------

void PlotKalman::plotUncertainty(const KalmanResult& result,
                                 const std::string&  dataset,
                                 const std::string&  component) const
{
    const std::size_t n = result.times.size();
    if (n == 0) { return; }

    const bool hasSmooth = !result.smoothedStd.empty();
    const auto outPath = outputPath(dataset, component, "uncertainty");
    Gnuplot gp;

    gp(terminalCmd(outPath, 1400, 500));
    gp("set xlabel 'MJD'");
    gp("set ylabel 'Std dev'");
    gp("set title 'Filter uncertainty -- " + component + "'");
    gp("set key top right");
    gp("set grid");

    std::ostringstream dat;
    for (std::size_t i = 0; i < n; ++i) {
        dat << result.times[i] << " " << result.filteredStd[i];
        if (hasSmooth) { dat << " " << result.smoothedStd[i]; }
        dat << "\n";
    }
    dat << "e\n";

    if (hasSmooth) {
        gp("plot '-' u 1:2 w l lw 1.5 lc rgb '#0055cc' title 'filter std', "
           "'-' u 1:3 w l lw 1.5 lc rgb '#cc2200' title 'smoother std'");
        gp(dat.str());
        gp(dat.str());
    } else {
        gp("plot '-' u 1:2 w l lw 1.5 lc rgb '#0055cc' title 'filter std'");
        gp(dat.str());
    }

    LOKI_INFO("PlotKalman: uncertainty -> " + outPath.filename().string());
}

// -----------------------------------------------------------------------------
//  plotForecast
// -----------------------------------------------------------------------------

void PlotKalman::plotForecast(const KalmanResult& result,
                              const std::string&  dataset,
                              const std::string&  component) const
{
    if (result.forecastState.empty() || result.forecastTimes.empty()) { return; }

    const std::size_t n     = result.times.size();
    const std::size_t tail  = std::min(n, static_cast<std::size_t>(1461));
    const std::size_t start = n - tail;
    const std::size_t nf    = result.forecastTimes.size();

    // xrange covers both historical tail and full forecast
    const double xMin = result.times[start];
    const double xMax = result.forecastTimes.back();

    const auto outPath = outputPath(dataset, component, "forecast");
    Gnuplot gp;

    gp(terminalCmd(outPath, 1400, 600));
    gp("set xlabel 'MJD'");
    gp("set ylabel '" + component + "'");
    gp("set title 'Kalman forecast -- " + component + "'");
    gp("set key top left");
    gp("set grid");
    gp("set xrange [" + std::to_string(xMin) + ":" + std::to_string(xMax) + "]");

    // Historical tail: col1=t, col2=orig, col3=filt
    std::ostringstream hist;
    for (std::size_t i = start; i < n; ++i) {
        hist << result.times[i]
             << " " << result.original[i]
             << " " << result.filteredState[i] << "\n";
    }
    hist << "e\n";

    // Forecast: col1=t, col2=state, col3=lo, col4=hi
    std::ostringstream fc;
    for (std::size_t i = 0; i < nf; ++i) {
        const double s  = result.forecastState[i];
        const double sd = result.forecastStd[i];
        fc << result.forecastTimes[i]
           << " " << s
           << " " << (s - 2.0 * sd)
           << " " << (s + 2.0 * sd) << "\n";
    }
    fc << "e\n";

    const std::string hBlock = hist.str();
    const std::string fBlock = fc.str();

    gp("plot '-' u 1:3:4 w filledcurves lc rgb '#ffcccc' fs transparent solid 0.4 notitle, "
       "'-' u 1:2 w p pt 7 ps 0.3 lc rgb '#aaaaaa' title 'original', "
       "'-' u 1:3 w l lw 1.5 lc rgb '#0055cc' title 'filtered', "
       "'-' u 1:2 w l lw 2.5 lc rgb '#cc2200' title 'forecast'");
    gp(fBlock);  // filledcurves: forecast cols 1:3:4
    gp(hBlock);  // original points
    gp(hBlock);  // filtered line
    gp(fBlock);  // forecast line: cols 1:2

    LOKI_INFO("PlotKalman: forecast -> " + outPath.filename().string());
}

// -----------------------------------------------------------------------------
//  plotDiagnostics
// -----------------------------------------------------------------------------

void PlotKalman::plotDiagnostics(const KalmanResult& result,
                                 const std::string&  dataset,
                                 const std::string&  component) const
{
    std::vector<double> innovations;
    std::vector<double> predicted;
    innovations.reserve(result.innovations.size());
    predicted.reserve(result.innovations.size());

    for (std::size_t i = 0; i < result.innovations.size(); ++i) {
        if (std::isfinite(result.innovations[i])) {
            innovations.push_back(result.innovations[i]);
            predicted.push_back(result.predictedState[i]);
        }
    }

    if (innovations.size() < 5) {
        LOKI_WARNING("PlotKalman: not enough innovations for diagnostics (need >= 5).");
        return;
    }

    const std::string stem = "kalman_" + dataset + "_" + component + "_diagnostics";
    loki::Plot corePlot(m_cfg);
    corePlot.residualDiagnostics(innovations, predicted, stem);

    LOKI_INFO("PlotKalman: diagnostics -> " + stem + "." + m_cfg.plots.outputFormat);
}

// -----------------------------------------------------------------------------
//  helpers
// -----------------------------------------------------------------------------

std::filesystem::path PlotKalman::outputPath(const std::string& dataset,
                                             const std::string& component,
                                             const std::string& plotType) const
{
    const std::string stem =
        "kalman_" + dataset + "_" + component + "_" + plotType
        + "." + m_cfg.plots.outputFormat;
    return m_cfg.imgDir / stem;
}

std::string PlotKalman::fwdSlash(const std::filesystem::path& p)
{
    std::string s = p.string();
    std::replace(s.begin(), s.end(), '\\', '/');
    return s;
}

std::string PlotKalman::terminalCmd(const std::filesystem::path& outPath,
                                    int widthPx,
                                    int heightPx) const
{
    const std::string fmt = m_cfg.plots.outputFormat;
    std::string terminal;
    if      (fmt == "png") { terminal = "pngcairo"; }
    else if (fmt == "svg") { terminal = "svg"; }
    else if (fmt == "eps") { terminal = "epscairo"; }
    else                   { terminal = "pngcairo"; }

    return "set terminal " + terminal + " noenhanced"
           + " size " + std::to_string(widthPx) + "," + std::to_string(heightPx)
           + " font 'Sans,12'\n"
           + "set output '" + fwdSlash(outPath) + "'";
}

} // namespace loki::kalman
