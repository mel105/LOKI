#pragma once

#include "loki/spectral/spectralResult.hpp"

#include <string>
#include <vector>

namespace loki::spectral {

// -----------------------------------------------------------------------------
//  PeakDetectorParams
// -----------------------------------------------------------------------------

/**
 * @brief Parameters forwarded to PeakDetector from SpectralConfig::peaks
 *        and SpectralConfig::lombScargle.
 */
struct PeakDetectorParams {
    int    topN          {10};    ///< Maximum number of peaks to return.
    double minPeriodDays {0.0};   ///< Ignore peaks with period < this (0 = no limit).
    double maxPeriodDays {0.0};   ///< Ignore peaks with period > this (0 = no limit).
    double fapThreshold  {0.01};  ///< Maximum FAP for Lomb-Scargle peaks (ignored for FFT).
};

// -----------------------------------------------------------------------------
//  PeakDetector
// -----------------------------------------------------------------------------

/**
 * @brief Identifies statistically significant local maxima in a periodogram
 *        or PSD, ranks them by power, and annotates them with FAP.
 *
 * ### Algorithm
 * 1. Find all local maxima: power[k] > power[k-1] && power[k] > power[k+1].
 * 2. Estimate the noise floor as the median of the full power vector.
 * 3. Retain only maxima whose power exceeds the noise floor.
 * 4. Apply period window filter (minPeriodDays / maxPeriodDays).
 * 5. Sort by power descending and keep the top N.
 * 6. For Lomb-Scargle: compute FAP via LombScargle::fap() and optionally
 *    filter by fapThreshold.
 * 7. Assign sequential rank starting at 1.
 * 8. Normalise power to [0, 1] relative to the strongest peak.
 */
class PeakDetector {
public:

    /**
     * @brief Constructs the detector with the given parameters.
     */
    explicit PeakDetector(const PeakDetectorParams& params);

    /**
     * @brief Detects peaks in a SpectralResult and populates result.peaks.
     *
     * The method field of result determines whether FAP filtering is applied
     * ("lomb_scargle") or skipped ("fft").
     *
     * @param result SpectralResult produced by FftAnalyzer or LombScargle.
     *               result.peaks is overwritten.
     * @throws loki::DataException if result.frequencies or result.power is empty.
     */
    void detect(SpectralResult& result) const;

private:

    PeakDetectorParams m_params;

    /**
     * @brief Returns indices of local maxima in power (strict, ignores endpoints).
     */
    [[nodiscard]] static std::vector<int>
    _localMaxima(const std::vector<double>& power);

    /**
     * @brief Returns the median of a vector (non-destructive copy).
     */
    [[nodiscard]] static double _median(std::vector<double> v);
};

} // namespace loki::spectral
