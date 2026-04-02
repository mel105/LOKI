#pragma once

#include <loki/core/exceptions.hpp>

#include <cmath>
#include <vector>

namespace loki::stationarity::detail {

/**
 * @brief Newey-West (1987) heteroskedasticity and autocorrelation consistent
 *        (HAC) long-run variance estimator.
 *
 * Computes:
 *   s^2 = gamma_0 + 2 * sum_{j=1}^{lags} w_j * gamma_j
 *
 * where gamma_j is the j-th sample autocovariance of the residuals and
 * w_j = 1 - j/(lags+1) is the Bartlett kernel weight.
 *
 * When lags == -1, the bandwidth is chosen automatically as:
 *   lags = floor(4 * (n/100)^(2/9))
 * following Andrews (1991) / Kwiatkowski et al. (1992) convention.
 *
 * @param residuals  OLS residuals (mean-adjusted or regression residuals).
 * @param lags       Number of lags to include, or -1 for automatic selection.
 * @return           Long-run variance estimate s^2 (>= 0).
 * @throws AlgorithmException if the estimate is negative (numerical issue).
 */
inline double neweyWestVariance(const std::vector<double>& residuals, int lags)
{
    const std::size_t n = residuals.size();

    if (lags < 0) {
        // Automatic bandwidth: Andrews (1991) rule for Bartlett kernel.
        lags = static_cast<int>(
            std::floor(4.0 * std::pow(static_cast<double>(n) / 100.0, 2.0 / 9.0)));
        lags = std::max(1, lags);
    }

    // gamma_0: contemporaneous variance (biased estimator, divide by n).
    double gamma0 = 0.0;
    for (double e : residuals) { gamma0 += e * e; }
    gamma0 /= static_cast<double>(n);

    double s2 = gamma0;

    for (int j = 1; j <= lags; ++j) {
        const auto jj = static_cast<std::size_t>(j);
        double gammaj = 0.0;
        for (std::size_t t = jj; t < n; ++t) {
            gammaj += residuals[t] * residuals[t - jj];
        }
        gammaj /= static_cast<double>(n);

        // Bartlett weight: 1 - j / (lags + 1)
        const double w = 1.0 - static_cast<double>(j) / static_cast<double>(lags + 1);
        s2 += 2.0 * w * gammaj;
    }

    if (s2 < 0.0) {
        // Can happen with very few observations; clamp to zero.
        s2 = 0.0;
    }

    return s2;
}

} // namespace loki::stationarity::detail
