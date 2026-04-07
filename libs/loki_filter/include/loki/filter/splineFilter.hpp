#pragma once

#include <loki/filter/filter.hpp>
#include <loki/filter/filterResult.hpp>
#include <loki/core/config.hpp>

#include <string>

namespace loki {

// ---------------------------------------------------------------------------
//  SplineFilter
// ---------------------------------------------------------------------------

/**
 * @brief Smoothing filter based on cubic spline interpolation.
 *
 * Subsamples the time series at every subsampleStep-th observation to form
 * the spline knots, then evaluates the spline at all original time points.
 * The difference between the original and the spline is the residual.
 *
 * Smoothing behaviour:
 *   subsampleStep = 1   -> exact interpolation through all points (no smoothing).
 *   subsampleStep = 10  -> knot every 10 samples; gentle smoothing.
 *   subsampleStep = 100 -> knot every 100 samples; strong smoothing (trend-like).
 *   subsampleStep = 0   -> automatic: n/200, suitable for most climatological series.
 *
 * The first and last observations are always included as knots regardless of
 * subsampleStep, so the spline is anchored at both ends.
 *
 * NaN handling: NaN observations are skipped when selecting knots and when
 * evaluating (residual is NaN for NaN input). The filter requires at least
 * 3 non-NaN observations.
 *
 * Boundary condition: "not_a_knot" (default) matches MATLAB spline() behaviour
 * and is recommended for general use. "natural" produces zero curvature at edges.
 *
 * Performance: spline construction is O(k) where k = n/subsampleStep.
 * Evaluation is O(n log k). For n = 1M and subsampleStep = 5000, k = 200
 * knots -- construction is near-instantaneous.
 */
class SplineFilter : public Filter {
public:
    using Config = SplineFilterConfig;

    /**
     * @brief Constructs a SplineFilter with the given configuration.
     * @param cfg Configuration. Default: automatic subsampleStep, not_a_knot BC.
     */
    explicit SplineFilter(Config cfg = Config{});

    /**
     * @brief Applies the spline smoothing filter to the series.
     *
     * @param series Input time series. Must have at least 3 non-NaN observations.
     * @return       FilterResult with smoothed series, residuals, and filter name.
     * @throws DataException      if fewer than 3 non-NaN observations are present.
     * @throws ConfigException    if bc string is not recognised.
     * @throws AlgorithmException if spline construction fails (degenerate knots).
     */
    FilterResult apply(const TimeSeries& series) const override;

    /// @brief Returns "SplineFilter".
    std::string name() const override;

private:
    Config m_cfg;
};

} // namespace loki
