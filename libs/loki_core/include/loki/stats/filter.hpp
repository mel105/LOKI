#pragma once

#include <loki/core/exceptions.hpp>

#include <string_view>
#include <vector>

namespace loki::stats {

/**
 * @brief Centered simple moving average filter.
 *
 * The output at position i is the mean of the window
 * [i - half, i + half] where half = windowSize / 2.
 * Positions within half steps of either boundary are set to NaN.
 *
 * windowSize must be >= 3. If an even value is supplied it is rounded up
 * to the next odd integer and a warning is logged.
 *
 * Input must not contain NaN values. Throws DataException if any NaN is found.
 *
 * @param x          Input values.
 * @param windowSize Window width (must be odd >= 3; even values rounded up).
 * @return Filtered series of the same length; edge positions are NaN.
 * @throws ConfigException if windowSize < 3 after rounding.
 * @throws DataException   if x contains NaN.
 */
[[nodiscard]]
std::vector<double> movingAverage(const std::vector<double>& x, int windowSize);

/**
 * @brief Exponential moving average (single forward pass).
 *
 * Recurrence: y[i] = alpha * x[i] + (1 - alpha) * y[i-1]
 * The first valid (non-NaN) value initialises y; preceding positions are NaN.
 *
 * If NaN is encountered after initialisation the output slot is NaN and
 * the internal accumulator retains its last valid value so that the filter
 * continues smoothly once valid data resumes.
 *
 * Input must not contain NaN values (enforced by pre-check).
 * Throws DataException if any NaN is found.
 *
 * @param x     Input values.
 * @param alpha Smoothing factor in (0, 1]. alpha = 1 returns a copy of x.
 * @return Filtered series of the same length.
 * @throws ConfigException if alpha is not in (0, 1].
 * @throws DataException   if x contains NaN.
 */
[[nodiscard]]
std::vector<double> exponentialMovingAverage(const std::vector<double>& x, double alpha);

/**
 * @brief Centered weighted moving average filter.
 *
 * weights defines a symmetric kernel. Its size must be odd and >= 3.
 * Weights are normalised internally (sum to 1) so the caller need not
 * pre-normalise them. Common kernels: triangular {1,2,3,2,1},
 * Gaussian-like {1,4,6,4,1}, uniform {1,1,1,1,1}.
 *
 * Positions within half-window steps of either boundary are set to NaN.
 * Input must not contain NaN values. Throws DataException if any NaN is found.
 *
 * @param x       Input values.
 * @param weights Kernel weights. Size must be odd >= 3.
 * @return Filtered series of the same length; edge positions are NaN.
 * @throws ConfigException if weights.size() < 3 or weights.size() is even.
 * @throws DataException   if x contains NaN or if all weights are zero.
 */
[[nodiscard]]
std::vector<double> weightedMovingAverage(const std::vector<double>& x,
                                          const std::vector<double>& weights);

} // namespace loki::stats
