#include <loki/filter/kernelSmoother.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <cmath>
#include <limits>
#include <vector>

using namespace loki;

static constexpr double INV_SQRT_2PI = 0.3989422804014327;

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

double KernelSmoother::kernelWeight(Kernel k, double u)
{
    const double absU = std::abs(u);
    switch (k) {
        case Kernel::EPANECHNIKOV:
            return (absU <= 1.0) ? 0.75 * (1.0 - u * u) : 0.0;
        case Kernel::GAUSSIAN:
            return INV_SQRT_2PI * std::exp(-0.5 * u * u);
        case Kernel::UNIFORM:
            return (absU <= 1.0) ? 0.5 : 0.0;
        case Kernel::TRIANGULAR:
            return (absU <= 1.0) ? (1.0 - absU) : 0.0;
    }
    return 0.0;
}

std::string KernelSmoother::kernelName(Kernel k)
{
    switch (k) {
        case Kernel::EPANECHNIKOV: return "Epanechnikov";
        case Kernel::GAUSSIAN:     return "Gaussian";
        case Kernel::UNIFORM:      return "Uniform";
        case Kernel::TRIANGULAR:   return "Triangular";
    }
    return "Unknown";
}

KernelSmoother::KernelSmoother(Config cfg)
    : m_cfg{std::move(cfg)}
{
    if (m_cfg.bandwidth <= 0.0 || m_cfg.bandwidth >= 1.0) {
        throw ConfigException(
            "KernelSmoother: bandwidth must be in (0, 1), got " +
            std::to_string(m_cfg.bandwidth) + ".");
    }
}

FilterResult KernelSmoother::apply(const TimeSeries& series) const
{
    const int n = static_cast<int>(series.size());
    if (n == 0) {
        throw DataException("KernelSmoother::apply: input series is empty.");
    }

    const double h = m_cfg.bandwidth * static_cast<double>(n);
    if (h < 1.0) {
        throw ConfigException(
            "KernelSmoother::apply: bandwidth " + std::to_string(m_cfg.bandwidth) +
            " produces a window smaller than 1 sample for series of length " +
            std::to_string(n) + ".");
    }

    const int radius = static_cast<int>(std::ceil(
        (m_cfg.kernel == Kernel::GAUSSIAN)
            ? m_cfg.gaussianCutoff * h
            : h));

    std::vector<double> smoothed(
        static_cast<std::size_t>(n),
        std::numeric_limits<double>::quiet_NaN());

    for (int i = 0; i < n; ++i) {
        double sumW  = 0.0;
        double sumWY = 0.0;
        const int jMin = std::max(0, i - radius);
        const int jMax = std::min(n - 1, i + radius);
        for (int j = jMin; j <= jMax; ++j) {
            const double u = static_cast<double>(i - j) / h;
            const double w = kernelWeight(m_cfg.kernel, u);
            if (w > 0.0) {
                sumW  += w;
                sumWY += w * series[static_cast<std::size_t>(j)].value;
            }
        }
        if (sumW > 0.0)
            smoothed[static_cast<std::size_t>(i)] = sumWY / sumW;
    }

    fillEdgeNaN(smoothed);

    TimeSeries filteredSeries{series.metadata()};
    TimeSeries residualSeries{series.metadata()};
    filteredSeries.reserve(series.size());
    residualSeries.reserve(series.size());

    for (int i = 0; i < n; ++i) {
        const std::size_t idx = static_cast<std::size_t>(i);
        filteredSeries.append(series[idx].time, smoothed[idx]);
        residualSeries.append(series[idx].time, series[idx].value - smoothed[idx]);
    }

    // h = bandwidth * n samples (half-window for compact kernels)
    FilterResult res{std::move(filteredSeries), std::move(residualSeries), name()};
    res.effectiveWindow    = static_cast<int>(std::round(h));
    res.effectiveBandwidth = m_cfg.bandwidth;
    return res;
}

std::string KernelSmoother::name() const
{
    return "KernelSmoother(" + kernelName(m_cfg.kernel) + ")";
}
