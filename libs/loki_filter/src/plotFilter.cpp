#include "loki/filter/plotFilter.hpp"
#include "loki/io/gnuplot.hpp"
#include "loki/io/plot.hpp"
#include "loki/core/logger.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>

using namespace loki;
using namespace loki::filter;

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

PlotFilter::PlotFilter(const AppConfig& cfg)
    : m_cfg{cfg}
{
    std::filesystem::create_directories(m_cfg.imgDir);
}

// ----------------------------------------------------------------------------
//  Public: plotAll
// ----------------------------------------------------------------------------

void PlotFilter::plotAll(const TimeSeries&   original,
                         const TimeSeries&   filled,
                         const FilterResult& result) const
{
    const auto& p = m_cfg.plots;

    // -- Pipeline-specific plots ----------------------------------------------
    if (p.filterOverlay) {
        try { plotOverlay(filled, result); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotFilter: overlay failed: " + std::string(ex.what()));
        }
    }

    if (p.filterOverlayResiduals) {
        try { plotOverlayResiduals(filled, result); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotFilter: overlay_residuals failed: " + std::string(ex.what()));
        }
    }

    if (p.filterResiduals) {
        try { plotResiduals(filled, result); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotFilter: residuals failed: " + std::string(ex.what()));
        }
    }

    if (p.filterResidualsAcf) {
        try { plotResidualsAcf(filled, result); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotFilter: residuals_acf failed: " + std::string(ex.what()));
        }
    }

    if (p.residualsHistogram) {
        try { plotResidualsHistogram(filled, result); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotFilter: residuals_histogram failed: " + std::string(ex.what()));
        }
    }

    if (p.residualsQq) {
        try { plotResidualsQq(filled, result); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotFilter: residuals_qq failed: " + std::string(ex.what()));
        }
    }

    // -- Generic plots via loki::Plot (on original series) --------------------
    try {
        Plot plot{m_cfg};
        if (p.timeSeries) plot.timeSeries(original);
        if (p.histogram)  plot.histogram(original);
        if (p.acf)        plot.acf(original);
        if (p.qqPlot)     plot.qqPlot(original);
        if (p.boxplot)    plot.boxplot(original);
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotFilter: generic plot failed: " + std::string(ex.what()));
    }
}

// ----------------------------------------------------------------------------
//  Public: plotOverlay
// ----------------------------------------------------------------------------

void PlotFilter::plotOverlay(const TimeSeries&   filled,
                              const FilterResult& result) const
{
    const std::string base    = _baseName(filled);
    const auto        datOrig = m_cfg.imgDir / (".tmp_" + base + "_ov_orig.dat");
    const auto        datFilt = m_cfg.imgDir / (".tmp_" + base + "_ov_filt.dat");
    const auto        outPath = _outPath(base, "overlay");

    _writeSeriesDat(filled, datOrig);
    _writeSeriesDat(result.filtered, datFilt);

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal(1200, 400));
        gp("set output '" + fwdSlash(outPath) + "'");
        gp("set title 'Original + filtered: " + base + "' noenhanced");
        gp("set xlabel 'MJD'");
        gp("set ylabel 'Value'");
        gp("set grid");
        gp("plot '" + fwdSlash(datOrig) + "' u 1:2 w l lc rgb '#aaaaaa' lw 1.0 t 'Original',"
           " '" + fwdSlash(datFilt) + "' u 1:2 w l lc rgb '#d7191c' lw 2.0 t '"
           + result.filterName + "'");
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotFilter::plotOverlay failed: " + std::string(ex.what()));
    }
    std::filesystem::remove(datOrig);
    std::filesystem::remove(datFilt);
}

// ----------------------------------------------------------------------------
//  Public: plotOverlayResiduals
// ----------------------------------------------------------------------------

