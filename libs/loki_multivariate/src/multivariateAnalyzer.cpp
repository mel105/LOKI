#include "loki/multivariate/multivariateAnalyzer.hpp"
#include "loki/multivariate/pca.hpp"
#include "loki/multivariate/mssa.hpp"
#include "loki/multivariate/var.hpp"
#include "loki/multivariate/factorAnalysis.hpp"
#include "loki/multivariate/cca.hpp"
#include "loki/multivariate/lda.hpp"
#include "loki/multivariate/mahalanobis.hpp"
#include "loki/multivariate/manova.hpp"

#include "loki/core/exceptions.hpp"
#include "loki/core/logger.hpp"

#include <algorithm>
#include <cmath>
#include <string>

using namespace loki::multivariate;

// -----------------------------------------------------------------------------
//  Construction
// -----------------------------------------------------------------------------

MultivariateAnalyzer::MultivariateAnalyzer(const AppConfig& cfg)
    : m_cfg(cfg)
{}

// -----------------------------------------------------------------------------
//  run
// -----------------------------------------------------------------------------

MultivariateResult MultivariateAnalyzer::run(const MultivariateSeries& data,
                                              const std::string&        stem) const
{
    const auto& mcfg = m_cfg.multivariate;
    MultivariateResult result;

    LOKI_INFO("MultivariateAnalyzer: dataset='" + stem + "'"
              + "  n=" + std::to_string(data.nObs())
              + "  channels=" + std::to_string(data.nChannels()));

    // -- CCF ------------------------------------------------------------------
    if (mcfg.ccf.enabled) {
        LOKI_INFO("MultivariateAnalyzer: running CCF matrix.");
        try {
            result.ccf = _computeCcf(data);
            LOKI_INFO("MultivariateAnalyzer: CCF done ("
                      + std::to_string(result.ccf.size()) + " pairs).");
        } catch (const LOKIException& ex) {
            LOKI_ERROR(std::string("MultivariateAnalyzer: CCF failed: ") + ex.what());
        }
    }

    // -- PCA ------------------------------------------------------------------
    if (mcfg.pca.enabled) {
        LOKI_INFO("MultivariateAnalyzer: running PCA.");
        try {
            Pca pca(mcfg.pca);
            result.pca = pca.compute(data);
            LOKI_INFO("MultivariateAnalyzer: PCA done, retained "
                      + std::to_string(result.pca->nComponents) + " components.");
        } catch (const LOKIException& ex) {
            LOKI_ERROR(std::string("MultivariateAnalyzer: PCA failed: ") + ex.what());
        }
    }

    // -- MSSA -----------------------------------------------------------------
    if (mcfg.mssa.enabled) {
        LOKI_INFO("MultivariateAnalyzer: running MSSA (window="
                  + std::to_string(mcfg.mssa.window) + ").");
        try {
            Mssa mssa(mcfg.mssa);
            result.mssa = mssa.compute(data);
            LOKI_INFO("MultivariateAnalyzer: MSSA done.");
        } catch (const LOKIException& ex) {
            LOKI_ERROR(std::string("MultivariateAnalyzer: MSSA failed: ") + ex.what());
        }
    }

    // -- VAR ------------------------------------------------------------------
    if (mcfg.var_.enabled) {
        LOKI_INFO("MultivariateAnalyzer: running VAR (max_order="
                  + std::to_string(mcfg.var_.maxOrder) + ").");
        try {
            Var var(mcfg.var_);
            result.var_ = var.compute(data);
            LOKI_INFO("MultivariateAnalyzer: VAR done, order="
                      + std::to_string(result.var_->order)
                      + " Granger pairs="
                      + std::to_string(result.var_->granger.size()) + ".");
        } catch (const LOKIException& ex) {
            LOKI_ERROR(std::string("MultivariateAnalyzer: VAR failed: ") + ex.what());
        }
    }

    // -- Factor Analysis ------------------------------------------------------
    if (mcfg.factor.enabled) {
        LOKI_INFO("MultivariateAnalyzer: running Factor Analysis (k="
                  + std::to_string(mcfg.factor.nFactors) + ").");
        try {
            FactorAnalysis fa(mcfg.factor);
            result.factor = fa.compute(data);
            LOKI_INFO("MultivariateAnalyzer: Factor Analysis done.");
        } catch (const LOKIException& ex) {
            LOKI_ERROR(std::string("MultivariateAnalyzer: Factor Analysis failed: ")
                       + ex.what());
        }
    }

    // -- CCA ------------------------------------------------------------------
    if (mcfg.cca.enabled) {
        LOKI_INFO("MultivariateAnalyzer: running CCA.");
        try {
            Cca cca(mcfg.cca);
            result.cca = cca.compute(data);
            LOKI_INFO("MultivariateAnalyzer: CCA done, rho_1="
                      + std::to_string(result.cca->canonicalCorrelations(0)) + ".");
        } catch (const LOKIException& ex) {
            LOKI_ERROR(std::string("MultivariateAnalyzer: CCA failed: ") + ex.what());
        }
    }

    // -- LDA ------------------------------------------------------------------
    if (mcfg.lda.enabled) {
        LOKI_INFO("MultivariateAnalyzer: running LDA (useQda="
                  + std::string(mcfg.lda.useQda ? "true" : "false") + ").");
        try {
            const std::vector<int> labels = _extractLabels(data, mcfg.lda.groupsColumn);
            Lda lda(mcfg.lda);
            result.lda = lda.compute(data, labels);
            LOKI_INFO("MultivariateAnalyzer: LDA done, accuracy="
                      + std::to_string(result.lda->accuracy) + ".");
        } catch (const LOKIException& ex) {
            LOKI_ERROR(std::string("MultivariateAnalyzer: LDA failed: ") + ex.what());
        }
    }

    // -- Mahalanobis ----------------------------------------------------------
    if (mcfg.mahalanobis.enabled) {
        LOKI_INFO("MultivariateAnalyzer: running Mahalanobis outlier detection.");
        try {
            Mahalanobis mah(mcfg.mahalanobis);
            result.mahalanobis = mah.compute(data);
            LOKI_INFO("MultivariateAnalyzer: Mahalanobis done, outliers="
                      + std::to_string(result.mahalanobis->nOutliers) + ".");
        } catch (const LOKIException& ex) {
            LOKI_ERROR(std::string("MultivariateAnalyzer: Mahalanobis failed: ")
                       + ex.what());
        }
    }

    // -- MANOVA ---------------------------------------------------------------
    if (mcfg.manova.enabled) {
        LOKI_INFO("MultivariateAnalyzer: running MANOVA.");
        try {
            const std::vector<int> labels =
                _extractLabels(data, mcfg.manova.groupsColumn);
            Manova manova(mcfg.manova);
            result.manova = manova.compute(data, labels);
            LOKI_INFO("MultivariateAnalyzer: MANOVA done, Wilks="
                      + std::to_string(result.manova->wilksLambda)
                      + " p=" + std::to_string(result.manova->wilksPvalue) + ".");
        } catch (const LOKIException& ex) {
            LOKI_ERROR(std::string("MultivariateAnalyzer: MANOVA failed: ")
                       + ex.what());
        }
    }

    return result;
}

