#include <loki/homogeneity/plotHomogeneity.hpp>
#include <loki/io/gnuplot.hpp>
#include <loki/core/logger.hpp>

#include <fstream>
#include <iomanip>
#include <sstream>

using namespace loki;
using namespace loki::homogeneity;

// ----------------------------------------------------------------------------
//  Path helper
// ----------------------------------------------------------------------------

// Gnuplot on Windows requires forward slashes in paths.
static std::string fwdSlash(const std::filesystem::path& p)
{
    std::string s = p.string();
    for (auto& c : s) { if (c == '\\') c = '/'; }
    return s;
}


// ----------------------------------------------------------------------------
//  Construction
// ----------------------------------------------------------------------------

PlotHomogeneity::PlotHomogeneity(const AppConfig& cfg)
    : m_cfg{cfg}
{}

// ----------------------------------------------------------------------------
//  Public: plotAll
// ----------------------------------------------------------------------------

void PlotHomogeneity::plotAll(const TimeSeries&          original,
                              const HomogenizerResult&   result,
                              const std::vector<double>& seasonal) const
{
    const auto& p = m_cfg.plots;

    if (p.originalSeries)   plotOriginal(original);
    if (p.seasonalOverlay)  plotSeasonalOverlay(original, seasonal);
    if (p.deseasonalized)   plotDeseasonalized(original, result.deseasonalizedValues);
    if (p.changePoints)     plotChangePoints(original, result.deseasonalizedValues,
                                             result.changePoints);
    if (p.adjustedSeries)   plotAdjusted(result.adjustedSeries);
    if (p.homogComparison)  plotComparison(original, result.adjustedSeries);
    if (p.shiftMagnitudes && !result.changePoints.empty())
        plotShiftMagnitudes(original, result.changePoints);
}

// ----------------------------------------------------------------------------
//  Public: individual plots
// ----------------------------------------------------------------------------

void PlotHomogeneity::plotOriginal(const TimeSeries& series) const
{
    const std::string base    = _baseName(series);
    const auto        datPath = m_cfg.imgDir / (".tmp_" + base + "_original.dat");
    const auto        outPath = _outPath(base, "original");

    _writeSeriesDat(series, datPath);

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal());
        gp("set output '" + fwdSlash(outPath) + "'");
        gp("set title 'Original series: " + base + "' noenhanced");
        gp("set xlabel 'MJD'");
        gp("set ylabel 'Value'");
        gp("set grid");
        gp("set key off");
        gp("plot '" + fwdSlash(datPath) + "' u 1:2 w l lc rgb '#2c7bb6' lw 1.5 notitle");
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotHomogeneity::plotOriginal failed: " + std::string(ex.what()));
    }
    std::filesystem::remove(datPath);
}

// ----------------------------------------------------------------------------

void PlotHomogeneity::plotSeasonalOverlay(const TimeSeries&          series,
                                           const std::vector<double>& seasonal) const
{
    const std::string base     = _baseName(series);
    const auto        datOrig  = m_cfg.imgDir / (".tmp_" + base + "_seas_orig.dat");
    const auto        datSeas  = m_cfg.imgDir / (".tmp_" + base + "_seas_seas.dat");
    const auto        outPath  = _outPath(base, "seasonal_overlay");

    _writeSeriesDat(series, datOrig);

    std::vector<double> times;
    times.reserve(series.size());
    for (std::size_t i = 0; i < series.size(); ++i)
        times.push_back(series[i].time.mjd());
    _writeVectorDat(times, seasonal, datSeas);

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal());
        gp("set output '" + fwdSlash(outPath) + "'");
        gp("set title 'Original + seasonal component: " + base + "' noenhanced");
        gp("set xlabel 'MJD'");
        gp("set ylabel 'Value'");
        gp("set grid");
        gp("plot '" + fwdSlash(datOrig) + "' u 1:2 w l lc rgb '#2c7bb6' lw 1.0 t 'Original',"
           " '" + fwdSlash(datSeas) + "' u 1:2 w l lc rgb '#d7191c' lw 2.0 t 'Seasonal'");
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotHomogeneity::plotSeasonalOverlay failed: " + std::string(ex.what()));
    }
    std::filesystem::remove(datOrig);
    std::filesystem::remove(datSeas);
}

