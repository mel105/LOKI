#include <loki/regression/plotRegression.hpp>
#include <loki/io/gnuplot.hpp>
#include <loki/io/plot.hpp>
#include <loki/core/logger.hpp>
#include <loki/core/exceptions.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>

using namespace loki;
using namespace loki::regression;

// ----------------------------------------------------------------------------
//  Path helper
// ----------------------------------------------------------------------------

static std::string fwdSlash(const std::filesystem::path& p)
{
    std::string s = p.string();
    for (auto& c : s) { if (c == '\\') c = '/'; }
    return s;
}

// ----------------------------------------------------------------------------
//  Construction
// ----------------------------------------------------------------------------

PlotRegression::PlotRegression(const AppConfig& cfg)
    : m_cfg{cfg}
{
    std::filesystem::create_directories(m_cfg.imgDir);
}

// ----------------------------------------------------------------------------
//  plotAll
// ----------------------------------------------------------------------------

void PlotRegression::plotAll(const TimeSeries&                  original,
                              const RegressionResult&             result,
                              const InfluenceMeasures&            influence,
                              const std::vector<PredictionPoint>& prediction) const
{
    const auto& p = m_cfg.plots;

    if (p.regressionOverlay) {
        try { plotOverlay(original, result); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotRegression: overlay failed: " + std::string(ex.what()));
        }
    }
    if (p.regressionResiduals) {
        try { plotResiduals(original, result); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotRegression: residuals failed: " + std::string(ex.what()));
        }
    }
    if (p.regressionQqBands) {
        try { plotQqBands(original, result); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotRegression: qq_bands failed: " + std::string(ex.what()));
        }
    }
    if (p.regressionCdfPlot) {
        try { plotCdf(original, result); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotRegression: cdf failed: " + std::string(ex.what()));
        }
    }
    if (p.regressionResidualAcf) {
        try { plotResidualAcf(original, result); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotRegression: residual_acf failed: " + std::string(ex.what()));
        }
    }
    if (p.regressionResidualHist) {
        try { plotResidualHist(original, result); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotRegression: residual_hist failed: " + std::string(ex.what()));
        }
    }
    if (p.regressionInfluence) {
        try { plotInfluence(original, result, influence); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotRegression: influence failed: " + std::string(ex.what()));
        }
    }
    if (p.regressionLeverage) {
        try { plotLeverage(original, result, influence); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotRegression: leverage failed: " + std::string(ex.what()));
        }
    }
    if (p.regressionOverlay && !prediction.empty()) {
        try { plotPrediction(original, result, prediction); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotRegression: prediction failed: " + std::string(ex.what()));
        }
    }

    // Generic plots on original series via loki::Plot.
    try {
        Plot plot{m_cfg};
        if (p.timeSeries) plot.timeSeries(original);
        if (p.histogram)  plot.histogram(original);
        if (p.acf)        plot.acf(original);
        if (p.qqPlot)     plot.qqPlot(original);
        if (p.boxplot)    plot.boxplot(original);
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotRegression: generic plot failed: " + std::string(ex.what()));
    }
}

// ----------------------------------------------------------------------------
//  plotOverlay
// ----------------------------------------------------------------------------

void PlotRegression::plotOverlay(const TimeSeries&       original,
                                  const RegressionResult& result) const
{
    const std::string base    = _baseName(original);
    const auto        datOrig = m_cfg.imgDir / (".tmp_" + base + "_ov_orig.dat");
    const auto        datFit  = m_cfg.imgDir / (".tmp_" + base + "_ov_fit.dat");
    const auto        outPath = _outPath(base, "overlay");

    _writeSeriesDat(original,      datOrig);
    _writeSeriesDat(result.fitted, datFit);

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal(1200, 400));
        gp("set output '" + fwdSlash(outPath) + "'");
        gp("set title 'Regression overlay: " + base + "' noenhanced");
        gp("set xlabel 'MJD'");
        gp("set ylabel 'Value'");
        gp("set grid");
        gp("set key top right");
        gp("plot '" + fwdSlash(datOrig) + "' u 1:2 w l lc rgb '#aaaaaa' lw 1.0 t 'Original',"
           " '" + fwdSlash(datFit)  + "' u 1:2 w l lc rgb '#d7191c' lw 2.0 t '"
           + result.modelName + "'");
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotRegression::plotOverlay failed: " + std::string(ex.what()));
    }
    std::filesystem::remove(datOrig);
    std::filesystem::remove(datFit);
}

// ----------------------------------------------------------------------------
//  plotResiduals
// ----------------------------------------------------------------------------

