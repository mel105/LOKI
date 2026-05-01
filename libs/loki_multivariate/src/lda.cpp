#include "loki/multivariate/lda.hpp"

#include "loki/core/exceptions.hpp"
#include "loki/core/logger.hpp"

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <string>

using namespace loki::multivariate;

// -----------------------------------------------------------------------------
//  Construction
// -----------------------------------------------------------------------------

Lda::Lda(const MultivariateLdaConfig& cfg)
    : m_cfg(cfg)
{}

// -----------------------------------------------------------------------------
//  compute
// -----------------------------------------------------------------------------

LdaResult Lda::compute(const MultivariateSeries& data,
                        const std::vector<int>&   labels) const
{
    if (data.empty()) {
        throw DataException("Lda::compute(): data is empty.");
    }

    const int n = static_cast<int>(data.nObs());
    const int p = static_cast<int>(data.nChannels());

    if (static_cast<int>(labels.size()) != n) {
        throw DataException(
            "Lda: labels.size()=" + std::to_string(labels.size())
            + " != n=" + std::to_string(n) + ".");
    }

    // Identify unique classes.
    std::vector<int> uniqueClasses = labels;
    std::sort(uniqueClasses.begin(), uniqueClasses.end());
    uniqueClasses.erase(std::unique(uniqueClasses.begin(), uniqueClasses.end()),
                        uniqueClasses.end());
    const int nClasses = static_cast<int>(uniqueClasses.size());

    if (nClasses < 2) {
        throw DataException("Lda: at least 2 classes required.");
    }

    // Map class label -> 0-based index.
    std::map<int,int> classIdx;
    for (int c = 0; c < nClasses; ++c) {
        classIdx[uniqueClasses[static_cast<std::size_t>(c)]] = c;
    }

    // -- Step 1: class-wise statistics ----------------------------------------
    std::vector<int>            classCount(static_cast<std::size_t>(nClasses), 0);
    std::vector<Eigen::VectorXd> classMean(static_cast<std::size_t>(nClasses),
                                           Eigen::VectorXd::Zero(p));

    for (int i = 0; i < n; ++i) {
        const int c = classIdx.at(labels[static_cast<std::size_t>(i)]);
        classMean[static_cast<std::size_t>(c)] += data.data().row(i).transpose();
        ++classCount[static_cast<std::size_t>(c)];
    }
    for (int c = 0; c < nClasses; ++c) {
        if (classCount[static_cast<std::size_t>(c)] > 0) {
            classMean[static_cast<std::size_t>(c)] /=
                static_cast<double>(classCount[static_cast<std::size_t>(c)]);
        }
    }

    const Eigen::VectorXd globalMean = data.data().colwise().mean();

    // -- Step 2: scatter matrices ---------------------------------------------
    Eigen::MatrixXd Sw = Eigen::MatrixXd::Zero(p, p); // within-class
    Eigen::MatrixXd Sb = Eigen::MatrixXd::Zero(p, p); // between-class

    for (int i = 0; i < n; ++i) {
        const int c = classIdx.at(labels[static_cast<std::size_t>(i)]);
        const Eigen::VectorXd diff =
            data.data().row(i).transpose() - classMean[static_cast<std::size_t>(c)];
        Sw += diff * diff.transpose();
    }
    for (int c = 0; c < nClasses; ++c) {
        const Eigen::VectorXd diff =
            classMean[static_cast<std::size_t>(c)] - globalMean;
        Sb += static_cast<double>(classCount[static_cast<std::size_t>(c)])
            * diff * diff.transpose();
    }

    // Regularise Sw slightly for numerical stability.
    Sw += Eigen::MatrixXd::Identity(p, p) * 1.0e-8 * Sw.trace() / p;

    // -- Step 3: generalised eigenvalue problem Sw^{-1} Sb w = lambda w -------
    const int nDims = std::min(nClasses - 1, p);

    Eigen::GeneralizedSelfAdjointEigenSolver<Eigen::MatrixXd> geig(Sb, Sw);
    // Eigenvalues in ascending order -- take top nDims.
    const int total = static_cast<int>(geig.eigenvalues().size());
    Eigen::MatrixXd W(p, nDims);
    Eigen::VectorXd eigenvalues(nDims);
    for (int i = 0; i < nDims; ++i) {
        const int idx = total - 1 - i;
        W.col(i)        = geig.eigenvectors().col(idx);
        eigenvalues(i)  = geig.eigenvalues()(idx);
    }

    // -- Step 4: project data -------------------------------------------------
    const Eigen::MatrixXd scores = data.data() * W; // (n x nDims)

    // Project class centroids.
    std::vector<Eigen::VectorXd> projCentroids(static_cast<std::size_t>(nClasses));
    for (int c = 0; c < nClasses; ++c) {
        projCentroids[static_cast<std::size_t>(c)] =
            W.transpose() * classMean[static_cast<std::size_t>(c)];
    }

    // -- Step 5: predict ------------------------------------------------------
    const std::vector<int> predicted = _predict(scores, projCentroids);

    // Compute accuracy.
    int correct = 0;
    for (int i = 0; i < n; ++i) {
        if (predicted[static_cast<std::size_t>(i)] ==
            classIdx.at(labels[static_cast<std::size_t>(i)])) {
            ++correct;
        }
    }
    const double accuracy = static_cast<double>(correct) / static_cast<double>(n);

    LOKI_INFO("Lda: " + std::to_string(nClasses) + " classes, "
              + std::to_string(nDims) + " discriminant axes, "
              + "accuracy=" + std::to_string(static_cast<int>(accuracy * 100.0)) + "%.");

    // -- Step 6: assemble result ----------------------------------------------
    LdaResult result;
    result.discriminantAxes  = W;
    result.eigenvalues       = eigenvalues;
    result.scores            = scores;
    result.classMeans        = classMean;
    result.predictedLabels   = predicted;
    result.trueLabels        = labels;
    result.accuracy          = accuracy;
    result.nClasses          = nClasses;
    result.nDims             = nDims;
    result.nObs              = n;
    result.nChannels         = p;
    result.isQda             = m_cfg.useQda;

    return result;
}

// -----------------------------------------------------------------------------
//  _predict
// -----------------------------------------------------------------------------

std::vector<int> Lda::_predict(
    const Eigen::MatrixXd&              scores,
    const std::vector<Eigen::VectorXd>& centroids) const
{
    const int n        = static_cast<int>(scores.rows());
    const int nClasses = static_cast<int>(centroids.size());
    std::vector<int> pred(static_cast<std::size_t>(n));

    for (int i = 0; i < n; ++i) {
        double bestDist = std::numeric_limits<double>::max();
        int    bestC    = 0;
        for (int c = 0; c < nClasses; ++c) {
            const double d =
                (scores.row(i).transpose() - centroids[static_cast<std::size_t>(c)])
                .squaredNorm();
            if (d < bestDist) {
                bestDist = d;
                bestC    = c;
            }
        }
        pred[static_cast<std::size_t>(i)] = bestC;
    }

    return pred;
}
