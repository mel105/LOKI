#pragma once

#include <loki/core/config.hpp>
#include <loki/spline/splineResult.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <filesystem>
#include <string>

namespace loki::spline {

/**
 * @brief Orchestrates the loki_spline B-spline approximation pipeline.
 *
 * Pipeline steps executed by run():
 *   1. Gap filling (configurable strategy).
 *   2. Knot placement detection (auto-select chord_length for non-uniform data).
 *   3. Exact interpolation guard (throw ConfigException for oversized series).
 *   4. Control point selection:
 *        a. Manual:    nControlPoints > 0 -- skip CV, fit directly.
 *        b. Automatic: nControlPoints = 0 -- run k-fold CV to select optimal nCtrl,
 *                      then fit on full data.
 *   5. Final B-spline fit on full series.
 *   6. Residual statistics, CI band construction (residual-based).
 *   7. Protocol and CSV output.
 *   8. Plots (controlled by PlotConfig flags).
 */
class SplineAnalyzer {
public:

    /**
     * @brief Construct the analyzer with the application configuration.
     * @param cfg  Fully parsed AppConfig (output paths, SplineConfig, PlotConfig).
     */
    explicit SplineAnalyzer(const AppConfig& cfg);

    /**
     * @brief Run the full B-spline pipeline on one time series.
     *
     * @param series      Input time series (may contain NaN / gaps).
     * @param datasetName Stem of the input file name (used for output file names).
     * @throws DataException      if the series is too short after gap filling.
     * @throws ConfigException    if spline parameters are invalid (e.g.
     *                            exact_interpolation on a large series).
     * @throws AlgorithmException if the B-spline LSQ system is rank-deficient.
     */
    void run(const TimeSeries& series, const std::string& datasetName);

private:

    const AppConfig& m_cfg;

    // -- Pipeline steps -------------------------------------------------------

    /**
     * @brief Detect whether the series has non-uniform sampling.
     *
     * Computes the coefficient of variation of the time step differences.
     * Returns true if CV > 0.1, indicating that chord-length knots are
     * preferable to uniform knots.
     */
    static bool _isNonUniform(const TimeSeries& ts);

    /**
     * @brief Compute the z-quantile for the configured confidence level.
     *
     * Uses the same bisection on erf as KrigingBase::_zQuantile.
     * Accurate for levels in [0.80, 0.999].
     */
    static double _zQuantile(double confidenceLevel);

    /**
     * @brief Write a plain-text analysis protocol to OUTPUT/PROTOCOLS/.
     */
    void _writeProtocol(const SplineResult& result,
                        const TimeSeries&   ts,
                        const std::string&  datasetName) const;

    /**
     * @brief Write fitted values and CI to a semicolon-delimited CSV.
     *
     * Columns: index;mjd;utc;observed;fitted;residual;ci_lower;ci_upper
     */
    void _writeCsv(const SplineResult& result,
                   const TimeSeries&   tsFilled,
                   const std::string&  datasetName,
                   const std::string&  component) const;
};

} // namespace loki::spline
