#include "loki/multivariate/multivariateAnalyzer.hpp"
#include "loki/multivariate/pca.hpp"
#include "loki/multivariate/mssa.hpp"
#include "loki/multivariate/var.hpp"

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

    // Significance threshold for white noise: z_{alpha/2} / sqrt(n).
    // For alpha = 0.05: z = 1.96.
    const double z = (sigLevel <= 0.01) ? 2.576
                   : (sigLevel <= 0.05) ? 1.960
                   :                      1.645;
    const double threshold = z / std::sqrt(static_cast<double>(n));

    std::vector<CcfPairResult> results;
    results.reserve(static_cast<std::size_t>(p * (p - 1) / 2));

    // Pre-compute means and std devs.
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

            // Compute CCF at lags [-maxLag .. +maxLag].
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
    // For lag >= 0: correlate x[t] with y[t + lag], t = 0..n-lag-1.
    // For lag <  0: correlate x[t - lag] with y[t], t = 0..n+lag-1.
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
