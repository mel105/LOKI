#pragma once

#include <loki/core/config.hpp>

#include <Eigen/Dense>

#include <cstddef>
#include <vector>

namespace loki::clustering {

/**
 * @brief k-means clustering with k-means++ initialisation and auto k selection.
 *
 * Features are assumed to be pre-normalised (z-score). When k = 0, the
 * algorithm evaluates k in [kMin, kMax] and selects the value with the
 * highest global silhouette coefficient.
 *
 * Multiple restarts (nInit) are performed and the run with the lowest total
 * inertia is retained.
 */
class KMeans {
public:

    /**
     * @brief Constructs a KMeans instance from the KMeansClusteringConfig.
     * @throws ConfigException if kMin < 2, kMax < kMin, maxIter < 1, or nInit < 1.
     */
    explicit KMeans(const KMeansClusteringConfig& cfg);

    /**
     * @brief Fits the model to the feature matrix X.
     *
     * Rows of X are observations; columns are features. X must be z-score
     * normalised by the caller (ClusteringAnalyzer handles this).
     *
     * @param X Feature matrix (n x d).
     * @return Label vector of length n. Values in [0, k-1].
     * @throws DataException if X has fewer rows than kMin.
     */
    std::vector<int> fit(const Eigen::MatrixXd& X);

    /// Centroid matrix (k x d) after fit(). Each row is one centroid.
    [[nodiscard]] const Eigen::MatrixXd& centroids() const noexcept;

    /// Global silhouette coefficient after fit().
    [[nodiscard]] double silhouetteScore() const noexcept;

    /// Total inertia (sum of squared distances to nearest centroid) after fit().
    [[nodiscard]] double inertia() const noexcept;

    /// k that was selected (equals cfg.k if k > 0, otherwise the auto-selected value).
    [[nodiscard]] int kSelected() const noexcept;

private:

    KMeansClusteringConfig m_cfg;
    Eigen::MatrixXd        m_centroids;
    double                 m_silhouette{0.0};
    double                 m_inertia{0.0};
    int                    m_kSelected{0};

    // -------------------------------------------------------------------------
    // Internal helpers
    // -------------------------------------------------------------------------

    /**
     * @brief Runs k-means for a fixed k with nInit restarts.
     * @return (labels, centroids, inertia) for the best restart.
     */
    std::tuple<std::vector<int>, Eigen::MatrixXd, double>
    _runKMeans(const Eigen::MatrixXd& X, int k) const;

    /**
     * @brief One complete k-means run from a given initialisation.
     * @return (labels, centroids, inertia).
     */
    std::tuple<std::vector<int>, Eigen::MatrixXd, double>
    _iterate(const Eigen::MatrixXd& X, const Eigen::MatrixXd& initCentroids) const;

    /**
     * @brief k-means++ initialisation.
     * Selects k centroids with probability proportional to squared distance
     * from the nearest already-selected centroid.
     */
    Eigen::MatrixXd _initKMeansPlusPlus(const Eigen::MatrixXd& X, int k) const;

    /**
     * @brief Uniform random initialisation (k distinct rows of X).
     */
    Eigen::MatrixXd _initRandom(const Eigen::MatrixXd& X, int k) const;

    /**
     * @brief Assigns each row of X to the nearest centroid row.
     * @return Label vector and total inertia.
     */
    std::pair<std::vector<int>, double>
    _assign(const Eigen::MatrixXd& X, const Eigen::MatrixXd& centroids) const;

    /**
     * @brief Recomputes centroids as the mean of points in each cluster.
     * If a cluster is empty, its centroid is re-initialised to a random point.
     */
    Eigen::MatrixXd _updateCentroids(const Eigen::MatrixXd& X,
                                     const std::vector<int>& labels,
                                     int k) const;

    /**
     * @brief Computes the global silhouette coefficient for a labelling.
     *
     * s(i) = (b(i) - a(i)) / max(a(i), b(i))
     * where a(i) = mean intra-cluster distance, b(i) = mean nearest-cluster distance.
     * Returns the mean of s(i) over all points.
     * Returns 0.0 if k == 1 (silhouette undefined for a single cluster).
     */
    double _silhouette(const Eigen::MatrixXd& X,
                       const std::vector<int>& labels,
                       int k) const;

    /**
     * @brief Auto-selects k in [kMin, kMax] by maximising silhouette score.
     * @return Selected k and corresponding labels, centroids, inertia.
     */
    std::tuple<int, std::vector<int>, Eigen::MatrixXd, double>
    _autoSelectK(const Eigen::MatrixXd& X) const;
};

} // namespace loki::clustering