void PlotFilter::plotOverlayResiduals(const TimeSeries&   filled,
                                       const FilterResult& result) const
{
    const std::string base    = _baseName(filled);
    const auto        datOrig = m_cfg.imgDir / (".tmp_" + base + "_ovr_orig.dat");
    const auto        datFilt = m_cfg.imgDir / (".tmp_" + base + "_ovr_filt.dat");
    const auto        datRes  = m_cfg.imgDir / (".tmp_" + base + "_ovr_res.dat");
    const auto        outPath = _outPath(base, "overlay_residuals");

    _writeSeriesDat(filled, datOrig);
    _writeSeriesDat(result.filtered, datFilt);
    _writeSeriesDat(result.residuals, datRes);

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal(1200, 700));
        gp("set output '" + fwdSlash(outPath) + "'");
        gp("set multiplot layout 2,1 title 'Filter result: " + base + "' noenhanced");

        // Upper panel: overlay
        gp("set tmargin 3");
        gp("set bmargin 0");
        gp("unset xlabel");
        gp("set ylabel 'Value'");
        gp("set grid");
        gp("set key top right");
        gp("plot '" + fwdSlash(datOrig) + "' u 1:2 w l lc rgb '#aaaaaa' lw 1.0 t 'Original',"
           " '" + fwdSlash(datFilt) + "' u 1:2 w l lc rgb '#d7191c' lw 2.0 t '"
           + result.filterName + "'");

        // Lower panel: residuals
        gp("set tmargin 0");
        gp("set bmargin 3");
        gp("set xlabel 'MJD'");
        gp("set ylabel 'Residual'");
        gp("set key off");
        gp("plot '" + fwdSlash(datRes) + "' u 1:2 w l lc rgb '#2c7bb6' lw 1.0 notitle");

        gp("unset multiplot");
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotFilter::plotOverlayResiduals failed: " + std::string(ex.what()));
    }
    std::filesystem::remove(datOrig);
    std::filesystem::remove(datFilt);
    std::filesystem::remove(datRes);
}

// ----------------------------------------------------------------------------
//  Public: plotResiduals
// ----------------------------------------------------------------------------

void PlotFilter::plotResiduals(const TimeSeries&   filled,
                                const FilterResult& result) const
{
    const std::string base    = _baseName(filled);
    const auto        datRes  = m_cfg.imgDir / (".tmp_" + base + "_res.dat");
    const auto        outPath = _outPath(base, "residuals");

    _writeSeriesDat(result.residuals, datRes);

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal(1200, 400));
        gp("set output '" + fwdSlash(outPath) + "'");
        gp("set title 'Residuals: " + base + "' noenhanced");
        gp("set xlabel 'MJD'");
        gp("set ylabel 'Residual'");
        gp("set grid");
        gp("set key off");
        gp("plot '" + fwdSlash(datRes) + "' u 1:2 w l lc rgb '#2c7bb6' lw 1.0 notitle");
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotFilter::plotResiduals failed: " + std::string(ex.what()));
    }
    std::filesystem::remove(datRes);
}

// ----------------------------------------------------------------------------
//  Public: plotResidualsAcf
// ----------------------------------------------------------------------------

void PlotFilter::plotResidualsAcf(const TimeSeries&   filled,
                                   const FilterResult& result) const
{
    const std::string base    = _baseName(filled);
    const auto        datAcf  = m_cfg.imgDir / (".tmp_" + base + "_res_acf.dat");
    const auto        outPath = _outPath(base, "residuals_acf");

    // Effective max lag: use config value or auto.
    const int configLag = m_cfg.plots.options.acfMaxLag;
    const std::size_t n = result.residuals.size();
    const int maxLag = (configLag > 0)
        ? configLag
        : std::max(10, std::min(static_cast<int>(n) / 10, 200));

    if (static_cast<int>(n) < maxLag + 2) {
        LOKI_WARNING("PlotFilter::plotResidualsAcf: series too short for ACF, skipping.");
        return;
    }

    std::vector<double> resVals;
    resVals.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
        resVals.push_back(result.residuals[i].value);

    const auto acfData = _computeAcf(resVals, maxLag);
    const double conf  = 1.96 / std::sqrt(static_cast<double>(n));

    {
        std::ofstream dat(datAcf);
        dat << std::fixed << std::setprecision(10);
        for (const auto& [lag, val] : acfData)
            dat << lag << "\t" << val << "\n";
    }

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal(1200, 400));
        gp("set output '" + fwdSlash(outPath) + "'");
        gp("set title 'ACF of residuals: " + base + "' noenhanced");
        gp("set xlabel 'Lag'");
        gp("set ylabel 'ACF'");
        gp("set yrange [-1:1]");
        gp("set ytics 0.2");
        gp("set grid");
        gp("set key off");

        const std::string confStr = std::to_string(conf);
        gp("set arrow 1 from graph 0, first  " + confStr
           + " to graph 1, first  " + confStr
           + " nohead lc rgb '#999999' lt 2");
        gp("set arrow 2 from graph 0, first -" + confStr
           + " to graph 1, first -" + confStr
           + " nohead lc rgb '#999999' lt 2");

        gp("plot '" + fwdSlash(datAcf) + "' u 1:2 w impulses lc rgb '#2c7bb6' lw 2 notitle");
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotFilter::plotResidualsAcf failed: " + std::string(ex.what()));
    }
    std::filesystem::remove(datAcf);
}

