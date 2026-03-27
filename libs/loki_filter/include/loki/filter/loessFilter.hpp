#pragma once

#include <loki/filter/filter.hpp>

namespace loki {

/**
 * @brief Locally Estimated Scatterplot Smoothing (LOESS / LOWESS) filter.
 *
 * For each point i, selects the k = ceil(bandwidth * n) nearest neighbours,
 * fits a weighted polynomial of the given degree using LsqSolver, and takes
 * the fitted value at i as the smoothed output. Because neighbours are always
 * available (the window shifts at the series edges), no edge fill is needed.
 *
 * Optionally applies Iteratively Reweighted Least Squares (IRLS) to
 * downweight observations whose residuals are large within the local window,
 * improving robustness against outliers in the input.
 *
 * LOESS performs a full LSQ solve per output point -- it is slower than
 * KernelSmoother or SavitzkyGolayFilter for the same window size. Prefer
 * KernelSmoother or SavitzkyGolayFilter for high-frequency (ms) data.
 *
 * Reference: Cleveland, W.S. (1979). Robust Locally Weighted Regression
 * and Smoothing Scatterplots. JASA 74(368), 829-836.
 *
 * The input series must be free of NaN. Use GapFiller in the pipeline
 * before applying this filter.
 */
class LoessFilter : public Filter {
public:
    /** @brief Kernel used for local weighting within the neighbourhood. */
    enum class Kernel {
        TRICUBE,      ///< K(u) = (1 - u^3)^3, u in [0,1]. Default for LOESS.
        EPANECHNIKOV, ///< K(u) = 0.75*(1 - u^2), u in [0,1].
        GAUSSIAN      ///< K(u) = exp(-0.5*(3u)^2). gaussianCutoff applies.
    };

    /**
     * @brief Configuration for LoessFilter.
     */
    struct Config {
        double bandwidth{0.25};      ///< Fraction of series length used as neighbourhood size.
                                     ///< k = ceil(bandwidth * n) neighbours per point.
                                     ///< Must be in (0, 1].
        int    degree{1};            ///< Local polynomial degree. Must be 1 or 2.
        bool   robust{false};        ///< Enable IRLS for robustness against local outliers.
        int    robustIterations{3};  ///< Number of IRLS iterations. Ignored if robust=false.
        Kernel kernel{Kernel::TRICUBE}; ///< Kernel weighting function.
        double gaussianCutoff{3.0};  ///< GAUSSIAN only: truncate at cutoff * h. Ignored otherwise.
    };

    /**
     * @brief Construct with configuration.
     * @param cfg Filter configuration.
     * @throws ConfigException if degree is not 1 or 2, or bandwidth is not in (0, 1].
     */
    explicit LoessFilter(Config cfg);

    /**
     * @brief Apply LOESS smoothing to the series.
     *
     * @param series Input time series. Must not contain NaN.
     * @return FilterResult with smoothed series, residuals, and filter name.
     * @throws DataException   if series is empty or contains NaN.
     * @throws ConfigException if bandwidth produces fewer neighbours than degree+1.
     * @throws AlgorithmException if local LSQ system is rank-deficient.
     */
    FilterResult apply(const TimeSeries& series) const override;

    /** @brief Returns "LOESS(degree=<d>, kernel=<k>)". */
    std::string name() const override;

private:
    Config m_cfg;

    /**
     * @brief Evaluate kernel weight for normalised distance u in [0, 1].
     *
     * u = |i - j| / maxDist where maxDist is the distance to the k-th
     * nearest neighbour. u=0 at the centre, u=1 at the boundary.
     *
     * @param k  Kernel type.
     * @param u  Normalised distance in [0, 1].
     * @return   Non-negative kernel weight.
     */
    static double kernelWeight(Kernel k, double u);

    /** @brief Human-readable kernel name. */
    static std::string kernelName(Kernel k);
};

} // namespace loki