void PlotRegression::plotResiduals(const TimeSeries&       original,
                                    const RegressionResult& result) const
{
    const std::string base    = _baseName(original);
    const auto        datRes  = m_cfg.imgDir / (".tmp_" + base + "_res.dat");
    const auto        outPath = _outPath(base, "residuals");

    {
        std::ofstream dat(datRes);
        dat << std::fixed << std::setprecision(10);
        const int n = static_cast<int>(result.residuals.size());
        for (int i = 0; i < n; ++i) {
            const double mjd = (i < static_cast<int>(result.fitted.size()))
                               ? result.fitted[static_cast<std::size_t>(i)].time.mjd()
                               : 0.0;
            dat << mjd << "\t" << result.residuals[i] << "\n";
        }
    }

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal(1200, 400));
        gp("set output '" + fwdSlash(outPath) + "'");
        gp("set title 'Residuals: " + base + "' noenhanced");
        gp("set xlabel 'MJD'");
        gp("set ylabel 'Residual'");
        gp("set grid");
        gp("set key off");
        gp("set arrow 1 from graph 0,0.5 to graph 1,0.5 nohead lc rgb '#888888' lw 1 dt 2");
        gp("plot '" + fwdSlash(datRes) + "' u 1:2 w l lc rgb '#2c7bb6' lw 1.0 notitle");
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotRegression::plotResiduals failed: " + std::string(ex.what()));
    }
    std::filesystem::remove(datRes);
}

// ----------------------------------------------------------------------------
//  plotQqBands
// ----------------------------------------------------------------------------

void PlotRegression::plotQqBands(const TimeSeries&       original,
                                  const RegressionResult& result) const
{
    const std::string base = _baseName(original);

    try {
        Plot plot{m_cfg};
        std::vector<double> res(result.residuals.data(),
                                result.residuals.data() + result.residuals.size());
        plot.qqPlotWithBands(res, base + "_qq_bands");
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotRegression::plotQqBands failed: " + std::string(ex.what()));
    }
}

// ----------------------------------------------------------------------------
//  plotCdf
// ----------------------------------------------------------------------------

void PlotRegression::plotCdf(const TimeSeries&       original,
                              const RegressionResult& result) const
{
    const std::string base = _baseName(original);

    try {
        Plot plot{m_cfg};
        std::vector<double> res(result.residuals.data(),
                                result.residuals.data() + result.residuals.size());
        plot.cdfPlot(res, DistributionType::NORMAL, {}, base + "_cdf");
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotRegression::plotCdf failed: " + std::string(ex.what()));
    }
}

// ----------------------------------------------------------------------------
//  plotResidualAcf
// ----------------------------------------------------------------------------

void PlotRegression::plotResidualAcf(const TimeSeries&       original,
                                      const RegressionResult& result) const
{
    const std::string base    = _baseName(original);
    const auto        datAcf  = m_cfg.imgDir / (".tmp_" + base + "_res_acf.dat");
    const auto        outPath = _outPath(base, "residual_acf");

    const int n = static_cast<int>(result.residuals.size());
    if (n < 4) {
        LOKI_WARNING("PlotRegression::plotResidualAcf: too few residuals, skipping.");
        return;
    }

    std::vector<double> resVec(result.residuals.data(), result.residuals.data() + n);

    const int maxLag = std::min(n / 2, m_cfg.plots.options.acfMaxLag > 0
                                       ? m_cfg.plots.options.acfMaxLag : 60);
    const auto acf = _computeAcf(resVec, maxLag);

    {
        std::ofstream dat(datAcf);
        dat << std::fixed << std::setprecision(10);
        for (const auto& [lag, val] : acf)
            dat << lag << "\t" << val << "\n";
    }

    const double band = 1.96 / std::sqrt(static_cast<double>(n));

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal(1200, 400));
        gp("set output '" + fwdSlash(outPath) + "'");
        gp("set title 'ACF of residuals: " + base + "' noenhanced");
        gp("set xlabel 'Lag'");
        gp("set ylabel 'ACF'");
        gp("set grid");
        gp("set key off");
        gp("set yrange [-1.1:1.1]");
        gp("set arrow 1 from graph 0, first " + std::to_string( band)
           + " to graph 1, first " + std::to_string( band)
           + " nohead lc rgb '#d7191c' lw 1 dt 2");
        gp("set arrow 2 from graph 0, first " + std::to_string(-band)
           + " to graph 1, first " + std::to_string(-band)
           + " nohead lc rgb '#d7191c' lw 1 dt 2");
        gp("plot '" + fwdSlash(datAcf) + "' u 1:2 w impulses lc rgb '#2c7bb6' lw 2 notitle");
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotRegression::plotResidualAcf failed: " + std::string(ex.what()));
    }
    std::filesystem::remove(datAcf);
}

