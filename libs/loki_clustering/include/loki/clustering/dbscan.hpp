#pragma once

#include <loki/core/config.hpp>

#include <Eigen/Dense>

#include <cstddef>
#include <vector>

namespace loki::clustering {

/**
 * @brief DBSCAN density-based clustering with automatic eps estimation.
 *
 * When DbscanClusteringConfig::eps == 0.0, the neighbourhood radius is
 * estimated automatically via the k-NN distance elbow method:
 *   1. For each point compute the distance to its minPts-th nearest neighbour.
 *   2. Sort these distances in ascending order.
 *   3. Find the elbow (point of maximum curvature via second derivative).
 *   4. Use the distance at the elbow as eps.
 *
 * Points with fewer than minPts neighbours within eps are classified as noise
 * (label = -1). All other points belong to a cluster in [0, K-1].
 *
 * Features are assumed to be z-score normalised by the caller.
 */
class Dbscan {
public:

    /**
     * @brief Constructs a Dbscan instance from DbscanClusteringConfig.
     * @throws ConfigException if minPts < 2.
     */
    explicit Dbscan(const DbscanClusteringConfig& cfg);

    /**
     * @brief Fits DBSCAN to the feature matrix X.
     *
     * @param X Feature matrix (n x d), z-score normalised.
     * @return Label vector of length n. -1 = noise, 0..K-1 = cluster index.
     * @throws DataException if X has fewer than minPts rows.
     */
    std::vector<int> fit(const Eigen::MatrixXd& X);

    /// Number of clusters found (excluding noise) after fit().
    [[nodiscard]] int nClusters() const noexcept;

    /// Number of noise points (label == -1) after fit().
    [[nodiscard]] int nNoise() const noexcept;

    /// Effective eps used (auto-estimated or configured).
    [[nodiscard]] double effectiveEps() const noexcept;

private:

    DbscanClusteringConfig m_cfg;
    int    m_nClusters{0};
    int    m_nNoise{0};
    double m_effectiveEps{0.0};

    // -------------------------------------------------------------------------
    // Internal helpers
    // -------------------------------------------------------------------------

    /**
     * @brief Estimates eps via the k-NN distance elbow method.
     *
     * Computes the distance from each point to its minPts-th nearest neighbour,
     * sorts these distances, then finds the elbow via maximum of the second
     * discrete derivative.
     */
    double _estimateEps(const Eigen::MatrixXd& X) const;

    /**
     * @brief Returns indices of all points within eps of point idx.
     */
    std::vector<std::size_t> _regionQuery(const Eigen::MatrixXd& X,
                                          std::size_t idx,
                                          double eps) const;

    /**
     * @brief Computes the distance between two row vectors according to m_cfg.metric.
     */
    double _distance(const Eigen::RowVectorXd& a,
                     const Eigen::RowVectorXd& b) const;
};

} // namespace loki::clustering
