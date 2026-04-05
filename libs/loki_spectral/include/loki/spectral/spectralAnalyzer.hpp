#pragma once

#include "loki/spectral/spectralResult.hpp"

#include "loki/core/config.hpp"
#include "loki/timeseries/timeSeries.hpp"

#include <string>

namespace loki::spectral {

// -----------------------------------------------------------------------------
//  SpectralAnalyzer
// -----------------------------------------------------------------------------

/**
 * @brief Orchestrator for the loki_spectral pipeline.
 *
 * For each TimeSeries passed to run():
 *   1. Gap-fill if needed (via GapFiller).
 *   2. Select method: "auto" inspects uniformity, "fft" or "lomb_scargle"
 *      overrides.
 *   3. Dispatch to FftAnalyzer or LombScargle.
 *   4. Detect and rank peaks via PeakDetector.
 *   5. Optionally compute STFT spectrogram.
 *   6. Delegate plotting to PlotSpectral.
 *   7. Write the analysis protocol.
 *
 * ### Method auto-selection rule
 * A series is considered uniformly sampled when fewer than 5 % of consecutive
 * time differences exceed 1.1x the median step. Uniformly sampled -> FFT;
 * otherwise -> Lomb-Scargle.
 */
class SpectralAnalyzer {
public:

    /**
     * @brief Constructs the analyzer bound to the given application config.
     * @param cfg Full AppConfig (SpectralConfig, PlotConfig, paths used).
     */
    explicit SpectralAnalyzer(const loki::AppConfig& cfg);

    /**
     * @brief Runs the full spectral analysis pipeline for one TimeSeries.
     *
     * @param ts          Input time series (may contain gaps).
     * @param datasetName Stem of the source file (used for output naming).
     * @throws loki::DataException     if the series is too short after gap filling.
     * @throws loki::AlgorithmException on internal numerical failures.
     * @throws loki::IoException        on plot or protocol write failures.
     */
    void run(const loki::TimeSeries& ts, const std::string& datasetName);

private:

    loki::AppConfig m_cfg;

    /**
     * @brief Determines whether the series is uniformly sampled.
     *
     * Returns true when < 5 % of consecutive steps exceed 1.1 * median_step.
     *
     * @param times Observation times in days (MJD).
     */
    [[nodiscard]] static bool _isUniform(const std::vector<double>& times);

    /**
     * @brief Extracts parallel (times, values) vectors from a TimeSeries,
     *        skipping NaN observations.
     */
    static void _extractTimesValues(const loki::TimeSeries& ts,
                                    std::vector<double>& times,
                                    std::vector<double>& values);

    /**
     * @brief Writes the analysis protocol to OUTPUT/PROTOCOLS/.
     *
     * @param result      Populated SpectralResult.
     * @param datasetName File stem.
     * @param component   Series componentName from metadata.
     */
    void _writeProtocol(const SpectralResult& result,
                         const std::string& datasetName,
                         const std::string& component) const;
};

} // namespace loki::spectral
