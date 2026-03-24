#pragma once

#include <cstddef>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace loki::outlier {

// ----------------------------------------------------------------------------
// OutlierPoint -- single detected outlier
// ----------------------------------------------------------------------------

/**
 * @brief Describes a single detected outlier within a time series.
 *
 * Produced by detector classes (IqrDetector, MadDetector, ZScoreDetector,
 * HatMatrixDetector). The index refers to the position within the residual
 * vector passed to the detector, which is the same as the position in the
 * full series if no sub-slicing was performed.
 *
 * @note replacedValue is set to NaN on construction. OutlierCleaner fills it
 *       after running GapFiller on the flagged positions.
 */
struct OutlierPoint {

    /// Zero-based index of the outlier in the input residual vector.
    std::size_t index{0};

    /// Original value at this position before any replacement.
    double originalValue{std::numeric_limits<double>::quiet_NaN()};

    /// Value after replacement by OutlierCleaner. NaN until replacement is done.
    double replacedValue{std::numeric_limits<double>::quiet_NaN()};

    /// Detector-specific score: z-score, IQR multiple, Mahalanobis distance, etc.
    double score{0.0};

    /// Threshold that was exceeded, triggering detection.
    double threshold{0.0};

    /// Quality flag written to the TimeSeries observation after replacement.
    /// Convention: 1 = outlier detected, 2 = outlier replaced.
    int flag{1};
};

// ----------------------------------------------------------------------------
// OutlierResult -- summary for one detection run on one series
// ----------------------------------------------------------------------------

/**
 * @brief Aggregated result of one outlier detection pass on a single series.
 *
 * Returned by all OutlierDetector::detect() implementations. The @p points
 * vector contains one OutlierPoint per detected outlier. Summary statistics
 * (location, scale) describe the robust estimates used during detection,
 * allowing the caller to audit and log the decision process.
 *
 * For detectors that operate on residuals (IQR, MAD, Z-score) the location
 * and scale refer to those residuals, not the raw series.
 */
struct OutlierResult {

    /// One entry per detected outlier. Empty if no outliers were found.
    std::vector<OutlierPoint> points;

    /// Human-readable detector identifier: "IQR", "MAD", "Z-score", "hat_matrix".
    std::string method;

    /// Robust location estimate used during detection (e.g. median, mean).
    double location{0.0};

    /// Robust scale estimate used during detection (e.g. IQR, MAD, std dev).
    double scale{0.0};

    /// Number of input points passed to the detector.
    std::size_t n{0};

    /// Number of detected outliers (equals points.size()).
    std::size_t nOutliers{0};
};

} // namespace loki::outlier
