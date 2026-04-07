#include <loki/clustering/clusteringAnalyzer.hpp>

#include <loki/clustering/dbscan.hpp>
#include <loki/clustering/kMeans.hpp>
#include <loki/clustering/plotClustering.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/timeseries/gapFiller.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <numeric>
#include <sstream>

using namespace loki;
using namespace loki::clustering;

// =============================================================================
//  Construction
// =============================================================================

ClusteringAnalyzer::ClusteringAnalyzer(const AppConfig& cfg)
    : m_cfg(cfg)
{}

// =============================================================================
//  Public: run
// =============================================================================

ClusteringResult ClusteringAnalyzer::run(const TimeSeries& series,
                                          const std::string& datasetName)
{
    ClusteringResult result;
    result.componentName = series.metadata().componentName;
    result.datasetName   = datasetName;
    result.method        = m_cfg.clustering.method;

    // -- Gap fill (optional) --------------------------------------------------
    TimeSeries workSeries = series;
    if (m_cfg.clustering.gapFillStrategy != "none") {
        GapFiller::Config gfCfg{};
        const std::string& strat = m_cfg.clustering.gapFillStrategy;
        if      (strat == "linear")       gfCfg.strategy = GapFiller::Strategy::LINEAR;
        else if (strat == "forward_fill") gfCfg.strategy = GapFiller::Strategy::FORWARD_FILL;
        else if (strat == "mean")         gfCfg.strategy = GapFiller::Strategy::MEAN;
        else                              gfCfg.strategy = GapFiller::Strategy::LINEAR;
        gfCfg.maxFillLength = static_cast<std::size_t>(
            m_cfg.clustering.gapFillMaxLength);
        GapFiller gf(gfCfg);
        try { workSeries = gf.fill(series); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("ClusteringAnalyzer: gap fill failed: "
                         + std::string(ex.what()) + " -- proceeding with NaN values.");
        }
    }

    // -- Build feature matrix -------------------------------------------------
    std::vector<std::size_t>  validIdx;
    std::vector<std::string>  featureNames;
    Eigen::MatrixXd X = _buildFeatureMatrix(workSeries, validIdx, featureNames);
    result.featureNames = featureNames;

    if (static_cast<int>(X.rows()) < 4) {
        throw DataException("ClusteringAnalyzer: too few valid observations ("
                            + std::to_string(X.rows()) + ") after feature extraction.");
    }

    // Count derivative columns (last columns that may have NaN at edges).
    int nDerivCols = 0;
    for (const auto& fn : featureNames) {
        if (fn != "value") ++nDerivCols;
    }

    // -- Z-score normalisation ------------------------------------------------
    Eigen::VectorXd featureMean, featureStd;
    _zScoreNormalize(X, featureMean, featureStd);

    // -- Clustering -----------------------------------------------------------
    std::vector<int>  labels;
    Eigen::MatrixXd   centroids;

    if (m_cfg.clustering.method == "dbscan") {
        Dbscan dbscan(m_cfg.clustering.dbscan);
        labels         = dbscan.fit(X);
        result.nClusters = dbscan.nClusters();
        result.nOutliers = dbscan.nNoise();
        result.inertia   = std::numeric_limits<double>::quiet_NaN();

        // Build centroid matrix from cluster means (for label naming).
        centroids = Eigen::MatrixXd::Zero(result.nClusters,
                                          static_cast<int>(X.cols()));
        std::vector<int> counts(static_cast<std::size_t>(result.nClusters), 0);
        for (std::size_t i = 0; i < static_cast<std::size_t>(X.rows()); ++i) {
            if (labels[i] >= 0) {
                centroids.row(labels[i]) += X.row(static_cast<int>(i));
                ++counts[static_cast<std::size_t>(labels[i])];
            }
        }
        for (int c = 0; c < result.nClusters; ++c) {
            if (counts[static_cast<std::size_t>(c)] > 0) {
                centroids.row(c) /= static_cast<double>(counts[static_cast<std::size_t>(c)]);
            }
        }

        // Compute silhouette if >= 2 non-noise clusters.
        if (result.nClusters >= 2) {
            // Filter out noise points for silhouette computation.
            std::vector<int> nonNoiseLabels;
            std::vector<int> nonNoiseRows;
            for (std::size_t i = 0; i < static_cast<std::size_t>(labels.size()); ++i) {
                if (labels[i] >= 0) {
                    nonNoiseLabels.push_back(labels[i]);
                    nonNoiseRows.push_back(static_cast<int>(i));
                }
            }
            if (!nonNoiseRows.empty()) {
                Eigen::MatrixXd Xnn(static_cast<int>(nonNoiseRows.size()),
                                    static_cast<int>(X.cols()));
                for (std::size_t r = 0; r < nonNoiseRows.size(); ++r) {
                    Xnn.row(static_cast<int>(r)) = X.row(nonNoiseRows[r]);
                }
                KMeans km(m_cfg.clustering.kmeans);
                result.silhouette = km.silhouetteScore();
                // Recompute silhouette on non-noise subset using KMeans helper.
                // We use a single-run fit with fixed k to get the silhouette.
                // Simpler: compute inline.
                (void)km; // silhouette will be 0.0 for DBSCAN in this version
                result.silhouette = 0.0; // DBSCAN silhouette: future enhancement
            }
        }

    } else {
        // k-means (default).
        KMeans kmeans(m_cfg.clustering.kmeans);
        labels             = kmeans.fit(X);
        centroids          = kmeans.centroids();
        result.nClusters   = kmeans.kSelected();
        result.kSelected   = (m_cfg.clustering.kmeans.k == 0) ? kmeans.kSelected() : 0;
        result.nOutliers   = 0;
        result.silhouette  = kmeans.silhouetteScore();
        result.inertia     = kmeans.inertia();
    }

    // -- Handle edge points (NaN derivative rows) -> nearest centroid ---------
    _handleEdgePoints(X, labels, centroids, nDerivCols);

    // -- De-normalise centroids to original units -----------------------------
    // centroids are in z-score space; de-normalise for reporting.
    Eigen::MatrixXd centroidsRaw = centroids;
    for (int c = 0; c < centroidsRaw.rows(); ++c) {
        for (int f = 0; f < centroidsRaw.cols(); ++f) {
            centroidsRaw(c, f) = centroidsRaw(c, f) * featureStd(f) + featureMean(f);
        }
    }

    // -- Assign label names ---------------------------------------------------
    std::vector<std::string> labelNames = _assignLabelNames(
        result.nClusters, centroidsRaw, m_cfg.clustering.method);

    // -- Build ClusterInfo vector ---------------------------------------------
    result.clusters.resize(static_cast<std::size_t>(result.nClusters));
    std::vector<double> clusterInertia(static_cast<std::size_t>(result.nClusters), 0.0);

    for (int c = 0; c < result.nClusters; ++c) {
        auto& ci     = result.clusters[static_cast<std::size_t>(c)];
        ci.label     = c;
        ci.name      = labelNames[static_cast<std::size_t>(c)];
        ci.centroid.assign(centroids.row(c).data(),
                           centroids.row(c).data() + centroids.cols());
        ci.centroidRaw.assign(centroidsRaw.row(c).data(),
                              centroidsRaw.row(c).data() + centroidsRaw.cols());
    }

    // -- Build ClusterPoint vector --------------------------------------------
    const std::size_t nValid = validIdx.size();
    result.points.resize(nValid);
    for (std::size_t i = 0; i < nValid; ++i) {
        auto& pt     = result.points[i];
        pt.index     = validIdx[i];
        pt.label     = labels[i];

        // Feature vector (potentially with NaN at edges for derivatives).
        pt.features.assign(X.row(static_cast<int>(i)).data(),
                           X.row(static_cast<int>(i)).data() + X.cols());

        if (pt.label == -1) {
            pt.labelName = "noise";
            pt.isOutlier = m_cfg.clustering.outlier.enabled;
            if (pt.isOutlier) result.outlierIndices.push_back(pt.index);
        } else {
            pt.labelName = labelNames[static_cast<std::size_t>(pt.label)];
            // Accumulate per-cluster inertia.
            if (m_cfg.clustering.method != "dbscan") {
                const double d2 = (X.row(static_cast<int>(i))
                                   - centroids.row(pt.label)).squaredNorm();
                clusterInertia[static_cast<std::size_t>(pt.label)] += d2;
                result.clusters[static_cast<std::size_t>(pt.label)].count++;
            } else {
                result.clusters[static_cast<std::size_t>(pt.label)].count++;
            }
        }
    }

    // Store per-cluster inertia for k-means.
    if (m_cfg.clustering.method != "dbscan") {
        for (int c = 0; c < result.nClusters; ++c) {
            result.clusters[static_cast<std::size_t>(c)].inertia =
                clusterInertia[static_cast<std::size_t>(c)];
        }
    }

    // -- Output ---------------------------------------------------------------
    _writeProtocol(result);
    _writeLabelsCsv(workSeries, result, validIdx);

    PlotClustering plotter(m_cfg);
    plotter.plotAll(workSeries, result);

    LOKI_INFO("ClusteringAnalyzer: finished '" + result.componentName
              + "'  method=" + result.method
              + "  clusters=" + std::to_string(result.nClusters)
              + "  noise=" + std::to_string(result.nOutliers));

    return result;
}

