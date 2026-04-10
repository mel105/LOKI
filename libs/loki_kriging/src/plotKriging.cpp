#include <loki/kriging/plotKriging.hpp>
#include <loki/kriging/variogram.hpp> 

#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/io/gnuplot.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>

using namespace loki;

namespace loki::kriging {

// =============================================================================
//  Constructor
// =============================================================================

PlotKriging::PlotKriging(const AppConfig& cfg)
    : m_cfg(cfg)
{}

// =============================================================================
//  Helpers
// =============================================================================

std::string PlotKriging::fwdSlash(const std::string& path)
{
    std::string s = path;
    for (char& c : s) if (c == '\\') c = '/';
    return s;
}

std::string PlotKriging::_plotPath(const std::string& dataset,
                                    const std::string& component,
                                    const std::string& plottype) const
{
    const std::string fmt = m_cfg.plots.outputFormat.empty()
        ? "png" : m_cfg.plots.outputFormat;
    const std::string filename = "kriging_" + dataset + "_"
                               + component + "_" + plottype + "." + fmt;
    return fwdSlash((m_cfg.imgDir / filename).string());
}

// =============================================================================
//  plot() -- dispatcher
// =============================================================================

void PlotKriging::plot(const KrigingResult& result,
                        const TimeSeries&    ts,
                        const std::string&   datasetName) const
{
    if (m_cfg.plots.krigingVariogram)
        _plotVariogram(result, datasetName);

    if (m_cfg.plots.krigingPredictions)
        _plotPredictions(result, ts, datasetName);

    if (m_cfg.plots.krigingCrossval
        && m_cfg.kriging.crossValidate
        && !result.crossValidation.errors.empty())
        _plotCrossValidation(result, datasetName);
}

// =============================================================================
//  _plotVariogram
// =============================================================================

void PlotKriging::_plotVariogram(const KrigingResult& result,
                                  const std::string&   datasetName) const
{
    if (result.empiricalVariogram.empty()) return;

    const std::string outPath = _plotPath(datasetName,
                                          result.componentName,
                                          "variogram");

    // Determine x/y ranges
    double maxLag   = result.empiricalVariogram.back().lag;
    double maxGamma = 0.0;
    for (const auto& pt : result.empiricalVariogram)
        maxGamma = std::max(maxGamma, pt.gamma);
    maxGamma = std::max(maxGamma, result.variogram.sill) * 1.15;

    // Build theoretical curve (200 points)
    const int    nCurve  = 200;
    const double step    = maxLag / static_cast<double>(nCurve);

    try {
        Gnuplot gp;
        gp("set terminal pngcairo noenhanced font 'Sans,12' size 900,600");
        gp("set output '" + outPath + "'");
        gp("set title 'Variogram: " + datasetName
           + " / " + result.componentName + "'");
        gp("set xlabel 'Lag (days)'");
        gp("set ylabel 'Semi-variance'");
        gp("set xrange [0:" + std::to_string(maxLag * 1.05) + "]");
        gp("set yrange [0:" + std::to_string(maxGamma) + "]");
        gp("set grid");
        gp("set key top left");

        // Annotations
        gp("set arrow from " + std::to_string(result.variogram.range)
           + ",0 to " + std::to_string(result.variogram.range) + ","
           + std::to_string(result.variogram.sill - result.variogram.nugget)
           + " nohead lc rgb '#888888' dt 2");
        gp("set label 1 'range' at " + std::to_string(result.variogram.range)
           + "," + std::to_string((result.variogram.sill - result.variogram.nugget) * 0.5)
           + " left offset 0.5,0 font 'Sans,10'");
        gp("set arrow from 0," + std::to_string(result.variogram.sill)
           + " to " + std::to_string(maxLag * 0.95) + ","
           + std::to_string(result.variogram.sill)
           + " nohead lc rgb '#888888' dt 3");
        gp("set label 2 'sill' at " + std::to_string(maxLag * 0.05) + ","
           + std::to_string(result.variogram.sill)
           + " left offset 0,0.5 font 'Sans,10'");

        // Two datasets: empirical (points) + theoretical curve (line)
        gp("plot '-' using 1:2 with points pt 7 ps 1.2 lc rgb '#1f77b4' "
           "title 'Empirical', "
           "'-' using 1:2 with lines lw 2 lc rgb '#d62728' "
           "title '" + result.variogram.model + " fit'");

        // Empirical data
        for (const auto& pt : result.empiricalVariogram) {
            std::ostringstream ss;
            ss << pt.lag << " " << pt.gamma;
            gp(ss.str());
        }
        gp("e");

        // Theoretical curve
        for (int k = 0; k <= nCurve; ++k) {
            const double h = static_cast<double>(k) * step;
            const double g = variogramEval(h, result.variogram);
            std::ostringstream ss;
            ss << h << " " << g;
            gp(ss.str());
        }
        gp("e");

        LOKI_INFO("  Plot: " + outPath);
    } catch (const std::exception& ex) {
        LOKI_WARNING("PlotKriging::_plotVariogram failed: " + std::string(ex.what()));
    }
}

// =============================================================================
//  _plotPredictions
// =============================================================================

void PlotKriging::_plotPredictions(const KrigingResult& result,
                                    const TimeSeries&    ts,
                                    const std::string&   datasetName) const
{
    if (result.predictions.empty()) return;

    const std::string outPath = _plotPath(datasetName,
                                          result.componentName,
                                          "predictions");

    // Build inline data blocks:
    //   Block 1: original observations (mjd, value)
    //   Block 2: all Kriging predictions (mjd, value, ci_lower, ci_upper)
    //   Block 3: forecast only (non-observed predictions)

    try {
        Gnuplot gp;
        gp("set terminal pngcairo noenhanced font 'Sans,12' size 1100,550");
        gp("set output '" + outPath + "'");
        gp("set title 'Kriging predictions: " + datasetName
           + " / " + result.componentName + "'");
        gp("set xlabel 'MJD'");
        gp("set ylabel 'Value'");
        gp("set grid");
        gp("set key top right");

        gp("plot "
           "'-' using 1:3:4 with filledcurves lc rgb '#aec7e8' fs transparent solid 0.4 "
           "notitle, "
           "'-' using 1:2 with lines lw 1.5 lc rgb '#1f77b4' title 'Kriging', "
           "'-' using 1:2 with points pt 6 ps 0.8 lc rgb '#d62728' title 'Observed'");

        // Block 1: CI band (mjd, value, ci_lower, ci_upper) for filledcurves
        for (const auto& p : result.predictions) {
            std::ostringstream ss;
            ss << p.mjd << " " << p.value << " " << p.ciLower << " " << p.ciUpper;
            gp(ss.str());
        }
        gp("e");

        // Block 2: Kriging line (mjd, value)
        for (const auto& p : result.predictions) {
            std::ostringstream ss;
            ss << p.mjd << " " << p.value;
            gp(ss.str());
        }
        gp("e");

        // Block 3: Original observations (mjd, value) from original series
        for (std::size_t i = 0; i < ts.size(); ++i) {
            if (!std::isnan(ts[i].value)) {
                std::ostringstream ss;
                ss << ts[i].time.mjd() << " " << ts[i].value;
                gp(ss.str());
            }
        }
        gp("e");

        LOKI_INFO("  Plot: " + outPath);
    } catch (const std::exception& ex) {
        LOKI_WARNING("PlotKriging::_plotPredictions failed: " + std::string(ex.what()));
    }
}

// =============================================================================
//  _plotCrossValidation
// =============================================================================

void PlotKriging::_plotCrossValidation(const KrigingResult& result,
                                        const std::string&   datasetName) const
{
    const auto& cv = result.crossValidation;
    if (cv.errors.empty()) return;

    const std::string outPath = _plotPath(datasetName,
                                          result.componentName,
                                          "crossval");

    // Two-panel plot: errors vs time (top), std error histogram (bottom)
    try {
        Gnuplot gp;
        gp("set terminal pngcairo noenhanced font 'Sans,12' size 1000,700");
        gp("set output '" + outPath + "'");
        gp("set multiplot layout 2,1 title 'LOO Cross-validation: "
           + datasetName + " / " + result.componentName + "'");

        // --- Top panel: errors vs time ---
        const double rmse = cv.rmse;
        gp("set xlabel 'MJD'");
        gp("set ylabel 'LOO Error'");
        gp("set grid");
        gp("set yrange [" + std::to_string(-3.0 * rmse)
           + ":" + std::to_string(3.0 * rmse) + "]");

        // +/- RMSE reference lines
        gp("set arrow 1 from graph 0,first "  + std::to_string( rmse)
           + " to graph 1,first " + std::to_string( rmse)
           + " nohead lc rgb '#888888' dt 2");
        gp("set arrow 2 from graph 0,first "  + std::to_string(-rmse)
           + " to graph 1,first " + std::to_string(-rmse)
           + " nohead lc rgb '#888888' dt 2");
        gp("set label 1 'RMSE=" + std::to_string(rmse).substr(0, 7)
           + "' at graph 0.02, graph 0.95 font 'Sans,10'");

        gp("plot '-' using 1:2 with points pt 7 ps 0.7 lc rgb '#1f77b4' "
           "notitle, '-' using 1:($2*0) with lines lc rgb 'black' notitle");

        for (std::size_t i = 0; i < cv.errors.size(); ++i) {
            std::ostringstream ss;
            ss << cv.mjd[i] << " " << cv.errors[i];
            gp(ss.str());
        }
        gp("e");
        // zero line
        for (std::size_t i = 0; i < cv.errors.size(); ++i) {
            std::ostringstream ss;
            ss << cv.mjd[i] << " " << cv.errors[i];
            gp(ss.str());
        }
        gp("e");

        // --- Bottom panel: histogram of standardised errors ---
        gp("unset arrow 1");
        gp("unset arrow 2");
        gp("unset label 1");

        // Compute histogram manually (20 bins over [-4, 4])
        const int    nBins  = 20;
        const double loHist = -4.0;
        const double hiHist =  4.0;
        const double bw     = (hiHist - loHist) / static_cast<double>(nBins);
        std::vector<int> hist(static_cast<std::size_t>(nBins), 0);
        for (const double se : cv.stdErrors) {
            const int b = static_cast<int>((se - loHist) / bw);
            if (b >= 0 && b < nBins)
                hist[static_cast<std::size_t>(b)]++;
        }
        const double total  = static_cast<double>(cv.stdErrors.size());

        gp("set xlabel 'Standardised LOO Error'");
        gp("set ylabel 'Frequency'");
        gp("set xrange [-4:4]");
        gp("unset yrange");
        gp("set grid");
        gp("set label 2 'meanSE=" + std::to_string(cv.meanSE).substr(0, 6)
           + "  meanSSE=" + std::to_string(cv.meanSSE).substr(0, 6)
           + "' at graph 0.02, graph 0.95 font 'Sans,10'");

        // N(0,1) reference curve (density * bw * n)
        gp("plot '-' using 1:2 with boxes lc rgb '#aec7e8' fs solid 0.7 "
           "title 'Std errors', "
           "'-' using 1:2 with lines lw 2 lc rgb '#d62728' title 'N(0,1)'");

        for (int k = 0; k < nBins; ++k) {
            const double centre = loHist + (static_cast<double>(k) + 0.5) * bw;
            std::ostringstream ss;
            ss << centre << " " << static_cast<double>(hist[static_cast<std::size_t>(k)]) / total;
            gp(ss.str());
        }
        gp("e");

        // N(0,1) curve
        const int    nNorm = 100;
        const double pi    = 3.14159265358979323846;
        for (int k = 0; k <= nNorm; ++k) {
            const double x = loHist + static_cast<double>(k) * (hiHist - loHist) / static_cast<double>(nNorm);
            const double y = bw * std::exp(-0.5 * x * x) / std::sqrt(2.0 * pi);
            std::ostringstream ss;
            ss << x << " " << y;
            gp(ss.str());
        }
        gp("e");

        gp("unset multiplot");
        LOKI_INFO("  Plot: " + outPath);
    } catch (const std::exception& ex) {
        LOKI_WARNING("PlotKriging::_plotCrossValidation failed: "
                     + std::string(ex.what()));
    }
}

} // namespace loki::kriging
