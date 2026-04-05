#include "loki/spectral/fftAnalyzer.hpp"

#include "loki/core/exceptions.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>

using namespace loki;

namespace loki::spectral {

// -----------------------------------------------------------------------------
//  Construction
// -----------------------------------------------------------------------------

FftAnalyzer::FftAnalyzer(const FftParams& params, double samplingStepDays)
    : m_params(params)
    , m_stepDays(samplingStepDays)
{
    if (m_stepDays <= 0.0)
        throw DataException("FftAnalyzer: samplingStepDays must be positive.");
}

// -----------------------------------------------------------------------------
//  Public: compute
// -----------------------------------------------------------------------------

SpectralResult FftAnalyzer::compute(const std::vector<double>& values) const
{
    if (static_cast<int>(values.size()) < 4)
        throw DataException(
            "FftAnalyzer: signal too short (n=" + std::to_string(values.size())
            + "); need at least 4 samples.");

    const double fsCpd = 1.0 / m_stepDays;

    FftFrame frame;
    if (m_params.welch && static_cast<int>(values.size()) >= m_params.welchSegments * 2)
        frame = _welch(values, fsCpd);
    else {
        const std::vector<double> window =
            buildWindow(m_params.windowFunction, static_cast<int>(values.size()));
        frame = _periodogram(values, window, fsCpd);
    }

    SpectralResult sr;
    sr.frequencies      = std::move(frame.frequencies);
    sr.power            = std::move(frame.power);
    sr.amplitudes       = std::move(frame.amplitudes);
    sr.phases           = std::move(frame.phases);
    sr.method           = "fft";
    sr.samplingStepDays = m_stepDays;
    sr.spanDays         = static_cast<double>(values.size()) * m_stepDays;
    sr.nObs             = static_cast<int>(values.size());
    return sr;
}

// -----------------------------------------------------------------------------
//  Public static: nextPow2
// -----------------------------------------------------------------------------

int FftAnalyzer::nextPow2(int n)
{
    if (n <= 1) return 1;
    int p = 1;
    while (p < n) p <<= 1;
    return p;
}

// -----------------------------------------------------------------------------
//  Public static: buildWindow
// -----------------------------------------------------------------------------

std::vector<double> FftAnalyzer::buildWindow(const std::string& type, int n)
{
    std::vector<double> w(static_cast<std::size_t>(n));
    const double pi = std::numbers::pi;
    const double N  = static_cast<double>(n - 1);

    if (type == "rectangular") {
        std::fill(w.begin(), w.end(), 1.0);
    } else if (type == "hann") {
        for (int i = 0; i < n; ++i)
            w[static_cast<std::size_t>(i)] = 0.5 * (1.0 - std::cos(2.0 * pi * i / N));
    } else if (type == "hamming") {
        for (int i = 0; i < n; ++i)
            w[static_cast<std::size_t>(i)] = 0.54 - 0.46 * std::cos(2.0 * pi * i / N);
    } else if (type == "blackman") {
        for (int i = 0; i < n; ++i)
            w[static_cast<std::size_t>(i)] =
                0.42 - 0.50 * std::cos(2.0 * pi * i / N)
                     + 0.08 * std::cos(4.0 * pi * i / N);
    } else if (type == "flattop") {
        constexpr double a0 = 0.21557895, a1 = 0.41663158, a2 = 0.27726316;
        constexpr double a3 = 0.08357895, a4 = 0.00694737;
        for (int i = 0; i < n; ++i) {
            const double t = 2.0 * pi * i / N;
            w[static_cast<std::size_t>(i)] =
                a0 - a1 * std::cos(t) + a2 * std::cos(2.0 * t)
                   - a3 * std::cos(3.0 * t) + a4 * std::cos(4.0 * t);
        }
    } else {
        throw AlgorithmException(
            "FftAnalyzer::buildWindow: unknown window type '" + type + "'.");
    }
    return w;
}

// -----------------------------------------------------------------------------
//  Private: _fft
// -----------------------------------------------------------------------------

void FftAnalyzer::_fft(std::vector<std::complex<double>>& x)
{
    const int n = static_cast<int>(x.size());
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(x[static_cast<std::size_t>(i)],
                             x[static_cast<std::size_t>(j)]);
    }
    for (int len = 2; len <= n; len <<= 1) {
        const double angle = -2.0 * std::numbers::pi / static_cast<double>(len);
        const std::complex<double> wlen(std::cos(angle), std::sin(angle));
        for (int i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (int k = 0; k < len / 2; ++k) {
                const std::size_t u = static_cast<std::size_t>(i + k);
                const std::size_t v = static_cast<std::size_t>(i + k + len / 2);
                const std::complex<double> t = w * x[v];
                x[v] = x[u] - t;
                x[u] = x[u] + t;
                w *= wlen;
            }
        }
    }
}

