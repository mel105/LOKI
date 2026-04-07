#include <loki/clustering/kMeans.hpp>

#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>

using namespace loki;
using namespace loki::clustering;

// =============================================================================
//  Construction
// =============================================================================

KMeans::KMeans(const KMeansClusteringConfig& cfg)
    : m_cfg(cfg)
{
    if (m_cfg.kMin < 2) {
        throw ConfigException("KMeans: kMin must be >= 2, got "
                              + std::to_string(m_cfg.kMin) + ".");
    }
    if (m_cfg.kMax < m_cfg.kMin) {
        throw ConfigException("KMeans: kMax must be >= kMin.");
    }
    if (m_cfg.maxIter < 1) {
        throw ConfigException("KMeans: maxIter must be >= 1.");
    }
    if (m_cfg.nInit < 1) {
        throw ConfigException("KMeans: nInit must be >= 1.");
    }
}

// =============================================================================
//  Public: fit
// =============================================================================

std::vector<int> KMeans::fit(const Eigen::MatrixXd& X)
{
    const int n = static_cast<int>(X.rows());
    if (n < m_cfg.kMin) {
        throw DataException("KMeans: too few observations ("
                            + std::to_string(n) + ") for kMin="
                            + std::to_string(m_cfg.kMin) + ".");
    }

    std::vector<int>  bestLabels;
    Eigen::MatrixXd   bestCentroids;
    double            bestInertia{0.0};

    if (m_cfg.k > 0) {
        // Fixed k.
        m_kSelected = m_cfg.k;
        auto [labels, centroids, inertia] = _runKMeans(X, m_cfg.k);
        bestLabels    = std::move(labels);
        bestCentroids = std::move(centroids);
        bestInertia   = inertia;
    } else {
        // Auto-select k.
        auto [k, labels, centroids, inertia] = _autoSelectK(X);
        m_kSelected   = k;
        bestLabels    = std::move(labels);
        bestCentroids = std::move(centroids);
        bestInertia   = inertia;
        LOKI_INFO("KMeans: auto-selected k=" + std::to_string(k));
    }

    m_centroids  = bestCentroids;
    m_inertia    = bestInertia;
    m_silhouette = _silhouette(X, bestLabels, m_kSelected);

    return bestLabels;
}

// =============================================================================
//  Accessors
// =============================================================================

const Eigen::MatrixXd& KMeans::centroids()      const noexcept { return m_centroids;  }
double                 KMeans::silhouetteScore() const noexcept { return m_silhouette; }
double                 KMeans::inertia()         const noexcept { return m_inertia;    }
int                    KMeans::kSelected()       const noexcept { return m_kSelected;  }

// =============================================================================
//  Private: _runKMeans
// =============================================================================

std::tuple<std::vector<int>, Eigen::MatrixXd, double>
KMeans::_runKMeans(const Eigen::MatrixXd& X, int k) const
{
    std::vector<int> bestLabels;
    Eigen::MatrixXd  bestCentroids;
    double           bestInertia = std::numeric_limits<double>::max();

    for (int run = 0; run < m_cfg.nInit; ++run) {
        Eigen::MatrixXd initC = (m_cfg.init == "random")
            ? _initRandom(X, k)
            : _initKMeansPlusPlus(X, k);

        auto [labels, centroids, inertia] = _iterate(X, initC);
        if (inertia < bestInertia) {
            bestInertia   = inertia;
            bestLabels    = std::move(labels);
            bestCentroids = std::move(centroids);
        }
    }
    return {bestLabels, bestCentroids, bestInertia};
}

// =============================================================================
//  Private: _iterate
// =============================================================================

std::tuple<std::vector<int>, Eigen::MatrixXd, double>
KMeans::_iterate(const Eigen::MatrixXd& X, const Eigen::MatrixXd& initCentroids) const
{
    Eigen::MatrixXd centroids = initCentroids;
    std::vector<int> labels(static_cast<std::size_t>(X.rows()), 0);

    for (int iter = 0; iter < m_cfg.maxIter; ++iter) {
        auto [newLabels, inertia] = _assign(X, centroids);
        Eigen::MatrixXd newCentroids = _updateCentroids(X, newLabels,
                                                         static_cast<int>(centroids.rows()));

        // Convergence: max centroid shift < tol.
        double maxShift = (newCentroids - centroids).rowwise().norm().maxCoeff();
        labels    = newLabels;
        centroids = newCentroids;

        if (maxShift < m_cfg.tol) break;
    }

    auto [finalLabels, finalInertia] = _assign(X, centroids);
    return {finalLabels, centroids, finalInertia};
}

