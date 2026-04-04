#include <loki/ssa/plotSsa.hpp>
#include <loki/io/gnuplot.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace loki;
using namespace loki::ssa;

// ----------------------------------------------------------------------------
//  Construction
// ----------------------------------------------------------------------------

PlotSsa::PlotSsa(const AppConfig& cfg)
    : m_cfg{cfg}
{
    std::filesystem::create_directories(m_cfg.imgDir);
}

// ----------------------------------------------------------------------------
//  plotAll
// ----------------------------------------------------------------------------

void PlotSsa::plotAll(const TimeSeries& filled,
                       const SsaResult&  result,
                       int               nComponents) const
{
    const auto& p = m_cfg.plots;

    if (p.ssaScree) {
        try { plotScree(filled, result); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotSsa: scree failed: " + std::string(ex.what()));
        }
    }

    if (p.ssaWCorr) {
        try { plotWCorr(filled, result); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotSsa: wcorr failed: " + std::string(ex.what()));
        }
    }

    if (p.ssaComponents) {
        try { plotComponents(filled, result, nComponents); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotSsa: components failed: " + std::string(ex.what()));
        }
    }

    if (p.ssaReconstruction) {
        try { plotReconstruction(filled, result); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotSsa: reconstruction failed: " + std::string(ex.what()));
        }
    }
}

// ----------------------------------------------------------------------------
//  plotScree
// ----------------------------------------------------------------------------

void PlotSsa::plotScree(const TimeSeries& filled,
                         const SsaResult&  result) const
{
    const int r = static_cast<int>(result.eigenvalues.size());
    if (r == 0) {
        throw DataException("PlotSsa::plotScree: eigenvalues are empty.");
    }

    const std::string base    = _baseName(filled);
    const auto        datFile = m_cfg.imgDir / (".tmp_" + base + "_scree.dat");
    const auto        outPath = _outPath(base, "scree");

    {
        std::ofstream dat(datFile);
        dat << std::fixed << std::setprecision(10);
        // col1=index(1-based), col2=eigenvalue, col3=cumulative variance fraction
        double cumVar = 0.0;
        for (int i = 0; i < r; ++i) {
            cumVar += result.varianceFractions[static_cast<std::size_t>(i)];
            dat << (i + 1) << "\t"
                << result.eigenvalues[static_cast<std::size_t>(i)] << "\t"
                << cumVar * 100.0 << "\n";
        }
    }

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal(1000, 500));
        gp("set output '" + _fwdSlash(outPath) + "'");
        gp("set title 'SSA scree plot: " + base + "' noenhanced");
        gp("set xlabel 'Eigentriple index'");
        gp("set ylabel 'Eigenvalue'");
        gp("set y2label 'Cumulative variance [%]'");
        gp("set yrange [*:*]");
        gp("set y2range [0:100]");
        gp("set ytics nomirror");
        gp("set y2tics 0, 10, 100");
        gp("set logscale y");
        gp("set grid");
        gp("set key top right");
        gp("set style data histograms");
        gp("set style fill solid 0.7");
        // Draw 95% cumulative variance reference line on y2
        gp("set arrow 1 from graph 0, second 95 to graph 1, second 95"
           " nohead lc rgb '#888888' lw 1.5 dt 2");
        gp("plot '" + _fwdSlash(datFile) + "' u 1:2 axes x1y1"
           " w boxes lc rgb '#2c7bb6' t 'Eigenvalue',"
           " '" + _fwdSlash(datFile) + "' u 1:3 axes x1y2"
           " w linespoints lc rgb '#d7191c' lw 1.5 pt 7 ps 0.6 t 'Cumul. variance'");
    } catch (const LOKIException&) {
        std::filesystem::remove(datFile);
        throw;
    }
    std::filesystem::remove(datFile);
}

// ----------------------------------------------------------------------------
//  plotWCorr
// ----------------------------------------------------------------------------

