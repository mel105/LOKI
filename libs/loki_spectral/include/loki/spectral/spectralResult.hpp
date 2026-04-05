#pragma once

#include <string>
#include <vector>

namespace loki::spectral {

// -----------------------------------------------------------------------------
//  SpectralPeak
// -----------------------------------------------------------------------------

/**
 * @brief Describes a single dominant peak identified in the spectrum.
 *
 * Frequencies are always in cycles per day (cpd); periods in days.
 * For the FFT path, fap is set to -1.0 (not computed).
 */
struct SpectralPeak {
    double freqCpd;    ///< Frequency in cycles per day.
    double periodDays; ///< Period in days (= 1 / freqCpd).
    double power;      ///< Normalised power in [0, 1] relative to the strongest peak.
    double fap;        ///< False alarm probability (Lomb-Scargle only; -1.0 for FFT).
    int    rank;       ///< 1 = strongest peak, 2 = second strongest, ...
};

// -----------------------------------------------------------------------------
//  SpectralResult
// -----------------------------------------------------------------------------

/**
 * @brief Full output of one spectral analysis run on a single TimeSeries.
 *
 * The frequencies / power vectors form the complete periodogram or PSD.
 * amplitudes holds the amplitude spectrum (physical units = sqrt(2 * PSD * df))
 * and phases holds the phase angle in radians at each frequency bin.
 * Both are filled by FftAnalyzer; for Lomb-Scargle, amplitudes = sqrt(power)
 * and phases is empty.
 * peaks contains only the top-N statistically significant entries.
 */
struct SpectralResult {
    std::vector<double>       frequencies;  ///< Frequency axis in cycles per day (cpd).
    std::vector<double>       power;        ///< PSD or normalised periodogram power.
    std::vector<double>       amplitudes;   ///< Amplitude spectrum |X[k]| (same length as frequencies).
    std::vector<double>       phases;       ///< Phase spectrum atan2(Im, Re) in radians (FFT only).
    std::vector<SpectralPeak> peaks;        ///< Top peaks sorted by power descending.
    std::string               method;       ///< "fft" | "lomb_scargle"
    double                    samplingStepDays; ///< Median sampling step in days.
    double                    spanDays;         ///< Total time span of the series in days.
    int                       nObs;             ///< Number of observations used.
};

// -----------------------------------------------------------------------------
//  SpectrogramResult
// -----------------------------------------------------------------------------

/**
 * @brief Output of a short-time Fourier transform (STFT) spectrogram run.
 *
 * power[i][j] is the spectral power at time window i and frequency bin j.
 * times contains the MJD of each window centre.
 * frequencies is in cycles per day, matching SpectralResult::frequencies layout.
 */
struct SpectrogramResult {
    std::vector<double>              times;        ///< MJD of each STFT window centre.
    std::vector<double>              frequencies;  ///< Frequency axis in cycles per day.
    std::vector<std::vector<double>> power;        ///< power[timeIdx][freqIdx]
};

} // namespace loki::spectral