#include <loki/simulate/plotSimulate.hpp>

#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/io/gnuplot.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <string>

using namespace loki;

namespace loki::simulate {

// -----------------------------------------------------------------------------
//  Helpers
// -----------------------------------------------------------------------------

std::string PlotSimulate::fwdSlash(const std::string& p)
{
    std::string r = p;
    for (char& c : r) if (c == '\\') c = '/';
    return r;
}

std::string PlotSimulate::_outPath(const SimulationResult& result,
                                   const std::string& plotType) const
{
    const std::string fname =
        "simulate_" + result.datasetName + "_" + result.componentName
        + "_" + plotType + "." + m_cfg.plots.outputFormat;
    return fwdSlash((m_cfg.imgDir / fname).string());
}

// -----------------------------------------------------------------------------
//  Constructor
// -----------------------------------------------------------------------------

PlotSimulate::PlotSimulate(const AppConfig& cfg) : m_cfg(cfg) {}

// -----------------------------------------------------------------------------
//  Public: plotAll
// -----------------------------------------------------------------------------

void PlotSimulate::plotAll(const SimulationResult& result)
{
    if (m_cfg.plots.simulateOverlay)   _plotOverlay(result);
    if (m_cfg.plots.simulateEnvelope)  _plotEnvelope(result);
    if (m_cfg.plots.simulateBootstrapDist
        && result.mode == "bootstrap"
        && !result.parameterCIs.empty())
    {
        _plotBootstrapDist(result);
    }
    if (m_cfg.plots.simulateAcfComparison
        && result.mode == "bootstrap"
        && !result.original.empty())
    {
        _plotAcfComparison(result);
    }
}

// -----------------------------------------------------------------------------
//  Private: _plotOverlay
// -----------------------------------------------------------------------------

void PlotSimulate::_plotOverlay(const SimulationResult& result)
{
    if (result.simulations.empty()) return;

    const std::string outFile = _outPath(result, "overlay");
    const std::string title =
        "Simulation overlay -- " + result.datasetName + " / " + result.componentName;

    const int nDraw = std::min(
        static_cast<int>(result.simulations.size()),
        MAX_OVERLAY_SERIES);
    const std::size_t n = result.simulations[0].size();

    try {
        Gnuplot gp;
        gp("set terminal pngcairo noenhanced font 'Sans,12' size 1200,500");
        gp("set output '" + outFile + "'");
        gp("set title '" + title + "'");
        gp("set xlabel 'Sample'");
        gp("set ylabel 'Value'");
        gp("set grid");
        gp("set xrange [0:" + std::to_string(n - 1) + "]");

        // Build plot command.
        std::string cmd = "plot";
        // Replicas (light blue, thin).
        for (int s = 0; s < nDraw; ++s) {
            cmd += " '-' using 1:2 with lines lc rgb '#AAAAEE' lw 1 notitle,";
        }
        // Median envelope.
        cmd += " '-' using 1:2 with lines lc rgb '#0000CC' lw 2 title 'Median'";
        // Original series if available.
        if (!result.original.empty()) {
            cmd += ", '-' using 1:2 with lines lc rgb '#CC0000' lw 2 title 'Original'";
        }
        gp(cmd);

        // Inline data -- replicas.
        for (int s = 0; s < nDraw; ++s) {
            const auto& sim = result.simulations[static_cast<std::size_t>(s)];
            for (std::size_t t = 0; t < n; ++t) {
                gp(std::to_string(t) + " " + std::to_string(sim[t]));
            }
            gp("e");
        }
        // Median.
        for (std::size_t t = 0; t < n; ++t) {
            gp(std::to_string(t) + " " + std::to_string(result.env50[t]));
        }
        gp("e");
        // Original.
        if (!result.original.empty()) {
            const std::size_t nOrig = std::min(result.original.size(), n);
            for (std::size_t t = 0; t < nOrig; ++t) {
                gp(std::to_string(t) + " " + std::to_string(result.original[t]));
            }
            gp("e");
        }

        LOKI_INFO("PlotSimulate: overlay -> '" + outFile + "'.");
    } catch (const std::exception& ex) {
        LOKI_WARNING("PlotSimulate: overlay plot failed: " + std::string(ex.what()));
    }
}

// -----------------------------------------------------------------------------
//  Private: _plotEnvelope
// -----------------------------------------------------------------------------

void PlotSimulate::_plotEnvelope(const SimulationResult& result)
{
    if (result.env50.empty()) return;

    const std::string outFile = _outPath(result, "envelope");
    const std::string title =
        "Simulation envelope -- " + result.datasetName + " / " + result.componentName;
    const std::size_t n = result.env50.size();

    try {
        Gnuplot gp;
        gp("set terminal pngcairo noenhanced font 'Sans,12' size 1200,500");
        gp("set output '" + outFile + "'");
        gp("set title '" + title + "'");
        gp("set xlabel 'Sample'");
        gp("set ylabel 'Value'");
        gp("set grid");
        gp("set xrange [0:" + std::to_string(n - 1) + "]");

        std::string cmd = "plot";
        // 5-95 band.
        cmd += " '-' using 1:2:3 with filledcurves lc rgb '#DDDDFF' title '5-95%',";
        // 25-75 band.
        cmd += " '-' using 1:2:3 with filledcurves lc rgb '#AAAAEE' title '25-75%',";
        // Median.
        cmd += " '-' using 1:2 with lines lc rgb '#0000CC' lw 2 title 'Median'";
        // Original.
        if (!result.original.empty()) {
            cmd += ", '-' using 1:2 with lines lc rgb '#CC0000' lw 2 title 'Original'";
        }
        gp(cmd);

        // 5-95 band data.
        for (std::size_t t = 0; t < n; ++t) {
            gp(std::to_string(t) + " " + std::to_string(result.env05[t])
               + " " + std::to_string(result.env95[t]));
        }
        gp("e");
        // 25-75 band data.
        for (std::size_t t = 0; t < n; ++t) {
            gp(std::to_string(t) + " " + std::to_string(result.env25[t])
               + " " + std::to_string(result.env75[t]));
        }
        gp("e");
        // Median.
        for (std::size_t t = 0; t < n; ++t) {
            gp(std::to_string(t) + " " + std::to_string(result.env50[t]));
        }
        gp("e");
        // Original.
        if (!result.original.empty()) {
            const std::size_t nOrig = std::min(result.original.size(), n);
            for (std::size_t t = 0; t < nOrig; ++t) {
                gp(std::to_string(t) + " " + std::to_string(result.original[t]));
            }
            gp("e");
        }

        LOKI_INFO("PlotSimulate: envelope -> '" + outFile + "'.");
    } catch (const std::exception& ex) {
        LOKI_WARNING("PlotSimulate: envelope plot failed: " + std::string(ex.what()));
    }
}

// -----------------------------------------------------------------------------
//  Private: _plotBootstrapDist
// -----------------------------------------------------------------------------

void PlotSimulate::_plotBootstrapDist(const SimulationResult& result)
{
    // Report CI for the "mean" parameter (first in list by convention).
    const ParameterCI& pci = result.parameterCIs[0];

    const std::string outFile = _outPath(result, "bootstrap_dist");
    const std::string title =
        "Bootstrap CI -- " + pci.name + " -- "
        + result.datasetName + " / " + result.componentName;

    // Collect the per-simulation means for the histogram.
    const std::size_t nSim = result.simulations.size();
    std::vector<double> simMeans(nSim);
    for (std::size_t s = 0; s < nSim; ++s) {
        const auto& v = result.simulations[s];
        if (v.empty()) { simMeans[s] = 0.0; continue; }
        simMeans[s] = std::accumulate(v.begin(), v.end(), 0.0)
                      / static_cast<double>(v.size());
    }

    try {
        Gnuplot gp;
        gp("set terminal pngcairo noenhanced font 'Sans,12' size 900,500");
        gp("set output '" + outFile + "'");
        gp("set title '" + title + "'");
        gp("set xlabel '" + pci.name + "'");
        gp("set ylabel 'Frequency'");
        gp("set grid");
        gp("set style data histogram");
        gp("set style fill solid 0.6");

        const int nBins = std::max(10, static_cast<int>(std::sqrt(
            static_cast<double>(nSim))));
        const double dMin = *std::min_element(simMeans.begin(), simMeans.end());
        const double dMax = *std::max_element(simMeans.begin(), simMeans.end());
        const double binW = (dMax > dMin) ? (dMax - dMin) / static_cast<double>(nBins) : 1.0;

        gp("binwidth = " + std::to_string(binW));
        gp("set boxwidth binwidth");
        gp("bin(x) = binwidth * floor(x / binwidth) + binwidth / 2.0");

        // CI bounds as vertical lines.
        const int arrowBase = 1;
        gp("set arrow " + std::to_string(arrowBase)
           + " from " + std::to_string(pci.lower)
           + ",0 to " + std::to_string(pci.lower)
           + ",graph 0.9 nohead lc rgb '#0000CC' lw 2");
        gp("set arrow " + std::to_string(arrowBase + 1)
           + " from " + std::to_string(pci.upper)
           + ",0 to " + std::to_string(pci.upper)
           + ",graph 0.9 nohead lc rgb '#0000CC' lw 2");
        gp("set arrow " + std::to_string(arrowBase + 2)
           + " from " + std::to_string(pci.estimate)
           + ",0 to " + std::to_string(pci.estimate)
           + ",graph 0.9 nohead lc rgb '#CC0000' lw 2");

        gp("plot '-' using (bin($1)):(1.0) smooth frequency "
           "with boxes lc rgb '#6688CC' title 'bootstrap dist'");

        for (double v : simMeans) {
            gp(std::to_string(v));
        }
        gp("e");

        LOKI_INFO("PlotSimulate: bootstrap_dist -> '" + outFile + "'.");
    } catch (const std::exception& ex) {
        LOKI_WARNING("PlotSimulate: bootstrap_dist plot failed: "
                     + std::string(ex.what()));
    }
}

// -----------------------------------------------------------------------------
//  Private: _plotAcfComparison
// -----------------------------------------------------------------------------

void PlotSimulate::_plotAcfComparison(const SimulationResult& result)
{
    const std::size_t n = result.original.size();
    if (n < 10) return;

    const int maxLag = std::min(40, static_cast<int>(n) / 4);
    const std::vector<double> acfOrig = _computeAcf(result.original, maxLag);

    // Mean ACF over simulations.
    std::vector<double> acfMean(static_cast<std::size_t>(maxLag), 0.0);
    int simCount = 0;
    for (const auto& sim : result.simulations) {
        if (sim.size() < 10) continue;
        const auto acfSim = _computeAcf(sim, maxLag);
        for (int lag = 0; lag < maxLag; ++lag) {
            acfMean[static_cast<std::size_t>(lag)] += acfSim[static_cast<std::size_t>(lag)];
        }
        ++simCount;
    }
    if (simCount > 0) {
        for (auto& v : acfMean) v /= static_cast<double>(simCount);
    }

    const std::string outFile = _outPath(result, "acf_comparison");
    const std::string title =
        "ACF comparison -- " + result.datasetName + " / " + result.componentName;
    const double confBound = 1.96 / std::sqrt(static_cast<double>(n));

    try {
        Gnuplot gp;
        gp("set terminal pngcairo noenhanced font 'Sans,12' size 900,500");
        gp("set output '" + outFile + "'");
        gp("set title '" + title + "'");
        gp("set xlabel 'Lag'");
        gp("set ylabel 'ACF'");
        gp("set grid");
        gp("set xrange [1:" + std::to_string(maxLag) + "]");
        gp("set yrange [-1.1:1.1]");

        // Confidence bounds.
        gp("set arrow 1 from 1," + std::to_string(confBound)
           + " to " + std::to_string(maxLag) + "," + std::to_string(confBound)
           + " nohead lc rgb '#AAAAAA' dt 2 lw 1");
        gp("set arrow 2 from 1," + std::to_string(-confBound)
           + " to " + std::to_string(maxLag) + "," + std::to_string(-confBound)
           + " nohead lc rgb '#AAAAAA' dt 2 lw 1");

        gp("plot '-' using 1:2 with impulses lc rgb '#CC0000' lw 2 title 'Original',"
           " '-' using 1:2 with linespoints lc rgb '#0000CC' lw 1 pt 7 ps 0.5 title 'Sim mean'");

        for (int lag = 1; lag <= maxLag; ++lag) {
            gp(std::to_string(lag) + " "
               + std::to_string(acfOrig[static_cast<std::size_t>(lag - 1)]));
        }
        gp("e");
        for (int lag = 1; lag <= maxLag; ++lag) {
            gp(std::to_string(lag) + " "
               + std::to_string(acfMean[static_cast<std::size_t>(lag - 1)]));
        }
        gp("e");

        LOKI_INFO("PlotSimulate: acf_comparison -> '" + outFile + "'.");
    } catch (const std::exception& ex) {
        LOKI_WARNING("PlotSimulate: acf_comparison plot failed: "
                     + std::string(ex.what()));
    }
}

// -----------------------------------------------------------------------------
//  Private: _computeAcf
// -----------------------------------------------------------------------------

std::vector<double> PlotSimulate::_computeAcf(const std::vector<double>& v, int maxLag)
{
    const std::size_t n = v.size();
    const double mean = std::accumulate(v.begin(), v.end(), 0.0)
                        / static_cast<double>(n);

    double var = 0.0;
    for (double x : v) {
        const double d = x - mean;
        var += d * d;
    }
    var /= static_cast<double>(n);

    std::vector<double> acf(static_cast<std::size_t>(maxLag), 0.0);
    if (var < 1e-15) return acf;

    for (int lag = 1; lag <= maxLag; ++lag) {
        double cov = 0.0;
        for (std::size_t i = 0; i + static_cast<std::size_t>(lag) < n; ++i) {
            cov += (v[i] - mean) * (v[i + static_cast<std::size_t>(lag)] - mean);
        }
        cov /= static_cast<double>(n);
        acf[static_cast<std::size_t>(lag - 1)] = cov / var;
    }
    return acf;
}

} // namespace loki::simulate
