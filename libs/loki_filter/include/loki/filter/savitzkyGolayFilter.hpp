#pragma once

#include <loki/filter/filter.hpp>

#include <vector>

namespace loki {

/**
 * @brief Savitzky-Golay polynomial convolution filter.
 *
 * Fits a polynomial of the given degree to a symmetric window of samples
 * using unweighted LSQ, then extracts the convolution coefficients. The
 * same coefficients are applied to every interior point via convolution,
 * giving O(n * window) complexity -- significantly faster than LOESS for
 * the same window size.
 *
 * Coefficients are computed once in the constructor and reused for every
 * apply() call, making repeated application to multiple series efficient.
 *
 * Edge handling: the window is asymmetrically fitted at the edges using
 * separate coefficient sets computed for each edge position, so no
 * nearest-neighbour fill or NaN padding is introduced.
 *
 * Well suited for high-frequency (ms-resolution) signal data where
 * peak shapes and edge features must be preserved.
 *
 * The input series must be free of NaN. Use GapFiller in the pipeline
 * before applying this filter.
 *
 * Reference: Savitzky, A. and Golay, M.J.E. (1964). Smoothing and
 * Differentiation of Data by Simplified Least Squares Procedures.
 * Analytical Chemistry 36(8), 1627-1639.
 */
class SavitzkyGolayFilter : public Filter {
public:
    /**
     * @brief Configuration for SavitzkyGolayFilter.
     */
    struct Config {
        int window{5};  ///< Number of samples in the filter window. Must be odd and >= degree+2.
        int degree{2};  ///< Polynomial degree. Must be >= 1 and < window.
    };

    /**
     * @brief Construct and precompute convolution coefficients.
     *
     * @param cfg Filter configuration.
     * @throws ConfigException if window is even, window < degree+2, or degree < 1.
     * @throws AlgorithmException if coefficient matrix is rank-deficient.
     */
    explicit SavitzkyGolayFilter(Config cfg);

    /**
     * @brief Apply Savitzky-Golay filter to the series.
     *
     * @param series Input time series. Must not contain NaN.
     * @return FilterResult with filtered series, residuals, and filter name.
     * @throws DataException   if series is empty or shorter than window.
     */
    FilterResult apply(const TimeSeries& series) const override;

    /** @brief Returns "SavitzkyGolay(window=<w>, degree=<d>)". */
    std::string name() const override;

private:
    Config m_cfg;

    /**
     * @brief Convolution coefficients for interior points.
     * Size = window. Symmetric around centre index (half = window/2).
     */
    std::vector<double> m_coeffs;

    /**
     * @brief Convolution coefficients for each left-edge position.
     * m_edgeLeft[p] gives coefficients for output position p (p < half),
     * where the window is right-aligned starting at index 0.
     * Each inner vector has size = window.
     */
    std::vector<std::vector<double>> m_edgeLeft;

    /**
     * @brief Convolution coefficients for each right-edge position.
     * m_edgeRight[p] gives coefficients for output position n-1-p (p < half).
     * Each inner vector has size = window.
     */
    std::vector<std::vector<double>> m_edgeRight;

    /**
     * @brief Compute SG coefficients for a window starting at offset relative to centre.
     *
     * @param windowSize  Number of points in the window.
     * @param degree      Polynomial degree.
     * @param evalOffset  Position within the window at which to evaluate the polynomial.
     *                    0 = leftmost sample, half = centre, window-1 = rightmost.
     * @return            Coefficient vector of length windowSize.
     */
    static std::vector<double> computeCoeffs(int windowSize, int degree, int evalOffset);
};

} // namespace loki
