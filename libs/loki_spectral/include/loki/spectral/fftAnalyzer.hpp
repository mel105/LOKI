#pragma once

#include "loki/spectral/spectralResult.hpp"

#include <complex>
#include <string>
#include <vector>

namespace loki::spectral {

// -----------------------------------------------------------------------------
//  FftParams
// -----------------------------------------------------------------------------

/**
 * @brief Parameters forwarded to FftAnalyzer from SpectralConfig::fft.
 */
struct FftParams {
    std::string windowFunction {"hann"};  ///< "hann"|"hamming"|"blackman"|"flattop"|"rectangular"
    bool        welch          {false};   ///< Enable Welch averaged PSD.
    int         welchSegments  {8};       ///< Number of overlapping segments.
    double      welchOverlap   {0.5};     ///< Segment overlap fraction [0, 1).
};

// -----------------------------------------------------------------------------
//  FftFrame  (internal result of one periodogram call)
// -----------------------------------------------------------------------------

/**
 * @brief Internal result of a single periodogram computation.
 *
 * Carries frequencies, power, amplitude, and phase so the orchestrator
 * can populate SpectralResult fully without a second FFT pass.
 */
struct FftFrame {
    std::vector<double> frequencies; ///< cpd
    std::vector<double> power;       ///< Single-sided PSD
    std::vector<double> amplitudes;  ///< |X[k]| in signal units
    std::vector<double> phases;      ///< atan2(Im, Re) in radians
};

// -----------------------------------------------------------------------------
//  FftAnalyzer
// -----------------------------------------------------------------------------

/**
 * @brief Computes a single-sided power spectral density via Cooley-Tukey
 *        radix-2 FFT, with optional Welch averaging.
 *
 * Amplitude and phase spectra are computed alongside the PSD in a single pass.
 * For Welch averaging, amplitudes and phases are averaged across segments
 * (phase averaging is circular via complex mean).
 */
class FftAnalyzer {
public:

    /**
     * @brief Constructs the analyzer.
     * @param params          FFT / Welch configuration.
     * @param samplingStepDays Uniform sampling interval in days.
     */
    explicit FftAnalyzer(const FftParams& params, double samplingStepDays);

    /**
     * @brief Computes PSD, amplitude spectrum, and phase spectrum.
     *
     * @param values Gap-free, uniformly sampled observations (>= 4 samples).
     * @return SpectralResult with frequencies, power, amplitudes, phases filled.
     *         peaks is left empty (filled by PeakDetector).
     * @throws loki::DataException if values is shorter than 4 samples.
     */
    [[nodiscard]] SpectralResult compute(const std::vector<double>& values) const;

    // -- Static helpers -------------------------------------------------------

    /// Returns the smallest power of two >= n.
    [[nodiscard]] static int nextPow2(int n);

    /// Builds a normalised window of the requested type (length n).
    [[nodiscard]] static std::vector<double> buildWindow(const std::string& type, int n);

private:

    FftParams m_params;
    double    m_stepDays;

    /// In-place Cooley-Tukey radix-2 DIT FFT. Length must be a power of two.
    static void _fft(std::vector<std::complex<double>>& x);

    /**
     * @brief Computes one full periodogram frame (PSD + amplitude + phase).
     */
    [[nodiscard]] FftFrame
    _periodogram(const std::vector<double>& values,
                 const std::vector<double>& window,
                 double fsCpd) const;

    /**
     * @brief Welch-averaged frame.
     */
    [[nodiscard]] FftFrame
    _welch(const std::vector<double>& values, double fsCpd) const;
};

} // namespace loki::spectral