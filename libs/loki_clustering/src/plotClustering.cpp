#include <loki/clustering/plotClustering.hpp>

#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/io/gnuplot.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>

using namespace loki;
using namespace loki::clustering;

// Palette: up to 10 distinct colours for clusters.
static const char* const CLUSTER_COLOURS[] = {
    "#4C72B0", // blue
    "#DD8452", // orange
    "#55A868", // green
    "#C44E52", // red
    "#8172B3", // purple
    "#937860", // brown
    "#DA8BC3", // pink
    "#8C8C8C", // grey
    "#CCB974", // yellow
    "#64B5CD"  // cyan
};
static constexpr int N_COLOURS = 10;

// =============================================================================
//  Construction
// =============================================================================

PlotClustering::PlotClustering(const AppConfig& cfg)
    : m_cfg(cfg)
{}

// =============================================================================
//  plotAll
// =============================================================================

void PlotClustering::plotAll(const TimeSeries&       series,
                              const ClusteringResult& result) const
{
    if (m_cfg.plots.clusteringLabels) {
        try { plotLabels(series, result); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotClustering: labels plot failed: " + std::string(ex.what()));
        }
    }
    if (m_cfg.plots.clusteringScatter && result.featureNames.size() >= 2) {
        try { plotScatter(result); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotClustering: scatter plot failed: " + std::string(ex.what()));
        }
    }
    if (m_cfg.plots.clusteringSilhouette && result.method == "kmeans") {
        try { plotSilhouette(result); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotClustering: silhouette plot failed: " + std::string(ex.what()));
        }
    }
}

// =============================================================================
//  plotLabels
// =============================================================================

void PlotClustering::plotLabels(const TimeSeries&       series,
                                 const ClusteringResult& result) const
{
    if (result.points.empty()) return;

    const std::string stem    = _stem(result, "labels");
    const auto        outPath = _outPath(stem);
    const int         nC      = result.nClusters;

    // Build lookup: series index -> label
    std::vector<int> labelVec(series.size(), -2); // -2 = NaN/invalid
    for (const auto& pt : result.points) {
        labelVec[pt.index] = pt.label;
    }

    // Use relative time in seconds when span < 1 day (sensor data).
    // Use MJD otherwise (climatological data).
    const double spanDays = (series.size() > 1)
        ? series[series.size() - 1].time.mjd() - series[0].time.mjd()
        : 1.0;
    const bool useRelTime = (spanDays < 1.0);
    const double t0 = series[0].time.mjd();
    const std::string xLabel = useRelTime ? "Time (s)" : "MJD";

    auto xVal = [&](std::size_t i) -> double {
        if (useRelTime) return (series[i].time.mjd() - t0) * 86400.0;
        return series[i].time.mjd();
    };

    Gnuplot gp;
    gp(_terminal(1400, 500));
    gp("set output '" + _fwdSlash(outPath) + "'");
    gp("set title 'Clustering: " + result.datasetName + " / "
       + result.componentName + " [" + result.method + "]' noenhanced");
    gp("set xlabel '" + xLabel + "'");
    gp("set ylabel '" + result.componentName + "'");
    gp("set key outside right top");

    // Build one inline dataset per cluster + noise.
    // Plot the series line in grey first, then overlay coloured points.

    // Grey series line.
    std::ostringstream lineData;
    for (std::size_t i = 0; i < series.size(); ++i) {
        if (!std::isnan(series[i].value)) {
            lineData << xVal(i) << " " << series[i].value << "\n";
        }
    }
    lineData << "e\n";

    gp("set style data linespoints");

    // First plot: grey line.
    std::string plotCmd = "plot '-' with lines lc rgb '#CCCCCC' lw 1 notitle";

    // Per-cluster coloured points.
    for (int c = 0; c < nC; ++c) {
        const std::string& name = result.clusters[static_cast<std::size_t>(c)].name;
        plotCmd += ", '-' with points pt 7 ps 0.6 lc rgb '"
                   + std::string(_colour(c, nC)) + "' title '" + name + "'";
    }

    // Noise points (DBSCAN) as red circles.
    if (result.nOutliers > 0) {
        plotCmd += ", '-' with points pt 6 ps 1.2 lc rgb '#CC0000' title 'noise'";
    }

    gp(plotCmd);
    gp(lineData.str()); // grey line data

    // Per-cluster data.
    for (int c = 0; c < nC; ++c) {
        std::ostringstream clData;
        for (const auto& pt : result.points) {
            if (pt.label == c) {
                clData << xVal(pt.index) << " "
                       << series[pt.index].value << "\n";
            }
        }
        clData << "e\n";
        gp(clData.str());
    }

    // Noise data.
    if (result.nOutliers > 0) {
        std::ostringstream noiseData;
        for (const auto& pt : result.points) {
            if (pt.label == -1) {
                noiseData << xVal(pt.index) << " "
                          << series[pt.index].value << "\n";
            }
        }
        noiseData << "e\n";
        gp(noiseData.str());
    }

    LOKI_INFO("PlotClustering: labels plot -> " + outPath.string());
}

// =============================================================================
//  plotScatter
// =============================================================================

