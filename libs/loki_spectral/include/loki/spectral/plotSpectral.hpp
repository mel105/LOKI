#pragma once

#include "loki/spectral/spectralResult.hpp"

#include "loki/core/config.hpp"
#include "loki/timeseries/timeSeries.hpp"

#include <filesystem>
#include <string>

namespace loki::spectral {

// -----------------------------------------------------------------------------
//  PlotSpectral
// -----------------------------------------------------------------------------

/**
 * @brief Generates all spectral analysis plots via gnuplot.
 *
 * Two plot types are available, each gated by a PlotConfig flag:
 *
 * - **PSD / periodogram** (plots.spectralPsd): power spectral density or
 *   normalised Lomb-Scargle periodogram on a log-log scale with dominant
 *   peaks annotated.
 *
 * - **Spectrogram** (plots.spectralSpectrogram): 2-D time-frequency heatmap
 *   produced by STFT. Only generated when SpectralConfig::spectrogram.enabled
 *   is true. X-axis: calendar time (MJD); Y-axis: period in days (log scale);
 *   colour: power (pm3d palette).
 *
 * All output files follow the project naming convention:
 *   spectral_<dataset>_<component>_<plottype>.<format>
 *
 * Temporary data files use the .tmp_ prefix and are deleted after each
 * gnuplot call.
 */
class PlotSpectral {
public:

    /**
     * @brief Constructs the plotter bound to the given application config.
     * @param cfg Full AppConfig (PlotConfig, SpectralConfig, imgDir used).
     */
    explicit PlotSpectral(const loki::AppConfig& cfg);

    /**
     * @brief Generates all enabled plots for a completed SpectralResult.
     *
     * @param result      Populated SpectralResult (including detected peaks).
     * @param ts          Original TimeSeries (for axis labels and metadata).
     * @param datasetName File stem used in output filenames.
     */
    void plot(const SpectralResult& result,
              const loki::TimeSeries& ts,
              const std::string& datasetName) const;

    /**
     * @brief Generates the spectrogram heatmap from a SpectrogramResult.
     *
     * Called from SpectralAnalyzer::run() after SpectrogramAnalyzer::compute().
     *
     * @param sgResult    Populated SpectrogramResult.
     * @param ts          Original TimeSeries (for axis labels and metadata).
     * @param datasetName File stem used in output filenames.
     */
    void plotSpectrogram(const SpectrogramResult& sgResult,
                         const loki::TimeSeries& ts,
                         const std::string& datasetName) const;

private:

    loki::AppConfig m_cfg;

    /**
     * @brief PSD / periodogram plot with annotated peaks.
     *
     * X-axis: period in days (log scale).
     * Y-axis: power (log scale).
     * Peaks annotated with rank number.
     */
    void _plotPsd(const SpectralResult& result,
                  const loki::TimeSeries& ts,
                  const std::string& datasetName) const;

    /**
     * @brief Amplitude spectrum plot.
     *
     * X-axis: period in days (log scale).
     * Y-axis: amplitude |X[k]| (linear scale, same units as input signal).
     */
    void _plotAmplitude(const SpectralResult& result,
                        const loki::TimeSeries& ts,
                        const std::string& datasetName) const;

    /**
     * @brief Phase spectrum plot (FFT only).
     *
     * X-axis: period in days (log scale).
     * Y-axis: phase in radians [-pi, pi].
     * Only plotted for the FFT method; skipped for Lomb-Scargle.
     */
    void _plotPhase(const SpectralResult& result,
                    const loki::TimeSeries& ts,
                    const std::string& datasetName) const;

    /**
     * @brief Converts a filesystem path to forward-slash form for gnuplot
     *        on Windows (gnuplot does not accept backslashes in paths).
     */
    [[nodiscard]] static std::string fwdSlash(const std::filesystem::path& p);
};

} // namespace loki::spectral