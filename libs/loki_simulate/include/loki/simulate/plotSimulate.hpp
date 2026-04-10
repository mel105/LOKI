#pragma once

#include <loki/simulate/simulationResult.hpp>
#include <loki/core/config.hpp>

#include <string>

namespace loki::simulate {

/**
 * @brief Produces all gnuplot plots for the loki_simulate pipeline.
 *
 * Plot types:
 *   - plotOverlay     : first N replicas overlaid on the original series
 *                       (bootstrap mode) or on the median envelope (synthetic).
 *   - plotEnvelope    : percentile envelope 5/25/50/75/95 filled areas.
 *   - plotBootstrapDist: histogram of one selected parameter bootstrap distribution
 *                       (bootstrap mode only; skipped in synthetic mode).
 *   - plotAcfComparison: ACF of original vs. mean ACF of simulations
 *                       (bootstrap mode only).
 *
 * Plot file naming:
 *   simulate_[dataset]_[component]_overlay.[format]
 *   simulate_[dataset]_[component]_envelope.[format]
 *   simulate_[dataset]_[component]_bootstrap_dist.[format]
 *   simulate_[dataset]_[component]_acf_comparison.[format]
 *
 * Plot flags are read from cfg.plots:
 *   simulateOverlay, simulateEnvelope, simulateBootstrapDist, simulateAcfComparison.
 */
class PlotSimulate {
public:

    /**
     * @brief Constructs with the application configuration.
     * @param cfg Full AppConfig (paths, plot flags, output format).
     */
    explicit PlotSimulate(const AppConfig& cfg);

    /**
     * @brief Produces all enabled plots for a completed SimulationResult.
     * @param result Completed simulation result.
     */
    void plotAll(const SimulationResult& result);

private:

    AppConfig m_cfg;

    static constexpr int MAX_OVERLAY_SERIES = 20;  ///< Maximum replicas to draw in overlay.

    /**
     * @brief Overlay plot: first N replicas + median + original (if available).
     */
    void _plotOverlay(const SimulationResult& result);

    /**
     * @brief Envelope plot: filled percentile bands 5/25/50/75/95.
     */
    void _plotEnvelope(const SimulationResult& result);

    /**
     * @brief Bootstrap distribution histogram for the "mean" parameter CI.
     *        Only produced in bootstrap mode when parameterCIs is non-empty.
     */
    void _plotBootstrapDist(const SimulationResult& result);

    /**
     * @brief ACF comparison: original series ACF vs. mean ACF of simulations.
     *        Only produced in bootstrap mode.
     */
    void _plotAcfComparison(const SimulationResult& result);

    /**
     * @brief Computes the autocorrelation function up to maxLag.
     * @param v      Input series.
     * @param maxLag Maximum lag to compute.
     * @return       ACF vector of length maxLag (lag 1..maxLag).
     */
    static std::vector<double> _computeAcf(const std::vector<double>& v, int maxLag);

    /**
     * @brief Converts backslashes to forward slashes for gnuplot on Windows.
     */
    static std::string fwdSlash(const std::string& p);

    /**
     * @brief Returns the output file path string for a given plot type.
     */
    std::string _outPath(const SimulationResult& result,
                         const std::string& plotType) const;
};

} // namespace loki::simulate
