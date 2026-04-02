#include <loki/outlier/plotOutlier.hpp>
#include <loki/io/gnuplot.hpp>
#include <loki/io/plot.hpp>
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
    const auto& meta  = series.metadata();
    std::string param = meta.componentName.empty() ? "series" : meta.componentName;
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

void PlotOutlier::_writeVectorDat(const std::vector<double>&   times,
                                   const std::vector<double>&   values,
                                   const std::filesystem::path& path) const
{
    std::ofstream dat(path);
    dat << std::fixed << std::setprecision(10);
    const std::size_t n = std::min(times.size(), values.size());
    for (std::size_t i = 0; i < n; ++i) {
        dat << times[i] << "\t" << values[i] << "\n";
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
        gp("set title 'Outlier detection: " + _datasetName()
           + "  n=" + std::to_string(detection.nOutliers)
           + " detected  method=" + detection.method + "' noenhanced");
        gp("set xlabel 'MJD'");
        gp("set ylabel 'Value'");
        gp("set grid");
        gp("set key top right");
        gp("set style line 1 lc rgb '#2166ac' lw 1.2");
        gp("set style line 2 lc rgb '#d7191c' pt 9 ps 1.2");
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
        gp("plot '" + _fwdSlash(datOrig) + "' u 1:2 w l lc rgb '#2166ac' lw 1.0 t 'Original',"
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
    _writeOutlierDat(residuals, detection, datOut);

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal());
        gp("set output '" + _fwdSlash(outPath) + "'");
        gp("set title 'Residuals: " + _datasetName()
           + "  n=" + std::to_string(detection.nOutliers) + " outliers' noenhanced");
        gp("set xlabel 'MJD'");
        gp("set ylabel 'Residual'");
        gp("set grid");
        gp("set key top right");
        gp("plot '" + _fwdSlash(datRes) + "' u 1:2 w l lc rgb '#1a9641' lw 1.0 t 'Residuals',"
           " '" + _fwdSlash(datOut) + "' u 1:2 w p lc rgb '#d7191c' pt 9 ps 1.2 t 'Outlier'");
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotOutlier::plotResiduals failed: " + std::string(ex.what()));
    }
    std::filesystem::remove(datRes);
    std::filesystem::remove(datOut);
}

// ----------------------------------------------------------------------------
//  plotSeasonalOverlay
// ----------------------------------------------------------------------------

void PlotOutlier::plotSeasonalOverlay(const TimeSeries&          series,
                                       const std::vector<double>& seasonal) const
{
    const std::string stem    = _stem(series, "seasonal_overlay");
    const auto        datOrig = m_cfg.imgDir / (".tmp_" + stem + "_orig.dat");
    const auto        datSeas = m_cfg.imgDir / (".tmp_" + stem + "_seas.dat");
    const auto        outPath = _outPath(stem);

    _writeSeriesDat(series, datOrig);

    // Build MJD vector for seasonal
    std::vector<double> mjds;
    mjds.reserve(series.size());
    for (std::size_t i = 0; i < series.size(); ++i)
        mjds.push_back(series[i].time.mjd());
    _writeVectorDat(mjds, seasonal, datSeas);

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal());
        gp("set output '" + _fwdSlash(outPath) + "'");
        gp("set title 'Seasonal overlay: " + _datasetName() + "' noenhanced");
        gp("set xlabel 'MJD'");
        gp("set ylabel 'Value'");
        gp("set grid");
        gp("set key top right");
        gp("plot '" + _fwdSlash(datOrig) + "' u 1:2 w l lc rgb '#2166ac' lw 1.0 t 'Original',"
           " '" + _fwdSlash(datSeas) + "' u 1:2 w l lc rgb '#d7191c' lw 1.5 t 'Seasonal'");
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotOutlier::plotSeasonalOverlay failed: " + std::string(ex.what()));
    }
    std::filesystem::remove(datOrig);
    std::filesystem::remove(datSeas);
}

// ----------------------------------------------------------------------------
//  plotLeverages
// ----------------------------------------------------------------------------

