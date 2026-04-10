#include "loki/evt/thresholdSelector.hpp"
#include "loki/evt/gpd.hpp"
#include "loki/core/exceptions.hpp"
#include "loki/core/logger.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

using namespace loki;

namespace loki::evt {

// -----------------------------------------------------------------------------
//  _evalCandidate
// -----------------------------------------------------------------------------

void ThresholdSelector::_evalCandidate(const std::vector<double>& sorted,
                                        double u,
                                        double& meanExc,
                                        double& sigma,
                                        double& xi,
                                        int    minExc)
{
    // Collect exceedances above u.
    std::vector<double> exc;
    for (const double x : sorted) {
        if (x > u) exc.push_back(x - u);
    }

    meanExc = std::numeric_limits<double>::quiet_NaN();
    sigma   = std::numeric_limits<double>::quiet_NaN();
    xi      = std::numeric_limits<double>::quiet_NaN();

    if (static_cast<int>(exc.size()) < minExc) return;

    const int n = static_cast<int>(exc.size());
    meanExc = std::accumulate(exc.begin(), exc.end(), 0.0)
              / static_cast<double>(n);

    // Attempt a lightweight GPD fit for stability diagnostics.
    try {
        const GpdFitResult r = Gpd::fit(exc, u);
        sigma = r.sigma;
        xi    = r.xi;
    } catch (...) {
        // Fit failure: leave sigma/xi as NaN.
    }
}

// -----------------------------------------------------------------------------
//  _elbowIndex
// -----------------------------------------------------------------------------

std::size_t ThresholdSelector::_elbowIndex(const std::vector<double>& y)
{
    const std::size_t n = y.size();
    if (n < 3) return 0;

    // Second difference: d2[i] = y[i-1] - 2*y[i] + y[i+1]
    // Normalise by range to make scale-independent.
    double yMin = *std::min_element(y.begin(), y.end());
    double yMax = *std::max_element(y.begin(), y.end());
    const double range = (yMax > yMin) ? (yMax - yMin) : 1.0;

    double bestVal = -1.0;
    std::size_t bestIdx = 0;

    for (std::size_t i = 1; i + 1 < n; ++i) {
        const double d2 = std::abs(y[i - 1] - 2.0 * y[i] + y[i + 1]) / range;
        if (d2 > bestVal) {
            bestVal = d2;
            bestIdx = i;
        }
    }
    return bestIdx;
}

// -----------------------------------------------------------------------------
//  autoSelect
// -----------------------------------------------------------------------------

ThresholdSelector::Result ThresholdSelector::autoSelect(
    const std::vector<double>& data,
    int nCandidates,
    int minExceedances)
{
    if (data.empty())
        throw DataException("ThresholdSelector::autoSelect: data is empty.");

    std::vector<double> sorted = data;
    std::sort(sorted.begin(), sorted.end());
    const int n = static_cast<int>(sorted.size());

    // Candidate range: from the quantile that leaves minExceedances above it,
    // down to the median.
    const std::size_t maxIdx = static_cast<std::size_t>(
        std::max(0, n - minExceedances - 1));
    const double uMax = sorted[maxIdx];
    const double uMin = sorted[n / 2];

    if (uMax <= uMin) {
        // Very small dataset: single candidate at uMin.
        Result res;
        res.selected = uMin;
        res.candidates = {uMin};
        double me, s, x;
        _evalCandidate(sorted, uMin, me, s, x, minExceedances);
        res.meanExcess     = {me};
        res.sigmaStability = {s};
        res.xiStability    = {x};
        return res;
    }

    const int nc = std::max(3, nCandidates);
    const double step = (uMax - uMin) / static_cast<double>(nc - 1);

    Result res;
    res.candidates.resize(static_cast<std::size_t>(nc));
    res.meanExcess.resize(static_cast<std::size_t>(nc));
    res.sigmaStability.resize(static_cast<std::size_t>(nc));
    res.xiStability.resize(static_cast<std::size_t>(nc));

    std::size_t nValid = 0;
    for (int i = 0; i < nc; ++i) {
        const double u = uMin + static_cast<double>(i) * step;
        res.candidates[static_cast<std::size_t>(i)] = u;

        double me, s, x;
        _evalCandidate(sorted, u, me, s, x, minExceedances);

        res.meanExcess    [static_cast<std::size_t>(i)] = me;
        res.sigmaStability[static_cast<std::size_t>(i)] = s;
        res.xiStability   [static_cast<std::size_t>(i)] = x;

        if (std::isfinite(me)) ++nValid;
    }

    if (nValid == 0) {
        throw DataException(
            "ThresholdSelector::autoSelect: no candidate threshold has at least "
            + std::to_string(minExceedances) + " exceedances. "
            "Reduce min_exceedances or provide more data.");
    }

    // Collect only valid (finite mean excess) candidates for elbow detection.
    std::vector<double> validMe;
    std::vector<std::size_t> validIdx;
    for (std::size_t i = 0; i < static_cast<std::size_t>(nc); ++i) {
        if (std::isfinite(res.meanExcess[i])) {
            validMe.push_back(res.meanExcess[i]);
            validIdx.push_back(i);
        }
    }

    const std::size_t elbowPos = _elbowIndex(validMe);
    res.selected = res.candidates[validIdx[elbowPos]];

    LOKI_INFO("ThresholdSelector: auto-selected threshold = "
              + std::to_string(res.selected)
              + " (elbow at candidate " + std::to_string(validIdx[elbowPos]) + ")");

    return res;
}

// -----------------------------------------------------------------------------
//  manual
// -----------------------------------------------------------------------------

ThresholdSelector::Result ThresholdSelector::manual(
    const std::vector<double>& data,
    double threshold,
    int    minExceedances)
{
    if (data.empty())
        throw DataException("ThresholdSelector::manual: data is empty.");

    std::vector<double> sorted = data;
    std::sort(sorted.begin(), sorted.end());

    double me, s, x;
    _evalCandidate(sorted, threshold, me, s, x, minExceedances);

    if (!std::isfinite(me)) {
        // Count actual exceedances for a useful error message.
        int cnt = 0;
        for (const double v : data)
            if (v > threshold) ++cnt;

        throw DataException(
            "ThresholdSelector::manual: threshold " + std::to_string(threshold)
            + " yields only " + std::to_string(cnt)
            + " exceedances (min_exceedances = "
            + std::to_string(minExceedances) + ").");
    }

    Result res;
    res.selected       = threshold;
    res.candidates     = {threshold};
    res.meanExcess     = {me};
    res.sigmaStability = {s};
    res.xiStability    = {x};
    return res;
}

} // namespace loki::evt
