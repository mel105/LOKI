#include <loki/ssa/ssaGrouper.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <random>
#include <set>
#include <sstream>

using namespace loki;
using namespace loki::ssa;

// ----------------------------------------------------------------------------
//  Construction
// ----------------------------------------------------------------------------

SsaGrouper::SsaGrouper(const AppConfig& cfg)
    : m_cfg{cfg}
{}

// ----------------------------------------------------------------------------
//  group  (dispatcher)
// ----------------------------------------------------------------------------

void SsaGrouper::group(SsaResult& result) const
{
    const std::string& method = m_cfg.ssa.grouping.method;
    result.groupingMethod = method;

    if (method == "manual") {
        _groupManual(result);
    } else if (method == "wcorr") {
        _groupWCorr(result);
    } else if (method == "kmeans") {
        _groupKMeans(result);
    } else if (method == "variance") {
        _groupVariance(result);
    } else {
        throw AlgorithmException(
            "SsaGrouper: unknown grouping method '" + method + "'.");
    }

    // Log group summary
    for (const auto& g : result.groups) {
        std::ostringstream oss;
        oss << "SsaGrouper [" << method << "] group='" << g.name
            << "'  varFrac=" << g.varianceFraction
            << "  indices=[";
        for (std::size_t i = 0; i < g.indices.size(); ++i) {
            if (i > 0) oss << ",";
            oss << g.indices[i];
        }
        oss << "]";
        LOKI_INFO(oss.str());
    }
}

// ----------------------------------------------------------------------------
//  _groupManual
// ----------------------------------------------------------------------------

void SsaGrouper::_groupManual(SsaResult& result) const
{
    const int r = static_cast<int>(result.eigenvalues.size());
    const auto& manualGroups = m_cfg.ssa.grouping.manualGroups;

    if (manualGroups.empty()) {
        throw ConfigException(
            "SsaGrouper: method='manual' but no manual_groups defined in config.");
    }

    // Validate indices and find catch-all group (empty index list)
    std::set<int> assigned;
    std::string catchAllName;
    int catchAllCount = 0;

    for (const auto& [name, indices] : manualGroups) {
        if (indices.empty()) {
            catchAllName = name;
            ++catchAllCount;
            if (catchAllCount > 1) {
                throw ConfigException(
                    "SsaGrouper: manual_groups has more than one catch-all "
                    "(empty index list) group.");
            }
            continue;
        }
        for (int idx : indices) {
            if (idx < 0 || idx >= r) {
                throw ConfigException(
                    "SsaGrouper: manual group '" + name
                    + "' index " + std::to_string(idx)
                    + " out of range [0, " + std::to_string(r - 1) + "].");
            }
            if (!assigned.insert(idx).second) {
                throw ConfigException(
                    "SsaGrouper: eigentriple " + std::to_string(idx)
                    + " assigned to multiple manual groups.");
            }
        }
    }

    // Build groups (preserve map order -- alphabetical by name in std::map)
    result.groups.clear();

    for (const auto& [name, indices] : manualGroups) {
        if (name == catchAllName) continue;   // handled below

        SsaGroup g;
        g.name    = name;
        g.indices = indices;

        double vf = 0.0;
        for (int idx : indices) {
            vf += result.varianceFractions[static_cast<std::size_t>(idx)];
        }
        g.varianceFraction = vf;
        result.groups.push_back(std::move(g));
    }

    // Catch-all group: all unassigned eigentriples
    if (!catchAllName.empty()) {
        SsaGroup g;
        g.name = catchAllName;
        double vf = 0.0;
        for (int i = 0; i < r; ++i) {
            if (assigned.find(i) == assigned.end()) {
                g.indices.push_back(i);
                vf += result.varianceFractions[static_cast<std::size_t>(i)];
            }
        }
        g.varianceFraction = vf;
        result.groups.push_back(std::move(g));
    }
}

// ----------------------------------------------------------------------------
//  _groupWCorr  -- Ward hierarchical clustering on distance = 1 - |w_ij|
// ----------------------------------------------------------------------------