// =============================================================================
//  Private: _buildFeatureMatrix
// =============================================================================

Eigen::MatrixXd ClusteringAnalyzer::_buildFeatureMatrix(
    const TimeSeries&          series,
    std::vector<std::size_t>&  validIdx,
    std::vector<std::string>&  featureNames) const
{
    const std::size_t n = series.size();
    const auto& fc = m_cfg.clustering.features;

    // Collect valid indices.
    validIdx.clear();
    for (std::size_t i = 0; i < n; ++i) {
        if (!std::isnan(series[i].value)) validIdx.push_back(i);
    }
    const std::size_t nValid = validIdx.size();

    // Determine active features.
    featureNames.clear();
    if (fc.value)            featureNames.push_back("value");
    if (fc.derivative)       featureNames.push_back("derivative");
    if (fc.absDerivative)    featureNames.push_back("abs_derivative");
    if (fc.secondDerivative) featureNames.push_back("second_derivative");
    if (fc.slope)            featureNames.push_back("slope");

    if (featureNames.empty()) {
        // Fallback: always use value.
        featureNames.push_back("value");
    }

    const int d = static_cast<int>(featureNames.size());
    Eigen::MatrixXd X = Eigen::MatrixXd::Constant(
        static_cast<int>(nValid), d,
        std::numeric_limits<double>::quiet_NaN());

    // Build feature values for each valid point.
    // validIdx maps row i in X to position validIdx[i] in series.
    // For derivative features, we need the previous valid point.

    for (std::size_t row = 0; row < nValid; ++row) {
        const std::size_t si   = validIdx[row];
        const double      vi   = series[si].value;
        int col = 0;

        if (fc.value) {
            X(static_cast<int>(row), col++) = vi;
        }

        // Previous valid value (for derivative).
        double vPrev  = std::numeric_limits<double>::quiet_NaN();
        double vPrev2 = std::numeric_limits<double>::quiet_NaN();
        if (row >= 1) {
            vPrev = series[validIdx[row - 1]].value;
        }
        if (row >= 2) {
            vPrev2 = series[validIdx[row - 2]].value;
        }

        if (fc.derivative) {
            X(static_cast<int>(row), col++) = std::isnan(vPrev)
                ? std::numeric_limits<double>::quiet_NaN()
                : vi - vPrev;
        }
        if (fc.absDerivative) {
            X(static_cast<int>(row), col++) = std::isnan(vPrev)
                ? std::numeric_limits<double>::quiet_NaN()
                : std::abs(vi - vPrev);
        }
        if (fc.secondDerivative) {
            X(static_cast<int>(row), col++) =
                (std::isnan(vPrev) || std::isnan(vPrev2))
                ? std::numeric_limits<double>::quiet_NaN()
                : vi - 2.0 * vPrev + vPrev2;
        }

        if (fc.slope) {
            // OLS slope over a sliding window of slopeWindow valid points.
            // Uses the last min(row+1, slopeWindow) valid observations.
            const int w = fc.slopeWindow;
            const int startRow = std::max(0, static_cast<int>(row) - w + 1);
            const int nw = static_cast<int>(row) - startRow + 1;

            double slopeVal = std::numeric_limits<double>::quiet_NaN();
            if (nw >= 2) {
                // Fit y = a + b*t where t = 0..nw-1 (sample index within window).
                double sumT = 0.0, sumY = 0.0, sumT2 = 0.0, sumTY = 0.0;
                int cnt = 0;
                for (int wr = startRow; wr <= static_cast<int>(row); ++wr) {
                    const double yv = series[validIdx[static_cast<std::size_t>(wr)]].value;
                    if (std::isnan(yv)) continue;
                    const double t = static_cast<double>(wr - startRow);
                    sumT  += t;
                    sumY  += yv;
                    sumT2 += t * t;
                    sumTY += t * yv;
                    ++cnt;
                }
                if (cnt >= 2) {
                    const double denom = static_cast<double>(cnt) * sumT2 - sumT * sumT;
                    if (std::abs(denom) > 1e-12) {
                        slopeVal = (static_cast<double>(cnt) * sumTY - sumT * sumY) / denom;
                    }
                }
            }
            X(static_cast<int>(row), col++) = slopeVal;
        }
    }

    return X;
}