void PlotSsa::plotWCorr(const TimeSeries& filled,
                         const SsaResult&  result) const
{
    if (result.wCorrMatrix.empty()) {
        throw DataException("PlotSsa::plotWCorr: wCorrMatrix is empty.");
    }

    const int r = static_cast<int>(result.wCorrMatrix.size());

    const std::string base    = _baseName(filled);
    const auto        datFile = m_cfg.imgDir / (".tmp_" + base + "_wcorr.dat");
    const auto        outPath = _outPath(base, "wcorr");

    // gnuplot image expects: col1=x, col2=y, col3=value
    // x = column index (0-based), y = row index (0-based, inverted for display)
    {
        std::ofstream dat(datFile);
        dat << std::fixed << std::setprecision(6);
        for (int i = 0; i < r; ++i) {
            for (int j = 0; j < r; ++j) {
                dat << j << "\t" << (r - 1 - i) << "\t"
                    << result.wCorrMatrix[static_cast<std::size_t>(i)]
                                         [static_cast<std::size_t>(j)] << "\n";
            }
            dat << "\n";   // blank line between rows for gnuplot pm3d
        }
    }

    // Square plot: size proportional to r, capped at 900px
    const int sz = std::min(900, std::max(400, r * 6));

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal(sz, sz));
        gp("set output '" + _fwdSlash(outPath) + "'");
        gp("set title 'SSA w-correlation: " + base + "' noenhanced");
        gp("unset xlabel");
        gp("unset ylabel");
        gp("set xrange [-0.5:" + std::to_string(r - 1) + ".5]");
        gp("set yrange [-0.5:" + std::to_string(r - 1) + ".5]");
        gp("set cbrange [0:1]");
        gp("set palette defined (0 'white', 0.5 '#6baed6', 1 '#08306b')");
        gp("set colorbox");
        gp("unset key");
        gp("set tics out nomirror");
        // Label axes with eigentriple indices (show every 5th for large r)
        const int step = (r <= 20) ? 1 : (r <= 50) ? 5 : 10;
        {
            std::ostringstream xtics;
            xtics << "set xtics (";
            bool first = true;
            for (int j = 0; j < r; j += step) {
                if (!first) xtics << ",";
                xtics << "'" << j << "' " << j;
                first = false;
            }
            xtics << ")";
            gp(xtics.str());
        }
        {
            std::ostringstream ytics;
            ytics << "set ytics (";
            bool first = true;
            for (int i = 0; i < r; i += step) {
                if (!first) ytics << ",";
                // y axis is inverted: row i is drawn at y = r-1-i
                ytics << "'" << i << "' " << (r - 1 - i);
                first = false;
            }
            ytics << ")";
            gp(ytics.str());
        }
        gp("plot '" + _fwdSlash(datFile) + "' u 1:2:3 with image");
    } catch (const LOKIException&) {
        std::filesystem::remove(datFile);
        throw;
    }
    std::filesystem::remove(datFile);
}

// ----------------------------------------------------------------------------
//  plotComponents
// ----------------------------------------------------------------------------