void SsaGrouper::_groupWCorr(SsaResult& result) const
{
    const int r = static_cast<int>(result.eigenvalues.size());

    if (result.wCorrMatrix.empty()) {
        throw AlgorithmException(
            "SsaGrouper: method='wcorr' but wCorrMatrix is empty. "
            "Set compute_wcorr=true in config.");
    }

    // Build distance matrix D[i][j] = 1 - |w_ij|
    // Using a flat representation for the Ward linkage algorithm.
    // D is symmetric, diagonal = 0.
    std::vector<std::vector<double>> D(static_cast<std::size_t>(r),
                                       std::vector<double>(static_cast<std::size_t>(r), 0.0));
    for (int i = 0; i < r; ++i) {
        for (int j = 0; j < r; ++j) {
            D[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] =
                1.0 - result.wCorrMatrix[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)];
        }
    }

    // Ward linkage agglomerative clustering.
    // Cluster labels: initially each eigentriple is its own cluster.
    std::vector<int> clusterOf(static_cast<std::size_t>(r));
    std::iota(clusterOf.begin(), clusterOf.end(), 0);

    // Maintain active clusters as sets of eigentriple indices.
    std::vector<std::set<int>> clusters(static_cast<std::size_t>(r));
    for (int i = 0; i < r; ++i) {
        clusters[static_cast<std::size_t>(i)].insert(i);
    }

    // Inter-cluster distance: average linkage (simpler than Ward for small r).
    // For SSA grouping with r typically small (< 100), average linkage is
    // a reasonable approximation and avoids the complexity of full Ward.
    auto clusterDist = [&](const std::set<int>& A, const std::set<int>& B) -> double {
        double sum = 0.0;
        int    cnt = 0;
        for (int a : A) {
            for (int b : B) {
                sum += D[static_cast<std::size_t>(a)][static_cast<std::size_t>(b)];
                ++cnt;
            }
        }
        return (cnt > 0) ? (sum / static_cast<double>(cnt)) : 0.0;
    };

    const double cutThreshold = 1.0 - m_cfg.ssa.grouping.wcorrThreshold;

    // Greedily merge the closest pair of clusters until all remaining pairs
    // are farther than cutThreshold.
    std::vector<bool> active(static_cast<std::size_t>(r), true);

    bool merged = true;
    while (merged) {
        merged = false;
        double bestDist = std::numeric_limits<double>::max();
        int    bestI = -1;
        int    bestJ = -1;

        for (int i = 0; i < r; ++i) {
            if (!active[static_cast<std::size_t>(i)]) continue;
            for (int j = i + 1; j < r; ++j) {
                if (!active[static_cast<std::size_t>(j)]) continue;
                const double d = clusterDist(clusters[static_cast<std::size_t>(i)],
                                             clusters[static_cast<std::size_t>(j)]);
                if (d < bestDist) {
                    bestDist = d;
                    bestI    = i;
                    bestJ    = j;
                }
            }
        }

        if (bestI < 0 || bestDist > cutThreshold) break;

        // Merge cluster bestJ into bestI
        for (int idx : clusters[static_cast<std::size_t>(bestJ)]) {
            clusters[static_cast<std::size_t>(bestI)].insert(idx);
        }
        clusters[static_cast<std::size_t>(bestJ)].clear();
        active[static_cast<std::size_t>(bestJ)] = false;
        merged = true;
    }

    // Collect active clusters and assign labels
    std::vector<int> labels(static_cast<std::size_t>(r), 0);
    std::vector<std::set<int>> finalClusters;
    for (int i = 0; i < r; ++i) {
        if (active[static_cast<std::size_t>(i)]) {
            const int label = static_cast<int>(finalClusters.size());
            finalClusters.push_back(clusters[static_cast<std::size_t>(i)]);
            for (int idx : clusters[static_cast<std::size_t>(i)]) {
                labels[static_cast<std::size_t>(idx)] = label;
            }
        }
    }

    result.groups = _labelsToGroups(labels, result.varianceFractions, r);
}

// ----------------------------------------------------------------------------
//  _groupKMeans  -- k-means on [varianceFraction, zeroCrossingRate]
// ----------------------------------------------------------------------------

void SsaGrouper::_groupKMeans(SsaResult& result) const
{
    const int r = static_cast<int>(result.eigenvalues.size());
    if (r < 2) {
        // Degenerate case: single component
        _groupVariance(result);
        return;
    }

    // Build 2D feature matrix: col0 = varianceFraction, col1 = zeroCrossingRate
    Eigen::MatrixXd X(r, 2);
    for (int i = 0; i < r; ++i) {
        X(i, 0) = result.varianceFractions[static_cast<std::size_t>(i)];
        X(i, 1) = _zeroCrossingRate(result.components[static_cast<std::size_t>(i)]);
    }

    // Normalize columns to [0, 1]
    for (int col = 0; col < 2; ++col) {
        const double mn  = X.col(col).minCoeff();
        const double mx  = X.col(col).maxCoeff();
        const double rng = mx - mn;
        if (rng > std::numeric_limits<double>::epsilon()) {
            X.col(col) = (X.col(col).array() - mn) / rng;
        }
    }

    int k = m_cfg.ssa.grouping.kmeansK;

    if (k <= 0) {
        // Auto-select k via silhouette score, k in [2, min(r, 8)]
        const int kMax = std::min(r, 8);
        double    bestSil = -1.0;
        int       bestK   = 2;

        // Pre-compute Euclidean distance matrix for silhouette
        Eigen::MatrixXd distMat(r, r);
        for (int i = 0; i < r; ++i) {
            distMat(i, i) = 0.0;
            for (int j = i + 1; j < r; ++j) {
                const double d = (X.row(i) - X.row(j)).norm();
                distMat(i, j) = d;
                distMat(j, i) = d;
            }
        }

        for (int kk = 2; kk <= kMax; ++kk) {
            const std::vector<int> lbl = _kMeans(X, kk);
            const double sil = _silhouetteScore(distMat, lbl, kk);
            if (sil > bestSil) {
                bestSil = sil;
                bestK   = kk;
            }
        }
        k = bestK;
        LOKI_INFO("SsaGrouper [kmeans]: auto-selected k=" + std::to_string(k)
                  + "  silhouette=" + std::to_string(bestSil));
    }

    const std::vector<int> labels = _kMeans(X, k);
    result.groups = _labelsToGroups(labels, result.varianceFractions, r);
}