// ----------------------------------------------------------------------------

void PlotHomogeneity::plotDeseasonalized(const TimeSeries&          series,
                                          const std::vector<double>& residuals) const
{
    const std::string base    = _baseName(series);
    const auto        datPath = m_cfg.imgDir / (".tmp_" + base + "_deseas.dat");
    const auto        outPath = _outPath(base, "deseas");

    std::vector<double> times;
    times.reserve(series.size());
    for (std::size_t i = 0; i < series.size(); ++i)
        times.push_back(series[i].time.mjd());
    _writeVectorDat(times, residuals, datPath);

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal());
        gp("set output '" + fwdSlash(outPath) + "'");
        gp("set title 'Deseasonalized series: " + base + "' noenhanced");
        gp("set xlabel 'MJD'");
        gp("set ylabel 'Residual'");
        gp("set grid");
        gp("set key off");
        gp("plot '" + fwdSlash(datPath) + "' u 1:2 w l lc rgb '#1a9641' lw 1.5 notitle");
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotHomogeneity::plotDeseasonalized failed: " + std::string(ex.what()));
    }
    std::filesystem::remove(datPath);
}

// ----------------------------------------------------------------------------

void PlotHomogeneity::plotChangePoints(const TimeSeries&               series,
                                        const std::vector<double>&      residuals,
                                        const std::vector<ChangePoint>& changePoints) const
{
    const std::string base    = _baseName(series);
    const auto        datPath = m_cfg.imgDir / (".tmp_" + base + "_cp.dat");
    const auto        outPath = _outPath(base, "cp");

    std::vector<double> times;
    times.reserve(series.size());
    for (std::size_t i = 0; i < series.size(); ++i)
        times.push_back(series[i].time.mjd());
    _writeVectorDat(times, residuals, datPath);

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal());
        gp("set output '" + fwdSlash(outPath) + "'");
        gp("set title 'Change points: " + base + "' noenhanced");
        gp("set xlabel 'MJD'");
        gp("set ylabel 'Residual'");
        gp("set grid");
        gp("set key off");

        for (const auto& cp : changePoints) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(6);
            oss << "set arrow from " << cp.mjd << ",graph 0 to "
                << cp.mjd << ",graph 1 nohead lc rgb '#d7191c' lw 2 dt 2";
            gp(oss.str());
        }

        gp("plot '" + fwdSlash(datPath) + "' u 1:2 w l lc rgb '#1a9641' lw 1.0 notitle");
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotHomogeneity::plotChangePoints failed: " + std::string(ex.what()));
    }
    std::filesystem::remove(datPath);
}

// ----------------------------------------------------------------------------

void PlotHomogeneity::plotAdjusted(const TimeSeries& adjusted) const
{
    const std::string base    = _baseName(adjusted);
    const auto        datPath = m_cfg.imgDir / (".tmp_" + base + "_adjusted.dat");
    const auto        outPath = _outPath(base, "adjusted");

    _writeSeriesDat(adjusted, datPath);

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal());
        gp("set output '" + fwdSlash(outPath) + "'");
        gp("set title 'Homogenized series: " + base + "' noenhanced");
        gp("set xlabel 'MJD'");
        gp("set ylabel 'Value'");
        gp("set grid");
        gp("set key off");
        gp("plot '" + fwdSlash(datPath) + "' u 1:2 w l lc rgb '#2c7bb6' lw 1.5 notitle");
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotHomogeneity::plotAdjusted failed: " + std::string(ex.what()));
    }
    std::filesystem::remove(datPath);
}

// ----------------------------------------------------------------------------

