#include <loki/filter/savitzkyGolayFilter.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/math/lsq.hpp>
#include <loki/math/designMatrix.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <Eigen/Dense>
#include <cmath>
#include <numeric>
#include <vector>

using namespace loki;

std::vector<double> SavitzkyGolayFilter::computeCoeffs(int windowSize,
                                                        int degree,
                                                        int evalOffset)
{
    Eigen::VectorXd x(windowSize);
    for (int j = 0; j < windowSize; ++j)
        x(j) = static_cast<double>(j - evalOffset);

    Eigen::MatrixXd A = DesignMatrix::polynomial(x, degree);

    LsqSolver::Config cfg;
    Eigen::VectorXd e0 = Eigen::VectorXd::Zero(windowSize);
    e0(evalOffset) = 1.0;
    LsqResult res = LsqSolver::solve(A, e0, cfg);
    (void)res;

    Eigen::MatrixXd AtA    = A.transpose() * A;
    Eigen::MatrixXd AtAinv = AtA.inverse();
    Eigen::RowVectorXd Arow   = A.row(evalOffset);
    Eigen::RowVectorXd hatRow = Arow * AtAinv * A.transpose();

    std::vector<double> coeffs(static_cast<std::size_t>(windowSize));
    for (int j = 0; j < windowSize; ++j)
        coeffs[static_cast<std::size_t>(j)] = hatRow(j);
    return coeffs;
}

SavitzkyGolayFilter::SavitzkyGolayFilter(Config cfg)
    : m_cfg{std::move(cfg)}
{
    if (m_cfg.degree < 1) {
        throw ConfigException(
            "SavitzkyGolayFilter: degree must be >= 1, got " +
            std::to_string(m_cfg.degree) + ".");
    }
    if (m_cfg.window % 2 == 0) {
        throw ConfigException(
            "SavitzkyGolayFilter: window must be odd, got " +
            std::to_string(m_cfg.window) + ".");
    }
    if (m_cfg.window < m_cfg.degree + 2) {
        throw ConfigException(
            "SavitzkyGolayFilter: window (" + std::to_string(m_cfg.window) +
            ") must be >= degree+2 (" + std::to_string(m_cfg.degree + 2) + ").");
    }

    const int half = m_cfg.window / 2;
    m_coeffs = computeCoeffs(m_cfg.window, m_cfg.degree, half);

    m_edgeLeft.resize(static_cast<std::size_t>(half));
    m_edgeRight.resize(static_cast<std::size_t>(half));

    for (int p = 0; p < half; ++p) {
        m_edgeLeft[static_cast<std::size_t>(p)] =
            computeCoeffs(m_cfg.window, m_cfg.degree, p);
        m_edgeRight[static_cast<std::size_t>(p)] =
            computeCoeffs(m_cfg.window, m_cfg.degree, m_cfg.window - 1 - p);
    }
}

FilterResult SavitzkyGolayFilter::apply(const TimeSeries& series) const
{
    const int n = static_cast<int>(series.size());
    if (n == 0) {
        throw DataException("SavitzkyGolayFilter::apply: input series is empty.");
    }
    if (n < m_cfg.window) {
        throw DataException(
            "SavitzkyGolayFilter::apply: series length (" + std::to_string(n) +
            ") is shorter than filter window (" + std::to_string(m_cfg.window) + ").");
    }

    const int half = m_cfg.window / 2;
    std::vector<double> smoothed(static_cast<std::size_t>(n), 0.0);

    // Left edge
    for (int i = 0; i < half; ++i) {
        const auto& c = m_edgeLeft[static_cast<std::size_t>(i)];
        double val = 0.0;
        for (int j = 0; j < m_cfg.window; ++j)
            val += c[static_cast<std::size_t>(j)] *
                   series[static_cast<std::size_t>(j)].value;
        smoothed[static_cast<std::size_t>(i)] = val;
    }

    // Interior
    for (int i = half; i < n - half; ++i) {
        double val = 0.0;
        for (int j = 0; j < m_cfg.window; ++j)
            val += m_coeffs[static_cast<std::size_t>(j)] *
                   series[static_cast<std::size_t>(i - half + j)].value;
        smoothed[static_cast<std::size_t>(i)] = val;
    }

    // Right edge
    for (int p = 0; p < half; ++p) {
        const int i = n - half + p;
        const auto& c = m_edgeRight[static_cast<std::size_t>(p)];
        double val = 0.0;
        for (int j = 0; j < m_cfg.window; ++j)
            val += c[static_cast<std::size_t>(j)] *
                   series[static_cast<std::size_t>(n - m_cfg.window + j)].value;
        smoothed[static_cast<std::size_t>(i)] = val;
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
    res.effectiveWindow    = m_cfg.window;
    res.effectiveBandwidth = (n > 0)
        ? static_cast<double>(m_cfg.window) / static_cast<double>(n) : 0.0;
    return res;
}

std::string SavitzkyGolayFilter::name() const
{
    return "SavitzkyGolay(window=" + std::to_string(m_cfg.window)
         + ", degree=" + std::to_string(m_cfg.degree) + ")";
}
