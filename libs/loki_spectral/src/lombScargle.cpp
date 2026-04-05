#include "loki/spectral/lombScargle.hpp"

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

LombScargle::LombScargle(const LombScargleParams& params)
    : m_params(params)
{}

// -----------------------------------------------------------------------------
//  Public: compute
// -----------------------------------------------------------------------------

SpectralResult LombScargle::compute(const std::vector<double>& times,
                                     const std::vector<double>& values) const
{
    if (times.size() != values.size()) {
        throw DataException(
            "LombScargle: times and values must have the same length.");
    }
    if (static_cast<int>(times.size()) < 4) {
        throw DataException(
            "LombScargle: series too short (n=" + std::to_string(times.size())
            + "); need at least 4 observations.");
    }

    // --- Basic statistics ---
    const int n = static_cast<int>(times.size());

    const double tMin = *std::min_element(times.begin(), times.end());
    const double tMax = *std::max_element(times.begin(), times.end());
    const double spanDays = tMax - tMin;

    if (spanDays <= 0.0) {
        throw DataException(
            "LombScargle: all observations have the same timestamp.");
    }

    // Median step (sorted differences)
    std::vector<double> steps;
    steps.reserve(static_cast<std::size_t>(n - 1));
    for (int i = 1; i < n; ++i)
        steps.push_back(times[static_cast<std::size_t>(i)]
                        - times[static_cast<std::size_t>(i - 1)]);
    std::sort(steps.begin(), steps.end());
    const double medianStep = steps[steps.size() / 2];

    if (medianStep <= 0.0) {
        throw DataException(
            "LombScargle: median sampling step is non-positive; "
            "check that times are in ascending order.");
    }

    // --- Frequency grid ---
    const std::vector<double> freqs = _buildFrequencyGrid(spanDays, medianStep);
    const int nFreqs = static_cast<int>(freqs.size());

    // --- Mean-subtract ---
    const std::vector<double> y = _demean(values);

    // Variance (normalisation denominator)
    double yvar = 0.0;
    for (const double v : y) yvar += v * v;
    yvar /= static_cast<double>(n);

    if (yvar < 1.0e-15) {
        throw DataException(
            "LombScargle: signal variance is effectively zero; "
            "cannot compute a meaningful periodogram.");
    }

    // --- Compute periodogram ---
    std::vector<double> power(static_cast<std::size_t>(nFreqs));
    for (int k = 0; k < nFreqs; ++k) {
        power[static_cast<std::size_t>(k)] =
            _power(times, y, yvar, freqs[static_cast<std::size_t>(k)]);
    }

    // Amplitude = sqrt(normalised power) for Lomb-Scargle
    // Phase is not available from the standard L-S formulation -- left empty.
    std::vector<double> amplitudes(static_cast<std::size_t>(nFreqs));
    for (int k = 0; k < nFreqs; ++k)
        amplitudes[static_cast<std::size_t>(k)] =
            std::sqrt(std::max(0.0, power[static_cast<std::size_t>(k)]));

    SpectralResult sr;
    sr.frequencies      = freqs;
    sr.power            = power;
    sr.amplitudes       = std::move(amplitudes);
    // sr.phases left empty for Lomb-Scargle
    sr.method           = "lomb_scargle";
    sr.samplingStepDays = medianStep;
    sr.spanDays         = spanDays;
    sr.nObs             = n;
    // peaks filled later by PeakDetector
    return sr;
}

// -----------------------------------------------------------------------------
//  Public static: fap  (Baluev 2008 analytic approximation)
// -----------------------------------------------------------------------------

double LombScargle::fap(double z, int nFreqs, int /*nObs*/)
{
    // W ~ nFreqs (effective number of independent frequencies for an
    // oversampled grid; a mild overestimate that keeps the approximation
    // conservative).
    const double W   = static_cast<double>(nFreqs);
    const double ez  = std::exp(-z);

    // Baluev (2008) eq. 6: FAP ~ W * sqrt(z) * exp(-z) for large z.
    // For small z we use the simpler M*exp(-z) form to avoid sqrt(0).
    double fapVal;
    if (z > 0.1) {
        fapVal = W * std::sqrt(z) * ez;
    } else {
        fapVal = W * ez;
    }
    // Clamp to [0, 1]
    return std::min(1.0, std::max(0.0, fapVal));
}

// -----------------------------------------------------------------------------
//  Private: _buildFrequencyGrid
// -----------------------------------------------------------------------------

std::vector<double>
LombScargle::_buildFrequencyGrid(double spanDays, double medianStepDays) const
{
    const double fMin = 1.0 / spanDays;
    const double fMax = 1.0 / (2.0 * medianStepDays);  // Nyquist

    if (fMax <= fMin) {
        // Degenerate case: series has only one or two distinct times
        return {fMin};
    }

    // Number of frequency points: oversampling * T * fMax
    const int nFreqs = std::max(
        10,
        static_cast<int>(std::round(m_params.oversampling * spanDays * fMax)));

    std::vector<double> freqs(static_cast<std::size_t>(nFreqs));
    const double df = (fMax - fMin) / static_cast<double>(nFreqs - 1);
    for (int k = 0; k < nFreqs; ++k)
        freqs[static_cast<std::size_t>(k)] = fMin + static_cast<double>(k) * df;

    return freqs;
}

// -----------------------------------------------------------------------------
//  Private: _demean
// -----------------------------------------------------------------------------

std::vector<double> LombScargle::_demean(const std::vector<double>& values)
{
    const double mean = std::accumulate(values.begin(), values.end(), 0.0)
                        / static_cast<double>(values.size());
    std::vector<double> y(values.size());
    for (std::size_t i = 0; i < values.size(); ++i)
        y[i] = values[i] - mean;
    return y;
}

// -----------------------------------------------------------------------------
//  Private: _power  (Scargle 1982 formula with tau offset)
// -----------------------------------------------------------------------------

double LombScargle::_power(const std::vector<double>& times,
                            const std::vector<double>& y,
                            double yvar,
                            double freqCpd)
{
    const double omega = 2.0 * std::numbers::pi * freqCpd;

    // Compute tau: the time offset that makes the estimator orthogonal
    //   tan(2 * omega * tau) = sum(sin(2*omega*t)) / sum(cos(2*omega*t))
    double s2 = 0.0, c2 = 0.0;
    for (const double t : times) {
        s2 += std::sin(2.0 * omega * t);
        c2 += std::cos(2.0 * omega * t);
    }
    const double tau = std::atan2(s2, c2) / (2.0 * omega);

    // Evaluate the two sums in the Scargle formula
    double yc = 0.0, ys = 0.0;  // cross-terms
    double cc = 0.0, ss = 0.0;  // normalisation
    const int n = static_cast<int>(times.size());
    for (int i = 0; i < n; ++i) {
        const double phase = omega * (times[static_cast<std::size_t>(i)] - tau);
        const double cv    = std::cos(phase);
        const double sv    = std::sin(phase);
        yc += y[static_cast<std::size_t>(i)] * cv;
        ys += y[static_cast<std::size_t>(i)] * sv;
        cc += cv * cv;
        ss += sv * sv;
    }

    // Guard against degenerate normalisations
    if (cc < 1.0e-15 || ss < 1.0e-15) return 0.0;

    const double pnorm = static_cast<double>(n) * yvar;
    return 0.5 * ((yc * yc) / cc + (ys * ys) / ss) / pnorm;
}

} // namespace loki::spectral