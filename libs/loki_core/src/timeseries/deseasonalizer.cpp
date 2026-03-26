#include <loki/timeseries/deseasonalizer.hpp>
#include <loki/stats/filter.hpp>
#include <loki/core/logger.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

using namespace loki;

namespace loki {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

Deseasonalizer::Deseasonalizer(Config cfg)
    : m_cfg{std::move(cfg)}
{}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

Deseasonalizer::Result Deseasonalizer::deseasonalize(
    const TimeSeries&                         series,
    std::function<double(const ::TimeStamp&)> profileLookup) const
{
    if (series.size() == 0) {
        throw DataException("Deseasonalizer::deseasonalize: input series is empty.");
    }

    switch (m_cfg.strategy) {
        case Strategy::MOVING_AVERAGE:
            return applyMovingAverage(series);

        case Strategy::MEDIAN_YEAR:
            if (!profileLookup) {
                throw ConfigException(
                    "Deseasonalizer::deseasonalize: strategy MEDIAN_YEAR requires "
                    "a profileLookup callable (MedianYearSeries::valueAt).");
            }
            checkMinResolution(series);
            return applyMedianYear(series, profileLookup);

        case Strategy::NONE:
            return applyNone(series);
    }

    throw AlgorithmException("Deseasonalizer::deseasonalize: unknown strategy.");
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

Deseasonalizer::Result Deseasonalizer::applyMovingAverage(
    const TimeSeries& series) const
{
    const std::size_t n = series.size();

    std::vector<double> values;
    values.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double v = series[i].value;
        if (std::isnan(v)) {
            throw DataException(
                "Deseasonalizer (MOVING_AVERAGE): NaN at index " +
                std::to_string(i) + ". Run GapFiller first.");
        }
        values.push_back(v);
    }

    const std::vector<double> trend =
        loki::stats::movingAverage(values, m_cfg.maWindowSize);

    std::vector<double> residuals(n);
    std::vector<double> seasonal(n);

    for (std::size_t i = 0; i < n; ++i) {
        if (std::isnan(trend[i])) {
            residuals[i] = std::numeric_limits<double>::quiet_NaN();
            seasonal[i]  = std::numeric_limits<double>::quiet_NaN();
        } else {
            seasonal[i]  = trend[i];
            residuals[i] = values[i] - trend[i];
        }
    }

    // Fill leading NaN edges with the first valid trend value (bfill).
    std::size_t firstValid = 0;
    while (firstValid < n && std::isnan(trend[firstValid])) { ++firstValid; }
    std::size_t lastValid = n - 1;
    while (lastValid > firstValid && std::isnan(trend[lastValid])) { --lastValid; }

    for (std::size_t i = 0; i < firstValid; ++i) {
        seasonal[i]  = trend[firstValid];
        residuals[i] = values[i] - trend[firstValid];
    }

    // Fill trailing NaN edges with the last valid trend value (ffill).
    for (std::size_t i = lastValid + 1; i < n; ++i) {
        seasonal[i]  = trend[lastValid];
        residuals[i] = values[i] - trend[lastValid];
    }

    TimeSeries outSeries;
    SeriesMetadata meta = series.metadata();
    meta.componentName += "_deseas";
    outSeries.setMetadata(meta);
    for (std::size_t i = 0; i < n; ++i) {
        outSeries.append(series[i].time, residuals[i]);
    }

    return Result{std::move(residuals), std::move(seasonal), std::move(outSeries)};
}

// ---------------------------------------------------------------------------

Deseasonalizer::Result Deseasonalizer::applyMedianYear(
    const TimeSeries&                                series,
    const std::function<double(const ::TimeStamp&)>& profileLookup) const
{
    const std::size_t n = series.size();

    std::vector<double> residuals(n);
    std::vector<double> seasonal(n);

    for (std::size_t i = 0; i < n; ++i) {
        const double v = series[i].value;
        if (std::isnan(v)) {
            throw DataException(
                "Deseasonalizer (MEDIAN_YEAR): NaN at index " +
                std::to_string(i) + ". Run GapFiller first.");
        }

        const double profile = profileLookup(series[i].time);

        if (std::isnan(profile)) {
            seasonal[i]  = std::numeric_limits<double>::quiet_NaN();
            residuals[i] = std::numeric_limits<double>::quiet_NaN();
        } else {
            seasonal[i]  = profile;
            residuals[i] = v - profile;
        }
    }

    TimeSeries outSeries;
    SeriesMetadata meta = series.metadata();
    meta.componentName += "_deseas";
    outSeries.setMetadata(meta);
    for (std::size_t i = 0; i < n; ++i) {
        outSeries.append(series[i].time, residuals[i]);
    }

    return Result{std::move(residuals), std::move(seasonal), std::move(outSeries)};
}

// ---------------------------------------------------------------------------

Deseasonalizer::Result Deseasonalizer::applyNone(
    const TimeSeries& series) const
{
    const std::size_t n = series.size();

    std::vector<double> residuals(n);
    std::vector<double> seasonal(n, 0.0);

    for (std::size_t i = 0; i < n; ++i) {
        const double v = series[i].value;
        if (std::isnan(v)) {
            throw DataException(
                "Deseasonalizer (NONE): NaN at index " +
                std::to_string(i) + ". Run GapFiller first.");
        }
        residuals[i] = v;
    }

    TimeSeries outSeries;
    SeriesMetadata meta = series.metadata();
    meta.componentName += "_deseas";
    outSeries.setMetadata(meta);
    for (std::size_t i = 0; i < n; ++i) {
        outSeries.append(series[i].time, residuals[i]);
    }

    return Result{std::move(residuals), std::move(seasonal), std::move(outSeries)};
}

// ---------------------------------------------------------------------------

void Deseasonalizer::checkMinResolution(const TimeSeries& series)
{
    if (series.size() < 2) return;

    std::vector<double> diffs;
    diffs.reserve(series.size() - 1);
    for (std::size_t i = 1; i < series.size(); ++i) {
        const double d = series[i].time.mjd() - series[i - 1].time.mjd();
        if (d > 0.0) diffs.push_back(d);
    }

    if (diffs.empty()) return;

    std::sort(diffs.begin(), diffs.end());
    const double medianStep = diffs[diffs.size() / 2];

    constexpr double ONE_HOUR_DAYS = 1.0 / 24.0;

    if (medianStep < ONE_HOUR_DAYS) {
        throw ConfigException(
            "Deseasonalizer: strategy MEDIAN_YEAR requires series resolution "
            ">= 1 hour. Detected step ~" +
            std::to_string(medianStep * 24.0 * 60.0) +
            " minutes. Use MOVING_AVERAGE for sub-hourly data.");
    }
}

} // namespace loki