// =============================================================================
//  Private: _initKMeansPlusPlus
// =============================================================================

Eigen::MatrixXd KMeans::_initKMeansPlusPlus(const Eigen::MatrixXd& X, int k) const
{
    const int n = static_cast<int>(X.rows());
    const int d = static_cast<int>(X.cols());

    // Collect indices of rows with no NaN values.
    std::vector<int> validRows;
    validRows.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        if (!X.row(i).array().isNaN().any()) {
            validRows.push_back(i);
        }
    }
    if (static_cast<int>(validRows.size()) < k) {
        throw DataException("KMeans: fewer valid (non-NaN) rows ("
                            + std::to_string(validRows.size())
                            + ") than k=" + std::to_string(k) + ".");
    }

    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> uniformIdx(
        0, static_cast<int>(validRows.size()) - 1);

    Eigen::MatrixXd centroids(k, d);
    centroids.row(0) = X.row(validRows[static_cast<std::size_t>(uniformIdx(rng))]);

    for (int c = 1; c < k; ++c) {
        // Distance squared from each valid point to nearest chosen centroid.
        std::vector<double> dist2(validRows.size());
        for (std::size_t vi = 0; vi < validRows.size(); ++vi) {
            double minD = std::numeric_limits<double>::max();
            for (int j = 0; j < c; ++j) {
                double d2 = (X.row(validRows[vi]) - centroids.row(j)).squaredNorm();
                if (d2 < minD) minD = d2;
            }
            dist2[vi] = minD;
        }

        // Sample proportional to dist2.
        std::discrete_distribution<int> dist(dist2.begin(), dist2.end());
        centroids.row(c) = X.row(validRows[static_cast<std::size_t>(dist(rng))]);
    }
    return centroids;
}

// =============================================================================
//  Private: _initRandom
// =============================================================================

Eigen::MatrixXd KMeans::_initRandom(const Eigen::MatrixXd& X, int k) const
{
    const int n = static_cast<int>(X.rows());
    const int d = static_cast<int>(X.cols());

    // Collect valid (non-NaN) row indices.
    std::vector<int> validRows;
    validRows.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        if (!X.row(i).array().isNaN().any()) {
            validRows.push_back(i);
        }
    }
    if (static_cast<int>(validRows.size()) < k) {
        throw DataException("KMeans: fewer valid (non-NaN) rows ("
                            + std::to_string(validRows.size())
                            + ") than k=" + std::to_string(k) + ".");
    }

    std::mt19937 rng(std::random_device{}());
    std::shuffle(validRows.begin(), validRows.end(), rng);

    Eigen::MatrixXd centroids(k, d);
    for (int c = 0; c < k; ++c) {
        centroids.row(c) = X.row(validRows[static_cast<std::size_t>(c)]);
    }
    return centroids;
}

// =============================================================================
//  Private: _assign
// =============================================================================

std::pair<std::vector<int>, double>
KMeans::_assign(const Eigen::MatrixXd& X, const Eigen::MatrixXd& centroids) const
{
    const int n = static_cast<int>(X.rows());
    const int k = static_cast<int>(centroids.rows());

    std::vector<int> labels(static_cast<std::size_t>(n), 0);
    double inertia = 0.0;

    for (int i = 0; i < n; ++i) {
        // Skip NaN rows -- they are edge points handled separately.
        if (X.row(i).array().isNaN().any()) {
            labels[static_cast<std::size_t>(i)] = -1; // will be fixed by _handleEdgePoints
            continue;
        }

        double minDist = std::numeric_limits<double>::max();
        int    bestC   = 0;
        for (int c = 0; c < k; ++c) {
            double d = (X.row(i) - centroids.row(c)).squaredNorm();
            if (d < minDist) { minDist = d; bestC = c; }
        }
        labels[static_cast<std::size_t>(i)] = bestC;
        inertia += minDist;
    }
    return {labels, inertia};
}

// =============================================================================
//  Private: _updateCentroids
// =============================================================================

