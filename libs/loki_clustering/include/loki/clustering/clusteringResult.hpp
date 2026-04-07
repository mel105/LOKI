#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace loki::clustering {

/**
 * @brief Feature vector and cluster assignment for a single time series point.
 */
struct ClusterPoint {
    std::size_t          index;        ///< Index in the original TimeSeries.
    std::vector<double>  features;     ///< Z-score normalised feature vector.
    int                  label{-1};    ///< Cluster label. -1 = DBSCAN noise / unassigned.
    std::string          labelName;    ///< Human-readable label ("cluster_0", "low", ...).
    bool                 isOutlier{false}; ///< True for DBSCAN noise when outlier.enabled.
};

/**
 * @brief Summary statistics for a single cluster.
 */
struct ClusterInfo {
    int                  label{0};
    std::string          name;
    std::size_t          count{0};
    std::vector<double>  centroid;     ///< Centroid in normalised feature space.
    std::vector<double>  centroidRaw;  ///< Centroid in original (un-normalised) units.
    double               inertia{0.0}; ///< Sum of squared distances to centroid.
    double               silhouette{0.0}; ///< Per-cluster mean silhouette coefficient.
};

/**
 * @brief Aggregated result of one clustering run on a single TimeSeries.
 *
 * Produced by ClusteringAnalyzer::run(). Contains per-point assignments,
 * per-cluster summaries, and global quality metrics.
 *
 * For DBSCAN, noise points have label = -1. When ClusteringOutlierConfig::enabled
 * is true, noise points are also flagged in outlierIndices and in the CSV output.
 *
 * For k-means, inertia and silhouetteScore are always populated.
 * For DBSCAN, inertia is NaN and silhouetteScore is computed only when
 * at least 2 non-noise clusters exist.
 */
struct ClusteringResult {
    std::string componentName;
    std::string datasetName;
    std::string method;                     ///< "kmeans" | "dbscan"

    std::vector<std::string> featureNames;  ///< Names of features used (e.g. "value", "abs_derivative").

    std::vector<ClusterPoint> points;       ///< One entry per valid (non-NaN) epoch.
    std::vector<ClusterInfo>  clusters;     ///< One entry per cluster (excluding noise).

    int    nClusters    {0};                ///< Number of clusters (excluding DBSCAN noise).
    int    nOutliers    {0};                ///< DBSCAN noise points; always 0 for k-means.
    double silhouette   {0.0};             ///< Global mean silhouette coefficient.
    double inertia      {0.0};             ///< Total within-cluster sum of squares (k-means).
    int    kSelected    {0};               ///< Auto-selected k (0 if k was specified manually).

    /// Indices into the original TimeSeries of DBSCAN noise points flagged as outliers.
    /// Empty if method is k-means or outlier.enabled is false.
    std::vector<std::size_t> outlierIndices;
};

} // namespace loki::clustering