// ----------------------------------------------------------------------------
//  plotResidualHist
// ----------------------------------------------------------------------------

void PlotRegression::plotResidualHist(const TimeSeries&       original,
                                       const RegressionResult& result) const
{
    const std::string base    = _baseName(original);
    const auto        datHist = m_cfg.imgDir / (".tmp_" + base + "_res_hist.dat");
    const auto        outPath = _outPath(base, "residual_hist");

    const int n = static_cast<int>(result.residuals.size());
    if (n < 4) {
        LOKI_WARNING("PlotRegression::plotResidualHist: too few residuals, skipping.");
        return;
    }

    std::vector<double> vals(result.residuals.data(), result.residuals.data() + n);

    double mean = 0.0;
    for (const double v : vals) mean += v;
    mean /= static_cast<double>(n);

    double var = 0.0;
    for (const double v : vals) var += (v - mean) * (v - mean);
    const double sigma = std::sqrt(var / static_cast<double>(n));

    const double minVal = *std::min_element(vals.begin(), vals.end());
    const double maxVal = *std::max_element(vals.begin(), vals.end());

    const int bins = m_cfg.plots.options.histogramBins > 0
                   ? m_cfg.plots.options.histogramBins
                   : static_cast<int>(std::ceil(std::log2(static_cast<double>(n)) + 1.0));

    {
        std::ofstream dat(datHist);
        dat << std::fixed << std::setprecision(10);
        for (const double v : vals) dat << v << "\n";
    }

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal(1200, 400));
        gp("set output '" + fwdSlash(outPath) + "'");
        gp("set title 'Residuals histogram: " + base + "' noenhanced");
        gp("set xlabel 'Residual'");
        gp("set ylabel 'Count'");
        gp("set grid");
        gp("set key top right");
        gp("n = " + std::to_string(bins));
        gp("min = " + std::to_string(minVal));
        gp("max = " + std::to_string(maxVal));
        gp("width = (max - min) / n");
        gp("hist(x, w) = w * floor(x / w) + w / 2.0");
        gp("set boxwidth width * 0.9");
        gp("set style fill solid 0.6 border -1");
        gp("gauss(x) = (" + std::to_string(n) + " * width) * "
           "exp(-0.5 * ((x - " + std::to_string(mean) + ") / "
           + std::to_string(sigma) + ")**2) / ("
           + std::to_string(sigma) + " * sqrt(2.0 * pi))");
        gp("plot '" + fwdSlash(datHist) + "' u (hist($1, width)):(1.0) smooth freq "
           "w boxes lc rgb '#2c7bb6' t 'Residuals', "
           "gauss(x) w l lc rgb '#d7191c' lw 2 t 'Normal fit'");
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotRegression::plotResidualHist failed: " + std::string(ex.what()));
    }
    std::filesystem::remove(datHist);
}

// ----------------------------------------------------------------------------
//  plotInfluence
// ----------------------------------------------------------------------------

void PlotRegression::plotInfluence(const TimeSeries&        original,
                                    const RegressionResult&  result,
                                    const InfluenceMeasures& influence) const
{
    const std::string base    = _baseName(original);
    const auto        datCook = m_cfg.imgDir / (".tmp_" + base + "_cook.dat");
    const auto        outPath = _outPath(base, "influence");

    const int n = static_cast<int>(influence.cooksDistance.size());
    if (n == 0) {
        LOKI_WARNING("PlotRegression::plotInfluence: no influence measures, skipping.");
        return;
    }

    {
        std::ofstream dat(datCook);
        dat << std::fixed << std::setprecision(10);
        for (int i = 0; i < n; ++i)
            dat << i << "\t" << influence.cooksDistance[i] << "\n";
    }

    const double threshold = influence.cooksThreshold;

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal(1200, 400));
        gp("set output '" + fwdSlash(outPath) + "'");
        gp("set title 'Cook s distance: " + base + "' noenhanced");
        gp("set xlabel 'Observation index'");
        gp("set ylabel 'Cook s D'");
        gp("set grid");
        gp("set key off");
        gp("set arrow 1 from graph 0, first " + std::to_string(threshold)
           + " to graph 1, first " + std::to_string(threshold)
           + " nohead lc rgb '#d7191c' lw 1.5 dt 2");
        gp("set label 1 '4/n=" + std::to_string(threshold) + "' "
           "at graph 0.02, first " + std::to_string(threshold * 1.05)
           + " tc rgb '#d7191c' font 'Sans,10'");
        gp("set style fill solid 0.7");
        gp("plot '" + fwdSlash(datCook) + "' u 1:2 w impulses lc rgb '#2c7bb6' lw 1.5 notitle");
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotRegression::plotInfluence failed: " + std::string(ex.what()));
    }
    std::filesystem::remove(datCook);
    (void)result;
}

