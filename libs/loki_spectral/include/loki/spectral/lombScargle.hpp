#pragma once

#include "loki/spectral/spectralResult.hpp"

#include <vector>

namespace loki::spectral {

// -----------------------------------------------------------------------------
//  LombScargleParams
// -----------------------------------------------------------------------------

/**
 * @brief Parameters forwarded to LombScargle from SpectralConfig::lombScargle.
 */
struct LombScargleParams {
    double oversampling {4.0};    ///< Frequency grid oversampling factor (>= 1).
    bool   fastNfft     {false};  ///< Reserved: NFFT approximation for n >= 100k.
    double fapThreshold {0.01};   ///< FAP significance threshold for peak reporting.
};

// -----------------------------------------------------------------------------
//  LombScargle
// -----------------------------------------------------------------------------

/**
 * @brief Lomb-Scargle periodogram for unevenly sampled or gappy time series.
 *
 * Computes the normalised Lomb-Scargle periodogram (Scargle 1982) over a
 * frequency grid spanning [1/T, fs/2] where T is the total time span and
 * fs = 1 / median_step. The grid density is controlled by the oversampling
 * factor (default 4).
 *
 * ### False Alarm Probability
 * FAP is estimated using the analytic approximation of Baluev (2008):
 *
 *   FAP(z) ~ W * tau(z) * exp(-z)
 *
 * where W is the effective number of independent frequencies and tau(z) is
 * a correction factor derived from the spectral window. This is more accurate
 * than the classical M * exp(-z) approximation for large oversampled grids.
 *
 * ### Normalisation
 * Power is normalised to [0, 1] such that a sinusoid with amplitude equal to
 * the RMS of the data produces a peak of 1.0 (Horne & Baliunas 1986).
 *
 * ### NFFT approximation
 * When fastNfft is true and n >= 100k, the Press & Rybicki (1989)
 * extirpolation onto a regular grid is used. Currently falls back to the
 * direct O(n * nFreq) computation (reserved for future implementation).
 */
class LombScargle {
public:

    /**
     * @brief Constructs the analyzer.
     * @param params Lomb-Scargle configuration.
     */
    explicit LombScargle(const LombScargleParams& params);

    /**
     * @brief Computes the Lomb-Scargle periodogram.
     *
     * @param times  Observation times in days (MJD or any consistent day unit).
     * @param values Observed values (same length as times, NaN-free).
     * @return SpectralResult with frequencies (cpd), power (normalised [0,1]),
     *         and samplingStepDays / spanDays / nObs filled in.
     *         The method field is set to "lomb_scargle".
     *         peaks is left empty (filled later by PeakDetector).
     * @throws loki::DataException if times or values have fewer than 4 elements,
     *         or if sizes differ.
     */
    [[nodiscard]] SpectralResult compute(const std::vector<double>& times,
                                         const std::vector<double>& values) const;

    /**
     * @brief Computes the FAP for a given peak power z using Baluev (2008).
     *
     * @param z       Observed peak power (normalised Lomb-Scargle statistic).
     * @param nFreqs  Number of frequencies in the grid (proxy for W).
     * @param nObs    Number of observations.
     * @return FAP in [0, 1].
     */
    [[nodiscard]] static double fap(double z, int nFreqs, int nObs);

private:

    LombScargleParams m_params;

    /**
     * @brief Builds the frequency grid [fMin, fMax] with oversampling.
     *
     * fMin = 1 / spanDays
     * fMax = 1 / (2 * medianStep)
     * nFreqs = oversampling * spanDays * fMax
     */
    [[nodiscard]] std::vector<double>
    _buildFrequencyGrid(double spanDays, double medianStepDays) const;

    /**
     * @brief Subtracts the mean from values (pre-processing step).
     */
    [[nodiscard]] static std::vector<double>
    _demean(const std::vector<double>& values);

    /**
     * @brief Computes the normalised periodogram power at a single frequency.
     *
     * Uses the Lomb-Scargle formula with the tau offset for orthogonality
     * (Scargle 1982, eq. 6-7).
     *
     * @param times   Observation times in days.
     * @param y       Mean-subtracted values.
     * @param yvar    Variance of y (for normalisation).
     * @param freqCpd Frequency in cycles per day.
     */
    [[nodiscard]] static double
    _power(const std::vector<double>& times,
           const std::vector<double>& y,
           double yvar,
           double freqCpd);
};

} // namespace loki::spectral