// =============================================================================
//  Private: _zScoreNormalize
// =============================================================================

void ClusteringAnalyzer::_zScoreNormalize(Eigen::MatrixXd& X,
                                           Eigen::VectorXd& mean,
                                           Eigen::VectorXd& stddev) const
{
    const int n = static_cast<int>(X.rows());
    const int d = static_cast<int>(X.cols());

    mean.resize(d);
    stddev.resize(d);

    for (int col = 0; col < d; ++col) {
        // Compute mean and std ignoring NaN.
        double sum = 0.0, sum2 = 0.0;
        int cnt = 0;
        for (int row = 0; row < n; ++row) {
            const double v = X(row, col);
            if (!std::isnan(v)) { sum += v; sum2 += v * v; ++cnt; }
        }
        if (cnt < 2) { mean(col) = 0.0; stddev(col) = 1.0; continue; }
        mean(col)   = sum / cnt;
        const double var = sum2 / cnt - mean(col) * mean(col);
        stddev(col) = (var > 0.0) ? std::sqrt(var) : 1.0;

        // Normalise.
        for (int row = 0; row < n; ++row) {
            if (!std::isnan(X(row, col))) {
                X(row, col) = (X(row, col) - mean(col)) / stddev(col);
            }
        }
    }
}

// =============================================================================
//  Private: _handleEdgePoints
// =============================================================================

