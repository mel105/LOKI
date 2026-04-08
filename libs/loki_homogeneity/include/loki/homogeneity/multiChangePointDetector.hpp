#pragma once

#include <loki/homogeneity/changePointDetector.hpp>
#include <loki/homogeneity/changePointResult.hpp>
#include <loki/homogeneity/snhtDetector.hpp>
#include <loki/homogeneity/peltDetector.hpp>
#include <loki/homogeneity/bocpdDetector.hpp>

#include <cstddef>
#include <vector>
#include <string>

namespace loki::homogeneity {

/**
 * @brief Configuration for MultiChangePointDetector.
 *
 * Defined outside the class to avoid a GCC bug with nested structs
 * that have member initializers used as default constructor arguments.
 */
//struct MultiChangePointDetectorConfig {
//    /// Minimum number of points a segment must have to be tested.
//    std::size_t minSegmentPoints{60};
//
//    /// Minimum time span [seconds] a segment must cover to be tested.
//    /// Ignored when the times vector passed to detect() is empty.
//    double minSegmentSeconds{0.0};
//
//    /// Configuration forwarded to the single-segment ChangePointDetector.
//    ChangePointDetectorConfig detectorConfig{};
//};

struct MultiChangePointDetectorConfig {
    std::string               method{"yao_davis"};  ///< "yao_davis" | "snht" | "pelt" | "bocpd"
    std::size_t               minSegmentPoints{60};
    double                    minSegmentSeconds{0.0};
    ChangePointDetectorConfig detectorConfig{};
    SnhtDetectorConfig        snhtConfig{};
    PeltDetectorConfig        peltConfig{};
    BocpdDetectorConfig       bocpdConfig{};
};

/**
 * @brief Detects multiple change points in a time series using recursive binary splitting.
 *
 * Applies ChangePointDetector repeatedly on sub-segments. When a change point is found
 * in segment [begin, end), the segment is split at that point and both halves are tested
 * recursively. Recursion stops when a segment is stationary or too short.
 *
 * Each sub-segment uses its own n, so the critical value is recomputed per segment
 * (correct asymptotic behaviour -- see Yao & Davis, 1986).
 *
 * Reference: Antoch et al. (1997), Elias et al. (2019).
 */
class MultiChangePointDetector {
public:

    /// Configuration type alias.
    using Config = MultiChangePointDetectorConfig;

    /**
     * @brief Constructs the detector with the given configuration.
     * @param cfg Detection configuration.
     */
    explicit MultiChangePointDetector(Config cfg = Config{});

    /**
     * @brief Detects all change points in the series z using recursive binary splitting.
     *
     * @param z     Deseasonalised, zero-mean time series values.
     * @param times Optional MJD timestamps corresponding to each element of z.
     *              Pass an empty vector when no calendar timestamps are available;
     *              in that case minSegmentSeconds is ignored and ChangePoint::mjd is 0.0.
     * @return Vector of detected change points, sorted by globalIndex (ascending).
     * @throws loki::DataException if z.size() != times.size() and times is non-empty.
     */
    [[nodiscard]]
    std::vector<ChangePoint> detect(const std::vector<double>& z,
                                    const std::vector<double>& times = {}) const;

private:

    Config m_cfg;

    /**
     * @brief Recursive helper -- tests segment [begin, end) and recurses on sub-segments.
     *
     * @param z       Full series (passed by reference, not copied).
     * @param times   Full MJD array (may be empty).
     * @param begin   Inclusive start index into z.
     * @param end     Exclusive end index into z.
     * @param result  Accumulator for detected change points.
     */
    void split(const std::vector<double>& z,
               const std::vector<double>& times,
               std::size_t begin,
               std::size_t end,
               std::vector<ChangePoint>& result) const;
};

} // namespace loki::homogeneity