Eigen::MatrixXd KMeans::_updateCentroids(const Eigen::MatrixXd& X,
                                          const std::vector<int>& labels,
                                          int k) const
{
    const int n = static_cast<int>(X.rows());
    const int d = static_cast<int>(X.cols());

    Eigen::MatrixXd centroids = Eigen::MatrixXd::Zero(k, d);
    std::vector<int> counts(static_cast<std::size_t>(k), 0);

    // Collect valid (non-NaN, assigned) rows for potential reinitialisation.
    std::vector<int> validRows;
    validRows.reserve(static_cast<std::size_t>(n));

    for (int i = 0; i < n; ++i) {
        const int c = labels[static_cast<std::size_t>(i)];
        if (c < 0 || X.row(i).array().isNaN().any()) {
            // NaN edge points or unassigned -- skip accumulation.
            continue;
        }
        validRows.push_back(i);
        centroids.row(c) += X.row(i);
        counts[static_cast<std::size_t>(c)]++;
    }

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> uid(
        0, validRows.empty() ? 0 : static_cast<int>(validRows.size()) - 1);

    for (int c = 0; c < k; ++c) {
        if (counts[static_cast<std::size_t>(c)] > 0) {
            centroids.row(c) /= static_cast<double>(counts[static_cast<std::size_t>(c)]);
        } else {
            // Empty cluster: reinitialise to a random valid point.
            LOKI_WARNING("KMeans: empty cluster " + std::to_string(c)
                         + " -- reinitialising to random point.");
            if (!validRows.empty()) {
                centroids.row(c) = X.row(validRows[static_cast<std::size_t>(uid(rng))]);
            }
        }
    }
    return centroids;
}

// =============================================================================
//  Private: _silhouette
// =============================================================================

double KMeans::_silhouette(const Eigen::MatrixXd& X,
                            const std::vector<int>& labels,
                            int k) const
{
    if (k <= 1) return 0.0;

    const int n = static_cast<int>(X.rows());
    double total = 0.0;
    int    nValid = 0;

    for (int i = 0; i < n; ++i) {
        const int ci = labels[static_cast<std::size_t>(i)];
        // Skip NaN rows and unassigned points.
        if (ci < 0 || X.row(i).array().isNaN().any()) continue;

        // a(i): mean distance to other points in same cluster.
        double a = 0.0;
        int    countA = 0;
        for (int j = 0; j < n; ++j) {
            if (j == i) continue;
            if (labels[static_cast<std::size_t>(j)] != ci) continue;
            if (X.row(j).array().isNaN().any()) continue;
            a += (X.row(i) - X.row(j)).norm();
            ++countA;
        }
        a = (countA > 0) ? a / static_cast<double>(countA) : 0.0;

        // b(i): mean distance to nearest other cluster.
        double b = std::numeric_limits<double>::max();
        for (int c = 0; c < k; ++c) {
            if (c == ci) continue;
            double meanD = 0.0;
            int    countB = 0;
            for (int j = 0; j < n; ++j) {
                if (labels[static_cast<std::size_t>(j)] != c) continue;
                if (X.row(j).array().isNaN().any()) continue;
                meanD += (X.row(i) - X.row(j)).norm();
                ++countB;
            }
            if (countB > 0) {
                meanD /= static_cast<double>(countB);
                if (meanD < b) b = meanD;
            }
        }
        if (b == std::numeric_limits<double>::max()) b = 0.0;

        const double s = (std::max(a, b) > 0.0) ? (b - a) / std::max(a, b) : 0.0;
        total += s;
        ++nValid;
    }
    return (nValid > 0) ? total / static_cast<double>(nValid) : 0.0;
}

// =============================================================================
//  Private: _autoSelectK
// =============================================================================

std::tuple<int, std::vector<int>, Eigen::MatrixXd, double>
KMeans::_autoSelectK(const Eigen::MatrixXd& X) const
{
    int              bestK         = m_cfg.kMin;
    double           bestSilhouette = -2.0;
    std::vector<int> bestLabels;
    Eigen::MatrixXd  bestCentroids;
    double           bestInertia   = 0.0;

    for (int k = m_cfg.kMin; k <= m_cfg.kMax; ++k) {
        if (static_cast<int>(X.rows()) < k) break;

        auto [labels, centroids, inertia] = _runKMeans(X, k);
        const double s = _silhouette(X, labels, k);

        LOKI_INFO("KMeans auto-select: k=" + std::to_string(k)
                  + "  silhouette=" + std::to_string(s));

        if (s > bestSilhouette) {
            bestSilhouette = s;
            bestK          = k;
            bestLabels     = std::move(labels);
            bestCentroids  = std::move(centroids);
            bestInertia    = inertia;
        }
    }
    return {bestK, bestLabels, bestCentroids, bestInertia};
}
