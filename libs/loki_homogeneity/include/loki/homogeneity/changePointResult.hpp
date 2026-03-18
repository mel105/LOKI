#pragma once

#include <cstddef>

namespace loki::homogeneity {

/**
 * @brief Result of a single change point detection on one segment.
 *
 * All index values are positions within the sub-series passed to
 * ChangePointDetector::detect(), i.e. relative to the 'begin' argument.
 * The caller (MultiChangePointDetector) is responsible for translating
 * them into global coordinates.
 */
struct ChangePointResult {

    /// True when the test rejects the null hypothesis of homogeneity.
    bool detected{false};

    /// Position of the detected change point within [begin, end).
    /// Set to -1 when detected == false.
    int index{-1};

    /// Estimated mean shift: meanAfter - meanBefore.
    double shift{0.0};

    double meanBefore{0.0};
    double meanAfter{0.0};

    /// Maximum value of the T_k statistic over the segment.
    double maxTk{0.0};

    /// Critical value used for the hypothesis test (already scaled by sigmaStar).
    double criticalValue{0.0};

    /// Asymptotic p-value derived from the Gumbel extreme-value distribution.
    double pValue{0.0};

    /// Lag-1 autocorrelation of the residuals (centered segments).
    double acfLag1{0.0};

    /// Noise-dependence correction factor (Antoch et al., 1995).
    /// sigmaStar == 1.0 implies no correction was needed.
    double sigmaStar{1.0};

    /// Lower bound of the confidence interval for the change point position.
    /// Set to -1 when not available.
    int confIntervalLow{-1};

    /// Upper bound of the confidence interval for the change point position.
    /// Set to -1 when not available.
    int confIntervalHigh{-1};
};

/**
 * @brief A detected change point in global (full-series) coordinates.
 *
 * Produced by MultiChangePointDetector after translating the segment-relative
 * ChangePointResult into absolute positions.
 */
struct ChangePoint {

    /// Zero-based index into the full series where the change point was detected.
    std::size_t globalIndex{0};

    /// MJD timestamp of the change point epoch.
    /// Set to 0.0 when no time axis was provided to MultiChangePointDetector.
    double mjd{0.0};

    /// Estimated mean shift at this change point (meanAfter - meanBefore).
    double shift{0.0};

    /// Asymptotic p-value at detection.
    double pValue{0.0};
};

} // namespace loki::homogeneity