void PlotSsa::plotComponents(const TimeSeries& filled,
                               const SsaResult&  result,
                               int               nComponents) const
{
    if (result.components.empty()) {
        throw DataException("PlotSsa::plotComponents: components are empty.");
    }

    const int r     = static_cast<int>(result.components.size());
    const int nPlot = std::min(nComponents, r);
    if (nPlot <= 0) return;

    const std::string base    = _baseName(filled);
    const auto        outPath = _outPath(base, "components");

    // Write one temp file per component
    std::vector<std::filesystem::path> datFiles;
    datFiles.reserve(static_cast<std::size_t>(nPlot));

    const std::size_t nTs = filled.size();

    for (int i = 0; i < nPlot; ++i) {
        const auto datFile = m_cfg.imgDir /
            (".tmp_" + base + "_comp" + std::to_string(i) + ".dat");
        datFiles.push_back(datFile);

        std::ofstream dat(datFile);
        dat << std::fixed << std::setprecision(10);
        const auto& comp = result.components[static_cast<std::size_t>(i)];
        const std::size_t nComp = std::min(nTs, comp.size());
        for (std::size_t k = 0; k < nComp; ++k) {
            dat << filled[k].time.mjd() << "\t" << comp[k] << "\n";
        }
    }

    // Stack nPlot panels vertically; use multiplot
    const int panelH = std::max(120, 600 / nPlot);
    const int totalH = panelH * nPlot + 80;   // extra for title

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal(1200, totalH));
        gp("set output '" + _fwdSlash(outPath) + "'");
        gp("set multiplot layout " + std::to_string(nPlot) + ",1"
           " title 'SSA elementary components: " + base + "' noenhanced");
        gp("set lmargin 10");
        gp("set rmargin 3");

        for (int i = 0; i < nPlot; ++i) {
            const double vf =
                result.varianceFractions[static_cast<std::size_t>(i)] * 100.0;
            std::ostringstream title;
            title << "F" << i << "  (" << std::fixed << std::setprecision(2)
                  << vf << "%)";

            const bool isLast = (i == nPlot - 1);
            if (isLast) {
                gp("set xlabel 'MJD'");
            } else {
                gp("unset xlabel");
                gp("set format x ''");
            }
            gp("set ylabel ''");
            gp("set title '" + title.str() + "' noenhanced");
            gp("set grid");
            gp("unset key");
            gp("set arrow 1 from graph 0, first 0 to graph 1, first 0"
               " nohead lc rgb '#aaaaaa' lw 1 dt 2");
            gp("plot '" + _fwdSlash(datFiles[static_cast<std::size_t>(i)])
               + "' u 1:2 w l lc rgb '#2c7bb6' lw 1.0");
        }
        gp("unset multiplot");
    } catch (const LOKIException&) {
        for (const auto& f : datFiles) std::filesystem::remove(f);
        throw;
    }
    for (const auto& f : datFiles) std::filesystem::remove(f);
}

// ----------------------------------------------------------------------------
//  plotReconstruction
// ----------------------------------------------------------------------------

