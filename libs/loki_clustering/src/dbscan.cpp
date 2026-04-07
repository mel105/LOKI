#include <loki/clustering/dbscan.hpp>

#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <queue>

using namespace loki;
using namespace loki::clustering;

// =============================================================================
//  Construction
// =============================================================================

Dbscan::Dbscan(const DbscanClusteringConfig& cfg)
    : m_cfg(cfg)
{
    if (m_cfg.minPts < 2) {
        throw ConfigException("Dbscan: minPts must be >= 2, got "
                              + std::to_string(m_cfg.minPts) + ".");
    }
}

// =============================================================================
//  Public: fit
// =============================================================================

std::vector<int> Dbscan::fit(const Eigen::MatrixXd& X)
{
    const std::size_t n = static_cast<std::size_t>(X.rows());
    if (n < static_cast<std::size_t>(m_cfg.minPts)) {
        throw DataException("Dbscan: too few observations ("
                            + std::to_string(n) + ") for minPts="
                            + std::to_string(m_cfg.minPts) + ".");
    }

    // Determine effective eps.
    m_effectiveEps = (m_cfg.eps > 0.0) ? m_cfg.eps : _estimateEps(X);
    LOKI_INFO("Dbscan: effective eps=" + std::to_string(m_effectiveEps));

    // BFS/queue-based DBSCAN.
    constexpr int UNVISITED = -2;
    constexpr int NOISE     = -1;

    std::vector<int> labels(n, UNVISITED);
    int currentCluster = 0;

    for (std::size_t i = 0; i < n; ++i) {
        if (labels[i] != UNVISITED) continue;

        std::vector<std::size_t> neighbours = _regionQuery(X, i, m_effectiveEps);

        if (static_cast<int>(neighbours.size()) < m_cfg.minPts) {
            labels[i] = NOISE;
            continue;
        }

        // Start a new cluster.
        labels[i] = currentCluster;
        std::queue<std::size_t> queue;
        for (std::size_t nb : neighbours) {
            if (nb != i) queue.push(nb);
        }

        while (!queue.empty()) {
            const std::size_t q = queue.front();
            queue.pop();

            if (labels[q] == NOISE) {
                labels[q] = currentCluster; // border point
                continue;
            }
            if (labels[q] != UNVISITED) continue;

            labels[q] = currentCluster;
            std::vector<std::size_t> qNeighbours = _regionQuery(X, q, m_effectiveEps);
            if (static_cast<int>(qNeighbours.size()) >= m_cfg.minPts) {
                for (std::size_t nb : qNeighbours) {
                    if (labels[nb] == UNVISITED || labels[nb] == NOISE) {
                        queue.push(nb);
                    }
                }
            }
        }
        ++currentCluster;
    }

    m_nClusters = currentCluster;
    m_nNoise    = static_cast<int>(
        std::count(labels.begin(), labels.end(), NOISE));

    LOKI_INFO("Dbscan: found " + std::to_string(m_nClusters)
              + " clusters, " + std::to_string(m_nNoise) + " noise points.");

    return labels;
}

// =============================================================================
//  Accessors
// =============================================================================

int    Dbscan::nClusters()    const noexcept { return m_nClusters;    }
int    Dbscan::nNoise()       const noexcept { return m_nNoise;       }
double Dbscan::effectiveEps() const noexcept { return m_effectiveEps; }

// =============================================================================
//  Private: _estimateEps
// =============================================================================

double Dbscan::_estimateEps(const Eigen::MatrixXd& X) const
{
    const std::size_t n = static_cast<std::size_t>(X.rows());
    const std::size_t k = static_cast<std::size_t>(m_cfg.minPts);

    // k-NN distance for each point.
    std::vector<double> knnDist(n);
    for (std::size_t i = 0; i < n; ++i) {
        std::vector<double> dists(n);
        for (std::size_t j = 0; j < n; ++j) {
            dists[j] = _distance(X.row(static_cast<int>(i)),
                                  X.row(static_cast<int>(j)));
        }
        std::partial_sort(dists.begin(),
                          dists.begin() + static_cast<std::ptrdiff_t>(k) + 1,
                          dists.end());
        knnDist[i] = dists[k]; // k-th nearest (index 0 is self = 0)
    }

    // Sort ascending.
    std::sort(knnDist.begin(), knnDist.end());

    // Find elbow via maximum of second discrete derivative.
    if (n < 3) {
        return knnDist.back();
    }

    double maxCurv = -1.0;
    std::size_t elbowIdx = n / 2; // fallback to midpoint

    for (std::size_t i = 1; i + 1 < n; ++i) {
        // Second derivative approximation.
        const double curv = std::abs(knnDist[i + 1] - 2.0 * knnDist[i] + knnDist[i - 1]);
        if (curv > maxCurv) {
            maxCurv  = curv;
            elbowIdx = i;
        }
    }

    LOKI_INFO("Dbscan: auto eps estimated at knnDist[" + std::to_string(elbowIdx)
              + "] = " + std::to_string(knnDist[elbowIdx]));

    return knnDist[elbowIdx];
}

// =============================================================================
//  Private: _regionQuery
// =============================================================================

std::vector<std::size_t> Dbscan::_regionQuery(const Eigen::MatrixXd& X,
                                               std::size_t idx,
                                               double eps) const
{
    const std::size_t n = static_cast<std::size_t>(X.rows());
    std::vector<std::size_t> neighbours;
    neighbours.reserve(32);

    for (std::size_t j = 0; j < n; ++j) {
        if (_distance(X.row(static_cast<int>(idx)),
                      X.row(static_cast<int>(j))) <= eps) {
            neighbours.push_back(j);
        }
    }
    return neighbours;
}

// =============================================================================
//  Private: _distance
// =============================================================================

double Dbscan::_distance(const Eigen::RowVectorXd& a,
                          const Eigen::RowVectorXd& b) const
{
    if (m_cfg.metric == "manhattan") {
        return (a - b).cwiseAbs().sum();
    }
    // Default: euclidean.
    return (a - b).norm();
}