void PlotHomogeneity::plotComparison(const TimeSeries& original,
                                      const TimeSeries& adjusted) const
{
    const std::string base    = _baseName(original);
    const auto        datOrig = m_cfg.imgDir / (".tmp_" + base + "_comp_orig.dat");
    const auto        datAdj  = m_cfg.imgDir / (".tmp_" + base + "_comp_adj.dat");
    const auto        outPath = _outPath(base, "comparison");

    _writeSeriesDat(original, datOrig);
    _writeSeriesDat(adjusted, datAdj);

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal());
        gp("set output '" + fwdSlash(outPath) + "'");
        gp("set title 'Original vs homogenized: " + base + "' noenhanced");
        gp("set xlabel 'MJD'");
        gp("set ylabel 'Value'");
        gp("set grid");
        gp("plot '" + fwdSlash(datOrig) + "' u 1:2 w l lc rgb '#808080' lw 1.0 t 'Original',"
           " '" + fwdSlash(datAdj) + "' u 1:2 w l lc rgb '#2c7bb6' lw 1.5 t 'Homogenized'");
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotHomogeneity::plotComparison failed: " + std::string(ex.what()));
    }
    std::filesystem::remove(datOrig);
    std::filesystem::remove(datAdj);
}

// ----------------------------------------------------------------------------

void PlotHomogeneity::plotShiftMagnitudes(const TimeSeries&               series,
                                           const std::vector<ChangePoint>&  changePoints) const
{
    const std::string base    = _baseName(series);
    const auto        datPath = m_cfg.imgDir / (".tmp_" + base + "_shifts.dat");
    const auto        outPath = _outPath(base, "shifts");

    {
        std::ofstream dat(datPath);
        dat << std::fixed << std::setprecision(6);
        for (const auto& cp : changePoints) {
            dat << cp.mjd << "\t" << cp.shift << "\n";
        }
    }

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal());
        gp("set output '" + fwdSlash(outPath) + "'");
        gp("set title 'Shift magnitudes: " + base + "' noenhanced");
        gp("set xlabel 'MJD'");
        gp("set ylabel 'Shift'");
        gp("set grid ytics");
        gp("set style data histogram");
        gp("set style fill solid 0.7 border -1");
        gp("set boxwidth 0.6");
        gp("set key off");
        gp("plot '" + fwdSlash(datPath) + "' u 2:xtic(1) lc rgb '#d7191c' notitle");
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotHomogeneity::plotShiftMagnitudes failed: " + std::string(ex.what()));
    }
    std::filesystem::remove(datPath);
}

// ----------------------------------------------------------------------------
//  Private helpers
// ----------------------------------------------------------------------------

std::string PlotHomogeneity::_baseName(const TimeSeries& series) const
{
    const auto& m = series.metadata();
    std::string name = m.stationId;
    if (!m.componentName.empty()) {
        if (!name.empty()) name += "_";
        name += m.componentName;
    }
    if (name.empty()) name = "series";
    return name;
}

std::filesystem::path PlotHomogeneity::_outPath(const std::string& baseName,
                                                  const std::string& plotType) const
{
    return m_cfg.imgDir / (baseName + "_" + plotType + "." + m_cfg.plots.outputFormat);
}

void PlotHomogeneity::_writeSeriesDat(const TimeSeries&           series,
                                       const std::filesystem::path& path) const
{
    std::ofstream dat(path);
    dat << std::fixed << std::setprecision(10);
    for (std::size_t i = 0; i < series.size(); ++i)
        dat << series[i].time.mjd() << "\t" << series[i].value << "\n";
}

void PlotHomogeneity::_writeVectorDat(const std::vector<double>&   times,
                                       const std::vector<double>&   values,
                                       const std::filesystem::path& path) const
{
    std::ofstream dat(path);
    dat << std::fixed << std::setprecision(10);
    const std::size_t n = std::min(times.size(), values.size());
    for (std::size_t i = 0; i < n; ++i)
        dat << times[i] << "\t" << values[i] << "\n";
}

std::string PlotHomogeneity::_terminal() const
{
    const std::string& fmt = m_cfg.plots.outputFormat;
    if (fmt == "eps") return "postscript eps color noenhanced solid font 'Sans,12'";
    if (fmt == "svg") return "svg noenhanced font 'Sans,12'";
    return "pngcairo noenhanced font 'Sans,12' size 1200,400";
}
