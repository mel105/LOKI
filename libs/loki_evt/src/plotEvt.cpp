#include "loki/evt/plotEvt.hpp"
#include "loki/evt/gpd.hpp"
#include "loki/core/logger.hpp"
#include "loki/io/gnuplot.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

using namespace loki;

namespace loki::evt {

// =============================================================================
//  Helpers
// =============================================================================

std::string PlotEvt::fwdSlash(const std::string& p)
{
    std::string r = p;
    for (char& c : r) if (c == '\\') c = '/';
    return r;
}

std::string PlotEvt::_outPath(const std::string& filename) const
{
    return fwdSlash(m_cfg.imgDir.string() + "/" + filename);
}

// =============================================================================
//  Constructor
// =============================================================================

PlotEvt::PlotEvt(const AppConfig& cfg)
    : m_cfg(cfg)
{}

// =============================================================================
//  plot (dispatcher)
// =============================================================================

void PlotEvt::plot(const EvtResult& result,
                    const std::string& datasetName,
                    const std::string& component) const
{
    const PlotConfig& pc = m_cfg.plots;
    const std::string fmt = pc.outputFormat.empty() ? "png" : pc.outputFormat;

    // Base filename prefix: evt_<dataset>_<component>
    const std::string base = datasetName + "_" + component;

    if (pc.evtMeanExcess   && result.method == "pot"
        && !result.thresholdCandidates.empty())
        _plotMeanExcess(result, base);

    if (pc.evtStability    && result.method == "pot"
        && !result.thresholdCandidates.empty())
        _plotStability(result, base);

    if (pc.evtReturnLevels && !result.returnLevels.empty())
        _plotReturnLevels(result, base);

    if (pc.evtExceedances  && result.method == "pot"
        && !result.exceedances.empty())
        _plotExceedances(result, base);

    if (pc.evtGpdFit       && result.method == "pot"
        && !result.exceedances.empty())
        _plotGpdFit(result, base);
}

// =============================================================================
//  Mean excess plot
// =============================================================================

void PlotEvt::_plotMeanExcess(const EvtResult& r,
                                const std::string& base) const
{
    const std::string fmt = m_cfg.plots.outputFormat.empty()
                          ? "png" : m_cfg.plots.outputFormat;
    const std::string outFile = _outPath("evt_" + base + "_mean_excess." + fmt);

    const auto& cands = r.thresholdCandidates;
    const auto& me    = r.meanExcessValues;
    const std::size_t n = cands.size();

    Gnuplot gp;
    gp("set terminal pngcairo noenhanced font 'Sans,12' size 900,500");
    gp("set output '" + outFile + "'");
    gp("set title 'Mean Excess Plot'");
    gp("set xlabel 'Threshold u'");
    gp("set ylabel 'Mean Excess E[X-u | X>u]'");
    gp("set grid");
    gp("set key top right");

    // Mark selected threshold.
    const double u_sel = r.gpd.threshold;
    gp("set arrow 1 from " + std::to_string(u_sel) + ", graph 0 to "
       + std::to_string(u_sel) + ", graph 1 nohead lc rgb '#CC0000' lw 2 dt 2");
    gp("set label 1 'Selected' at " + std::to_string(u_sel)
       + ", graph 0.95 left offset 0.5,0 tc rgb '#CC0000'");

    // Build inline data.
    std::ostringstream dat;
    for (std::size_t i = 0; i < n; ++i) {
        if (std::isfinite(me[i]))
            dat << cands[i] << " " << me[i] << "\n";
    }
    dat << "e\n";

    gp("plot '-' using 1:2 with linespoints lw 2 lc rgb '#2166AC' pt 7 ps 0.6 title 'Mean excess'");
    gp(dat.str());

    LOKI_INFO("EVT: mean excess plot -> " + outFile);
}

// =============================================================================
//  Stability plot (xi and sigma vs threshold)
// =============================================================================

void PlotEvt::_plotStability(const EvtResult& r,
                               const std::string& base) const
{
    const std::string fmt = m_cfg.plots.outputFormat.empty()
                          ? "png" : m_cfg.plots.outputFormat;
    const std::string outFile = _outPath("evt_" + base + "_stability." + fmt);

    const auto& cands = r.thresholdCandidates;
    const auto& xi    = r.xiStability;
    const auto& sigma = r.sigmaStability;
    const std::size_t n = cands.size();
    const double u_sel  = r.gpd.threshold;

    // Two-panel: xi on top, sigma on bottom.
    Gnuplot gp;
    gp("set terminal pngcairo noenhanced font 'Sans,12' size 900,700");
    gp("set output '" + outFile + "'");
    gp("set multiplot layout 2,1");

    // ---- xi panel ---
    gp("set title 'Shape Parameter Stability'");
    gp("set xlabel 'Threshold u'");
    gp("set ylabel 'xi (shape)'");
    gp("set grid");
    gp("set arrow 1 from " + std::to_string(u_sel) + ", graph 0 to "
       + std::to_string(u_sel) + ", graph 1 nohead lc rgb '#CC0000' lw 2 dt 2");
    gp("set label 1 'Selected' at " + std::to_string(u_sel)
       + ", graph 0.95 left offset 0.5,0 tc rgb '#CC0000'");

    std::ostringstream xi_dat;
    for (std::size_t i = 0; i < n; ++i)
        if (std::isfinite(xi[i]))
            xi_dat << cands[i] << " " << xi[i] << "\n";
    xi_dat << "e\n";

    gp("plot '-' using 1:2 with linespoints lw 2 lc rgb '#4DAC26' pt 7 ps 0.6 title 'xi'");
    gp(xi_dat.str());

    // ---- sigma panel ---
    gp("unset arrow 1");
    gp("unset label 1");
    gp("set title 'Scale Parameter Stability'");
    gp("set ylabel 'sigma (scale)'");
    gp("set arrow 1 from " + std::to_string(u_sel) + ", graph 0 to "
       + std::to_string(u_sel) + ", graph 1 nohead lc rgb '#CC0000' lw 2 dt 2");
    gp("set label 1 'Selected' at " + std::to_string(u_sel)
       + ", graph 0.95 left offset 0.5,0 tc rgb '#CC0000'");

    std::ostringstream sig_dat;
    for (std::size_t i = 0; i < n; ++i)
        if (std::isfinite(sigma[i]))
            sig_dat << cands[i] << " " << sigma[i] << "\n";
    sig_dat << "e\n";

    gp("plot '-' using 1:2 with linespoints lw 2 lc rgb '#E66101' pt 7 ps 0.6 title 'sigma'");
    gp(sig_dat.str());

    gp("unset multiplot");
    LOKI_INFO("EVT: stability plot -> " + outFile);
}

// =============================================================================
//  Return levels plot
// =============================================================================

void PlotEvt::_plotReturnLevels(const EvtResult& r,
                                  const std::string& base) const
{
    const std::string fmt = m_cfg.plots.outputFormat.empty()
                          ? "png" : m_cfg.plots.outputFormat;
    const std::string outFile = _outPath("evt_" + base + "_" + r.method
                                          + "_return_levels." + fmt);

    const auto& rls = r.returnLevels;
    if (rls.empty()) return;

    Gnuplot gp;
    gp("set terminal pngcairo noenhanced font 'Sans,12' size 900,500");
    gp("set output '" + outFile + "'");
    gp("set title 'Return Level Plot (" + r.method + ")'");
    gp("set xlabel 'Return Period (" + m_cfg.evt.timeUnit + ")'");
    gp("set ylabel 'Return Level'");
    gp("set logscale x");
    gp("set grid");
    gp("set key top left");

    // Three data streams: estimate, lower CI, upper CI.
    std::ostringstream est_dat, lo_dat, hi_dat;
    for (const auto& rl : rls) {
        est_dat << rl.period << " " << rl.estimate << "\n";
        lo_dat  << rl.period << " " << rl.lower    << "\n";
        hi_dat  << rl.period << " " << rl.upper    << "\n";
    }
    est_dat << "e\n";
    lo_dat  << "e\n";
    hi_dat  << "e\n";

    gp("plot '-' using 1:2 with lines lw 2 lc rgb '#2166AC' title 'Estimate', "
       "     '-' using 1:2 with lines lw 1 lc rgb '#92C5DE' dt 2 title 'Lower CI', "
       "     '-' using 1:2 with lines lw 1 lc rgb '#92C5DE' dt 2 title 'Upper CI'");
    gp(est_dat.str());
    gp(lo_dat.str());
    gp(hi_dat.str());

    LOKI_INFO("EVT: return levels plot -> " + outFile);
}

// =============================================================================
//  Exceedances diagnostic plot
// =============================================================================

void PlotEvt::_plotExceedances(const EvtResult& r,
                                 const std::string& base) const
{
    const std::string fmt = m_cfg.plots.outputFormat.empty()
                          ? "png" : m_cfg.plots.outputFormat;
    const std::string outFile = _outPath("evt_" + base + "_exceedances." + fmt);

    const auto& exc = r.exceedances;
    const int n = static_cast<int>(exc.size());
    if (n == 0) return;

    std::vector<double> sorted = exc;
    std::sort(sorted.begin(), sorted.end());

    Gnuplot gp;
    gp("set terminal pngcairo noenhanced font 'Sans,12' size 900,500");
    gp("set output '" + outFile + "'");
    gp("set title 'Exceedances above Threshold (n=" + std::to_string(n) + ")'");
    gp("set xlabel 'Exceedance value'");
    gp("set ylabel 'Empirical CDF'");
    gp("set grid");
    gp("set key top left");

    // Build empirical CDF and fitted GPD CDF.
    std::ostringstream emp_dat, fit_dat;
    for (int i = 0; i < n; ++i) {
        const double p = static_cast<double>(i + 1) / static_cast<double>(n);
        emp_dat << sorted[static_cast<std::size_t>(i)] << " " << p << "\n";
        const double fc = Gpd::cdf(sorted[static_cast<std::size_t>(i)],
                                    r.gpd.xi, r.gpd.sigma);
        fit_dat << sorted[static_cast<std::size_t>(i)] << " " << fc << "\n";
    }
    emp_dat << "e\n";
    fit_dat << "e\n";

    gp("plot '-' using 1:2 with points pt 7 ps 0.5 lc rgb '#2166AC' title 'Empirical CDF', "
       "     '-' using 1:2 with lines lw 2 lc rgb '#D73027' title 'Fitted GPD'");
    gp(emp_dat.str());
    gp(fit_dat.str());

    LOKI_INFO("EVT: exceedances plot -> " + outFile);
}

// =============================================================================
//  GPD Q-Q plot
// =============================================================================

void PlotEvt::_plotGpdFit(const EvtResult& r,
                            const std::string& base) const
{
    const std::string fmt = m_cfg.plots.outputFormat.empty()
                          ? "png" : m_cfg.plots.outputFormat;
    const std::string outFile = _outPath("evt_" + base + "_gpd_qq." + fmt);

    const auto& exc = r.exceedances;
    const int n = static_cast<int>(exc.size());
    if (n == 0) return;

    std::vector<double> sorted = exc;
    std::sort(sorted.begin(), sorted.end());

    double xMin = sorted.front();
    double xMax = sorted.back();

    Gnuplot gp;
    gp("set terminal pngcairo noenhanced font 'Sans,12' size 700,700");
    gp("set output '" + outFile + "'");
    gp("set title 'GPD Q-Q Plot'");
    gp("set xlabel 'Theoretical GPD Quantile'");
    gp("set ylabel 'Empirical Quantile'");
    gp("set grid");
    gp("set key top left");

    // Q-Q data: theoretical = GPD quantile at plotting position (i-0.5)/n.
    std::ostringstream dat;
    for (int i = 0; i < n; ++i) {
        const double p    = (static_cast<double>(i) + 0.5) / static_cast<double>(n);
        const double theo = Gpd::quantile(p, r.gpd.xi, r.gpd.sigma);
        dat << theo << " " << sorted[static_cast<std::size_t>(i)] << "\n";
    }
    dat << "e\n";

    // Reference line y = x.
    const double refMax = std::max(xMax, sorted.back()) * 1.1;
    gp("set arrow 1 from 0," + std::to_string(xMin) + " to "
       + std::to_string(refMax) + "," + std::to_string(refMax)
       + " nohead lc rgb '#999999' lw 1 dt 2");

    gp("plot '-' using 1:2 with points pt 7 ps 0.6 lc rgb '#2166AC' title 'Q-Q'");
    gp(dat.str());

    LOKI_INFO("EVT: GPD Q-Q plot -> " + outFile);
}

} // namespace loki::evt
