#include <loki/spline/plotSpline.hpp>

#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/io/gnuplot.hpp>
#include <loki/io/plot.hpp>
#include <loki/math/bspline.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>

using namespace loki;

namespace loki::spline {

// =============================================================================
//  Constructor
// =============================================================================

PlotSpline::PlotSpline(const AppConfig& cfg)
    : m_cfg(cfg)
{}

// =============================================================================
//  plot()  --  dispatch to individual generators
// =============================================================================

void PlotSpline::plot(const SplineResult& result,
                      const TimeSeries&   tsFilled,
                      const std::string&  datasetName) const
{
    const PlotConfig& pcfg = m_cfg.plots;
    const std::string component = result.componentName;

    if (pcfg.splineOverlay)
        _plotOverlay(result, tsFilled, _makeStem(datasetName, component, "overlay"));

    if (pcfg.splineResiduals)
        _plotResiduals(result, _makeStem(datasetName, component, "residuals"));

    if (pcfg.splineBasis)
        _plotBasis(result, _makeStem(datasetName, component, "basis"));

    if (pcfg.splineKnots)
        _plotKnots(result, tsFilled, _makeStem(datasetName, component, "knots"));

    if (pcfg.splineCv && !result.cvCurve.empty())
        _plotCv(result, _makeStem(datasetName, component, "cv"));

    if (pcfg.splineDiagnostics) {
        loki::Plot corePlot(m_cfg);
        corePlot.residualDiagnostics(result.residuals, result.fitted,
                                     "B-spline residuals -- " + component);
    }
}

// =============================================================================
//  _plotOverlay
//
//  Panel layout (single plot, no multiplot needed):
//    - filledcurves: CI band (index, ci_lower, ci_upper) -- pink #ffb3c1
//    - points: original observations -- grey
//    - lines:  fitted B-spline -- steel blue #4472C4
// =============================================================================

void PlotSpline::_plotOverlay(const SplineResult& result,
                               const TimeSeries&   /*tsFilled*/,
                               const std::string&  stem) const
{
    const int n = result.nObs;
    if (n == 0) return;

    // Build inline datablock: col1=index, col2=observed, col3=fitted,
    //                          col4=ci_lower, col5=ci_upper
    std::ostringstream ds;
    for (int i = 0; i < n; ++i) {
        const std::size_t si = static_cast<std::size_t>(i);
        ds << result.tObs[si]    << " "
           << result.zObs[si]    << " "
           << result.fitted[si]  << " "
           << result.ciLower[si] << " "
           << result.ciUpper[si] << "\n";
    }

    const std::string fmt   = m_cfg.plots.outputFormat;
    const std::string title = "B-spline fit -- " + result.componentName
                            + "  [degree=" + std::to_string(result.degree)
                            + ", nCtrl=" + std::to_string(result.nCtrl)
                            + ", RMSE=" + [&]{
                                std::ostringstream s;
                                s << std::fixed;
                                s.precision(4);
                                s << result.rmse;
                                return s.str(); }()
                            + ", R2=" + [&]{
                                std::ostringstream s;
                                s << std::fixed;
                                s.precision(4);
                                s << result.rSquared;
                                return s.str(); }()
                            + "]";

    Gnuplot gp;
    gp("set terminal pngcairo noenhanced font 'Sans,12' size 1200,500");
    gp("set output '" + _fwdSlash(stem + "." + fmt) + "'");
    gp("set title  '" + title + "'");
    gp("set xlabel 'Sample index'");
    gp("set ylabel '" + result.componentName + "'");
    gp("set grid");
    gp("set key top right");

    // Define datablock before any plot command.
    gp("$data << EOD");
    gp(ds.str() + "EOD");

    gp("plot $data using 1:4:5 with filledcurves fillstyle solid 0.4 "
       "lc rgb '#ffb3c1' notitle, "
       "$data using 1:2 with points pt 7 ps 0.4 lc rgb '#aaaaaa' title 'Observed', "
       "$data using 1:3 with lines lw 2 lc rgb '#4472C4' "
       "title 'B-spline (nCtrl=" + std::to_string(result.nCtrl) + ")'");

    LOKI_INFO("  plot written: " + stem + "." + fmt);
}

// =============================================================================
//  _plotResiduals
// =============================================================================

void PlotSpline::_plotResiduals(const SplineResult& result,
                                 const std::string&  stem) const
{
    const int n = result.nObs;
    if (n == 0) return;

    // Build datablock: col1=index, col2=residual
    std::ostringstream ds;
    for (int i = 0; i < n; ++i) {
        const std::size_t si = static_cast<std::size_t>(i);
        ds << result.tObs[si]      << " "
           << result.residuals[si] << "\n";
    }

    const std::string fmt = m_cfg.plots.outputFormat;

    // Horizontal reference lines at 0, +/-rmse, +/-2*rmse
    const double rmse = result.rmse;
    const auto fmtD   = [](double v) {
        std::ostringstream s;
        s << std::fixed;
        s.precision(5);
        s << v;
        return s.str();
    };

    Gnuplot gp;
    gp("set terminal pngcairo noenhanced font 'Sans,12' size 1200,400");
    gp("set output '" + _fwdSlash(stem + "." + fmt) + "'");
    gp("set title 'Residuals -- " + result.componentName + "'");
    gp("set xlabel 'Sample index'");
    gp("set ylabel 'Residual'");
    gp("set grid");
    gp("set key top right");

    gp("$data << EOD");
    gp(ds.str() + "EOD");

    // Reference lines via arrow/label trick (no extra datablock needed).
    gp("set arrow 1 from graph 0,first 0        to graph 1,first 0        "
       "nohead lw 1 lc rgb '#333333' dt 1");
    gp("set arrow 2 from graph 0,first  " + fmtD( rmse) +
       " to graph 1,first  " + fmtD( rmse) + " nohead lw 1 lc rgb '#cc0000' dt 2");
    gp("set arrow 3 from graph 0,first  " + fmtD(-rmse) +
       " to graph 1,first  " + fmtD(-rmse) + " nohead lw 1 lc rgb '#cc0000' dt 2");
    gp("set arrow 4 from graph 0,first  " + fmtD( 2.0*rmse) +
       " to graph 1,first  " + fmtD( 2.0*rmse) + " nohead lw 1 lc rgb '#ff6666' dt 3");
    gp("set arrow 5 from graph 0,first  " + fmtD(-2.0*rmse) +
       " to graph 1,first  " + fmtD(-2.0*rmse) + " nohead lw 1 lc rgb '#ff6666' dt 3");

    gp("plot $data using 1:2 with points pt 7 ps 0.5 lc rgb '#4472C4' "
       "title 'Residual  (RMSE=" + fmtD(rmse) + ")'");

    LOKI_INFO("  plot written: " + stem + "." + fmt);
}

// =============================================================================
//  _plotBasis
// =============================================================================

void PlotSpline::_plotBasis(const SplineResult& result,
                             const std::string&  stem) const
{
    const int nCtrl  = result.nCtrl;
    const int degree = result.degree;
    if (nCtrl <= 0) return;

    // Evaluate all basis functions on 300 uniform points in [0, 1].
    const int nEval = 300;
    std::vector<double> tEval(static_cast<std::size_t>(nEval));
    for (int i = 0; i < nEval; ++i) {
        tEval[static_cast<std::size_t>(i)] = static_cast<double>(i)
                                           / static_cast<double>(nEval - 1);
    }

    // Build datablock: each column = one basis function, col 0 = t.
    // Format: t  N_0  N_1  ...  N_{nCtrl-1}
    const std::vector<double>& knots = result.fit.knots;

    std::ostringstream ds;
    for (int i = 0; i < nEval; ++i) {
        const std::size_t si  = static_cast<std::size_t>(i);
        const double       ti = tEval[si];
        ds << ti;
        const std::vector<double> row = loki::math::bsplineBasisRow(ti, degree, knots);
        for (const double v : row) ds << " " << v;
        ds << "\n";
    }

    const std::string fmt = m_cfg.plots.outputFormat;

    // Interior knot positions (de-duplicate clamped repeats).
    std::vector<double> interiorKnots;
    for (std::size_t k = static_cast<std::size_t>(degree + 1);
         k < knots.size() - static_cast<std::size_t>(degree + 1); ++k) {
        interiorKnots.push_back(knots[k]);
    }

    Gnuplot gp;
    gp("set terminal pngcairo noenhanced font 'Sans,12' size 1200,500");
    gp("set output '" + _fwdSlash(stem + "." + fmt) + "'");
    gp("set title 'B-spline basis functions N_{i," + std::to_string(degree)
       + "}  [nCtrl=" + std::to_string(nCtrl) + "]'");
    gp("set xlabel 'Normalised parameter t'");
    gp("set ylabel 'Basis value'");
    gp("set yrange [0:1.1]");
    gp("set grid");

    // Vertical dashed lines for interior knots.
    for (std::size_t k = 0; k < interiorKnots.size(); ++k) {
        const std::ostringstream kv;
        gp("set arrow " + std::to_string(k + 1)
           + " from first " + std::to_string(interiorKnots[k]) + ",0"
           + " to first "   + std::to_string(interiorKnots[k]) + ",1.1"
           + " nohead lw 1 lc rgb '#999999' dt 2");
    }

    gp("$data << EOD");
    gp(ds.str() + "EOD");

    // Build plot command: one line per basis function.
    std::string plotCmd = "plot ";
    for (int i = 0; i < nCtrl; ++i) {
        if (i > 0) plotCmd += ", ";
        plotCmd += "$data using 1:" + std::to_string(i + 2)
                 + " with lines lw 1.5 notitle";
    }
    gp(plotCmd);

    LOKI_INFO("  plot written: " + stem + "." + fmt);
}

// =============================================================================
//  _plotKnots
// =============================================================================

void PlotSpline::_plotKnots(const SplineResult& result,
                             const TimeSeries&   /*tsFilled*/,
                             const std::string&  stem) const
{
    const int n = result.nObs;
    if (n == 0) return;

    // Build datablock for the observed series.
    std::ostringstream ds;
    for (int i = 0; i < n; ++i) {
        const std::size_t si = static_cast<std::size_t>(i);
        ds << result.tObs[si] << " " << result.zObs[si] << "\n";
    }

    // Interior knot positions in sample-index space:
    // tNorm[k] * (tMax - tMin) + tMin
    const double tMin    = result.fit.tMin;
    const double tMax    = result.fit.tMax;
    const double tRange  = tMax - tMin;
    const int    degree  = result.degree;
    const auto&  knots   = result.fit.knots;
    const int    nKnots  = static_cast<int>(knots.size());

    // Collect unique interior knots (skip the degree+1 clamped endpoints).
    std::vector<double> interiorKnotsIdx;
    for (int k = degree + 1; k < nKnots - degree - 1; ++k) {
        interiorKnotsIdx.push_back(knots[static_cast<std::size_t>(k)] * tRange + tMin);
    }

    const std::string fmt = m_cfg.plots.outputFormat;

    Gnuplot gp;
    gp("set terminal pngcairo noenhanced font 'Sans,12' size 1200,500");
    gp("set output '" + _fwdSlash(stem + "." + fmt) + "'");
    gp("set title 'Knot positions -- " + result.componentName
       + "  [nCtrl=" + std::to_string(result.nCtrl)
       + ", knotPlacement=" + result.knotPlacement + "]'");
    gp("set xlabel 'Sample index'");
    gp("set ylabel '" + result.componentName + "'");
    gp("set grid");

    // Vertical dashed lines at each interior knot position (sample index).
    for (std::size_t k = 0; k < interiorKnotsIdx.size(); ++k) {
        gp("set arrow " + std::to_string(k + 1)
           + " from first " + std::to_string(interiorKnotsIdx[k]) + ",graph 0"
           + " to first "   + std::to_string(interiorKnotsIdx[k]) + ",graph 1"
           + " nohead lw 1 lc rgb '#cc4400' dt 2");
    }

    gp("$data << EOD");
    gp(ds.str() + "EOD");

    gp("plot $data using 1:2 with lines lw 1 lc rgb '#4472C4' "
       "title '" + result.componentName + "  (" + std::to_string(interiorKnotsIdx.size())
       + " interior knots)'");

    LOKI_INFO("  plot written: " + stem + "." + fmt);
}

// =============================================================================
//  _plotCv
// =============================================================================

void PlotSpline::_plotCv(const SplineResult& result,
                          const std::string&  stem) const
{
    if (result.cvCurve.empty()) return;

    // Build datablock: col1=nCtrl, col2=cv_rmse
    std::ostringstream ds;
    for (const auto& pt : result.cvCurve) {
        ds << pt.nCtrl << " " << pt.rmse << "\n";
    }

    const std::string fmt = m_cfg.plots.outputFormat;

    Gnuplot gp;
    gp("set terminal pngcairo noenhanced font 'Sans,12' size 900,450");
    gp("set output '" + _fwdSlash(stem + "." + fmt) + "'");
    gp("set title 'CV RMSE vs nCtrl -- " + result.componentName
       + "  [degree=" + std::to_string(result.degree) + "]'");
    gp("set xlabel 'Number of control points (nCtrl)'");
    gp("set ylabel 'CV RMSE'");
    gp("set grid");
    gp("set key top right");

    // Vertical line at optimal nCtrl.
    gp("set arrow 1 from first " + std::to_string(result.optimalNCtrl) + ",graph 0"
       + " to first " + std::to_string(result.optimalNCtrl) + ",graph 1"
       + " nohead lw 2 lc rgb '#cc0000' dt 2");
    gp("set label 1 'optimal=" + std::to_string(result.optimalNCtrl) + "' "
       "at first " + std::to_string(result.optimalNCtrl) + ",graph 0.95 "
       "offset 0.5,0 tc rgb '#cc0000'");

    gp("$data << EOD");
    gp(ds.str() + "EOD");

    gp("plot $data using 1:2 with linespoints lw 2 pt 7 ps 0.8 "
       "lc rgb '#4472C4' title 'CV RMSE (one-SE elbow rule)'");

    LOKI_INFO("  plot written: " + stem + "." + fmt);
}

// =============================================================================
//  _makeStem
// =============================================================================

std::string PlotSpline::_makeStem(const std::string& datasetName,
                                   const std::string& component,
                                   const std::string& plotType) const
{
    const std::filesystem::path p =
        m_cfg.imgDir / ("spline_" + datasetName + "_" + component + "_" + plotType);
    return p.string();
}

// =============================================================================
//  _fwdSlash
// =============================================================================

std::string PlotSpline::_fwdSlash(const std::string& path)
{
    std::string s = path;
    for (char& c : s) {
        if (c == '\\') c = '/';
    }
    return s;
}

} // namespace loki::spline