// ----------------------------------------------------------------------------
//  Public: plotResidualsHistogram
// ----------------------------------------------------------------------------

void PlotFilter::plotResidualsHistogram(const TimeSeries&   filled,
                                         const FilterResult& result) const
{
    const std::string base    = _baseName(filled);
    const auto        datHist = m_cfg.imgDir / (".tmp_" + base + "_res_hist.dat");
    const auto        outPath = _outPath(base, "residuals_histogram");

    // Filter out NaN residuals.
    std::vector<double> valid;
    valid.reserve(result.residuals.size());
    for (std::size_t i = 0; i < result.residuals.size(); ++i) {
        const double v = result.residuals[i].value;
        if (v == v) valid.push_back(v);  // NaN check: NaN != NaN
    }

    if (valid.empty()) {
        LOKI_WARNING("PlotFilter::plotResidualsHistogram: no valid residuals, skipping.");
        return;
    }

    const std::size_t n = valid.size();
    const int configBins = m_cfg.plots.options.histogramBins;
    const int bins = (configBins > 0)
        ? configBins
        : std::max(10, std::min(static_cast<int>(
              std::ceil(std::log2(static_cast<double>(n)) + 1.0)), 100));

    const double minVal = *std::min_element(valid.begin(), valid.end());
    const double maxVal = *std::max_element(valid.begin(), valid.end());
    const double mean   = std::accumulate(valid.begin(), valid.end(), 0.0)
                          / static_cast<double>(n);
    double var = 0.0;
    for (const double v : valid) var += (v - mean) * (v - mean);
    const double sigma = std::sqrt(var / static_cast<double>(n));

    {
        std::ofstream dat(datHist);
        dat << std::fixed << std::setprecision(10);
        for (const double v : valid) dat << v << "\n";
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
        LOKI_WARNING("PlotFilter::plotResidualsHistogram failed: " + std::string(ex.what()));
    }
    std::filesystem::remove(datHist);
}

// ----------------------------------------------------------------------------
//  Public: plotResidualsQq
// ----------------------------------------------------------------------------

void PlotFilter::plotResidualsQq(const TimeSeries&   filled,
                                  const FilterResult& result) const
{
    const std::string base   = _baseName(filled);
    const auto        datQq  = m_cfg.imgDir / (".tmp_" + base + "_res_qq.dat");
    const auto        outPath = _outPath(base, "residuals_qq");

    std::vector<double> valid;
    valid.reserve(result.residuals.size());
    for (std::size_t i = 0; i < result.residuals.size(); ++i) {
        const double v = result.residuals[i].value;
        if (v == v) valid.push_back(v);
    }

    if (valid.size() < 3) {
        LOKI_WARNING("PlotFilter::plotResidualsQq: fewer than 3 valid residuals, skipping.");
        return;
    }

    std::sort(valid.begin(), valid.end());
    const std::size_t n = valid.size();

    // Normal quantile approximation (Beasley-Springer-Moro).
    auto normalQuantile = [](double p) -> double {
        static constexpr double C0 = 2.515517, C1 = 0.802853, C2 = 0.010328;
        static constexpr double D1 = 1.432788, D2 = 0.189269, D3 = 0.001308;
        const bool   upper = (p > 0.5);
        const double q = upper ? (1.0 - p) : p;
        const double t = std::sqrt(-2.0 * std::log(q));
        const double z = t - (C0 + t * (C1 + t * C2))
                           / (1.0 + t * (D1 + t * (D2 + t * D3)));
        return upper ? z : -z;
    };

    // Q-Q reference line through Q1 and Q3.
    const std::size_t i1 = n / 4;
    const std::size_t i3 = 3 * n / 4;

    std::vector<std::pair<double, double>> qqData;
    qqData.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double p = (static_cast<double>(i) + 1.0 - 0.375)
                       / (static_cast<double>(n) + 0.25);
        qqData.emplace_back(normalQuantile(p), valid[i]);
    }

    const double x1 = qqData[i1].first,  y1 = qqData[i1].second;
    const double x3 = qqData[i3].first,  y3 = qqData[i3].second;
    const double slope     = (y3 - y1) / (x3 - x1 + 1e-12);
    const double intercept = y1 - slope * x1;
    const double xMin = qqData.front().first;
    const double xMax = qqData.back().first;

    {
        std::ofstream dat(datQq);
        dat << std::fixed << std::setprecision(10);
        for (const auto& [th, sa] : qqData)
            dat << th << "\t" << sa << "\n";
    }

    std::ostringstream refLine;
    refLine << std::fixed;
    refLine << "set arrow 1 from " << xMin << "," << (slope * xMin + intercept)
            << " to " << xMax << "," << (slope * xMax + intercept)
            << " nohead lc rgb '#d7191c' lw 2";

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal(1200, 400));
        gp("set output '" + fwdSlash(outPath) + "'");
        gp("set title 'Q-Q plot of residuals: " + base + "' noenhanced");
        gp("set xlabel 'Theoretical quantiles'");
        gp("set ylabel 'Sample quantiles'");
        gp("set grid");
        gp("set key top left");
        gp(refLine.str());
        gp("plot '" + fwdSlash(datQq) + "' u 1:2 t 'Residuals' "
           "w p pt 7 ps 0.4 lc rgb '#2c7bb6', "
           "1/0 t 'Normal line' w l lc rgb '#d7191c' lw 2");
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotFilter::plotResidualsQq failed: " + std::string(ex.what()));
    }
    std::filesystem::remove(datQq);
}