void PlotOutlier::plotLeverages(const TimeSeries&      series,
                                 const Eigen::VectorXd& leverages,
                                 double                 threshold,
                                 int                    arOrder,
                                 std::size_t            nOutliers) const
{
    if (leverages.size() == 0) return;

    const std::string stem    = _stem(series, "leverage");
    const auto        datLev  = m_cfg.imgDir / (".tmp_" + stem + "_lev.dat");
    const auto        datOut  = m_cfg.imgDir / (".tmp_" + stem + "_out.dat");
    const auto        outPath = _outPath(stem);

    // Write leverage data: col1 = MJD (offset by arOrder), col2 = h_ii
    // Outlier file: same columns but only rows exceeding threshold
    {
        std::ofstream levDat(datLev);
        std::ofstream outDat(datOut);
        levDat << std::fixed << std::setprecision(10);
        outDat << std::fixed << std::setprecision(10);

        const std::size_t rows   = static_cast<std::size_t>(leverages.size());
        const std::size_t offset = static_cast<std::size_t>(arOrder);

        for (std::size_t i = 0; i < rows; ++i) {
            const std::size_t serIdx = i + offset;
            if (serIdx >= series.size()) break;

            const double mjd = series[serIdx].time.mjd();
            const double h   = leverages(static_cast<Eigen::Index>(i));

            levDat << mjd << "\t" << h << "\n";
            if (h > threshold) {
                outDat << mjd << "\t" << h << "\n";
            }
        }
    }

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal());
        gp("set output '" + _fwdSlash(outPath) + "'");

        std::ostringstream title;
        title << "Hat matrix leverages (DEH): " << _datasetName()
              << "  AR order=" << arOrder
              << "  n=" << nOutliers << " flagged"
              << "  threshold=" << std::fixed << std::setprecision(6) << threshold;
        gp("set title '" + title.str() + "' noenhanced");
        gp("set xlabel 'MJD'");
        gp("set ylabel 'h_{ii}'");
        gp("set grid");
        gp("set key top right");

        // Dashed red threshold line
        std::ostringstream thr;
        thr << std::fixed << std::setprecision(10) << threshold;
        gp("set arrow 1 from graph 0, first " + thr.str()
           + " to graph 1, first " + thr.str()
           + " nohead lc rgb '#d7191c' lw 1.5 dt 2");

        std::string plotCmd =
            "plot '" + _fwdSlash(datLev) + "' u 1:2 w l lc rgb '#2166ac' lw 1.0 t 'leverage h_{ii}'";

        if (nOutliers > 0) {
            plotCmd += ", '" + _fwdSlash(datOut)
                    + "' u 1:2 w p lc rgb '#d7191c' pt 9 ps 1.2 t 'flagged'";
        }

        gp(plotCmd);
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotOutlier::plotLeverages failed: " + std::string(ex.what()));
    }

    std::filesystem::remove(datLev);
    std::filesystem::remove(datOut);
}

// ----------------------------------------------------------------------------
//  plotAll
// ----------------------------------------------------------------------------

void PlotOutlier::plotAll(const TimeSeries&          original,
                           const TimeSeries&          cleaned,
                           const TimeSeries&          residuals,
                           const OutlierResult&        detection,
                           bool                        hasComponent,
                           const std::vector<double>& seasonal) const
{
    const auto& p = m_cfg.plots;

    if (p.originalSeries)  plotOriginalWithOutliers(original, detection);
    if (p.adjustedSeries)  plotCleaned(cleaned);
    if (p.homogComparison) plotComparison(original, cleaned);

    if (p.deseasonalized && hasComponent)
        plotResiduals(original, residuals, detection);

    if (p.seasonalOverlay && hasComponent && !seasonal.empty())
        plotSeasonalOverlay(original, seasonal);

    if (p.residualsWithBounds) {
        const OutlierResult empty{};
        if (hasComponent) {
            plotResidualsWithBounds(original, residuals, empty, detection);
        } else {
            plotResidualsWithBounds(original, residuals, detection, empty);
        }
    }

    // Generic core plots
    try {
        Plot corePlot(m_cfg);
        if (p.timeSeries) corePlot.timeSeries(original);
        if (p.histogram)  corePlot.histogram(original);
        if (p.acf)        corePlot.acf(original);
        if (p.qqPlot)     corePlot.qqPlot(original);
        if (p.boxplot)    corePlot.boxplot(original);
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotOutlier::plotAll core plots failed: " + std::string(ex.what()));
    }
}

// ----------------------------------------------------------------------------
//  plotOutlierOverlay
// ----------------------------------------------------------------------------