// ----------------------------------------------------------------------------
//  _groupVariance  -- greedy cumulative variance threshold
// ----------------------------------------------------------------------------

void SsaGrouper::_groupVariance(SsaResult& result) const
{
    const int    r         = static_cast<int>(result.eigenvalues.size());
    const double threshold = m_cfg.ssa.grouping.varianceThreshold;

    SsaGroup signal;
    signal.name = "signal";
    SsaGroup noise;
    noise.name = "noise";

    double cumVar = 0.0;
    for (int i = 0; i < r; ++i) {
        const double vf = result.varianceFractions[static_cast<std::size_t>(i)];
        if (cumVar < threshold) {
            signal.indices.push_back(i);
            signal.varianceFraction += vf;
            cumVar += vf;
        } else {
            noise.indices.push_back(i);
            noise.varianceFraction += vf;
        }
    }

    result.groups.clear();
    result.groups.push_back(std::move(signal));
    if (!noise.indices.empty()) {
        result.groups.push_back(std::move(noise));
    }
}

// ----------------------------------------------------------------------------
//  _zeroCrossingRate
// ----------------------------------------------------------------------------

double SsaGrouper::_zeroCrossingRate(const std::vector<double>& v)
{
    if (v.size() < 2) return 0.0;
    int crossings = 0;
    for (std::size_t i = 1; i < v.size(); ++i) {
        if ((v[i - 1] >= 0.0) != (v[i] >= 0.0)) ++crossings;
    }
    return static_cast<double>(crossings) / static_cast<double>(v.size() - 1);
}

// ----------------------------------------------------------------------------
//  _kMeans  -- Lloyd's algorithm, random++ initialisation
// ----------------------------------------------------------------------------

std::vector<int> SsaGrouper::_kMeans(const Eigen::MatrixXd& X, int k, int maxIter)
{
    const int n = static_cast<int>(X.rows());
    if (k >= n) {
        // Degenerate: one cluster per point
        std::vector<int> lbl(static_cast<std::size_t>(n));
        std::iota(lbl.begin(), lbl.end(), 0);
        return lbl;
    }

    // Initialise centroids: first centroid at random, subsequent by k-means++
    std::mt19937 rng(42);
    Eigen::MatrixXd centroids(k, X.cols());

    // First centroid: uniformly random
    std::uniform_int_distribution<int> uniform(0, n - 1);
    centroids.row(0) = X.row(uniform(rng));

    for (int c = 1; c < k; ++c) {
        // Distance from each point to nearest existing centroid
        std::vector<double> dist2(static_cast<std::size_t>(n), std::numeric_limits<double>::max());
        for (int i = 0; i < n; ++i) {
            for (int cc = 0; cc < c; ++cc) {
                const double d = (X.row(i) - centroids.row(cc)).squaredNorm();
                if (d < dist2[static_cast<std::size_t>(i)]) {
                    dist2[static_cast<std::size_t>(i)] = d;
                }
            }
        }
        // Sample proportional to distance squared
        std::discrete_distribution<int> weighted(dist2.begin(), dist2.end());
        centroids.row(c) = X.row(weighted(rng));
    }

    std::vector<int> labels(static_cast<std::size_t>(n), 0);

    for (int iter = 0; iter < maxIter; ++iter) {
        // Assignment step
        bool changed = false;
        for (int i = 0; i < n; ++i) {
            double bestD = std::numeric_limits<double>::max();
            int    bestC = 0;
            for (int c = 0; c < k; ++c) {
                const double d = (X.row(i) - centroids.row(c)).squaredNorm();
                if (d < bestD) { bestD = d; bestC = c; }
            }
            if (labels[static_cast<std::size_t>(i)] != bestC) {
                labels[static_cast<std::size_t>(i)] = bestC;
                changed = true;
            }
        }
        if (!changed) break;

        // Update step
        Eigen::MatrixXd newCentroids = Eigen::MatrixXd::Zero(k, X.cols());
        std::vector<int> counts(static_cast<std::size_t>(k), 0);
        for (int i = 0; i < n; ++i) {
            newCentroids.row(labels[static_cast<std::size_t>(i)]) += X.row(i);
            ++counts[static_cast<std::size_t>(labels[static_cast<std::size_t>(i)])];
        }
        for (int c = 0; c < k; ++c) {
            if (counts[static_cast<std::size_t>(c)] > 0) {
                centroids.row(c) = newCentroids.row(c) / static_cast<double>(counts[static_cast<std::size_t>(c)]);
            }
        }
    }

    return labels;
}