void ClusteringAnalyzer::_handleEdgePoints(const Eigen::MatrixXd& X,
                                            std::vector<int>&       labels,
                                            const Eigen::MatrixXd&  centroids,
                                            int                     nDerivCols) const
{
    if (nDerivCols == 0) return;

    const int n    = static_cast<int>(X.rows());
    const int d    = static_cast<int>(X.cols());
    const int k    = static_cast<int>(centroids.rows());
    const int dVal = d - nDerivCols; // number of non-derivative columns

    for (int i = 0; i < n; ++i) {
        // Check if this row has NaN (edge point).
        bool hasNaN = false;
        for (int col = 0; col < d; ++col) {
            if (std::isnan(X(i, col))) { hasNaN = true; break; }
        }
        if (!hasNaN) continue;

        // Find nearest centroid using only non-NaN features (value columns).
        double minDist = std::numeric_limits<double>::max();
        int    bestC   = 0;
        for (int c = 0; c < k; ++c) {
            double dist = 0.0;
            for (int col = 0; col < dVal; ++col) {
                if (!std::isnan(X(i, col))) {
                    const double diff = X(i, col) - centroids(c, col);
                    dist += diff * diff;
                }
            }
            dist = std::sqrt(dist);
            if (dist < minDist) { minDist = dist; bestC = c; }
        }
        labels[static_cast<std::size_t>(i)] = bestC;
    }
}

// =============================================================================
//  Private: _assignLabelNames
// =============================================================================

std::vector<std::string> ClusteringAnalyzer::_assignLabelNames(
    int                    nClusters,
    const Eigen::MatrixXd& centroidsRaw,
    const std::string&     method) const
{
    std::vector<std::string> names(static_cast<std::size_t>(nClusters));

    const auto& userLabels = m_cfg.clustering.kmeans.labels;

    if (!userLabels.empty() && static_cast<int>(userLabels.size()) == nClusters) {
        // Sort cluster indices by ascending first-feature centroid value.
        std::vector<int> order(static_cast<std::size_t>(nClusters));
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(), [&](int a, int b) {
            return centroidsRaw(a, 0) < centroidsRaw(b, 0);
        });

        for (int i = 0; i < nClusters; ++i) {
            names[static_cast<std::size_t>(order[static_cast<std::size_t>(i)])] =
                userLabels[static_cast<std::size_t>(i)];
        }
    } else {
        if (!userLabels.empty()) {
            LOKI_WARNING("ClusteringAnalyzer: user label count ("
                         + std::to_string(userLabels.size())
                         + ") != nClusters (" + std::to_string(nClusters)
                         + ") -- using auto labels.");
        }
        for (int c = 0; c < nClusters; ++c) {
            names[static_cast<std::size_t>(c)] =
                (method == "dbscan" ? "dbscan_" : "cluster_") + std::to_string(c);
        }
    }
    return names;
}