// -----------------------------------------------------------------------------
//  Private: _periodogram
// -----------------------------------------------------------------------------

FftFrame FftAnalyzer::_periodogram(const std::vector<double>& values,
                                    const std::vector<double>& window,
                                    double fsCpd) const
{
    const int n = static_cast<int>(values.size());

    // Window sum of squares for power normalisation
    double wss = 0.0;
    for (const double w : window) wss += w * w;

    // Window sum for amplitude normalisation
    double ws = 0.0;
    for (const double w : window) ws += w;
    if (ws < 1.0e-15) ws = 1.0;

    const int nfft = nextPow2(n);
    std::vector<std::complex<double>> buf(static_cast<std::size_t>(nfft), {0.0, 0.0});
    for (int i = 0; i < n; ++i)
        buf[static_cast<std::size_t>(i)] = {
            values[static_cast<std::size_t>(i)] * window[static_cast<std::size_t>(i)], 0.0};

    _fft(buf);

    const int nBins = nfft / 2 + 1;
    FftFrame frame;
    frame.frequencies.resize(static_cast<std::size_t>(nBins));
    frame.power.resize(static_cast<std::size_t>(nBins));
    frame.amplitudes.resize(static_cast<std::size_t>(nBins));
    frame.phases.resize(static_cast<std::size_t>(nBins));

    const double normPsd = 1.0 / (fsCpd * wss);
    // Amplitude normalisation: scale so a pure sinusoid with amplitude A
    // gives amplitudes[k] ~ A (two-sided to one-sided factor applied below)
    const double normAmp = 1.0 / ws;

    for (int k = 0; k < nBins; ++k) {
        const std::size_t ki = static_cast<std::size_t>(k);
        frame.frequencies[ki] =
            static_cast<double>(k) * fsCpd / static_cast<double>(nfft);
        const double mag2 = std::norm(buf[ki]);
        frame.power[ki]   = mag2 * normPsd;
        // Amplitude: |X[k]| / ws; double non-DC non-Nyquist (one-sided)
        frame.amplitudes[ki] = std::abs(buf[ki]) * normAmp;
        // Phase: atan2(Im, Re)
        frame.phases[ki] = std::arg(buf[ki]);
    }

    // Double non-DC, non-Nyquist bins for one-sided spectrum
    for (int k = 1; k < nBins - 1; ++k) {
        frame.power[static_cast<std::size_t>(k)]     *= 2.0;
        frame.amplitudes[static_cast<std::size_t>(k)] *= 2.0;
        // Phase is not doubled -- it stays in (-pi, pi)
    }

    return frame;
}

// -----------------------------------------------------------------------------
//  Private: _welch
// -----------------------------------------------------------------------------

FftFrame FftAnalyzer::_welch(const std::vector<double>& values, double fsCpd) const
{
    const int n    = static_cast<int>(values.size());
    const int nseg = m_params.welchSegments;
    const int segLen = static_cast<int>(
        std::round(static_cast<double>(n) /
                   (1.0 + (1.0 - m_params.welchOverlap) * (nseg - 1))));
    const int hop = std::max(1,
        static_cast<int>(std::round(static_cast<double>(segLen)
                                    * (1.0 - m_params.welchOverlap))));

    const std::vector<double> window = buildWindow(m_params.windowFunction, segLen);

    FftFrame accum;
    // Accumulate complex X for circular phase averaging
    std::vector<std::complex<double>> complexAccum;
    int count = 0;

    for (int start = 0; start + segLen <= n; start += hop) {
        std::vector<double> seg(values.begin() + start,
                                values.begin() + start + segLen);
        FftFrame f = _periodogram(seg, window, fsCpd);

        if (accum.frequencies.empty()) {
            accum = f;
            complexAccum.resize(f.amplitudes.size(), {0.0, 0.0});
            std::fill(accum.power.begin(),     accum.power.end(),     0.0);
            std::fill(accum.amplitudes.begin(), accum.amplitudes.end(), 0.0);
        }
        for (std::size_t i = 0; i < f.power.size(); ++i) {
            accum.power[i]     += f.power[i];
            accum.amplitudes[i] += f.amplitudes[i];
            // Accumulate complex phasor for phase averaging
            complexAccum[i] +=
                std::polar(1.0, f.phases[i]);
        }
        ++count;
        if (count >= nseg) break;
    }

    if (count == 0) {
        const std::vector<double> window2 = buildWindow(m_params.windowFunction, n);
        return _periodogram(values, window2, fsCpd);
    }

    const double inv = 1.0 / static_cast<double>(count);
    for (std::size_t i = 0; i < accum.power.size(); ++i) {
        accum.power[i]      *= inv;
        accum.amplitudes[i] *= inv;
        // Circular mean phase
        accum.phases[i] = std::arg(complexAccum[i]);
    }

    return accum;
}

} // namespace loki::spectral