#include <loki/filter/splineFilter.hpp>

#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/math/spline.hpp>

#include <cmath>
#include <string>
#include <vector>

using namespace loki;

// ---------------------------------------------------------------------------
//  Construction
// ---------------------------------------------------------------------------

SplineFilter::SplineFilter(Config cfg)
    : m_cfg(std::move(cfg))
{}

// ---------------------------------------------------------------------------
//  apply
// ---------------------------------------------------------------------------

FilterResult SplineFilter::apply(const TimeSeries& series) const
{
    if (series.empty()) {
        throw DataException("SplineFilter::apply: input series is empty.");
    }

    const std::size_t n = series.size();

    // Parse boundary condition string.
    loki::math::BoundaryCondition bc;
    if      (m_cfg.bc == "natural")     bc = loki::math::BoundaryCondition::NATURAL;
    else if (m_cfg.bc == "not_a_knot")  bc = loki::math::BoundaryCondition::NOT_A_KNOT;
    else if (m_cfg.bc == "clamped")     bc = loki::math::BoundaryCondition::CLAMPED;
    else {
        throw ConfigException(
            "SplineFilter: unknown boundary condition '" + m_cfg.bc
            + "'. Use 'natural', 'not_a_knot', or 'clamped'.");
    }

    // Collect valid (non-NaN) observations -- these are candidates for knots.
    std::vector<std::size_t> validIdx;
    validIdx.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        if (series[i].value == series[i].value) {  // NaN check
            validIdx.push_back(i);
        }
    }

    if (validIdx.size() < 3) {
        throw DataException(
            "SplineFilter::apply: at least 3 non-NaN observations required, got "
            + std::to_string(validIdx.size()) + ".");
    }

    // Resolve subsampleStep.
    int step = m_cfg.subsampleStep;
    if (step <= 0) {
        // Automatic: n/200, clamped to [1, n/4].
        const std::size_t autoStep = std::max(std::size_t{1},
                                              std::min(validIdx.size() / 200,
                                                       validIdx.size() / 4));
        step = static_cast<int>(autoStep);
        LOKI_INFO("SplineFilter: auto subsampleStep = " + std::to_string(step)
                  + " (n_valid=" + std::to_string(validIdx.size()) + ").");
    }

    // Select knot indices from valid observations: every step-th, plus always
    // include the first and last valid observation as anchors.
    std::vector<std::size_t> knotIdx;
    knotIdx.reserve(validIdx.size() / static_cast<std::size_t>(step) + 2);

    knotIdx.push_back(validIdx.front());
    for (std::size_t k = static_cast<std::size_t>(step);
         k < validIdx.size();
         k += static_cast<std::size_t>(step))
    {
        knotIdx.push_back(validIdx[k]);
    }
    // Always include last valid observation.
    if (knotIdx.back() != validIdx.back()) {
        knotIdx.push_back(validIdx.back());
    }

    // Build knot vectors (MJD as abscissa for numerical stability).
    std::vector<double> kx, ky;
    kx.reserve(knotIdx.size());
    ky.reserve(knotIdx.size());
    for (std::size_t idx : knotIdx) {
        kx.push_back(series[idx].time.mjd());
        ky.push_back(series[idx].value);
    }

    // Construct spline.
    loki::math::CubicSplineConfig splineCfg;
    splineCfg.bc = bc;
    const auto spline = loki::math::CubicSpline::interpolate(kx, ky, splineCfg);

    LOKI_INFO("SplineFilter: built spline with " + std::to_string(kx.size())
              + " knots from " + std::to_string(n) + " observations.");

    // Evaluate spline at all original time points.
    TimeSeries filteredSeries{series.metadata()};
    TimeSeries residualSeries{series.metadata()};
    filteredSeries.reserve(n);
    residualSeries.reserve(n);

    for (std::size_t i = 0; i < n; ++i) {
        const double xq  = series[i].time.mjd();
        const double val = series[i].value;

        if (val != val) {
            // NaN input -- propagate NaN to both filtered and residual.
            filteredSeries.append(series[i].time, std::numeric_limits<double>::quiet_NaN());
            residualSeries.append(series[i].time, std::numeric_limits<double>::quiet_NaN());
        } else {
            const double smoothed = spline.evaluate(xq);
            filteredSeries.append(series[i].time, smoothed);
            residualSeries.append(series[i].time, val - smoothed);
        }
    }

    const double effectiveBw = (n > 0)
        ? static_cast<double>(step) / static_cast<double>(n)
        : 0.0;

    FilterResult res{std::move(filteredSeries), std::move(residualSeries), name()};
    res.effectiveWindow    = step;
    res.effectiveBandwidth = effectiveBw;
    return res;
}

// ---------------------------------------------------------------------------
//  name
// ---------------------------------------------------------------------------

std::string SplineFilter::name() const
{
    return "SplineFilter";
}