void PlotSsa::plotReconstruction(const TimeSeries& filled,
                                  const SsaResult&  result) const
{
    if (result.groups.empty()) {
        throw DataException("PlotSsa::plotReconstruction: no groups in result.");
    }

    const std::string base    = _baseName(filled);
    const auto        outPath = _outPath(base, "reconstruction");
    const std::size_t nTs     = filled.size();

    // Determine if a "noise" group exists -> two-panel layout
    bool hasNoise = false;
    for (const auto& g : result.groups) {
        if (g.name == "noise" && !g.reconstruction.empty()) {
            hasNoise = true;
            break;
        }
    }

    // Write original series
    const auto datOrig = m_cfg.imgDir / (".tmp_" + base + "_rec_orig.dat");
    {
        std::ofstream dat(datOrig);
        dat << std::fixed << std::setprecision(10);
        for (std::size_t k = 0; k < nTs; ++k) {
            dat << filled[k].time.mjd() << "\t" << filled[k].value << "\n";
        }
    }

    // Write one dat file per group (excluding noise, handled separately)
    struct GroupDat {
        std::string           name;
        std::filesystem::path path;
    };
    std::vector<GroupDat> groupDats;
    std::filesystem::path datNoise;

    for (const auto& g : result.groups) {
        if (g.reconstruction.empty()) continue;
        const auto datFile = m_cfg.imgDir /
            (".tmp_" + base + "_rec_" + g.name + ".dat");

        std::ofstream dat(datFile);
        dat << std::fixed << std::setprecision(10);
        const std::size_t nRec = std::min(nTs, g.reconstruction.size());
        for (std::size_t k = 0; k < nRec; ++k) {
            dat << filled[k].time.mjd() << "\t" << g.reconstruction[k] << "\n";
        }

        if (g.name == "noise") {
            datNoise = datFile;
        } else {
            groupDats.push_back({g.name, datFile});
        }
    }

    // Colour palette for non-noise groups
    static const std::vector<std::string> palette = {
        "#d7191c",   // trend -> red
        "#2c7bb6",   // group_1 -> blue
        "#1a9641",   // group_2 -> green
        "#f46d43",   // group_3 -> orange
        "#762a83",   // group_4 -> purple
        "#4dac26",   // group_5 -> lime
    };

    const int nPanels = hasNoise ? 2 : 1;
    const int panelH  = hasNoise ? 350 : 500;

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal(1400, nPanels * panelH));
        gp("set output '" + _fwdSlash(outPath) + "'");

        if (nPanels > 1) {
            gp("set multiplot layout 2,1"
               " title 'SSA reconstruction: " + base + "' noenhanced");
        } else {
            gp("set title 'SSA reconstruction: " + base + "' noenhanced");
        }

        // ---- Top panel: original + signal groups ----
        if (nPanels > 1) {
            gp("unset xlabel");
            gp("set format x ''");
        } else {
            gp("set xlabel 'MJD'");
        }
        gp("set ylabel 'Value'");
        gp("set grid");
        gp("set key top right");
        gp("set lmargin 10");
        gp("set rmargin 3");

        // Build plot command
        std::ostringstream plotCmd;
        plotCmd << "plot '" << _fwdSlash(datOrig)
                << "' u 1:2 w l lc rgb '#cccccc' lw 1.0 t 'Original'";

        for (std::size_t gi = 0; gi < groupDats.size(); ++gi) {
            const std::string& col =
                palette[gi % palette.size()];
            plotCmd << ", '" << _fwdSlash(groupDats[gi].path)
                    << "' u 1:2 w l lc rgb '" << col
                    << "' lw 2.0 t '" << groupDats[gi].name << "'";
        }
        gp(plotCmd.str());

        // ---- Bottom panel: noise ----
        if (hasNoise) {
            gp("set xlabel 'MJD'");
            gp("set format x");
            gp("set ylabel 'Noise'");
            gp("set title '' ");
            gp("unset key");
            gp("plot '" + _fwdSlash(datNoise)
               + "' u 1:2 w l lc rgb '#888888' lw 1.0 t 'Noise'");
        }

        if (nPanels > 1) gp("unset multiplot");

    } catch (const LOKIException&) {
        std::filesystem::remove(datOrig);
        for (const auto& gd : groupDats) std::filesystem::remove(gd.path);
        if (!datNoise.empty()) std::filesystem::remove(datNoise);
        throw;
    }

    std::filesystem::remove(datOrig);
    for (const auto& gd : groupDats) std::filesystem::remove(gd.path);
    if (!datNoise.empty()) std::filesystem::remove(datNoise);
}

// ----------------------------------------------------------------------------
//  Private helpers
// ----------------------------------------------------------------------------

std::string PlotSsa::_baseName(const TimeSeries& ts) const
{
    const auto& meta    = ts.metadata();
    std::string dataset = m_cfg.input.file.stem().string();
    if (dataset.empty()) dataset = "data";
    std::string param   = meta.componentName.empty() ? "series" : meta.componentName;
    return "ssa_" + dataset + "_" + param;
}

std::filesystem::path PlotSsa::_outPath(const std::string& base,
                                         const std::string& plotType) const
{
    return m_cfg.imgDir / (base + "_" + plotType + "." + m_cfg.plots.outputFormat);
}

std::string PlotSsa::_terminal(int widthPx, int heightPx) const
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

std::string PlotSsa::_fwdSlash(const std::filesystem::path& p)
{
    std::string s = p.string();
    for (auto& c : s) { if (c == '\\') c = '/'; }
    return s;
}
