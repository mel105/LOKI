#pragma once

#include <loki/core/config.hpp>
#include <loki/evt/evtResult.hpp>
#include <loki/math/nelderMead.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <functional>
#include <string>
#include <vector>

namespace loki::evt {

/**
 * @brief Orchestrator for the EVT analysis pipeline.
 *
 * Runs the full Extreme Value Theory workflow for a single time series:
 *   1. Gap filling (configured via EvtConfig::gapFillStrategy).
 *   2. Threshold selection (auto or manual via EvtConfig::threshold).
 *   3. GPD fit via Peaks-Over-Threshold (POT) and/or GEV fit via block maxima.
 *   4. Return level estimation with confidence intervals.
 *   5. Goodness-of-fit tests (Anderson-Darling, Kolmogorov-Smirnov).
 *   6. Protocol, CSV, and plot output.
 *
 * CI methods:
 *   "profile_likelihood" -- recommended for large return periods (SIL 4).
 *   "bootstrap"          -- resampling-based, subsampleed to maxExceedancesBootstrap.
 *   "delta"              -- delta method (fast but unreliable for large T).
 *
 * Usage:
 * @code
 *   loki::evt::EvtAnalyzer analyzer(cfg);
 *   analyzer.run(ts, "DATASET_NAME");
 * @endcode
 */
class EvtAnalyzer {
public:

    /**
     * @brief Construct with a full application config.
     * @param cfg Application config. evt and plots sub-configs are used.
     */
    explicit EvtAnalyzer(const AppConfig& cfg);

    /**
     * @brief Run EVT analysis on a single time series.
     *
     * @param series      Input time series (will be gap-filled internally).
     * @param datasetName File stem used for output naming (e.g. "CLIM_DATA_EX1").
     * @throws DataException      if the series is too short after gap filling.
     * @throws AlgorithmException on numerical failure.
     */
    void run(const TimeSeries& series, const std::string& datasetName);

private:

    const AppConfig& m_cfg;

    // -- Analysis steps -------------------------------------------------------

    /**
     * @brief Run POT/GPD analysis on clean data values.
     *
     * @param data   Valid (non-NaN) values from the gap-filled series.
     * @param dtHours Median time step in hours (for lambda estimation).
     * @return EvtResult with method == "pot".
     */
    EvtResult _runPot(const std::vector<double>& data,
                       double dtHours) const;

    /**
     * @brief Run block maxima / GEV analysis on clean data values.
     *
     * @param data   Valid (non-NaN) values.
     * @return EvtResult with method == "block_maxima".
     */
    EvtResult _runBlockMaxima(const std::vector<double>& data) const;

    /**
     * @brief Compute return levels with CI for a fitted GPD.
     *
     * @param exc    Exceedances above threshold.
     * @param gpd    Fitted GPD parameters.
     * @return Vector of ReturnLevelCI, one per configured return period.
     */
    std::vector<ReturnLevelCI> _returnLevelsGpd(
        const std::vector<double>& exc,
        const GpdFitResult&        gpd) const;

    /**
     * @brief Compute return levels with CI for a fitted GEV.
     *
     * @param maxima Block maxima.
     * @param gev    Fitted GEV parameters.
     * @return Vector of ReturnLevelCI, one per configured return period.
     */
    std::vector<ReturnLevelCI> _returnLevelsGev(
        const std::vector<double>& maxima,
        const GevFitResult&        gev) const;

    // -- CI computation -------------------------------------------------------

    /**
     * @brief Profile likelihood CI for a GPD return level.
     *
     * Fixes the return level and re-maximises the profile log-likelihood
     * to find the CI bounds. Uses Brent's method for root finding.
     *
     * @param exc      Exceedances.
     * @param gpd      Fitted GPD.
     * @param T        Return period.
     * @param level    Confidence level (e.g. 0.95).
     * @param lower    Output lower bound.
     * @param upper    Output upper bound.
     */
    void _profileLikCiGpd(const std::vector<double>& exc,
                            const GpdFitResult&        gpd,
                            double T, double level,
                            double& lower, double& upper) const;

    /**
     * @brief Bootstrap CI for a GPD return level.
     *
     * @param exc      Exceedances (subsampled to maxExceedancesBootstrap if large).
     * @param gpd      Fitted GPD (for point estimate).
     * @param T        Return period.
     * @param level    Confidence level.
     * @param lower    Output lower bound.
     * @param upper    Output upper bound.
     */
    void _bootstrapCiGpd(const std::vector<double>& exc,
                           const GpdFitResult&        gpd,
                           double T, double level,
                           double& lower, double& upper) const;

    /**
     * @brief Delta method CI for a GPD return level (fast, unreliable for large T).
     */
    void _deltaCiGpd(const std::vector<double>& exc,
                      const GpdFitResult&        gpd,
                      double T, double level,
                      double& lower, double& upper) const;

    // -- GoF tests ------------------------------------------------------------

    /**
     * @brief Anderson-Darling and KS tests for GPD fit.
     */
    GoFResult _gofGpd(const std::vector<double>& exc,
                       const GpdFitResult& gpd) const;

    /**
     * @brief Anderson-Darling and KS tests for GEV fit.
     */
    GoFResult _gofGev(const std::vector<double>& maxima,
                       const GevFitResult& gev) const;

    // -- Output ---------------------------------------------------------------

    void _writeProtocol(const EvtResult& result,
                         const TimeSeries& ts,
                         const std::string& datasetName) const;

    void _writeCsv(const EvtResult& result,
                    const std::string& datasetName,
                    const std::string& component) const;

    // -- Helpers --------------------------------------------------------------

    /**
     * @brief Extract valid (non-NaN) values from a time series.
     */
    static std::vector<double> _extractValid(const TimeSeries& ts);

    /**
     * @brief Estimate median time step in hours from the time series.
     */
    static double _medianDtHours(const TimeSeries& ts);

    /**
     * @brief Brent root finding on a 1D function over [a, b].
     */
    static double _brentRoot(std::function<double(double)> f,
                              double a, double b,
                              double tol = 1.0e-6,
                              int maxIter = 100);
};

} // namespace loki::evt
