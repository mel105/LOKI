#pragma once

#include <loki/ssa/ssaResult.hpp>
#include <loki/core/config.hpp>

#include <Eigen/Dense>

#include <string>
#include <vector>

namespace loki::ssa {

/**
 * @brief Assigns SSA eigentriples to named groups.
 *
 * Four grouping strategies are supported:
 *
 *   "manual"   -- User specifies indices explicitly in SsaGroupingConfig::manualGroups.
 *                 A group with empty index list receives all unassigned eigentriples
 *                 (typically used for "noise"). At most one such catch-all group
 *                 is allowed.
 *
 *   "wcorr"    -- Hierarchical agglomerative clustering (Ward linkage) on the
 *                 w-correlation distance matrix (distance = 1 - |w_ij|).
 *                 The dendrogram is cut at threshold (1 - wcorrThreshold).
 *                 Groups are named "group_0", "group_1", ... in order of
 *                 decreasing total eigenvalue. The largest group is renamed
 *                 "trend" if it contains eigentriple 0.
 *
 *   "kmeans"   -- k-means clustering on the 2D feature vector
 *                 [varianceFraction, zeroCrossingRate] per eigentriple.
 *                 kmeansK == 0 triggers automatic k selection via silhouette score
 *                 (k in [2, min(r, 8)]). Groups named as for "wcorr".
 *
 *   "variance" -- Greedy: keep the first k eigentriples whose cumulative
 *                 variance fraction reaches varianceThreshold. Remaining
 *                 eigentriples form a "noise" group.
 *                 Two groups produced: "signal" and "noise".
 *
 * @note The "wcorr" method requires a non-empty wCorrMatrix in SsaResult.
 */
class SsaGrouper {
public:

    /**
     * @brief Constructs a grouper bound to the given SSA configuration.
     * @param cfg Application configuration (SsaGroupingConfig used).
     */
    explicit SsaGrouper(const AppConfig& cfg);

    /**
     * @brief Assigns eigentriples to named groups and fills result.groups.
     *
     * On return, result.groups contains the assigned groups, each with
     * name, indices, and varianceFraction filled. The reconstruction
     * field of each group is NOT filled here -- that is done by
     * SsaReconstructor::reconstruct().
     *
     * result.groupingMethod is set to the method string used.
     *
     * @param result SsaResult with eigenvalues, varianceFractions, components,
     *               and wCorrMatrix already populated by SsaAnalyzer.
     * @throws AlgorithmException if grouping fails (e.g. wCorrMatrix empty for wcorr).
     * @throws ConfigException    if manual group indices are out of range.
     */
    void group(SsaResult& result) const;

private:

    AppConfig m_cfg;

    /// Manual grouping: indices explicitly specified in config.
    void _groupManual  (SsaResult& result) const;

    /// W-correlation hierarchical clustering.
    void _groupWCorr   (SsaResult& result) const;

    /// K-means on [variance, zeroCrossingRate] features.
    void _groupKMeans  (SsaResult& result) const;

    /// Greedy cumulative variance threshold.
    void _groupVariance(SsaResult& result) const;

    /// Estimates the zero-crossing rate of a component series.
    static double _zeroCrossingRate(const std::vector<double>& v);

    /// Runs k-means on rows of X (n_samples x n_features), returns cluster labels.
    static std::vector<int> _kMeans(const Eigen::MatrixXd& X,
                                    int k,
                                    int maxIter = 100);

    /// Computes silhouette score for a given assignment on distance matrix D.
    static double _silhouetteScore(const Eigen::MatrixXd& D,
                                   const std::vector<int>& labels,
                                   int k);

    /// Builds groups from a cluster label vector, names them, detects trend group.
    static std::vector<SsaGroup> _labelsToGroups(
        const std::vector<int>&     labels,
        const std::vector<double>&  varianceFractions,
        int                         r);
};

} // namespace loki::ssa