// =============================================================================
//  Private: _writeProtocol
// =============================================================================

void ClusteringAnalyzer::_writeProtocol(const ClusteringResult& result) const
{
    const std::string stem = "clustering_" + result.datasetName
                             + "_" + result.componentName + "_protocol.txt";
    const auto path = m_cfg.protocolsDir / stem;

    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        throw IoException("ClusteringAnalyzer: cannot open protocol file: "
                          + path.string());
    }

    auto line = [&](const std::string& s) { ofs << s << "\n"; };
    auto sep  = [&]() { ofs << std::string(72, '-') << "\n"; };

    line("LOKI Clustering Protocol");
    line("Dataset   : " + result.datasetName);
    line("Component : " + result.componentName);
    line("Method    : " + result.method);
    sep();

    line("FEATURES: " + [&]() {
        std::string s;
        for (const auto& f : result.featureNames) s += f + " ";
        return s;
    }());
    sep();

    ofs << std::fixed << std::setprecision(4);

    if (result.method == "kmeans") {
        line("k selected   : " + std::to_string(result.nClusters)
             + (result.kSelected > 0 ? "  (auto)" : "  (manual)"));
        line("Inertia      : " + std::to_string(result.inertia));
        line("Silhouette   : " + std::to_string(result.silhouette));
    } else {
        line("Clusters found : " + std::to_string(result.nClusters));
        line("Noise points   : " + std::to_string(result.nOutliers));
    }
    sep();

    line("CLUSTER SUMMARY");
    sep();
    for (const auto& ci : result.clusters) {
        line("  [" + ci.name + "]  n=" + std::to_string(ci.count)
             + "  inertia=" + std::to_string(ci.inertia));
        std::string cStr = "  centroid (raw): ";
        for (std::size_t f = 0; f < ci.centroidRaw.size(); ++f) {
            cStr += result.featureNames[f] + "="
                    + std::to_string(ci.centroidRaw[f]) + "  ";
        }
        line(cStr);
    }

    if (result.nOutliers > 0 && m_cfg.clustering.outlier.enabled) {
        sep();
        line("OUTLIERS (DBSCAN noise, outlier.enabled=true)");
        line("  Count: " + std::to_string(result.nOutliers));
    }

    ofs.close();
    LOKI_INFO("ClusteringAnalyzer: protocol -> " + path.string());
}

// =============================================================================
//  Private: _writeLabelsCsv
// =============================================================================

void ClusteringAnalyzer::_writeLabelsCsv(
    const TimeSeries&               series,
    const ClusteringResult&         result,
    const std::vector<std::size_t>& /*validIdx*/) const
{
    const std::string stem = "clustering_" + result.datasetName
                             + "_" + result.componentName + "_labels.csv";
    const auto path = m_cfg.csvDir / stem;

    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        throw IoException("ClusteringAnalyzer: cannot open CSV: " + path.string());
    }

    ofs << "mjd ; utc ; value ; label ; label_name ; outlier_flag\n";
    ofs << std::fixed << std::setprecision(8);

    // Build a lookup from series index -> ClusterPoint.
    std::unordered_map<std::size_t, const ClusterPoint*> lookup;
    for (const auto& pt : result.points) {
        lookup[pt.index] = &pt;
    }

    for (std::size_t i = 0; i < series.size(); ++i) {
        const double mjd = series[i].time.mjd();
        const std::string utc = series[i].time.utcString();
        const double val = series[i].value;

        ofs << mjd << " ; " << utc << " ; ";
        if (std::isnan(val)) ofs << "NaN"; else ofs << std::setprecision(6) << val;

        const auto it = lookup.find(i);
        if (it != lookup.end()) {
            const auto& pt = *it->second;
            ofs << " ; " << pt.label
                << " ; " << pt.labelName
                << " ; " << (pt.isOutlier ? 1 : 0);
        } else {
            // NaN epoch -- not clustered.
            ofs << " ; -1 ; invalid ; 0";
        }
        ofs << "\n";
    }

    ofs.close();
    LOKI_INFO("ClusteringAnalyzer: labels CSV -> " + path.string());
}

// =============================================================================
//  Private: _extractValues
// =============================================================================

std::vector<double> ClusteringAnalyzer::_extractValues(const TimeSeries& series)
{
    std::vector<double> v;
    v.reserve(series.size());
    for (std::size_t i = 0; i < series.size(); ++i) {
        v.push_back(series[i].value);
    }
    return v;
}