// -----------------------------------------------------------------------------
//  _computeCcf
// -----------------------------------------------------------------------------

std::vector<CcfPairResult>
MultivariateAnalyzer::_computeCcf(const MultivariateSeries& data) const
{
    const int n      = static_cast<int>(data.nObs());
    const int p      = static_cast<int>(data.nChannels());
    const int maxLag = m_cfg.multivariate.ccf.maxLag;
    const double sigLevel = m_cfg.multivariate.ccf.significanceLevel;

    const double z = (sigLevel <= 0.01) ? 2.576
                   : (sigLevel <= 0.05) ? 1.960
                   :                      1.645;
    const double threshold = z / std::sqrt(static_cast<double>(n));

    std::vector<CcfPairResult> results;
    results.reserve(static_cast<std::size_t>(p * (p - 1) / 2));

    std::vector<double> means(static_cast<std::size_t>(p));
    std::vector<double> stds (static_cast<std::size_t>(p));
    for (int j = 0; j < p; ++j) {
        const Eigen::VectorXd col = data.data().col(j);
        means[static_cast<std::size_t>(j)] = col.mean();
        const double var = (col.array() - means[static_cast<std::size_t>(j)])
                           .square().mean();
        stds[static_cast<std::size_t>(j)] = (var > 0.0) ? std::sqrt(var) : 1.0;
    }

    for (int a = 0; a < p; ++a) {
        for (int b = a + 1; b < p; ++b) {
            CcfPairResult pr;
            pr.channelA  = a;
            pr.channelB  = b;
            pr.nameA     = data.channelName(a);
            pr.nameB     = data.channelName(b);
            pr.maxLag    = maxLag;
            pr.threshold = threshold;

            const Eigen::VectorXd xa =
                (data.data().col(a).array() - means[static_cast<std::size_t>(a)])
                / stds[static_cast<std::size_t>(a)];
            const Eigen::VectorXd xb =
                (data.data().col(b).array() - means[static_cast<std::size_t>(b)])
                / stds[static_cast<std::size_t>(b)];

            const int nLags = 2 * maxLag + 1;
            pr.ccf.resize(static_cast<std::size_t>(nLags));

            double bestAbs = -1.0;
            int    bestLag = 0;
            double bestVal = 0.0;

            for (int lag = -maxLag; lag <= maxLag; ++lag) {
                const double c = _ccfAtLag(xa, xb, lag, n);
                pr.ccf[static_cast<std::size_t>(lag + maxLag)] = c;
                if (std::abs(c) > bestAbs) {
                    bestAbs = std::abs(c);
                    bestLag = lag;
                    bestVal = c;
                }
            }

            pr.peakLag    = bestLag;
            pr.peakCorr   = bestVal;
            pr.significant = (bestAbs > threshold);

            results.push_back(std::move(pr));
        }
    }

    return results;
}