// ----------------------------------------------------------------------------
//  _silhouetteScore
// ----------------------------------------------------------------------------

double SsaGrouper::_silhouetteScore(const Eigen::MatrixXd& D,
                                     const std::vector<int>& labels,
                                     int k)
{
    const int n = static_cast<int>(D.rows());
    double totalSil = 0.0;
    int    count    = 0;

    for (int i = 0; i < n; ++i) {
        const int ci = labels[static_cast<std::size_t>(i)];

        // a(i): mean distance to points in same cluster
        double sumA = 0.0;
        int    cntA = 0;
        for (int j = 0; j < n; ++j) {
            if (j != i && labels[static_cast<std::size_t>(j)] == ci) {
                sumA += D(i, j);
                ++cntA;
            }
        }
        if (cntA == 0) continue;  // singleton cluster
        const double a = sumA / static_cast<double>(cntA);

        // b(i): minimum mean distance to points in any other cluster
        double b = std::numeric_limits<double>::max();
        for (int c = 0; c < k; ++c) {
            if (c == ci) continue;
            double sumB = 0.0;
            int    cntB = 0;
            for (int j = 0; j < n; ++j) {
                if (labels[static_cast<std::size_t>(j)] == c) {
                    sumB += D(i, j);
                    ++cntB;
                }
            }
            if (cntB > 0) {
                const double mb = sumB / static_cast<double>(cntB);
                if (mb < b) b = mb;
            }
        }
        if (b == std::numeric_limits<double>::max()) continue;

        const double maxAB = std::max(a, b);
        if (maxAB > std::numeric_limits<double>::epsilon()) {
            totalSil += (b - a) / maxAB;
            ++count;
        }
    }

    return (count > 0) ? (totalSil / static_cast<double>(count)) : 0.0;
}

// ----------------------------------------------------------------------------
//  _labelsToGroups
// ----------------------------------------------------------------------------

std::vector<SsaGroup> SsaGrouper::_labelsToGroups(
    const std::vector<int>&    labels,
    const std::vector<double>& varianceFractions,
    int                        r)
{
    // Collect eigentriple indices per cluster label
    std::map<int, std::vector<int>> clusterMap;
    for (int i = 0; i < r; ++i) {
        clusterMap[labels[static_cast<std::size_t>(i)]].push_back(i);
    }

    // Build groups sorted by descending total variance fraction
    std::vector<SsaGroup> groups;
    for (auto& [label, indices] : clusterMap) {
        SsaGroup g;
        g.indices = indices;
        for (int idx : indices) {
            g.varianceFraction += varianceFractions[static_cast<std::size_t>(idx)];
        }
        groups.push_back(std::move(g));
    }

    std::sort(groups.begin(), groups.end(),
              [](const SsaGroup& a, const SsaGroup& b) {
                  return a.varianceFraction > b.varianceFraction;
              });

    // Name groups: the highest-variance group containing eigentriple 0 is "trend";
    // the lowest-variance group is "noise"; others are "group_1", "group_2", ...
    bool trendAssigned = false;
    bool noiseAssigned = false;
    const std::size_t ng = groups.size();

    for (std::size_t gi = 0; gi < ng; ++gi) {
        auto& g = groups[gi];

        const bool containsZero = std::find(g.indices.begin(), g.indices.end(), 0)
                                  != g.indices.end();

        if (!trendAssigned && containsZero) {
            g.name        = "trend";
            trendAssigned = true;
        } else if (!noiseAssigned && gi == ng - 1 && ng > 1) {
            g.name        = "noise";
            noiseAssigned = true;
        } else {
            // Count non-trend, non-noise groups assigned so far for the number suffix
            int gnum = 0;
            for (std::size_t prev = 0; prev < gi; ++prev) {
                if (groups[prev].name != "trend") ++gnum;
            }
            g.name = "group_" + std::to_string(gnum + 1);
        }
    }

    return groups;
}
