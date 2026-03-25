#include <loki/outlier/plotOutlier.hpp>
#include <loki/io/gnuplot.hpp>
#include <loki/core/logger.hpp>
#include <loki/core/exceptions.hpp>

#include <fstream>
#include <iomanip>
#include <sstream>

using namespace loki;
using namespace loki::outlier;

// ----------------------------------------------------------------------------
//  Internal helpers
// ----------------------------------------------------------------------------

std::string PlotOutlier::_fwdSlash(const std::filesystem::path& p)
{
    std::string s = p.string();
    for (auto& c : s) { if (c == '\\') c = '/'; }
    return s;
}

std::string PlotOutlier::_terminal() const
{
    const std::string& fmt = m_cfg.plots.outputFormat;
    if (fmt == "eps") return "postscript eps color noenhanced solid font 'Sans,12'";
    if (fmt == "svg") return "svg noenhanced font 'Sans,12'";
    return "pngcairo noenhanced font 'Sans,12' size 1200,400";
}

std::string PlotOutlier::_datasetName() const
{
    const std::filesystem::path p = m_cfg.input.file;
    std::string stem = p.stem().string();
    if (stem.empty()) stem = "data";
    return stem;
}

std::string PlotOutlier::_stem(const TimeSeries&  series,
                                const std::string& plotType) const
{
    const auto& meta   = series.metadata();
    std::string param  = meta.componentName.empty() ? "series" : meta.componentName;
    return m_programName + "_" + _datasetName() + "_" + param + "_" + plotType;
}

std::filesystem::path PlotOutlier::_outPath(const std::string& stem) const
{
    return m_cfg.imgDir / (stem + "." + m_cfg.plots.outputFormat);
}

void PlotOutlier::_writeSeriesDat(const TimeSeries&            series,
                                   const std::filesystem::path& path) const
{
    std::ofstream dat(path);
    dat << std::fixed << std::setprecision(10);
    for (std::size_t i = 0; i < series.size(); ++i) {
        dat << series[i].time.mjd() << "\t" << series[i].value << "\n";
    }
}

void PlotOutlier::_writeOutlierDat(const TimeSeries&            series,
                                    const OutlierResult&          detection,
                                    const std::filesystem::path& path) const
{
    std::ofstream dat(path);
    dat << std::fixed << std::setprecision(10);
    for (const auto& pt : detection.points) {
        if (pt.index < series.size()) {
            dat << series[pt.index].time.mjd() << "\t"
                << series[pt.index].value      << "\t"
                << pt.score                    << "\n";
        }
    }
}

// ----------------------------------------------------------------------------
//  Construction
// ----------------------------------------------------------------------------

PlotOutlier::PlotOutlier(const AppConfig& cfg, const std::string& programName)
    : m_cfg{cfg}
    , m_programName{programName}
{
    std::filesystem::create_directories(m_cfg.imgDir);
}

// ----------------------------------------------------------------------------
//  plotOriginalWithOutliers
// ----------------------------------------------------------------------------

void PlotOutlier::plotOriginalWithOutliers(const TimeSeries&   series,
                                            const OutlierResult& detection) const
{
    const std::string stem    = _stem(series, "original_with_outliers");
    const auto        datSer  = m_cfg.imgDir / (".tmp_" + stem + "_ser.dat");
    const auto        datOut  = m_cfg.imgDir / (".tmp_" + stem + "_out.dat");
    const auto        outPath = _outPath(stem);

    _writeSeriesDat(series, datSer);
    _writeOutlierDat(series, detection, datOut);

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal());
        gp("set output '" + _fwdSlash(outPath) + "'");
        gp("set title 'Outlier detection: " + _datasetName() + "  n="
           + std::to_string(detection.nOutliers) + " detected  method=" + detection.method + "' noenhanced");
        gp("set xlabel 'MJD'");
        gp("set ylabel 'Value'");
        gp("set grid");
        gp("set key top right");
        gp("set style line 1 lc rgb '#2166ac' lw 1.2");
        gp("set style line 2 lc rgb '#d7191c' pt 9 ps 1.2");  // pt 9 = filled triangle up
        gp("plot '" + _fwdSlash(datSer) + "' u 1:2 w l ls 1 t 'Original',"
           " '" + _fwdSlash(datOut) + "' u 1:2 w p ls 2 t 'Outlier'");
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotOutlier::plotOriginalWithOutliers failed: " + std::string(ex.what()));
    }
    std::filesystem::remove(datSer);
    std::filesystem::remove(datOut);
}

