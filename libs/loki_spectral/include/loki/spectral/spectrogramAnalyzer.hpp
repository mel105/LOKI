#pragma once

#include "loki/spectral/spectralResult.hpp"
#include "loki/spectral/fftAnalyzer.hpp"

#include "loki/core/config.hpp"

#include <vector>

namespace loki::spectral {

// -----------------------------------------------------------------------------
//  SpectrogramParams
// -----------------------------------------------------------------------------

/**
 * @brief Parameters forwarded to SpectrogramAnalyzer from SpectralConfig.
 */
struct SpectrogramParams {
    int    windowLength   {1461};  ///< STFT window in samples.
    double overlap        {0.5};   ///< Window overlap fraction [0, 1).
    double focusPeriodMin {0.0};   ///< Zoom lower bound in days (0 = no zoom).
    double focusPeriodMax {0.0};   ///< Zoom upper bound in days (0 = no zoom).
    std::string windowFunction {"hann"};  ///< Window type (reuses FftAnalyzer::buildWindow).
};

// -----------------------------------------------------------------------------
//  SpectrogramAnalyzer
// -----------------------------------------------------------------------------

/**
 * @brief Computes a Short-Time Fourier Transform (STFT) spectrogram.
 *
 * Slides a windowed FFT across the signal with a configurable overlap,
 * producing a 2-D power matrix indexed by [time_window][frequency_bin].
 *
 * ### Algorithm
 * 1. Build the window function (same types as FftAnalyzer).
 * 2. For each frame starting at hop * frameIdx:
 *    a. Extract the segment of length windowLength.
 *    b. Apply window and compute single-sided periodogram via FftAnalyzer internals.
 *    c. Store in result.power[frameIdx].
 * 3. Frequency axis in cpd; time axis in MJD of each frame centre.
 * 4. Optionally restrict the frequency axis to the focus period window.
 *
 * The output SpectrogramResult can be passed directly to PlotSpectral.
 */
class SpectrogramAnalyzer {
public:

    /**
     * @brief Constructs the analyzer.
     * @param params     Spectrogram configuration.
     * @param stepDays   Uniform sampling step of the input signal in days.
     */
    explicit SpectrogramAnalyzer(const SpectrogramParams& params, double stepDays);

    /**
     * @brief Computes the STFT spectrogram.
     *
     * @param times  Observation times in MJD (uniform, gap-free).
     * @param values Signal values (same length as times, NaN-free).
     * @return SpectrogramResult with times (MJD), frequencies (cpd),
     *         and power[frame][bin].
     * @throws loki::DataException if the signal is shorter than one window.
     */
    [[nodiscard]]
    SpectrogramResult compute(const std::vector<double>& times,
                              const std::vector<double>& values) const;

private:

    SpectrogramParams m_params;
    double            m_stepDays;

    /**
     * @brief Computes single-sided power spectrum of one windowed segment.
     *
     * @param segment  Signal block of length windowLength (already extracted).
     * @param window   Pre-built window coefficients (same length).
     * @param fsCpd    Sampling frequency in cpd.
     * @return Power vector (single-sided, length = nfft/2 + 1).
     */
    [[nodiscard]] static std::vector<double>
    _segmentPower(const std::vector<double>& segment,
                  const std::vector<double>& window,
                  double fsCpd);

    /**
     * @brief Builds the frequency axis for a given FFT size.
     * @param nfft   FFT length (power of two).
     * @param fsCpd  Sampling frequency in cpd.
     * @return Single-sided frequency vector (length = nfft/2 + 1).
     */
    [[nodiscard]] static std::vector<double>
    _buildFreqAxis(int nfft, double fsCpd);
};

} // namespace loki::spectral
