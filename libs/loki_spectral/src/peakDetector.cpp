#include "loki/spectral/peakDetector.hpp"

#include "loki/spectral/lombScargle.hpp"

#include "loki/core/exceptions.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

using namespace loki;

namespace loki::spectral {

// -----------------------------------------------------------------------------
//  Construction
// -----------------------------------------------------------------------------

PeakDetector::PeakDetector(const PeakDetectorParams& params)
    : m_params(params)
{}

// -----------------------------------------------------------------------------
//  Public: detect
// -----------------------------------------------------------------------------

void PeakDetector::detect(SpectralResult& result) const
{
    if (result.frequencies.empty() || result.power.empty()) {
        throw DataException("PeakDetector: frequencies or power vector is empty.");
    }
    if (result.frequencies.size() != result.power.size()) {
        throw DataException(
            "PeakDetector: frequencies and power vectors have different lengths.");
    }

    result.peaks.clear();

    const int nFreqs    = static_cast<int>(result.frequencies.size());
    const bool isLombScargle = (result.method == "lomb_scargle");

    // 1. Find local maxima
    const std::vector<int> maxIdx = _localMaxima(result.power);
    if (maxIdx.empty()) return;

    // 2. Noise floor: median of entire power spectrum
    const double noiseFloor = _median(result.power);

    // 3. Build candidate list: above noise floor, within period window
    struct Candidate {
        int    idx;
        double freqCpd;
        double periodDays;
        double power;
        double fap;
    };
    std::vector<Candidate> candidates;
    candidates.reserve(maxIdx.size());

    for (const int k : maxIdx) {
        const double freq = result.frequencies[static_cast<std::size_t>(k)];
        if (freq <= 0.0) continue;  // skip DC

        const double period = 1.0 / freq;
        const double pwr    = result.power[static_cast<std::size_t>(k)];

        // Period window filter
        if (m_params.minPeriodDays > 0.0 && period < m_params.minPeriodDays) continue;
        if (m_params.maxPeriodDays > 0.0 && period > m_params.maxPeriodDays) continue;

        // Must exceed noise floor
        if (pwr <= noiseFloor) continue;

        // FAP (Lomb-Scargle only)
        double fapVal = -1.0;
        if (isLombScargle) {
            fapVal = LombScargle::fap(pwr, nFreqs, result.nObs);
            if (fapVal > m_params.fapThreshold) continue;
        }

        candidates.push_back({k, freq, period, pwr, fapVal});
    }

    if (candidates.empty()) return;

    // 4. Sort by power descending, keep top N
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) {
                  return a.power > b.power;
              });

    const int topN = std::min(m_params.topN, static_cast<int>(candidates.size()));

    // 5. Normalise power to [0, 1] relative to the strongest peak
    const double maxPower = candidates.front().power;
    const double normFactor = (maxPower > 0.0) ? 1.0 / maxPower : 1.0;

    result.peaks.reserve(static_cast<std::size_t>(topN));
    for (int r = 0; r < topN; ++r) {
        const auto& c = candidates[static_cast<std::size_t>(r)];
        SpectralPeak pk;
        pk.freqCpd    = c.freqCpd;
        pk.periodDays = c.periodDays;
        pk.power      = c.power * normFactor;
        pk.fap        = c.fap;
        pk.rank       = r + 1;
        result.peaks.push_back(pk);
    }
}

// -----------------------------------------------------------------------------
//  Private: _localMaxima
// -----------------------------------------------------------------------------

std::vector<int> PeakDetector::_localMaxima(const std::vector<double>& power)
{
    std::vector<int> idx;
    const int n = static_cast<int>(power.size());
    for (int k = 1; k < n - 1; ++k) {
        if (power[static_cast<std::size_t>(k)] > power[static_cast<std::size_t>(k - 1)] &&
            power[static_cast<std::size_t>(k)] > power[static_cast<std::size_t>(k + 1)]) {
            idx.push_back(k);
        }
    }
    return idx;
}

// -----------------------------------------------------------------------------
//  Private: _median
// -----------------------------------------------------------------------------

double PeakDetector::_median(std::vector<double> v)
{
    if (v.empty()) return 0.0;
    const std::size_t mid = v.size() / 2;
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(mid), v.end());
    if (v.size() % 2 == 1) return v[mid];
    // Even: average of two middle elements
    const double upper = v[mid];
    std::nth_element(v.begin(),
                     v.begin() + static_cast<std::ptrdiff_t>(mid - 1),
                     v.end());
    return 0.5 * (v[mid - 1] + upper);
}

} // namespace loki::spectral
