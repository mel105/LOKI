#include <loki/spatial/plotSpatial.hpp>

#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/io/gnuplot.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace loki;

namespace loki::spatial {

// =============================================================================
//  Helpers
// =============================================================================

PlotSpatial::PlotSpatial(const AppConfig& cfg) : m_cfg(cfg) {}

std::string PlotSpatial::fwdSlash(const std::string& p)
{
    std::string s = p;
    for (char& c : s) if (c == '\\') c = '/';
    return s;
}

std::string PlotSpatial::_plotPath(const std::string& dataset,
                                    const std::string& variable,
                                    const std::string& plottype) const
{
    const std::string fmt = m_cfg.plots.outputFormat;
    return fwdSlash((m_cfg.imgDir / ("spatial_" + dataset + "_"
                                     + variable + "_" + plottype
                                     + "." + fmt)).string());
}

// =============================================================================
//  plot  (dispatcher)
// =============================================================================

void PlotSpatial::plot(const SpatialResult& result,
                        const std::string&   datasetName) const
{
    const auto& pcfg = m_cfg.plots;

    if (pcfg.spatialHeatmap)   _plotHeatmap  (result, datasetName);
    if (pcfg.spatialScatter)   _plotScatter  (result, datasetName);
    if (pcfg.spatialVariogram && result.method == "kriging")
                                _plotVariogram(result, datasetName);
    if (pcfg.spatialCrossval && !result.crossValidation.errors.empty())
                                _plotCrossVal (result, datasetName);
    if (pcfg.spatialVariance && result.method == "kriging")
                                _plotVariance (result, datasetName);
}

// =============================================================================
//  _plotHeatmap  -- interpolated grid as pm3d colour map
// =============================================================================

void PlotSpatial::_plotHeatmap(const SpatialResult& result,
                                const std::string&   dataset) const
{
    // Write grid to a temporary file as "nonuniform matrix" format for gnuplot.
    const std::filesystem::path tmpPath =
        m_cfg.imgDir / (".tmp_spatial_heatmap_" + result.variableName + ".dat");

    {
        std::ofstream ofs(tmpPath);
        if (!ofs.is_open()) {
            LOKI_WARNING("PlotSpatial: cannot write tmp file " + tmpPath.string());
            return;
        }

        const auto& ext = result.grid.extent;
        const auto& val = result.grid.values;

        // nonuniform matrix format:
        //   First row: 0 x0 x1 x2 ...
        //   Each subsequent row: yi v00 v01 ...
        ofs << std::fixed << std::setprecision(9);
        ofs << "0";
        for (int col = 0; col < ext.nCols; ++col) {
            ofs << " " << (ext.xMin + static_cast<double>(col) * ext.resX);
        }
        ofs << "\n";
        for (int row = 0; row < ext.nRows; ++row) {
            ofs << (ext.yMin + static_cast<double>(row) * ext.resY);
            for (int col = 0; col < ext.nCols; ++col) {
                ofs << " " << val(row, col);
            }
            ofs << "\n";
        }
        ofs.flush();
    }

    const std::string outPath = _plotPath(dataset, result.variableName, "heatmap");
    const std::string fmt     = m_cfg.plots.outputFormat;

    try {
        Gnuplot gp;
        gp("set terminal pngcairo noenhanced font 'Sans,12' size 900,750");
        gp("set output '" + outPath + "'");
        gp("set title 'Spatial interpolation -- " + result.variableName
           + "  [" + result.method + "]'");
        gp("set xlabel 'X'");
        gp("set ylabel 'Y'");
        gp("set cblabel '" + result.variableName + "'");
        gp("set palette rgbformulae 33,13,10");
        gp("set pm3d map interpolate 0,0");
        gp("set colorbox");
        gp("splot '" + fwdSlash(tmpPath.string())
           + "' nonuniform matrix with pm3d notitle");
    } catch (const std::exception& e) {
        LOKI_WARNING(std::string("PlotSpatial: heatmap plot failed: ") + e.what());
    }

    std::filesystem::remove(tmpPath);
    LOKI_INFO("Plot: " + outPath);
}

// =============================================================================
//  _plotScatter  -- input points coloured by value
// =============================================================================

void PlotSpatial::_plotScatter(const SpatialResult& result,
                                const std::string&   dataset) const
{
    if (result.nObs == 0) return;

    const std::string outPath = _plotPath(dataset, result.variableName, "scatter");

    try {
        Gnuplot gp;
        gp("set terminal pngcairo noenhanced font 'Sans,12' size 800,700");
        gp("set output '" + outPath + "'");
        gp("set title 'Input scatter -- " + result.variableName + "'");
        gp("set xlabel 'X'");
        gp("set ylabel 'Y'");
        gp("set cblabel '" + result.variableName + "'");
        gp("set palette rgbformulae 33,13,10");
        gp("set colorbox");
        gp("set pointsize 1.5");

        // Build datablock with input points.
        std::ostringstream data;
        data << "$scatter << EOD\n";
        data << std::fixed << std::setprecision(9);

        // We need the original points -- stored in result we don't have direct
        // access to them in PlotSpatial. Use the CV x/y as a proxy if available,
        // otherwise skip. For a full solution the result should carry the input
        // points. Here we emit a warning and skip.
        if (!result.crossValidation.x.empty()) {
            const int n = static_cast<int>(result.crossValidation.x.size());
            for (int i = 0; i < n; ++i) {
                const std::size_t si = static_cast<std::size_t>(i);
                // Reconstruct observed value: z_i = z_hat_i + e_i
                // z_hat_i is the LOO prediction at (x_i, y_i).
                // We approximate z_hat from the full-data grid at nearest cell.
                const double qx  = result.crossValidation.x[si];
                const double qy  = result.crossValidation.y[si];
                const auto&  ext = result.grid.extent;
                const int col = std::clamp(
                    static_cast<int>(std::round((qx - ext.xMin) / ext.resX)),
                    0, ext.nCols - 1);
                const int row = std::clamp(
                    static_cast<int>(std::round((qy - ext.yMin) / ext.resY)),
                    0, ext.nRows - 1);
                const double zHat = result.grid.values(row, col);
                const double zi   = zHat + result.crossValidation.errors[si];
                data << qx << " " << qy << " " << zi << "\n";
            }
        } else {
            LOKI_WARNING("PlotSpatial: scatter plot skipped (enable cross_validate to get point coordinates).");
            return;
        }
        data << "EOD\n";

        gp(data.str());
        gp("plot $scatter using 1:2:3 with points pt 7 palette notitle");
    } catch (const std::exception& e) {
        LOKI_WARNING(std::string("PlotSpatial: scatter plot failed: ") + e.what());
    }
    LOKI_INFO("Plot: " + outPath);
}

// =============================================================================
//  _plotVariogram
// =============================================================================

void PlotSpatial::_plotVariogram(const SpatialResult& result,
                                  const std::string&   dataset) const
{
    if (result.empiricalVariogram.empty()) return;

    const std::string outPath = _plotPath(dataset, result.variableName, "variogram");

    try {
        Gnuplot gp;
        gp("set terminal pngcairo noenhanced font 'Sans,12' size 800,550");
        gp("set output '" + outPath + "'");
        gp("set title 'Spatial variogram -- " + result.variableName + "'");
        gp("set xlabel 'Lag distance'");
        gp("set ylabel 'Semi-variance'");
        gp("set grid");

        // Empirical points datablock.
        std::ostringstream emp;
        emp << "$emp << EOD\n";
        emp << std::fixed << std::setprecision(9);
        for (const auto& vp : result.empiricalVariogram) {
            emp << vp.lag << " " << vp.gamma << " " << vp.count << "\n";
        }
        emp << "EOD\n";
        gp(emp.str());

        // Theoretical model curve -- computed in C++ and sent as inline data.
        // This avoids gnuplot user-defined functions entirely, which are
        // unreliable via pipe on Windows/MinGW: gp() sends each string as a
        // separate fputs+newline call, so multi-line strings do not work and
        // user-defined functions cannot safely reference previously-set variables.
        const auto&  vf     = result.variogram;
        const double maxLag = result.empiricalVariogram.back().lag * 1.1;
        const double nug    = vf.nugget;
        const double sill   = vf.sill;
        const double range  = (vf.range > 0.0) ? vf.range : 1.0;

        const bool hasModel = (vf.model == "spherical"   ||
                               vf.model == "exponential" ||
                               vf.model == "gaussian");

        // Sample model at 200 points and send as a gnuplot named datablock.
        if (hasModel) {
            const int    N    = 200;
            const double step = maxLag / static_cast<double>(N - 1);
            std::ostringstream mdl;
            mdl << "$model << EOD\n";
            mdl << std::fixed << std::setprecision(9);
            for (int k = 0; k < N; ++k) {
                const double h = static_cast<double>(k) * step;
                double gamma = 0.0;
                if (vf.model == "spherical") {
                    gamma = (h <= range)
                        ? nug + (sill - nug) * (1.5*(h/range) - 0.5*std::pow(h/range, 3.0))
                        : nug + (sill - nug);
                } else if (vf.model == "exponential") {
                    gamma = nug + (sill - nug) * (1.0 - std::exp(-h / range));
                } else if (vf.model == "gaussian") {
                    gamma = nug + (sill - nug) * (1.0 - std::exp(-std::pow(h/range, 2.0)));
                }
                mdl << h << " " << gamma << "\n";
            }
            mdl << "EOD\n";
            gp(mdl.str());
        }

        gp("set xrange [0:" + std::to_string(maxLag) + "]");

        if (hasModel) {
            gp("plot $emp using 1:2 with points pt 7 ps 1.2 lc rgb '#2266bb' "
               "title 'Empirical', "
               "$model using 1:2 with lines lw 2 lc rgb '#cc3300' title '"
               + vf.model + " model'");
        } else {
            gp("plot $emp using 1:2 with points pt 7 ps 1.2 lc rgb '#2266bb' "
               "title 'Empirical variogram'");
        }
    } catch (const std::exception& e) {
        LOKI_WARNING(std::string("PlotSpatial: variogram plot failed: ") + e.what());
    }
    LOKI_INFO("Plot: " + outPath);
}

// =============================================================================
//  _plotCrossVal
// =============================================================================

void PlotSpatial::_plotCrossVal(const SpatialResult& result,
                                 const std::string&   dataset) const
{
    const auto& cv = result.crossValidation;
    if (cv.errors.empty()) return;

    const std::string outPath = _plotPath(dataset, result.variableName, "crossval");

    try {
        Gnuplot gp;
        gp("set terminal pngcairo noenhanced font 'Sans,12' size 800,650");
        gp("set output '" + outPath + "'");
        gp("set title 'LOO cross-validation errors -- " + result.variableName + "'");
        gp("set xlabel 'X'");
        gp("set ylabel 'Y'");
        gp("set cblabel 'LOO error'");
        gp("set palette defined (-1 'blue', 0 'white', 1 'red')");
        gp("set colorbox");

        std::ostringstream data;
        data << "$cv << EOD\n";
        data << std::fixed << std::setprecision(9);
        const int n = static_cast<int>(cv.errors.size());
        for (int i = 0; i < n; ++i) {
            const std::size_t si = static_cast<std::size_t>(i);
            data << cv.x[si] << " " << cv.y[si] << " " << cv.errors[si] << "\n";
        }
        data << "EOD\n";
        gp(data.str());

        gp("set pointsize 1.5");
        gp("plot $cv using 1:2:3 with points pt 7 palette notitle");
    } catch (const std::exception& e) {
        LOKI_WARNING(std::string("PlotSpatial: crossval plot failed: ") + e.what());
    }
    LOKI_INFO("Plot: " + outPath);
}

// =============================================================================
//  _plotVariance  -- Kriging variance grid
// =============================================================================

void PlotSpatial::_plotVariance(const SpatialResult& result,
                                 const std::string&   dataset) const
{
    if (result.method != "kriging") return;

    const std::filesystem::path tmpPath =
        m_cfg.imgDir / (".tmp_spatial_variance_" + result.variableName + ".dat");

    {
        std::ofstream ofs(tmpPath);
        if (!ofs.is_open()) return;

        const auto& ext = result.grid.extent;
        const auto& var = result.grid.variance;

        ofs << std::fixed << std::setprecision(9);
        ofs << "0";
        for (int col = 0; col < ext.nCols; ++col) {
            ofs << " " << (ext.xMin + static_cast<double>(col) * ext.resX);
        }
        ofs << "\n";
        for (int row = 0; row < ext.nRows; ++row) {
            ofs << (ext.yMin + static_cast<double>(row) * ext.resY);
            for (int col = 0; col < ext.nCols; ++col) {
                ofs << " " << var(row, col);
            }
            ofs << "\n";
        }
        ofs.flush();
    }

    const std::string outPath = _plotPath(dataset, result.variableName, "variance");

    try {
        Gnuplot gp;
        gp("set terminal pngcairo noenhanced font 'Sans,12' size 900,750");
        gp("set output '" + outPath + "'");
        gp("set title 'Kriging variance -- " + result.variableName + "'");
        gp("set xlabel 'X'");
        gp("set ylabel 'Y'");
        gp("set cblabel 'Variance'");
        gp("set palette defined (0 'white', 1 'orange', 2 'red')");
        gp("set pm3d map interpolate 0,0");
        gp("splot '" + fwdSlash(tmpPath.string())
           + "' nonuniform matrix with pm3d notitle");
    } catch (const std::exception& e) {
        LOKI_WARNING(std::string("PlotSpatial: variance plot failed: ") + e.what());
    }

    std::filesystem::remove(tmpPath);
    LOKI_INFO("Plot: " + outPath);
}

} // namespace loki::spatial
