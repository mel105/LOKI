#include <loki/filter/movingAverageFilter.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/stats/filter.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <cmath>
#include <vector>

using namespace loki;

namespace {

void fillEdgeNaN(std::vector<double>& v)
{
    const std::size_t n = v.size();
    std::size_t first = n;
    for (std::size_t i = 0; i < n; ++i) {
        if (!std::isnan(v[i])) { first = i; break; }
    }
    if (first == n) return;
    for (std::size_t i = 0; i < first; ++i) v[i] = v[first];

    std::size_t last = 0;
    for (std::size_t i = n; i-- > 0;) {
        if (!std::isnan(v[i])) { last = i; break; }
    }
    for (std::size_t i = last + 1; i < n; ++i) v[i] = v[last];
}

} // anonymous namespace

MovingAverageFilter::MovingAverageFilter(Config cfg)
    : m_cfg{std::move(cfg)}
{}

FilterResult MovingAverageFilter::apply(const TimeSeries& series) const
{
    if (series.empty()) {
        throw DataException("MovingAverageFilter::apply: input series is empty.");
    }

    std::vector<double> values;
    values.reserve(series.size());
    for (std::size_t i = 0; i < series.size(); ++i)
        values.push_back(series[i].value);

    std::vector<double> filtered = loki::stats::movingAverage(values, m_cfg.window);
    fillEdgeNaN(filtered);

    TimeSeries filteredSeries{series.metadata()};
    TimeSeries residualSeries{series.metadata()};
    filteredSeries.reserve(series.size());
    residualSeries.reserve(series.size());

    for (std::size_t i = 0; i < series.size(); ++i) {
        filteredSeries.append(series[i].time, filtered[i]);
        residualSeries.append(series[i].time, series[i].value - filtered[i]);
    }

    const int    n   = static_cast<int>(series.size());
    const double bw  = (n > 0) ? static_cast<double>(m_cfg.window) / static_cast<double>(n) : 0.0;
    FilterResult res{std::move(filteredSeries), std::move(residualSeries), name()};
    res.effectiveWindow    = m_cfg.window;
    res.effectiveBandwidth = bw;
    return res;
}

std::string MovingAverageFilter::name() const
{
    return "MovingAverage";
}
