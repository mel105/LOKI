#pragma once

#include <loki/ssa/ssaResult.hpp>
#include <loki/core/config.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <string>
#include <vector>

namespace loki::ssa {

/**
 * @brief Orchestrates the single-channel SSA decomposition pipeline.
 *
 * Pipeline executed by analyze():
 *   1. Validate and resolve window length L (auto or explicit, with safety cap).
 *   2. Build K x L Hankel trajectory matrix via buildEmbedMatrix().
 *   3. Compute thin SVD: X = U * diag(sv) * V^T  using JacobiSVD.
 *   4. Fill SsaResult eigenvalues and varianceFractions.
 *   5. SsaReconstructor: diagonal averaging -> result.components (all r eigentriples).
 *   6. Compute w-correlation matrix via computeWCorrelation() if computeWCorr=true.
 *   7. SsaGrouper: assign eigentriples to named groups.
 *   8. SsaReconstructor: per-group reconstruction sums + convenience aliases.
 *
 * Window length auto-selection:
 *   - If windowLength > 0: use as-is (validated: >= 2 and <= n/2).
 *   - If windowLength == 0 and period > 0: L = period * periodMultiplier.
 *   - If windowLength == 0 and period == 0: L = n / 2.
 *   - In all auto cases: L = min(L, maxWindowLength, n/2), L = max(L, 2).
 *   - If explicit windowLength violates constraints: LOKI_WARNING + fallback to auto.
 */
class SsaAnalyzer {
public:

    /**
     * @brief Constructs an analyzer bound to the given application configuration.
     * @param cfg Application configuration. SsaConfig used for all parameters.
     */
    explicit SsaAnalyzer(const AppConfig& cfg);

    /**
     * @brief Runs the full SSA pipeline on a pre-processed value series.
     *
     * The input series must be NaN-free (gap-filled and deseasonalized
     * upstream if required). The caller is responsible for those steps.
     *
     * @param y Gap-filled, NaN-free time series values. Length n >= 3.
     * @return  Populated SsaResult.
     * @throws SeriesTooShortException if y.size() < 3.
     * @throws AlgorithmException      on SVD or reconstruction failure.
     * @throws ConfigException         on invalid grouping configuration.
     */
    SsaResult analyze(const std::vector<double>& y) const;

    /**
     * @brief Resolves and returns the effective window length L for series y.
     *
     * Applies the auto-selection logic documented above. Useful for logging
     * before calling analyze().
     *
     * @param n Series length.
     * @return  Resolved window length L >= 2.
     */
    int resolveWindowLength(std::size_t n) const;

private:

    AppConfig m_cfg;
};

} // namespace loki::ssa