// ----------------------------------------------------------------------------
//  plotCleaned
// ----------------------------------------------------------------------------

void PlotOutlier::plotCleaned(const TimeSeries& cleaned) const
{
    const std::string stem    = _stem(cleaned, "cleaned");
    const auto        datPath = m_cfg.imgDir / (".tmp_" + stem + ".dat");
    const auto        outPath = _outPath(stem);

    _writeSeriesDat(cleaned, datPath);

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal());
        gp("set output '" + _fwdSlash(outPath) + "'");
        gp("set title 'Cleaned series: " + _datasetName() + "' noenhanced");
        gp("set xlabel 'MJD'");
        gp("set ylabel 'Value'");
        gp("set grid");
        gp("set key off");
        gp("set style line 1 lc rgb '#1a9641' lw 1.2");
        gp("plot '" + _fwdSlash(datPath) + "' u 1:2 w l ls 1 notitle");
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotOutlier::plotCleaned failed: " + std::string(ex.what()));
    }
    std::filesystem::remove(datPath);
}

// ----------------------------------------------------------------------------
//  plotComparison
// ----------------------------------------------------------------------------

void PlotOutlier::plotComparison(const TimeSeries& original,
                                  const TimeSeries& cleaned) const
{
    const std::string stem    = _stem(original, "comparison");
    const auto        datOrig = m_cfg.imgDir / (".tmp_" + stem + "_orig.dat");
    const auto        datCln  = m_cfg.imgDir / (".tmp_" + stem + "_cln.dat");
    const auto        outPath = _outPath(stem);

    _writeSeriesDat(original, datOrig);
    _writeSeriesDat(cleaned,  datCln);

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal());
        gp("set output '" + _fwdSlash(outPath) + "'");
        gp("set title 'Original vs cleaned: " + _datasetName() + "' noenhanced");
        gp("set xlabel 'MJD'");
        gp("set ylabel 'Value'");
        gp("set grid");
        gp("set key top right");
        gp("plot '" + _fwdSlash(datOrig) + "' u 1:2 w l lc rgb '#808080' lw 1.0 t 'Original',"
           " '" + _fwdSlash(datCln)  + "' u 1:2 w l lc rgb '#1a9641' lw 1.5 t 'Cleaned'");
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotOutlier::plotComparison failed: " + std::string(ex.what()));
    }
    std::filesystem::remove(datOrig);
    std::filesystem::remove(datCln);
}

// ----------------------------------------------------------------------------
//  plotResiduals
// ----------------------------------------------------------------------------

void PlotOutlier::plotResiduals(const TimeSeries&   original,
                                 const TimeSeries&   residuals,
                                 const OutlierResult& detection) const
{
    const std::string stem    = _stem(original, "residuals");
    const auto        datRes  = m_cfg.imgDir / (".tmp_" + stem + "_res.dat");
    const auto        datOut  = m_cfg.imgDir / (".tmp_" + stem + "_out.dat");
    const auto        outPath = _outPath(stem);

    _writeSeriesDat(residuals, datRes);
    // Outlier markers use residual values, not original.
    _writeOutlierDat(residuals, detection, datOut);

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal());
        gp("set output '" + _fwdSlash(outPath) + "'");
        gp("set title 'Residuals + outliers: " + _datasetName()
           + "  n=" + std::to_string(detection.nOutliers) + "' noenhanced");
        gp("set xlabel 'MJD'");
        gp("set ylabel 'Residual'");
        gp("set grid");
        gp("set key top right");
        gp("set style line 1 lc rgb '#1a9641' lw 1.0");
        gp("set style line 2 lc rgb '#d7191c' pt 9 ps 1.2");
        gp("plot '" + _fwdSlash(datRes) + "' u 1:2 w l ls 1 t 'Residuals',"
           " '" + _fwdSlash(datOut) + "' u 1:2 w p ls 2 t 'Outlier'");
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotOutlier::plotResiduals failed: " + std::string(ex.what()));
    }
    std::filesystem::remove(datRes);
    std::filesystem::remove(datOut);
}

