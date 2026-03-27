#include <loki/filter/loessFilter.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/math/lsq.hpp>
#include <loki/math/designMatrix.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <Eigen/Dense>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

using namespace loki;

double LoessFilter::kernelWeight(Kernel k, double u)
{
    const double uc = std::min(std::max(u, 0.0), 1.0);
    switch (k) {
        case Kernel::TRICUBE: {
            const double t = 1.0 - uc * uc * uc;
            return t * t * t;
        }
        case Kernel::EPANECHNIKOV:
            return 0.75 * (1.0 - uc * uc);
        case Kernel::GAUSSIAN:
            return std::exp(-0.5 * (3.0 * uc) * (3.0 * uc));
    }
    return 0.0;
}

std::string LoessFilter::kernelName(Kernel k)
{
    switch (k) {
        case Kernel::TRICUBE:      return "Tricube";
        case Kernel::EPANECHNIKOV: return "Epanechnikov";
        case Kernel::GAUSSIAN:     return "Gaussian";
    }
    return "Unknown";
}

LoessFilter::LoessFilter(Config cfg)
    : m_cfg{std::move(cfg)}
{
    if (m_cfg.degree != 1 && m_cfg.degree != 2) {
        throw ConfigException(
            "LoessFilter: degree must be 1 or 2, got " +
            std::to_string(m_cfg.degree) + ".");
    }
    if (m_cfg.bandwidth <= 0.0 || m_cfg.bandwidth > 1.0) {
        throw ConfigException(
            "LoessFilter: bandwidth must be in (0, 1], got " +
            std::to_string(m_cfg.bandwidth) + ".");
    }
}

FilterResult LoessFilter::apply(const TimeSeries& series) const
{
    const int n = static_cast<int>(series.size());
    if (n == 0) {
        throw DataException("LoessFilter::apply: input series is empty.");
    }

    const int k = static_cast<int>(
        std::ceil(m_cfg.bandwidth * static_cast<double>(n)));

    if (k < m_cfg.degree + 1) {
        throw ConfigException(
            "LoessFilter::apply: bandwidth produces k=" + std::to_string(k) +
            " neighbours, but degree=" + std::to_string(m_cfg.degree) +
            " requires at least " + std::to_string(m_cfg.degree + 1) + ".");
    }

    std::vector<double> y(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
        y[static_cast<std::size_t>(i)] = series[static_cast<std::size_t>(i)].value;

    std::vector<double> smoothed(static_cast<std::size_t>(n), 0.0);

    std::vector<int> idx(static_cast<std::size_t>(n));
    std::iota(idx.begin(), idx.end(), 0);

    for (int i = 0; i < n; ++i) {
        std::partial_sort(idx.begin(), idx.begin() + k, idx.end(),
            [i](int a, int b) {
                return std::abs(a - i) < std::abs(b - i);
            });

        const double maxDist = static_cast<double>(
            std::abs(idx[static_cast<std::size_t>(k - 1)] - i));

        if (maxDist < 1.0e-12) {
            smoothed[static_cast<std::size_t>(i)] = y[static_cast<std::size_t>(i)];
            continue;
        }

        Eigen::VectorXd xLocal(k);
        Eigen::VectorXd l(k);
        Eigen::VectorXd w(k);

        for (int s = 0; s < k; ++s) {
            const int j = idx[static_cast<std::size_t>(s)];
            const double u = static_cast<double>(std::abs(j - i)) / maxDist;
            xLocal(s) = static_cast<double>(j - i);
            l(s)      = y[static_cast<std::size_t>(j)];
            w(s)      = kernelWeight(m_cfg.kernel, u);
        }

        Eigen::MatrixXd A = DesignMatrix::polynomial(xLocal, m_cfg.degree);

        LsqSolver::Config lsqCfg;
        lsqCfg.weighted      = true;
        lsqCfg.robust        = m_cfg.robust;
        lsqCfg.maxIterations = m_cfg.robustIterations;
        lsqCfg.weightFn      = LsqSolver::WeightFunction::BISQUARE;

        LsqResult fit = LsqSolver::solve(A, l, lsqCfg, w);
        smoothed[static_cast<std::size_t>(i)] = fit.coefficients(0);
    }

    TimeSeries filteredSeries{series.metadata()};
    TimeSeries residualSeries{series.metadata()};
    filteredSeries.reserve(series.size());
    residualSeries.reserve(series.size());

    for (int i = 0; i < n; ++i) {
        const std::size_t si = static_cast<std::size_t>(i);
        filteredSeries.append(series[si].time, smoothed[si]);
        residualSeries.append(series[si].time, series[si].value - smoothed[si]);
    }

    FilterResult res{std::move(filteredSeries), std::move(residualSeries), name()};
    res.effectiveWindow    = k;
    res.effectiveBandwidth = m_cfg.bandwidth;
    return res;
}

std::string LoessFilter::name() const
{
    return "LOESS(degree=" + std::to_string(m_cfg.degree)
         + ", kernel=" + kernelName(m_cfg.kernel) + ")";
}