// ----------------------------------------------------------------------------
//  plotLeverage
// ----------------------------------------------------------------------------

void PlotRegression::plotLeverage(const TimeSeries&        original,
                                   const RegressionResult&  result,
                                   const InfluenceMeasures& influence) const
{
    const std::string base    = _baseName(original);
    const auto        datLev  = m_cfg.imgDir / (".tmp_" + base + "_lev.dat");
    const auto        outPath = _outPath(base, "leverage");

    const int n = static_cast<int>(influence.leverages.size());
    if (n == 0) {
        LOKI_WARNING("PlotRegression::plotLeverage: no influence measures, skipping.");
        return;
    }

    {
        std::ofstream dat(datLev);
        dat << std::fixed << std::setprecision(10);
        for (int i = 0; i < n; ++i)
            dat << influence.leverages[i] << "\t"
                << influence.standardizedResiduals[i] << "\n";
    }

    const double hThresh = influence.leverageThreshold;

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal(1200, 500));
        gp("set output '" + fwdSlash(outPath) + "'");
        gp("set title 'Leverage vs standardized residuals: " + base + "' noenhanced");
        gp("set xlabel 'Leverage h_ii'");
        gp("set ylabel 'Standardized residual'");
        gp("set grid");
        gp("set key off");
        gp("set arrow 1 from " + std::to_string(hThresh) + ", graph 0"
           + " to " + std::to_string(hThresh) + ", graph 1"
           + " nohead lc rgb '#d7191c' lw 1.5 dt 2");
        gp("set arrow 2 from graph 0, first  2.0 to graph 1, first  2.0"
           " nohead lc rgb '#888888' lw 1 dt 3");
        gp("set arrow 3 from graph 0, first -2.0 to graph 1, first -2.0"
           " nohead lc rgb '#888888' lw 1 dt 3");
        gp("set label 1 '2p/n=" + std::to_string(hThresh) + "' "
           "at " + std::to_string(hThresh * 1.02) + ", graph 0.05"
           + " tc rgb '#d7191c' font 'Sans,10'");
        gp("plot '" + fwdSlash(datLev) + "' u 1:2 w p pt 7 ps 0.5 lc rgb '#2c7bb6' notitle");
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotRegression::plotLeverage failed: " + std::string(ex.what()));
    }
    std::filesystem::remove(datLev);
    (void)result;
}

// ----------------------------------------------------------------------------
//  plotPrediction
// ----------------------------------------------------------------------------

void PlotRegression::plotPrediction(const TimeSeries&                  original,
                                     const RegressionResult&             result,
                                     const std::vector<PredictionPoint>& prediction) const
{
    if (prediction.empty()) {
        LOKI_WARNING("PlotRegression::plotPrediction: prediction vector is empty, skipping.");
        return;
    }
 
    const std::string base    = _baseName(original);
    const auto        datOrig = m_cfg.imgDir / (".tmp_" + base + "_pred_orig.dat");
    const auto        datFit  = m_cfg.imgDir / (".tmp_" + base + "_pred_fit.dat");
    const auto        datPred = m_cfg.imgDir / (".tmp_" + base + "_pred_fc.dat");
    const auto        outPath = _outPath(base, "prediction");
 
    _writeSeriesDat(original,      datOrig);
    _writeSeriesDat(result.fitted, datFit);
 
    // Write forecast: mjd, predicted, conf_low, conf_high, pred_low, pred_high.
    // PredictionPoint.x is mjd - tRef, convert back to absolute MJD.
    {
        std::ofstream dat(datPred);
        dat << std::fixed << std::setprecision(10);
        for (const auto& pt : prediction) {
            const double mjd = pt.x + result.tRef;
            dat << mjd          << "\t"
                << pt.predicted << "\t"
                << pt.confLow   << "\t"
                << pt.confHigh  << "\t"
                << pt.predLow   << "\t"
                << pt.predHigh  << "\n";
        }
    }
 
    // Vertical separator at end of fitted region.
    double splitMjd = 0.0;
    if (result.fitted.size() > 0)
        splitMjd = result.fitted[result.fitted.size() - 1].time.mjd();
 
    try {
        Gnuplot gp;
        gp("set terminal " + _terminal(1400, 500));
        gp("set output '" + fwdSlash(outPath) + "'");
        gp("set title 'Regression forecast: " + base + "' noenhanced");
        gp("set xlabel 'MJD'");
        gp("set ylabel 'Value'");
        gp("set grid");
        gp("set key top left");
 
        // Vertical dashed line at fit/forecast boundary.
        gp("set arrow 1 from " + std::to_string(splitMjd) + ", graph 0"
           + " to "            + std::to_string(splitMjd) + ", graph 1"
           + " nohead lc rgb '#888888' lw 1.5 dt 2");
 
        // col 1=mjd, 2=predicted, 3=conf_low, 4=conf_high, 5=pred_low, 6=pred_high
        gp("plot '" + fwdSlash(datOrig) + "' u 1:2"
           " w l lc rgb '#cccccc' lw 1.0 t 'Original',"
           " '" + fwdSlash(datFit) + "' u 1:2"
           " w l lc rgb '#d7191c' lw 2.0 t 'Fitted',"
           " '" + fwdSlash(datPred) + "' u 1:5:6"
           " w filledcurves lc rgb '#d0e8f5' fs transparent solid 0.5 t 'Prediction interval',"
           " '" + fwdSlash(datPred) + "' u 1:3:4"
           " w filledcurves lc rgb '#6baed6' fs transparent solid 0.6 t 'Confidence interval',"
           " '" + fwdSlash(datPred) + "' u 1:2"
           " w l lc rgb '#2c7bb6' lw 2.0 t 'Forecast'");
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotRegression::plotPrediction failed: " + std::string(ex.what()));
    }
 
    std::filesystem::remove(datOrig);
    std::filesystem::remove(datFit);
    std::filesystem::remove(datPred);
}

