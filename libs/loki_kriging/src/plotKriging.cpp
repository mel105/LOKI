#include <loki/kriging/plotKriging.hpp>

#include <loki/core/exceptions.hpp>
#include <loki/math/krigingVariogram.hpp>
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
            const double g = loki::math::variogramEval(h, result.variogram);
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

    // Collect y range from predictions and original observations
    double yMin =  1.0e15;
    double yMax = -1.0e15;
    for (const auto& p : result.predictions) {
        yMin = std::min(yMin, p.ciLower);
        yMax = std::max(yMax, p.ciUpper);
    }
    for (std::size_t i = 0; i < ts.size(); ++i) {
        if (!std::isnan(ts[i].value)) {
            yMin = std::min(yMin, ts[i].value);
            yMax = std::max(yMax, ts[i].value);
        }
    }
    const double yPad = (yMax - yMin) * 0.05;

    // Map each prediction MJD to a sequential index for the x-axis.
    // This avoids gnuplot precision issues when MJD values span < 1 day.
    const int nPred = static_cast<int>(result.predictions.size());

    // Build a map: mjd -> index (sorted predictions are already in time order)
    // For observed points, find the closest prediction index.
    std::vector<double> predMjd(static_cast<std::size_t>(nPred));
    for (int i = 0; i < nPred; ++i)
        predMjd[static_cast<std::size_t>(i)] = result.predictions[static_cast<std::size_t>(i)].mjd;

    try {
        Gnuplot gp;
        gp("set terminal pngcairo noenhanced font 'Sans,12' size 1100,550");
        gp("set output '" + outPath + "'");
        gp("set title 'Kriging predictions: " + datasetName
           + " / " + result.componentName + "'");
        gp("set xlabel 'Sample index'");
        gp("set ylabel 'Value'");
        gp("set grid");
        gp("set key top right");
        gp("set xrange [-1:" + std::to_string(nPred) + "]");
        gp("set yrange [" + std::to_string(yMin - yPad)
           + ":" + std::to_string(yMax + yPad) + "]");

        gp("plot "
           "'-' using 1:2:3 with filledcurves lc rgb '#ffb3c1' fs transparent solid 0.5 "
           "title 'CI', "
           "'-' using 1:2 with lines lw 1.5 lc rgb '#1f77b4' title 'Kriging', "
           "'-' using 1:2 with points pt 6 ps 0.8 lc rgb '#d62728' title 'Observed'");

        // Block 1: CI band (index, ci_lower, ci_upper)
        for (int i = 0; i < nPred; ++i) {
            const auto& p = result.predictions[static_cast<std::size_t>(i)];
            std::ostringstream ss;
            ss << i << " " << p.ciLower << " " << p.ciUpper;
            gp(ss.str());
        }
        gp("e");

        // Block 2: Kriging line (index, value)
        for (int i = 0; i < nPred; ++i) {
            const auto& p = result.predictions[static_cast<std::size_t>(i)];
            std::ostringstream ss;
            ss << i << " " << p.value;
            gp(ss.str());
        }
        gp("e");

        // Block 3: Original observations mapped to nearest prediction index
        for (std::size_t oi = 0; oi < ts.size(); ++oi) {
            if (std::isnan(ts[oi].value)) continue;
            const double tmjd = ts[oi].time.mjd();
            // Find closest prediction index by MJD
            int best = 0;
            double bestDist = std::abs(predMjd[0] - tmjd);
            for (int pi = 1; pi < nPred; ++pi) {
                const double d = std::abs(predMjd[static_cast<std::size_t>(pi)] - tmjd);
                if (d < bestDist) { bestDist = d; best = pi; }
            }
            std::ostringstream ss;
            ss << best << " " << ts[oi].value;
            gp(ss.str());
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

    // Two-panel plot using datablocks (required inside multiplot -- '-' not supported).
    try {
        // Build data strings before opening gnuplot
        const double rmse   = cv.rmse;
        const int    nBins  = 20;
        const double loHist = -4.0;
        const double hiHist =  4.0;
        const double bw     = (hiHist - loHist) / static_cast<double>(nBins);
        const double pi     = 3.14159265358979323846;

        // Histogram of standardised errors
        std::vector<int> hist(static_cast<std::size_t>(nBins), 0);
        for (const double se : cv.stdErrors) {
            const int b = static_cast<int>((se - loHist) / bw);
            if (b >= 0 && b < nBins)
                hist[static_cast<std::size_t>(b)]++;
        }
        const double total = static_cast<double>(cv.stdErrors.size());

        // Build datablock strings -- use sequential index on x-axis
        // to avoid gnuplot float precision issues with dense MJD values.
        const int nCV = static_cast<int>(cv.errors.size());
        std::ostringstream dsErrors;
        for (int i = 0; i < nCV; ++i)
            dsErrors << i << " " << cv.errors[static_cast<std::size_t>(i)] << "\n";


        std::ostringstream dsHist;
        for (int k = 0; k < nBins; ++k) {
            const double centre = loHist + (static_cast<double>(k) + 0.5) * bw;
            dsHist << centre << " "
                   << static_cast<double>(hist[static_cast<std::size_t>(k)]) / total
                   << "\n";

        }

        std::ostringstream dsNorm;
        for (int k = 0; k <= 100; ++k) {
            const double x = loHist + static_cast<double>(k) * (hiHist - loHist) / 100.0;
            const double y = bw * std::exp(-0.5 * x * x) / std::sqrt(2.0 * pi);
            dsNorm << x << " " << y << "\n";

        }

        Gnuplot gp;
        gp("set terminal pngcairo noenhanced font 'Sans,12' size 1000,700");
        gp("set output '" + outPath + "'");

        // Define datablocks BEFORE multiplot
        gp("$errors << EOD");
        gp(dsErrors.str() + "EOD");

        gp("$hist << EOD");
        gp(dsHist.str() + "EOD");

        gp("$norm << EOD");
        gp(dsNorm.str() + "EOD");

        gp("set multiplot layout 2,1 title 'LOO Cross-validation: "
           + datasetName + " / " + result.componentName + "'");

        // --- Top panel: errors vs time ---
        gp("set xlabel 'Sample index'");
        gp("set ylabel 'LOO Error'");
        gp("set grid");
        gp("set xrange [-1:" + std::to_string(nCV) + "]");
        gp("set yrange [" + std::to_string(-3.0 * rmse)
           + ":" + std::to_string(3.0 * rmse) + "]");
        gp("set arrow 1 from graph 0,first "  + std::to_string( rmse)
           + " to graph 1,first " + std::to_string( rmse)
           + " nohead lc rgb '#888888' dt 2");
        gp("set arrow 2 from graph 0,first "  + std::to_string(-rmse)
           + " to graph 1,first " + std::to_string(-rmse)
           + " nohead lc rgb '#888888' dt 2");
        gp("set label 1 'RMSE=" + std::to_string(rmse).substr(0, 7)
           + "' at graph 0.02, graph 0.95 font 'Sans,10'");

        gp("plot $errors using 1:2 with points pt 7 ps 0.7 lc rgb '#1f77b4' notitle, "
           "0 with lines lc rgb 'black' notitle");

        // --- Bottom panel: histogram of standardised errors ---
        gp("unset arrow 1");
        gp("unset arrow 2");
        gp("unset label 1");
        gp("set xlabel 'Standardised LOO Error'");
        gp("set ylabel 'Frequency'");
        gp("set xrange [-4:4]");
        gp("unset yrange");
        gp("set grid");
        gp("set label 2 'meanSE=" + std::to_string(cv.meanSE).substr(0, 6)
           + "  meanSSE=" + std::to_string(cv.meanSSE).substr(0, 6)
           + "' at graph 0.02, graph 0.95 font 'Sans,10'");

        gp("plot $hist using 1:2 with boxes lc rgb '#aec7e8' fs solid 0.7 "
           "title 'Std errors', "
           "$norm using 1:2 with lines lw 2 lc rgb '#d62728' title 'N(0,1)'");

        gp("unset multiplot");
        LOKI_INFO("  Plot: " + outPath);
    } catch (const std::exception& ex) {
        LOKI_WARNING("PlotKriging::_plotCrossValidation failed: "
                     + std::string(ex.what()));
    }
}

} // namespace loki::kriging
