#pragma once

#include "loki/kalman/kalmanFilter.hpp"
#include "loki/kalman/kalmanModel.hpp"
#include "loki/kalman/kalmanResult.hpp"
#include "loki/kalman/rtsSmoother.hpp"

#include "loki/core/config.hpp"
#include "loki/timeseries/timeSeries.hpp"

#include <Eigen/Dense>

#include <string>
#include <vector>

namespace loki::kalman {

/**
 * @brief Orchestrates the full loki_kalman analysis pipeline.
 *
 * For each input TimeSeries, run() performs these steps in order:
 *
 *  1. Gap detection and filling (GapFiller -- LINEAR or MEDIAN_YEAR).
 *     NaN observations that remain after gap filling are handled inside
 *     the filter as predict-only epochs.
 *  2. Sampling interval estimation (dt in seconds, from median of time diffs).
 *  3. Noise estimation (manual / heuristic / EM) to determine Q and R.
 *  4. KalmanModel construction via KalmanModelBuilder.
 *  5. KalmanFilter forward pass.
 *  6. RTS smoothing (if cfg.kalman.smoother == "rts").
 *  7. Forecast generation (if cfg.kalman.forecast.steps > 0).
 *  8. KalmanResult assembly.
 *  9. PlotKalman::plotAll().
 * 10. Protocol file written to protocolsDir.
 *
 * The analyzer does not own any persistent state beyond the AppConfig.
 * It is safe to call run() multiple times with different series.
 */
class KalmanAnalyzer {
public:

    /**
     * @brief Constructs the analyzer with the given application configuration.
     * @param cfg Full application configuration (kalman section, paths, plot flags).
     */
    explicit KalmanAnalyzer(const AppConfig& cfg);

    /**
     * @brief Runs the complete pipeline for one time series.
     *
     * @param series      Input time series (raw, possibly with gaps and outliers).
     * @param datasetName Stem of the input file (used in output file names).
     * @throws DataException      if the series is too short for the configured model.
     * @throws ConfigException    if a model or noise method name is unrecognised.
     * @throws AlgorithmException if the EM estimator fails to converge.
     */
    void run(const TimeSeries& series, const std::string& datasetName);

private:

    AppConfig m_cfg;

    // ---- pipeline steps -----------------------------------------------------

    /**
     * @brief Fills gaps in the series according to kalman.gapFillStrategy.
     *
     * For 6h climatological data (dt ~ 0.25 days) with series >= minSeriesYears:
     *   uses MEDIAN_YEAR strategy.
     * Otherwise: LINEAR interpolation.
     *
     * After gap filling, any remaining NaN values are handled inside
     * the Kalman filter as predict-only epochs.
     *
     * @param series Input series (may contain NaN and time gaps).
     * @return Gap-filled series (same length; gaps reconstructed or still NaN if
     *         gap length exceeds gapFillMaxLength).
     */
    [[nodiscard]]
    TimeSeries fillGaps(const TimeSeries& series) const;

    /**
     * @brief Estimates the sampling interval from the series.
     *
     * Computes the median of consecutive MJD differences and converts to seconds.
     * Robust to occasional outlier jumps (gaps that were not filled).
     *
     * @param series Input series (at least 2 observations).
     * @return Sampling interval dt in seconds.
     */
    [[nodiscard]]
    double estimateDt(const TimeSeries& series) const;

    /**
     * @brief Extracts raw measurement values and times from a TimeSeries.
     *
     * Missing observations (flag != 0 or gap-unfilled positions) remain as NaN.
     *
     * @param series Input series.
     * @param times  Output: MJD timestamps.
     * @param values Output: measurement values (NaN for missing epochs).
     */
    static void extractMeasurements(const TimeSeries&    series,
                                    std::vector<double>& times,
                                    std::vector<double>& values);

    /**
     * @brief Estimates Q and R according to kalman.noise.estimation.
     *
     * manual:    Q = cfg.kalman.noise.Q, R = cfg.kalman.noise.R (no estimation)
     * heuristic: R = var(measurements), Q = R / smoothingFactor
     * em:        Runs EmEstimator with QInit/RInit until convergence
     *
     * @param measurements Raw measurement values (NaN for missing).
     * @param model        Initial model (Q and R will be overwritten on return).
     * @param qOut         Estimated process noise scalar.
     * @param rOut         Estimated measurement noise scalar.
     * @param emIterations EM iterations actually performed (0 if not EM).
     * @param emConverged  True if EM converged (false if not EM or did not converge).
     */
    void estimateNoise(const std::vector<double>& measurements,
                       double&                    qOut,
                       double&                    rOut,
                       int&                       emIterations,
                       bool&                      emConverged) const;

    /**
     * @brief Generates forecast epochs and predicted values.
     *
     * Starts from the last filter state and applies repeated predict steps.
     * dt is used to advance MJD timestamps.
     *
     * @param lastXFilt  Filtered state vector at last epoch.
     * @param lastPFilt  Filtered covariance at last epoch.
     * @param model      Kalman model (F, Q).
     * @param lastMjd    MJD of last observation.
     * @param dt         Sampling interval in seconds.
     * @param steps      Number of steps to forecast.
     * @param forecastTimes Output MJD timestamps.
     * @param forecastState Output predicted first state component.
     * @param forecastStd   Output predicted std dev.
     */
    static void generateForecast(const Eigen::VectorXd&  lastXFilt,
                                 const Eigen::MatrixXd&  lastPFilt,
                                 const KalmanModel&      model,
                                 double                  lastMjd,
                                 double                  dt,
                                 int                     steps,
                                 std::vector<double>&    forecastTimes,
                                 std::vector<double>&    forecastState,
                                 std::vector<double>&    forecastStd);

    /**
     * @brief Assembles a KalmanResult from all pipeline outputs.
     */
    [[nodiscard]]
    static KalmanResult assembleResult(
        const std::vector<double>&        times,
        const std::vector<double>&        original,
        const std::vector<FilterStep>&    steps,
        const std::vector<SmootherStep>&  smootherSteps,
        const std::vector<double>&        forecastTimes,
        const std::vector<double>&        forecastState,
        const std::vector<double>&        forecastStd,
        double                            logLikelihood,
        double                            q,
        double                            r,
        int                               emIterations,
        bool                              emConverged,
        const std::string&                modelName,
        const std::string&                noiseMethod,
        const std::string&                smoother);

    /**
     * @brief Writes a plain-text protocol to protocolsDir.
     */
    void writeProtocol(const KalmanResult& result,
                       const std::string&  datasetName,
                       const std::string&  componentName) const;
};

} // namespace loki::kalman
