#include <loki/decomposition/classicalDecomposer.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/stats/filter.hpp>
#include <loki/timeseries/gapFiller.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

using namespace loki;

namespace {

constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

// Median of a non-empty vector (modifies the vector -- caller passes a copy).
double medianOf(std::vector<double> v)
{
    const std::size_t n = v.size();
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(n / 2), v.end());
    if (n % 2 == 1) {
        return v[n / 2];
    }
    // Even count: average of two middle elements.
    const double upper = v[n / 2];
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(n / 2 - 1), v.end());
    return 0.5 * (v[n / 2 - 1] + upper);
}

} // anonymous namespace

// -----------------------------------------------------------------------------
//  ClassicalDecomposer
// -----------------------------------------------------------------------------

ClassicalDecomposer::ClassicalDecomposer(Config cfg)
    : m_cfg{std::move(cfg)}
{}

// -----------------------------------------------------------------------------

DecompositionResult ClassicalDecomposer::decompose(const TimeSeries& ts,
                                                    int period) const
{
    if (period < 2) {
        throw ConfigException(
            "ClassicalDecomposer::decompose: period must be >= 2, got "
            + std::to_string(period) + ".");
    }

    const int n = static_cast<int>(ts.size());
    if (n < 2 * period) {
        throw DataException(
            "ClassicalDecomposer::decompose: series length " + std::to_string(n)
            + " must be at least 2 * period = " + std::to_string(2 * period) + ".");
    }

    // Check for NaN in input.
    for (int i = 0; i < n; ++i) {
        if (std::isnan(ts[static_cast<std::size_t>(i)].value)) {
            throw DataException(
                "ClassicalDecomposer::decompose: input series contains NaN at index "
                + std::to_string(i) + ". Run GapFiller before decomposing.");
        }
    }

    // Step 1: trend via centered MA + edge fill.
    const std::vector<double> trend = estimateTrend(ts, period);

    // Step 2: detrended = Y - T.
    std::vector<double> detrended(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        detrended[static_cast<std::size_t>(i)] =
            ts[static_cast<std::size_t>(i)].value - trend[static_cast<std::size_t>(i)];
    }

    // Step 3: seasonal from per-slot aggregation.
    const std::vector<double> seasonal = estimateSeasonal(detrended, period);

    // Step 4: residual = Y - T - S.
    std::vector<double> residual(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        const std::size_t si = static_cast<std::size_t>(i);
        residual[si] = ts[si].value - trend[si] - seasonal[si];
    }

    DecompositionResult result;
    result.trend    = trend;
    result.seasonal = seasonal;
    result.residual = residual;
    result.method   = DecompositionMethod::CLASSICAL;
    result.period   = period;
    return result;
}

// -----------------------------------------------------------------------------

std::vector<double> ClassicalDecomposer::estimateTrend(const TimeSeries& ts,
                                                         int period) const
{
    const int n = static_cast<int>(ts.size());

    // Extract raw values for the filter.
    std::vector<double> y(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        y[static_cast<std::size_t>(i)] = ts[static_cast<std::size_t>(i)].value;
    }

    // Centered MA -- produces NaN on the first and last (period/2) positions.
    // Period must be odd for movingAverage; if even, round up by one so the
    // window stays symmetric. This matches the behavior of movingAverage().
    const int window  = (period % 2 == 0) ? period + 1 : period;
    std::vector<double> trendRaw = loki::stats::movingAverage(y, window);

    // Build a temporary TimeSeries carrying the MA output so GapFiller can
    // detect and fill the NaN edges via bfill/ffill (LINEAR strategy).
    TimeSeries trendTs{ts.metadata()};
    trendTs.reserve(ts.size());
    for (int i = 0; i < n; ++i) {
        const std::size_t si = static_cast<std::size_t>(i);
        trendTs.append(ts[si].time, trendRaw[si]);
    }

    GapFiller::Config gfCfg;
    gfCfg.strategy      = GapFiller::Strategy::LINEAR; // bfill/ffill at edges
    gfCfg.maxFillLength = 0;                           // unlimited -- fill all edge NaN
    const GapFiller   gf{gfCfg};
    const TimeSeries  trendFilled = gf.fill(trendTs);

    // Extract values from the filled series.
    std::vector<double> trend(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        trend[static_cast<std::size_t>(i)] = trendFilled[static_cast<std::size_t>(i)].value;
    }

    return trend;
}

// -----------------------------------------------------------------------------

std::vector<double> ClassicalDecomposer::estimateSeasonal(
    const std::vector<double>& detrended, int period) const
{
    const int n = static_cast<int>(detrended.size());

    // Collect detrended values per slot.
    std::vector<std::vector<double>> slots(static_cast<std::size_t>(period));
    for (int i = 0; i < n; ++i) {
        const int slot = i % period;
        slots[static_cast<std::size_t>(slot)].push_back(detrended[static_cast<std::size_t>(i)]);
    }

    // Aggregate each slot.
    const bool useMedian = (m_cfg.seasonalType == "median");
    std::vector<double> slotValues(static_cast<std::size_t>(period), 0.0);

    for (int s = 0; s < period; ++s) {
        auto& v = slots[static_cast<std::size_t>(s)];
        if (v.empty()) {
            slotValues[static_cast<std::size_t>(s)] = 0.0;
            continue;
        }
        if (useMedian) {
            slotValues[static_cast<std::size_t>(s)] = medianOf(v);
        } else {
            // Mean.
            const double sum = std::accumulate(v.begin(), v.end(), 0.0);
            slotValues[static_cast<std::size_t>(s)] = sum / static_cast<double>(v.size());
        }
    }

    // Normalize: subtract the mean of slot values so that one period sums to zero.
    const double slotMean = std::accumulate(slotValues.begin(), slotValues.end(), 0.0)
                            / static_cast<double>(period);
    for (double& sv : slotValues) {
        sv -= slotMean;
    }

    // Expand slot values back to the full series length.
    std::vector<double> seasonal(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        seasonal[static_cast<std::size_t>(i)] =
            slotValues[static_cast<std::size_t>(i % period)];
    }

    return seasonal;
}