void PlotOutlier::plotOutlierOverlay(const TimeSeries&   series,
                                      const OutlierResult& preDetection,
                                      const OutlierResult& postDetection) const
{
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
        gp("set style line 1 lc rgb '#808080' lw 1.0");
        gp("set style line 2 lc rgb '#d7191c' pt 9  ps 1.4");
        gp("set style line 3 lc rgb '#2166ac' pt 11 ps 1.4");

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

// ----------------------------------------------------------------------------
//  plotResidualsWithBounds
// ----------------------------------------------------------------------------

void PlotOutlier::plotResidualsWithBounds(const TimeSeries&   series,
                                           const TimeSeries&   residuals,
                                           const OutlierResult& preDetection,
                                           const OutlierResult& postDetection) const
{
    const bool hasPre  = (preDetection.n  > 0);
    const bool hasPost = (postDetection.n > 0);
    if (!hasPre && !hasPost) return;

    const std::string stem    = _stem(series, "residuals_with_bounds");
    const auto        datRes  = m_cfg.imgDir / (".tmp_" + stem + "_res.dat");
    const auto        datPre  = m_cfg.imgDir / (".tmp_" + stem + "_pre.dat");
    const auto        datPost = m_cfg.imgDir / (".tmp_" + stem + "_post.dat");
    const auto        outPath = _outPath(stem);

    _writeSeriesDat(residuals, datRes);
    if (hasPre)  _writeOutlierDat(residuals, preDetection,  datPre);
    if (hasPost) _writeOutlierDat(residuals, postDetection, datPost);

    auto getBounds = [](const OutlierResult& det) -> std::pair<double, double> {
        double offset = 0.0;
        if (!det.points.empty()) {
            offset = det.points.front().threshold - det.location;
            if (offset < 0.0) offset = -offset;
        } else {
            offset = det.scale;
        }
        return {det.location - offset, det.location + offset};
    };

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal());
        gp("set output '" + _fwdSlash(outPath) + "'");

        std::string title = "Residuals + detection bounds: " + _datasetName();
        if (hasPre)  title += "  pre="  + std::to_string(preDetection.nOutliers);
        if (hasPost) title += "  post=" + std::to_string(postDetection.nOutliers);
        gp("set title '" + title + "' noenhanced");
        gp("set xlabel 'MJD'");
        gp("set ylabel 'Residual'");
        gp("set grid");
        gp("set key top right");

        if (hasPre) {
            const auto [lo, hi] = getBounds(preDetection);
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(6);
            oss << "set arrow 1 from graph 0, first " << hi
                << " to graph 1, first " << hi
                << " nohead lc rgb '#d7191c' lw 1.5 dt 2";
            gp(oss.str());
            oss.str("");
            oss << "set arrow 2 from graph 0, first " << lo
                << " to graph 1, first " << lo
                << " nohead lc rgb '#d7191c' lw 1.5 dt 2";
            gp(oss.str());
        }

        if (hasPost) {
            const auto [lo, hi] = getBounds(postDetection);
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(6);
            oss << "set arrow 3 from graph 0, first " << hi
                << " to graph 1, first " << hi
                << " nohead lc rgb '#2166ac' lw 1.5 dt 2";
            gp(oss.str());
            oss.str("");
            oss << "set arrow 4 from graph 0, first " << lo
                << " to graph 1, first " << lo
                << " nohead lc rgb '#2166ac' lw 1.5 dt 2";
            gp(oss.str());
        }

        std::string plotCmd =
            "plot '" + _fwdSlash(datRes) + "' u 1:2 w l lc rgb '#1a9641' lw 1.0 t 'Residuals'";

        if (hasPre && preDetection.nOutliers > 0)
            plotCmd += ", '" + _fwdSlash(datPre)
                    + "' u 1:2 w p lc rgb '#d7191c' pt 9 ps 1.2 t 'Pre-outlier'";

        if (hasPost && postDetection.nOutliers > 0)
            plotCmd += ", '" + _fwdSlash(datPost)
                    + "' u 1:2 w p lc rgb '#2166ac' pt 11 ps 1.2 t 'Post-outlier'";

        gp(plotCmd);
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotOutlier::plotResidualsWithBounds failed: " + std::string(ex.what()));
    }

    std::filesystem::remove(datRes);
    if (hasPre)  std::filesystem::remove(datPre);
    if (hasPost) std::filesystem::remove(datPost);
}
