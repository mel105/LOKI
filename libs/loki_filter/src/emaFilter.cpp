#include <loki/filter/emaFilter.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/stats/filter.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <vector>

using namespace loki;

EmaFilter::EmaFilter(Config cfg)
    : m_cfg{std::move(cfg)}
{}

FilterResult EmaFilter::apply(const TimeSeries& series) const
{
    if (series.empty()) {
        throw DataException("EmaFilter::apply: input series is empty.");
    }

    std::vector<double> values;
    values.reserve(series.size());
    for (std::size_t i = 0; i < series.size(); ++i)
        values.push_back(series[i].value);

    std::vector<double> filtered = loki::stats::exponentialMovingAverage(values, m_cfg.alpha);

    TimeSeries filteredSeries{series.metadata()};
    TimeSeries residualSeries{series.metadata()};
    filteredSeries.reserve(series.size());
    residualSeries.reserve(series.size());

    for (std::size_t i = 0; i < series.size(); ++i) {
        filteredSeries.append(series[i].time, filtered[i]);
        residualSeries.append(series[i].time, series[i].value - filtered[i]);
    }

    return FilterResult{std::move(filteredSeries), std::move(residualSeries), name()};
    // EMA has no fixed window -- effectiveWindow and effectiveBandwidth remain 0.
}

std::string EmaFilter::name() const
{
    return "EMA";
}