// ----------------------------------------------------------------------------
//  Private helpers
// ----------------------------------------------------------------------------

std::string PlotRegression::_baseName(const TimeSeries& series) const
{
    const auto& meta    = series.metadata();
    std::string dataset = m_cfg.input.file.stem().string();
    if (dataset.empty()) dataset = "data";
    std::string param   = meta.componentName.empty() ? "series" : meta.componentName;
    return "regression_" + dataset + "_" + param;
}

std::filesystem::path PlotRegression::_outPath(const std::string& base,
                                                const std::string& plotType) const
{
    return m_cfg.imgDir / (base + "_" + plotType + "." + m_cfg.plots.outputFormat);
}

std::string PlotRegression::_terminal(int widthPx, int heightPx) const
{
    const std::string& fmt = m_cfg.plots.outputFormat;
    if (fmt == "eps")
        return "postscript eps color noenhanced solid font 'Sans,12'";
    if (fmt == "svg")
        return "svg noenhanced font 'Sans,12' size "
               + std::to_string(widthPx) + "," + std::to_string(heightPx);
    return "pngcairo noenhanced font 'Sans,12' size "
           + std::to_string(widthPx) + "," + std::to_string(heightPx);
}

void PlotRegression::_writeSeriesDat(const TimeSeries&            series,
                                      const std::filesystem::path& path) const
{
    std::ofstream dat(path);
    dat << std::fixed << std::setprecision(10);
    for (std::size_t i = 0; i < series.size(); ++i)
        dat << series[i].time.mjd() << "\t" << series[i].value << "\n";
}

void PlotRegression::_writeVectorDat(const std::vector<double>&   x,
                                      const std::vector<double>&   y,
                                      const std::filesystem::path& path) const
{
    std::ofstream dat(path);
    dat << std::fixed << std::setprecision(10);
    const std::size_t n = std::min(x.size(), y.size());
    for (std::size_t i = 0; i < n; ++i)
        dat << x[i] << "\t" << y[i] << "\n";
}

std::vector<std::pair<double, double>>
PlotRegression::_computeAcf(const std::vector<double>& values, int maxLag)
{
    const std::size_t n    = values.size();
    const double      mean = std::accumulate(values.begin(), values.end(), 0.0)
                             / static_cast<double>(n);
    double var = 0.0;
    for (const double v : values) var += (v - mean) * (v - mean);
    if (var == 0.0)
        throw AlgorithmException(
            "PlotRegression::_computeAcf: residuals have zero variance.");

    std::vector<std::pair<double, double>> acf;
    acf.reserve(static_cast<std::size_t>(maxLag) + 1);
    for (int lag = 0; lag <= maxLag; ++lag) {
        double cov = 0.0;
        for (std::size_t i = 0; i + static_cast<std::size_t>(lag) < n; ++i)
            cov += (values[i] - mean)
                 * (values[i + static_cast<std::size_t>(lag)] - mean);
        acf.emplace_back(static_cast<double>(lag), cov / var);
    }
    return acf;
}