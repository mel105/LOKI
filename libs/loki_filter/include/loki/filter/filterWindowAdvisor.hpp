#pragma once

#include <loki/timeseries/timeSeries.hpp>

#include <string>

namespace loki {

/**
 * @brief Estimates a suitable smoothing window size for filter configuration.
 *
 * Provides three estimation strategies:
 *
 *   SILVERMAN_MAD -- Silverman's rule of thumb with MAD-based robust sigma.
 *                    sigma_H = MAD / 0.6745
 *                    bw = sigma_H * (4 / (3*n))^0.2
 *                    Robust against outliers. Recommended default.
 *
 *   SILVERMAN     -- Classical Silverman rule using min(stddev, IQR/1.34).
 *                    bw = 0.9 * min(std, IQR/1.34) * n^(-1/5)
 *                    Standard kernel density estimation bandwidth.
 *
 *   ACF_PEAK      -- Estimates the dominant period from the ACF by locating
 *                    the first significant local maximum beyond lag 0.
 *                    Useful when the series has a known periodic component
 *                    and the window should cover exactly one period.
 *
 * Output windowSamples is always forced to an odd integer >= 3.
 * bandwidth = windowSamples / n (fraction of series length).
 *
 * Use Advice::rationale for logging -- it explains the chosen values.
 */

// -----------------------------------------------------------------------------
//  FilterWindowAdvisorMethod -- defined outside class to avoid GCC 13
//  aggregate-init bug with enum members in nested structs.
// -----------------------------------------------------------------------------

/** @brief Bandwidth estimation method. */
enum class FilterWindowAdvisorMethod {
    SILVERMAN_MAD, ///< MAD-based Silverman (robust, recommended default).
    SILVERMAN,     ///< Classical Silverman with min(std, IQR/1.34).
    ACF_PEAK       ///< Dominant period from ACF first local maximum.
};

// -----------------------------------------------------------------------------
//  FilterWindowAdvisorConfig -- defined outside class for the same reason.
// -----------------------------------------------------------------------------

/**
 * @brief Configuration for FilterWindowAdvisor.
 */
struct FilterWindowAdvisorConfig {
    FilterWindowAdvisorMethod method{FilterWindowAdvisorMethod::SILVERMAN_MAD};
    int acfMaxLag{0}; ///< ACF_PEAK only: max lag to search. 0 = auto: min(n/10, 200).
    int minWindow{3}; ///< Minimum window size in samples (must be odd >= 3).
};

// -----------------------------------------------------------------------------
//  FilterWindowAdvisor
// -----------------------------------------------------------------------------

class FilterWindowAdvisor {
public:

    // Aliases for backward compatibility.
    using Method = FilterWindowAdvisorMethod;
    using Config = FilterWindowAdvisorConfig;

    /**
     * @brief Estimated smoothing window parameters.
     */
    struct Advice {
        int         windowSamples{3};  ///< Recommended window in samples (always odd >= 3).
        double      bandwidth{0.0};    ///< windowSamples / n (for KernelSmoother / LOESS Config).
        std::string rationale;         ///< Human-readable explanation for logging.
    };

    /**
     * @brief Estimate the smoothing window for the given series.
     *
     * @param series  Input time series. Must not be empty.
     * @param cfg     Estimation configuration.
     * @return        Advice struct with recommended window and rationale.
     * @throws DataException   if series is empty.
     * @throws ConfigException if ACF_PEAK finds no peak within maxLag.
     */
    static Advice advise(const TimeSeries& series,
                         const Config& cfg = Config{});

private:
    FilterWindowAdvisor() = delete;

    static int silvermanMad(const std::vector<double>& values, int n, std::string& rationale);
    static int silverman   (const std::vector<double>& values, int n, std::string& rationale);
    static int acfPeak     (const std::vector<double>& values, int n, int maxLag, std::string& rationale);
    static int toOdd(int v, int minVal);
};

} // namespace loki