void PlotClustering::plotScatter(const ClusteringResult& result) const
{
    if (result.featureNames.size() < 2 || result.points.empty()) return;

    const std::string stem    = _stem(result, "scatter");
    const auto        outPath = _outPath(stem);
    const int         nC      = result.nClusters;

    Gnuplot gp;
    gp(_terminal(800, 700));
    gp("set output '" + _fwdSlash(outPath) + "'");
    gp("set title 'Feature Scatter: " + result.datasetName + " / "
       + result.componentName + "' noenhanced");
    gp("set xlabel '" + result.featureNames[0] + " (normalised)'");
    gp("set ylabel '" + result.featureNames[1] + " (normalised)'");
    gp("set key outside right top");

    // Build plot command.
    std::string plotCmd = "plot";
    for (int c = 0; c < nC; ++c) {
        if (c > 0) plotCmd += ",";
        const std::string& name = result.clusters[static_cast<std::size_t>(c)].name;
        plotCmd += " '-' with points pt 7 ps 0.8 lc rgb '"
                   + std::string(_colour(c, nC)) + "' title '" + name + "'";
    }
    if (result.nOutliers > 0) {
        plotCmd += ", '-' with points pt 2 ps 1.0 lc rgb '#CC0000' title 'noise'";
    }

    gp(plotCmd);

    for (int c = 0; c < nC; ++c) {
        std::ostringstream data;
        for (const auto& pt : result.points) {
            if (pt.label == c && pt.features.size() >= 2
                && !std::isnan(pt.features[0]) && !std::isnan(pt.features[1])) {
                data << pt.features[0] << " " << pt.features[1] << "\n";
            }
        }
        data << "e\n";
        gp(data.str());
    }

    if (result.nOutliers > 0) {
        std::ostringstream noiseData;
        for (const auto& pt : result.points) {
            if (pt.label == -1 && pt.features.size() >= 2
                && !std::isnan(pt.features[0]) && !std::isnan(pt.features[1])) {
                noiseData << pt.features[0] << " " << pt.features[1] << "\n";
            }
        }
        noiseData << "e\n";
        gp(noiseData.str());
    }

    LOKI_INFO("PlotClustering: scatter plot -> " + outPath.string());
}

// =============================================================================
//  plotSilhouette
// =============================================================================

void PlotClustering::plotSilhouette(const ClusteringResult& result) const
{
    if (result.method != "kmeans" || result.clusters.empty()) return;

    const std::string stem    = _stem(result, "silhouette");
    const auto        outPath = _outPath(stem);

    Gnuplot gp;
    gp(_terminal(700, 500));
    gp("set output '" + _fwdSlash(outPath) + "'");
    gp("set title 'Silhouette: " + result.datasetName + " / "
       + result.componentName + "' noenhanced");
    gp("set xlabel 'Mean silhouette coefficient'");
    gp("set ylabel 'Cluster'");
    gp("set yrange [-0.5:" + std::to_string(result.nClusters) + "]");
    gp("set xrange [-1:1]");
    gp("set xzeroaxis lw 1");
    gp("set style fill solid 0.8 noborder");
    gp("set boxwidth 0.6");

    // Global mean line -- clip to [-1,1] range.
    const double sClipped = std::max(-1.0, std::min(1.0, result.silhouette));
    gp("set arrow 1 from " + std::to_string(sClipped)
       + ",-0.5 to " + std::to_string(sClipped)
       + "," + std::to_string(result.nClusters)
       + " nohead lc rgb '#333333' lw 2 dt 2");

    // One bar per cluster: x=silhouette, y=cluster index, colour index.
    // Per-cluster silhouette from ClusterInfo.
    gp("plot '-' using 1:2:3 with boxes lc variable notitle, \\");
    gp("     '-' using 1:2:(sprintf('%s', stringcolumn(3))) with labels offset 1,0 notitle");

    std::ostringstream data;
    for (int c = 0; c < result.nClusters; ++c) {
        const auto& ci = result.clusters[static_cast<std::size_t>(c)];
        // Use global silhouette as fallback if per-cluster not computed.
        const double s = (ci.silhouette != 0.0) ? ci.silhouette : result.silhouette;
        data << s << " " << c << " " << c << "\n";
    }
    data << "e\n";
    gp(data.str());

    // Label data (cluster names).
    std::ostringstream labelData;
    for (int c = 0; c < result.nClusters; ++c) {
        const auto& ci = result.clusters[static_cast<std::size_t>(c)];
        const double s = (ci.silhouette != 0.0) ? ci.silhouette : result.silhouette;
        labelData << s << " " << c << " " << ci.name << "\n";
    }
    labelData << "e\n";
    gp(labelData.str());

    LOKI_INFO("PlotClustering: silhouette plot -> " + outPath.string());
}

// =============================================================================
//  Helpers
// =============================================================================

std::string PlotClustering::_stem(const ClusteringResult& result,
                                   const std::string& plotType) const
{
    return "clustering_" + result.datasetName + "_"
           + result.componentName + "_" + plotType;
}

std::filesystem::path PlotClustering::_outPath(const std::string& stem) const
{
    return m_cfg.imgDir / (stem + "." + m_cfg.plots.outputFormat);
}

std::string PlotClustering::_terminal(int w, int h) const
{
    return "set terminal pngcairo noenhanced font 'Sans,12' size "
           + std::to_string(w) + "," + std::to_string(h);
}

std::string PlotClustering::_fwdSlash(const std::filesystem::path& p)
{
    std::string s = p.string();
    std::replace(s.begin(), s.end(), '\\', '/');
    return s;
}

std::string PlotClustering::_colour(int label, int nClusters)
{
    (void)nClusters;
    return CLUSTER_COLOURS[label % N_COLOURS];
}
