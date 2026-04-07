#pragma once

#include <loki/core/config.hpp>
#include <loki/clustering/clusteringResult.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <filesystem>
#include <string>

namespace loki::clustering {

/**
 * @brief Generates gnuplot plots for the loki_clustering pipeline.
 *
 * Plot catalogue (controlled by PlotConfig flags):
 *   plotLabels     (clusteringLabels)     -- time axis coloured by cluster label.
 *   plotScatter    (clusteringScatter)    -- 2-D feature scatter (>= 2 features).
 *   plotSilhouette (clusteringSilhouette) -- silhouette bar chart (k-means only).
 *
 * Naming convention: clustering_[dataset]_[component]_[plottype].[format]
 */
class PlotClustering {
public:

    explicit PlotClustering(const AppConfig& cfg);

    /**
     * @brief Time axis with coloured segments per cluster label.
     *
     * Each epoch is drawn as a vertical bar coloured by its cluster label.
     * DBSCAN noise points are drawn as red circles above the series line.
     * Uses gnuplot inline data.
     */
    void plotLabels(const TimeSeries& series, const ClusteringResult& result) const;

    /**
     * @brief 2-D scatter of feature[0] vs feature[1], coloured by label.
     *
     * Only generated when result.featureNames.size() >= 2.
     * Noise points (label=-1) are plotted as red crosses.
     */
    void plotScatter(const ClusteringResult& result) const;

    /**
     * @brief Silhouette coefficient bar chart (k-means only).
     *
     * One horizontal bar per cluster, coloured by cluster, length = mean
     * silhouette coefficient. Global mean drawn as vertical dashed line.
     * Only generated when method == "kmeans".
     */
    void plotSilhouette(const ClusteringResult& result) const;

    /// Calls all enabled plots based on PlotConfig flags.
    void plotAll(const TimeSeries& series, const ClusteringResult& result) const;

private:

    AppConfig m_cfg;

    [[nodiscard]] std::string             _stem(const ClusteringResult& result,
                                                const std::string& plotType) const;
    [[nodiscard]] std::filesystem::path   _outPath(const std::string& stem) const;
    [[nodiscard]] std::string             _terminal(int w = 1400, int h = 500) const;
    [[nodiscard]] static std::string      _fwdSlash(const std::filesystem::path& p);

    /// Returns a gnuplot colour string for a cluster index (cycles through palette).
    [[nodiscard]] static std::string _colour(int label, int nClusters);
};

} // namespace loki::clustering
