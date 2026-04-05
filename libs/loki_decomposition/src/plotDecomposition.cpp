#include <loki/decomposition/plotDecomposition.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/io/gnuplot.hpp>
#include <loki/io/plot.hpp>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <string>

using namespace loki;

// -----------------------------------------------------------------------------

PlotDecomposition::PlotDecomposition(const AppConfig& cfg)
    : m_cfg{cfg}
{
    std::filesystem::create_directories(m_cfg.imgDir);
}

// -----------------------------------------------------------------------------

std::string PlotDecomposition::fwdSlash(const std::filesystem::path& p)
{
    std::string s = p.string();
    std::replace(s.begin(), s.end(), '\\', '/');
    return s;
}

// -----------------------------------------------------------------------------

std::string PlotDecomposition::terminal() const
{
    const std::string& fmt = m_cfg.plots.outputFormat;
    if (fmt == "eps") return "postscript eps color noenhanced solid font 'Sans,12'";
    if (fmt == "svg") return "svg noenhanced font 'Sans,12'";
    return "pngcairo noenhanced font 'Sans,12' size 1200,500";
}

// -----------------------------------------------------------------------------

std::filesystem::path PlotDecomposition::outPath(const std::string& datasetName,
                                                   const std::string& compName,
                                                   const std::string& plotType) const
{
    const std::string fname =
        "decomposition_" + datasetName + "_" + compName + "_" + plotType
        + "." + m_cfg.plots.outputFormat;
    return m_cfg.imgDir / fname;
}

// -----------------------------------------------------------------------------

std::filesystem::path PlotDecomposition::writeTmp(const TimeSeries& ts,
                                                   const DecompositionResult& result,
                                                   const std::string& stem) const
{
    const std::filesystem::path tmpPath = m_cfg.imgDir / (".tmp_" + stem + ".dat");

    std::ofstream ofs(tmpPath);
    if (!ofs.is_open()) {
        throw IoException("PlotDecomposition: cannot write temp file '"
                          + tmpPath.string() + "'.");
    }

    ofs << std::fixed << std::setprecision(9);
    const int n = static_cast<int>(ts.size());
    for (int i = 0; i < n; ++i) {
        const std::size_t si = static_cast<std::size_t>(i);
        // col1=mjd  col2=original  col3=trend  col4=seasonal  col5=residual
        ofs << ts[si].time.mjd()   << "\t"
            << ts[si].value        << "\t"
            << result.trend[si]    << "\t"
            << result.seasonal[si] << "\t"
            << result.residual[si] << "\n";
    }

    return tmpPath;
}

// -----------------------------------------------------------------------------

void PlotDecomposition::plotOverlay(const TimeSeries&          ts,
                                     const DecompositionResult& result,
                                     const std::string&         datasetName,
                                     const std::string&         compName) const
{
    const std::string stem    = "decomposition_" + datasetName + "_" + compName + "_overlay";
    const auto        tmpPath = writeTmp(ts, result, stem);
    const auto        imgPath = outPath(datasetName, compName, "overlay");

    const std::string methodStr =
        (result.method == DecompositionMethod::CLASSICAL) ? "Classical" : "STL";
    const std::string title =
        compName + " -- " + methodStr
        + " decomposition overlay (period=" + std::to_string(result.period) + ")";

    try {
        Gnuplot gp;
        gp("set terminal " + terminal());
        gp("set output '"  + fwdSlash(imgPath) + "'");
        gp("set title '"   + title             + "' noenhanced");
        gp("set xlabel 'Time (MJD)'");
        gp("set ylabel '"  + compName          + "' noenhanced");
        gp("set grid");
        gp("set key top right");
        gp("plot '" + fwdSlash(tmpPath) + "' u 1:2 w l lc rgb '#aaaaaa' lw 1.0 t 'Original',"
           " '' u 1:3 w l lc rgb '#c0392b' lw 2.0 t 'Trend'");
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotDecomposition::plotOverlay failed: " + std::string(ex.what()));
    }

    std::filesystem::remove(tmpPath);
    LOKI_INFO("PlotDecomposition: overlay -> " + imgPath.string());
}

// -----------------------------------------------------------------------------

void PlotDecomposition::plotPanels(const TimeSeries&          ts,
                                    const DecompositionResult& result,
                                    const std::string&         datasetName,
                                    const std::string&         compName) const
{
    const std::string stem    = "decomposition_" + datasetName + "_" + compName + "_panels";
    const auto        tmpPath = writeTmp(ts, result, stem);
    const auto        imgPath = outPath(datasetName, compName, "panels");

    // 3-panel plot uses taller canvas -- build terminal string directly.
    const std::string& fmt = m_cfg.plots.outputFormat;
    std::string term;
    if (fmt == "eps")      term = "postscript eps color noenhanced solid font 'Sans,12'";
    else if (fmt == "svg") term = "svg noenhanced font 'Sans,12' size 1200,900";
    else                   term = "pngcairo noenhanced font 'Sans,12' size 1200,900";

    const std::string methodStr =
        (result.method == DecompositionMethod::CLASSICAL) ? "Classical" : "STL";
    const std::string mainTitle =
        compName + " -- " + methodStr
        + " decomposition (period=" + std::to_string(result.period) + " samples)";

    try {
        Gnuplot gp;
        gp("set terminal " + term);
        gp("set output '"  + fwdSlash(imgPath) + "'");
        gp("set multiplot layout 3,1 title '" + mainTitle + "' noenhanced");
        gp("set grid");
        gp("set key off");

        // Panel 1: trend.
        gp("unset xlabel");
        gp("set ylabel 'Trend'");
        gp("set tmargin 2");
        gp("set bmargin 1");
        gp("plot '" + fwdSlash(tmpPath) + "' u 1:3 w l lc rgb '#c0392b' lw 1.5 notitle");

        // Panel 2: seasonal.
        gp("set ylabel 'Seasonal'");
        gp("set tmargin 1");
        gp("plot '" + fwdSlash(tmpPath) + "' u 1:4 w l lc rgb '#2980b9' lw 1.5 notitle");

        // Panel 3: residual.
        gp("set ylabel 'Residual'");
        gp("set xlabel 'Time (MJD)'");
        gp("set bmargin 3");
        gp("plot '" + fwdSlash(tmpPath) + "' u 1:5 w l lc rgb '#27ae60' lw 1.0 notitle");

        gp("unset multiplot");
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotDecomposition::plotPanels failed: " + std::string(ex.what()));
    }

    std::filesystem::remove(tmpPath);
    LOKI_INFO("PlotDecomposition: panels -> " + imgPath.string());
}

// -----------------------------------------------------------------------------

void PlotDecomposition::plotDiagnostics(const DecompositionResult& result,
                                         const std::string&         datasetName,
                                         const std::string&         compName) const
{
    // Delegate to Plot::residualDiagnostics() -- it is a non-static member method.
    // The title argument is used internally as the output file stem.
    const std::string title =
        "decomposition_" + datasetName + "_" + compName + "_diagnostics";

    try {
        Plot corePlot(m_cfg);
        // Second argument: fittedValues -- empty means residuals are plotted vs index.
        corePlot.residualDiagnostics(result.residual, {}, title);
    } catch (const LOKIException& ex) {
        LOKI_WARNING("PlotDecomposition::plotDiagnostics failed: " + std::string(ex.what()));
    }

    LOKI_INFO("PlotDecomposition: diagnostics -> " + title + "." + m_cfg.plots.outputFormat);
}
