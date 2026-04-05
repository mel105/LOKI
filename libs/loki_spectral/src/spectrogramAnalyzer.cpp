#include "loki/spectral/spectrogramAnalyzer.hpp"

#include "loki/core/exceptions.hpp"
#include "loki/core/logger.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

using namespace loki;

namespace loki::spectral {

// -----------------------------------------------------------------------------
//  Construction
// -----------------------------------------------------------------------------

SpectrogramAnalyzer::SpectrogramAnalyzer(const SpectrogramParams& params,
                                         double stepDays)
    : m_params(params)
    , m_stepDays(stepDays)
{
    if (m_stepDays <= 0.0) {
        throw DataException(
            "SpectrogramAnalyzer: stepDays must be positive.");
    }
    if (m_params.windowLength < 4) {
        throw DataException(
            "SpectrogramAnalyzer: windowLength must be >= 4, got "
            + std::to_string(m_params.windowLength) + ".");
    }
    if (m_params.overlap < 0.0 || m_params.overlap >= 1.0) {
        throw DataException(
            "SpectrogramAnalyzer: overlap must be in [0, 1), got "
            + std::to_string(m_params.overlap) + ".");
    }
}

// -----------------------------------------------------------------------------
//  Public: compute
// -----------------------------------------------------------------------------

SpectrogramResult SpectrogramAnalyzer::compute(const std::vector<double>& times,
                                                const std::vector<double>& values) const
{
    const int n      = static_cast<int>(values.size());
    const int winLen = m_params.windowLength;

    if (n < winLen) {
        throw DataException(
            "SpectrogramAnalyzer: signal length (" + std::to_string(n)
            + ") is shorter than window (" + std::to_string(winLen) + ").");
    }

    const double fsCpd = 1.0 / m_stepDays;
    const int    hop   = std::max(1,
        static_cast<int>(std::round(static_cast<double>(winLen)
                                    * (1.0 - m_params.overlap))));
    const int    nfft  = FftAnalyzer::nextPow2(winLen);

    // Build window once
    const std::vector<double> window =
        FftAnalyzer::buildWindow(m_params.windowFunction, winLen);

    // Full frequency axis (before focus filtering)
    const std::vector<double> allFreqs = _buildFreqAxis(nfft, fsCpd);
    const int nBins = static_cast<int>(allFreqs.size());

    // Determine frequency index range based on focus period window.
    // focusPeriodMax (e.g. 400 days) -> lowest frequency  -> lowest bin index
    // focusPeriodMin (e.g. 300 days) -> highest frequency -> highest bin index
    int binStart = 0;
    int binEnd   = nBins - 1;

    if (m_params.focusPeriodMax > 0.0) {
        // Long period -> low frequency -> search from start for first bin >= fLow
        const double fLow = 1.0 / m_params.focusPeriodMax;
        binStart = 0;
        for (int k = 0; k < nBins; ++k) {
            if (allFreqs[static_cast<std::size_t>(k)] >= fLow) {
                binStart = k;
                break;
            }
        }
    }
    if (m_params.focusPeriodMin > 0.0) {
        // Short period -> high frequency -> search from end for last bin <= fHigh
        const double fHigh = 1.0 / m_params.focusPeriodMin;
        binEnd = nBins - 1;
        for (int k = nBins - 1; k >= 0; --k) {
            if (allFreqs[static_cast<std::size_t>(k)] <= fHigh) {
                binEnd = k;
                break;
            }
        }
    }
    // Sanity: binEnd must be >= binStart
    if (binEnd < binStart) {
        LOKI_WARNING(
            "SpectrogramAnalyzer: focus period window ["
            + std::to_string(m_params.focusPeriodMin) + ", "
            + std::to_string(m_params.focusPeriodMax)
            + "] produced no frequency bins -- using full frequency axis.");
        binStart = 0;
        binEnd   = nBins - 1;
    }

    // Slice the frequency axis to the focus window
    std::vector<double> freqs(
        allFreqs.begin() + binStart,
        allFreqs.begin() + binEnd + 1);

    // --- Compute STFT frames -------------------------------------------------
    std::vector<double>              frameTimes;
    std::vector<std::vector<double>> power;

    for (int start = 0; start + winLen <= n; start += hop) {
        // Extract segment
        std::vector<double> seg(
            values.begin() + start,
            values.begin() + start + winLen);

        // Frame centre time (MJD)
        const double tCentre = times[static_cast<std::size_t>(start + winLen / 2)];
        frameTimes.push_back(tCentre);

        // Compute power spectrum of this frame
        const std::vector<double> fullPower = _segmentPower(seg, window, fsCpd);

        // Slice to focus frequency range
        std::vector<double> framePower(
            fullPower.begin() + binStart,
            fullPower.begin() + binEnd + 1);

        power.push_back(std::move(framePower));
    }

    LOKI_INFO("SpectrogramAnalyzer: computed " + std::to_string(frameTimes.size())
              + " frames  x  " + std::to_string(freqs.size()) + " freq bins"
              + "  (hop=" + std::to_string(hop) + " samples)");

    SpectrogramResult result;
    result.times       = std::move(frameTimes);
    result.frequencies = std::move(freqs);
    result.power       = std::move(power);
    return result;
}

// -----------------------------------------------------------------------------
//  Private static: _segmentPower
// -----------------------------------------------------------------------------

std::vector<double>
SpectrogramAnalyzer::_segmentPower(const std::vector<double>& segment,
                                    const std::vector<double>& window,
                                    double fsCpd)
{
    const int n    = static_cast<int>(segment.size());
    const int nfft = FftAnalyzer::nextPow2(n);

    // Coherent power normalisation
    double wss = 0.0;
    for (const double w : window) wss += w * w;
    if (wss < 1.0e-15) wss = 1.0;

    // Build complex buffer: apply window + zero-pad
    std::vector<std::complex<double>> buf(
        static_cast<std::size_t>(nfft), {0.0, 0.0});
    for (int i = 0; i < n; ++i) {
        buf[static_cast<std::size_t>(i)] = {
            segment[static_cast<std::size_t>(i)]
            * window[static_cast<std::size_t>(i)], 0.0};
    }

    // In-place FFT via bit-reversal + butterfly (Cooley-Tukey, same as FftAnalyzer)
    for (int i = 1, j = 0; i < nfft; ++i) {
        int bit = nfft >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(buf[static_cast<std::size_t>(i)],
                             buf[static_cast<std::size_t>(j)]);
    }
    for (int len = 2; len <= nfft; len <<= 1) {
        const double angle = -2.0 * std::numbers::pi / static_cast<double>(len);
        const std::complex<double> wlen(std::cos(angle), std::sin(angle));
        for (int i = 0; i < nfft; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (int k = 0; k < len / 2; ++k) {
                const std::size_t u = static_cast<std::size_t>(i + k);
                const std::size_t v = static_cast<std::size_t>(i + k + len / 2);
                const std::complex<double> t = w * buf[v];
                buf[v] = buf[u] - t;
                buf[u] = buf[u] + t;
                w *= wlen;
            }
        }
    }

    // Single-sided PSD
    const int nBins = nfft / 2 + 1;
    const double norm = 1.0 / (fsCpd * wss);
    std::vector<double> psd(static_cast<std::size_t>(nBins));
    for (int k = 0; k < nBins; ++k) {
        const double mag2 = std::norm(buf[static_cast<std::size_t>(k)]);
        psd[static_cast<std::size_t>(k)] = mag2 * norm;
    }
    // Double non-DC, non-Nyquist bins
    for (int k = 1; k < nBins - 1; ++k)
        psd[static_cast<std::size_t>(k)] *= 2.0;

    return psd;
}

// -----------------------------------------------------------------------------
//  Private static: _buildFreqAxis
// -----------------------------------------------------------------------------

std::vector<double>
SpectrogramAnalyzer::_buildFreqAxis(int nfft, double fsCpd)
{
    const int nBins = nfft / 2 + 1;
    std::vector<double> freqs(static_cast<std::size_t>(nBins));
    for (int k = 0; k < nBins; ++k) {
        freqs[static_cast<std::size_t>(k)] =
            static_cast<double>(k) * fsCpd / static_cast<double>(nfft);
    }
    return freqs;
}

} // namespace loki::spectral
