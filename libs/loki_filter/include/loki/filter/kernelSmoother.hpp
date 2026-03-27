#pragma once

#include <loki/filter/filter.hpp>

namespace loki {

/**
 * @brief Non-parametric kernel smoother for time series data.
 *
 * Computes a weighted local average at each point using a kernel function.
 * For each output point i, only neighbours within the effective bandwidth
 * window are visited, giving O(n * w) complexity instead of the naive O(n^2).
 *
 * Bandwidth is specified as a fraction of series length so that the smoother
 * behaves consistently across different sampling rates without reconfiguration.
 *
 * Supported kernels:
 *   EPANECHNIKOV -- optimal in MSE sense, compact support: zero outside [-1, 1]
 *   GAUSSIAN     -- infinite support, truncated at gaussianCutoff * h samples
 *   UNIFORM      -- rectangular window, equivalent to simple moving average
 *   TRIANGULAR   -- linear decay, good general-purpose compromise
 *
 * Edge positions are filled using nearest-neighbour extrapolation (same
 * strategy as MovingAverageFilter). The input series must be free of NaN;
 * use GapFiller in the pipeline before applying this filter.
 *
 * Reference: Simonoff, J.S. (1996). Smoothing Methods in Statistics. Springer.
 */
class KernelSmoother : public Filter {
public:
    /** @brief Kernel function used for local weighting. */
    enum class Kernel {
        EPANECHNIKOV, ///< K(u) = 0.75*(1-u^2) for |u| <= 1, else 0.
        GAUSSIAN,     ///< K(u) = exp(-0.5*u^2) / sqrt(2*pi), truncated at gaussianCutoff.
        UNIFORM,      ///< K(u) = 0.5 for |u| <= 1, else 0.
        TRIANGULAR    ///< K(u) = 1 - |u| for |u| <= 1, else 0.
    };

    /**
     * @brief Configuration for KernelSmoother.
     */
    struct Config {
        double bandwidth{0.1};              ///< Fraction of series length used as half-window.
                                            ///< Must be in (0, 1). E.g. 0.1 = 10% of series.
        Kernel kernel{Kernel::EPANECHNIKOV}; ///< Kernel function.
        double gaussianCutoff{3.0};         ///< GAUSSIAN only: truncate at cutoff * h samples.
                                            ///< Ignored for other kernels.
    };

    /**
     * @brief Construct with configuration.
     * @param cfg Smoother configuration.
     * @throws ConfigException if bandwidth is not in (0, 1).
     */
    explicit KernelSmoother(Config cfg);

    /**
     * @brief Apply kernel smoothing to the series.
     *
     * @param series Input time series. Must not contain NaN.
     * @return FilterResult with smoothed series, residuals, and filter name.
     * @throws DataException   if series is empty or contains NaN.
     * @throws ConfigException if bandwidth produces a window smaller than 1 sample.
     */
    FilterResult apply(const TimeSeries& series) const override;

    /** @brief Returns "KernelSmoother(<kernel name>)". */
    std::string name() const override;

private:
    Config m_cfg;

    /**
     * @brief Evaluate the kernel weight for normalised distance u = (i - j) / h.
     *
     * @param k  Kernel type.
     * @param u  Normalised distance. Positive or negative.
     * @return   Non-negative kernel weight. Zero outside the kernel's support.
     */
    static double kernelWeight(Kernel k, double u);

    /** @brief Human-readable kernel name for use in filter name string. */
    static std::string kernelName(Kernel k);
};

} // namespace loki