// -----------------------------------------------------------------------------
//  _ccfAtLag
// -----------------------------------------------------------------------------

double MultivariateAnalyzer::_ccfAtLag(const Eigen::VectorXd& x,
                                        const Eigen::VectorXd& y,
                                        int lag, int n)
{
    int tStart = 0;
    int tEnd   = n - 1;
    double sum = 0.0;
    int    cnt = 0;

    if (lag >= 0) {
        tEnd = n - 1 - lag;
        for (int t = tStart; t <= tEnd; ++t) {
            sum += x(t) * y(t + lag);
            ++cnt;
        }
    } else {
        tStart = -lag;
        for (int t = tStart; t <= tEnd; ++t) {
            sum += x(t) * y(t + lag);
            ++cnt;
        }
    }

    return (cnt > 0) ? sum / static_cast<double>(n) : 0.0;
}

// -----------------------------------------------------------------------------
//  _extractLabels
// -----------------------------------------------------------------------------

std::vector<int> MultivariateAnalyzer::_extractLabels(
    const MultivariateSeries& data,
    const std::string& groupsColumn) const
{
    // Find channel by name.
    int colIdx = -1;
    for (int j = 0; j < static_cast<int>(data.nChannels()); ++j) {
        if (data.channelName(j) == groupsColumn) {
            colIdx = j;
            break;
        }
    }

    // Try numeric interpretation (1-based column index as string).
    if (colIdx < 0) {
        try {
            const int idx = std::stoi(groupsColumn) - 1;
            if (idx >= 0 && idx < static_cast<int>(data.nChannels())) {
                colIdx = idx;
            }
        } catch (...) {}
    }

    if (colIdx < 0) {
        throw DataException(
            "MultivariateAnalyzer: groups_column '" + groupsColumn
            + "' not found in channel names.");
    }

    const Eigen::VectorXd col = data.data().col(colIdx);
    const int n = static_cast<int>(col.size());

    std::vector<int> labels(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        labels[static_cast<std::size_t>(i)] =
            static_cast<int>(std::round(col(i)));
    }

    return labels;
}