// ----------------------------------------------------------------------------
//  plotAll
// ----------------------------------------------------------------------------

void PlotOutlier::plotAll(const TimeSeries&   original,
                           const TimeSeries&   cleaned,
                           const TimeSeries&   residuals,
                           const OutlierResult& detection,
                           bool                hasResiduals) const
{
    const auto& p = m_cfg.plots;

    if (p.originalSeries)  plotOriginalWithOutliers(original, detection);
    if (p.adjustedSeries)  plotCleaned(cleaned);
    if (p.homogComparison) plotComparison(original, cleaned);
    if (p.deseasonalized && hasResiduals)
        plotResiduals(original, residuals, detection);
}

// ----------------------------------------------------------------------------
//  plotOutlierOverlay  (homogeneity pipeline)
// ----------------------------------------------------------------------------

void PlotOutlier::plotOutlierOverlay(const TimeSeries&   series,
                                      const OutlierResult& preDetection,
                                      const OutlierResult& postDetection) const
{
    // Skip if nothing was detected in either pass.
    if (preDetection.nOutliers == 0 && postDetection.nOutliers == 0) return;

    const std::string stem    = _stem(series, "outlier_overlay");
    const auto        datSer  = m_cfg.imgDir / (".tmp_" + stem + "_ser.dat");
    const auto        datPre  = m_cfg.imgDir / (".tmp_" + stem + "_pre.dat");
    const auto        datPost = m_cfg.imgDir / (".tmp_" + stem + "_post.dat");
    const auto        outPath = _outPath(stem);

    _writeSeriesDat(series, datSer);
    _writeOutlierDat(series, preDetection,  datPre);
    _writeOutlierDat(series, postDetection, datPost);

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal());
        gp("set output '" + _fwdSlash(outPath) + "'");
        gp("set title 'Outlier overlay: " + _datasetName()
           + "  pre=" + std::to_string(preDetection.nOutliers)
           + "  post=" + std::to_string(postDetection.nOutliers)
           + "' noenhanced");
        gp("set xlabel 'MJD'");
        gp("set ylabel 'Value'");
        gp("set grid");
        gp("set key top right");

        // Series in grey, pre-outlier in red (triangle up), post-outlier in blue (triangle down).
        gp("set style line 1 lc rgb '#808080' lw 1.0");
        gp("set style line 2 lc rgb '#d7191c' pt 9  ps 1.4");  // filled triangle up
        gp("set style line 3 lc rgb '#2166ac' pt 11 ps 1.4");  // filled triangle down

        std::string plotCmd =
            "plot '" + _fwdSlash(datSer) + "' u 1:2 w l ls 1 t 'Series'";

        if (preDetection.nOutliers > 0)
            plotCmd += ", '" + _fwdSlash(datPre)
                    + "' u 1:2 w p ls 2 t 'Pre-outlier'";

        if (postDetection.nOutliers > 0)
            plotCmd += ", '" + _fwdSlash(datPost)
                    + "' u 1:2 w p ls 3 t 'Post-outlier'";

        gp(plotCmd);

    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotOutlier::plotOutlierOverlay failed: " + std::string(ex.what()));
    }
    std::filesystem::remove(datSer);
    std::filesystem::remove(datPre);
    std::filesystem::remove(datPost);
}
