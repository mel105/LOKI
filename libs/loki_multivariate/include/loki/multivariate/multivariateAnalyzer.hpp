#pragma once

#include "loki/multivariate/multivariateResult.hpp"
#include "loki/multivariate/multivariateSeries.hpp"
#include "loki/core/config.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace loki::multivariate {

/**
 * @brief Orchestrator for the loki_multivariate analysis pipeline.
 *
 * Receives the assembled MultivariateSeries and runs each enabled method
 * in sequence:
 *   1. CCF matrix (pairwise cross-correlation)
 *   2. PCA
 *   3. MSSA
 *   4. VAR + Granger causality
 *
 * Results are collected into a MultivariateResult and returned to main()
 * for protocol writing and plotting.
 *
 * Each method is wrapped in a try-catch so that a failure in one method
 * does not abort the remaining methods. Failures are logged as errors.
 */
class MultivariateAnalyzer {
public:

    /**
     * @brief Constructs the analyzer with the given application configuration.
     * @param cfg Full AppConfig. Uses cfg.multivariate for all parameters.
     */
    explicit MultivariateAnalyzer(const AppConfig& cfg);

    /**
     * @brief Runs all enabled methods on the assembled multivariate series.
     *
     * @param data     Synchronised, gap-filled, optionally standardised series.
     * @param stem     Dataset stem name used for output file naming.
     * @return         Aggregated result.
     */
    [[nodiscard]]
    MultivariateResult run(const MultivariateSeries& data,
                           const std::string&        stem) const;

private:

    const AppConfig& m_cfg;

    // -------------------------------------------------------------------------
    //  Private helpers
    // -------------------------------------------------------------------------

    /**
     * @brief Computes CCF for all channel pairs.
     *
     * For pair (A, B): normalised cross-correlation at lags [-maxLag, +maxLag].
     * Significance threshold: 1.96 / sqrt(n) (two-sided 95% CI for white noise).
     *
     * @param data   Multivariate series.
     * @return       Vector of CcfPairResult, one per ordered pair (A < B).
     */
    [[nodiscard]]
    std::vector<CcfPairResult> _computeCcf(const MultivariateSeries& data) const;

    /**
     * @brief Computes normalised CCF between two vectors at a single lag.
     *
     * ccf(lag) = sum_t (x[t] - mx) * (y[t+lag] - my) / (n * sx * sy)
     * where mx, my are means and sx, sy are standard deviations.
     *
     * @param x    First channel (zero-mean expected but not required).
     * @param y    Second channel.
     * @param lag  Lag (positive = y lags behind x; negative = x lags behind y).
     * @param n    Series length.
     * @return     Normalised correlation coefficient in [-1, 1].
     */
    [[nodiscard]]
    static double _ccfAtLag(const Eigen::VectorXd& x,
                             const Eigen::VectorXd& y,
                             int lag, int n);
};

} // namespace loki::multivariate