// ----------------------------------------------------------------------------
//  Private helpers
// ----------------------------------------------------------------------------

std::string PlotFilter::_baseName(const TimeSeries& series) const
{
    const auto&  meta    = series.metadata();
    std::string  dataset = m_cfg.input.file.stem().string();
    if (dataset.empty()) dataset = "data";
    std::string  param   = meta.componentName.empty() ? "series" : meta.componentName;
    return "filter_" + dataset + "_" + param;
}

std::filesystem::path PlotFilter::_outPath(const std::string& baseName,
                                            const std::string& plotType) const
{
    return m_cfg.imgDir / (baseName + "_" + plotType + "." + m_cfg.plots.outputFormat);
}

void PlotFilter::_writeSeriesDat(const TimeSeries&            series,
                                  const std::filesystem::path& path) const
{
    std::ofstream dat(path);
    dat << std::fixed << std::setprecision(10);
    for (std::size_t i = 0; i < series.size(); ++i)
        dat << series[i].time.mjd() << "\t" << series[i].value << "\n";
}

void PlotFilter::_writeVectorDat(const std::vector<double>&   times,
                                  const std::vector<double>&   values,
                                  const std::filesystem::path& path) const
{
    std::ofstream dat(path);
    dat << std::fixed << std::setprecision(10);
    const std::size_t n = std::min(times.size(), values.size());
    for (std::size_t i = 0; i < n; ++i)
        dat << times[i] << "\t" << values[i] << "\n";
}

std::vector<std::pair<double, double>>
PlotFilter::_computeAcf(const std::vector<double>& values, int maxLag)
{
    const std::size_t n = values.size();
    const double mean = std::accumulate(values.begin(), values.end(), 0.0)
                        / static_cast<double>(n);
    double var = 0.0;
    for (const double v : values) var += (v - mean) * (v - mean);
    if (var == 0.0)
        throw AlgorithmException("PlotFilter::_computeAcf: residuals have zero variance.");

    std::vector<std::pair<double, double>> result;
    result.reserve(static_cast<std::size_t>(maxLag) + 1);
    for (int lag = 0; lag <= maxLag; ++lag) {
        double cov = 0.0;
        for (std::size_t i = 0; i + static_cast<std::size_t>(lag) < n; ++i)
            cov += (values[i] - mean)
                 * (values[i + static_cast<std::size_t>(lag)] - mean);
        result.emplace_back(static_cast<double>(lag), cov / var);
    }
    return result;
}

std::string PlotFilter::_terminal(int widthPx, int heightPx) const
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

std::vector<double> PlotFilter::_mjdVec(const TimeSeries& series)
{
    std::vector<double> times;
    times.reserve(series.size());
    for (std::size_t i = 0; i < series.size(); ++i)
        times.push_back(series[i].time.mjd());
    return times;
}
