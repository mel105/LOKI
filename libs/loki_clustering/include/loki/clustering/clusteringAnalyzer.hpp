#pragma once

#include <loki/core/config.hpp>
#include <loki/clustering/clusteringResult.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <Eigen/Dense>

#include <string>
#include <vector>

namespace loki::clustering {

/**
 * @brief Orchestrates the full clustering pipeline for a single TimeSeries.
 *
 * Pipeline steps:
 *   1. Gap fill (linear, optional).
 *   2. Build feature matrix from configured features (value, derivative, etc.).
 *   3. Z-score normalise each feature column.
 *   4. Assign edge points (missing derivatives) to nearest centroid.
 *   5. Run k-means or DBSCAN.
 *   6. Assign label names (user-defined or auto-generated).
 *   7. Build ClusteringResult.
 *   8. Write protocol and flags CSV.
 *   9. Call PlotClustering.
 *
 * No data is modified. The original series is unchanged.
 *
 * Outputs:
 *   Protocol : OUTPUT/PROTOCOLS/clustering_[dataset]_[component]_protocol.txt
 *   CSV      : OUTPUT/CSV/clustering_[dataset]_[component]_labels.csv
 */
class ClusteringAnalyzer {
public:

    /**
     * @brief Constructs a ClusteringAnalyzer bound to the given config.
     */
    explicit ClusteringAnalyzer(const AppConfig& cfg);

    /**
     * @brief Runs the full clustering pipeline on a single series.
     *
     * @param series      Input time series. Not modified.
     * @param datasetName Stem of the input file (used in output filenames).
     * @return Populated ClusteringResult.
     * @throws DataException if the series has fewer valid observations than kMin.
     */
    ClusteringResult run(const TimeSeries& series, const std::string& datasetName);

private:

    AppConfig m_cfg;

    // -------------------------------------------------------------------------
    // Feature engineering
    // -------------------------------------------------------------------------

    /**
     * @brief Builds the (n x d) feature matrix from valid series observations.
     *
     * Edge points that cannot have a derivative (first 1-2 observations) receive
     * NaN in derivative columns. The caller handles these via _handleEdgePoints().
     *
     * @param series       Input series.
     * @param validIdx     Output: indices into series of valid (non-NaN) epochs.
     * @param featureNames Output: names of columns in the returned matrix.
     * @return Feature matrix (rows = valid epochs, cols = features), unnormalised.
     */
    Eigen::MatrixXd _buildFeatureMatrix(const TimeSeries& series,
                                        std::vector<std::size_t>& validIdx,
                                        std::vector<std::string>& featureNames) const;

    /**
     * @brief Z-score normalises each column of X in place.
     *
     * Columns with zero standard deviation are left unchanged (all values
     * identical -- no normalisation needed).
     *
     * @param X      Input/output matrix (modified in place).
     * @param mean   Output: per-column mean (for de-normalisation).
     * @param stddev Output: per-column standard deviation.
     */
    void _zScoreNormalize(Eigen::MatrixXd& X,
                          Eigen::VectorXd& mean,
                          Eigen::VectorXd& stddev) const;

    /**
     * @brief Assigns edge rows (NaN derivative positions) to nearest centroid.
     *
     * Only the non-NaN feature columns are used for distance computation.
     * Modifies labels in place.
     *
     * @param X        Full feature matrix (may contain NaN in derivative cols).
     * @param labels   Label vector to update.
     * @param centroids Centroid matrix (k x d).
     * @param nDerivCols Number of derivative columns (last columns of X).
     */
    void _handleEdgePoints(const Eigen::MatrixXd& X,
                           std::vector<int>&       labels,
                           const Eigen::MatrixXd&  centroids,
                           int                     nDerivCols) const;

    // -------------------------------------------------------------------------
    // Label naming
    // -------------------------------------------------------------------------

    /**
     * @brief Assigns human-readable names to clusters.
     *
     * If user labels are provided (cfg.clustering.kmeans.labels), they are
     * assigned in order of ascending centroid value (first feature). If the
     * count does not match nClusters, auto names are used with a warning.
     *
     * For DBSCAN, noise (label=-1) always gets name "noise".
     *
     * @param nClusters  Number of clusters (excluding noise).
     * @param centroids  Centroid matrix in original units (for sorting).
     * @param method     "kmeans" or "dbscan".
     * @return Map from label int -> name string.
     */
    std::vector<std::string> _assignLabelNames(int                    nClusters,
                                               const Eigen::MatrixXd& centroids,
                                               const std::string&     method) const;

    // -------------------------------------------------------------------------
    // Output
    // -------------------------------------------------------------------------

    void _writeProtocol (const ClusteringResult& result) const;

    /**
     * @brief Writes the labels CSV.
     *
     * Format: mjd ; utc ; value ; label ; label_name ; outlier_flag
     * One row per epoch in the original series (including NaN epochs).
     * NaN epochs receive label = -1, label_name = "invalid", outlier_flag = 0.
     */
    void _writeLabelsCsv(const TimeSeries&       series,
                         const ClusteringResult& result,
                         const std::vector<std::size_t>& validIdx) const; // validIdx unused -- kept for API consistency

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    static std::vector<double> _extractValues(const TimeSeries& series);
};

} // namespace loki::clustering