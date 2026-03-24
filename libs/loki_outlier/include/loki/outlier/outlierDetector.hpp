#pragma once

#include <loki/outlier/outlierResult.hpp>
#include <loki/core/exceptions.hpp>

#include <string>
#include <vector>

namespace loki::outlier {

/**
 * @brief Abstract base class for all outlier detectors.
 *
 * Implements the Template Method pattern: detect() drives the full detection
 * pipeline while derived classes supply the robust location/scale estimates,
 * the per-point score function, the threshold, and the method name.
 *
 * All detectors operate on a vector of residuals (deseasonalized values).
 * They do NOT modify the input series -- replacement is performed by
 * OutlierCleaner via GapFiller.
 */
class OutlierDetector {
public:

    virtual ~OutlierDetector() = default;

    /**
     * @brief Runs outlier detection on a residual series.
     *
     * Computes robust location and scale, scores every point, and collects
     * those whose absolute score exceeds the threshold into OutlierResult.
     * When scale == 0 (all values identical), returns an empty result.
     *
     * @param residuals Input residual series. Must not contain NaN.
     * @return OutlierResult with one OutlierPoint per detected outlier.
     * @throws SeriesTooShortException if residuals.size() < minSeriesLength().
     * @throws MissingValueException   if any residual value is NaN.
     */
    OutlierResult detect(const std::vector<double>& residuals) const;

protected:

    /**
     * @brief Minimum number of points required by this detector.
     *
     * Default is 4 (sufficient for IQR and MAD). Override in derived classes
     * if a larger sample is required (e.g. Z-score needs >= 3).
     */
    virtual std::size_t minSeriesLength() const { return 4; }

    /**
     * @brief Computes the robust location estimate (e.g. median or mean).
     * @param x Validated, NaN-free input series.
     * @return Location estimate.
     */
    virtual double computeLocation(const std::vector<double>& x) const = 0;

    /**
     * @brief Computes the robust scale estimate (e.g. IQR, MAD, std dev).
     * @param x Validated, NaN-free input series.
     * @return Scale estimate. May be 0 when all values are identical.
     */
    virtual double computeScale(const std::vector<double>& x) const = 0;

    /**
     * @brief Computes the outlier score for a single value.
     *
     * The sign of the returned score is preserved so that OutlierPoint::score
     * indicates the direction of the deviation.
     *
     * @param value    Data point being scored.
     * @param location Robust location estimate for this series.
     * @param scale    Robust scale estimate for this series.
     * @return Signed score. Outlier when |score| > threshold().
     */
    virtual double computeScore(double value, double location, double scale) const = 0;

    /**
     * @brief Returns the threshold above which |score| is flagged as outlier.
     * @return Positive threshold value.
     */
    virtual double threshold() const = 0;

    /**
     * @brief Returns the human-readable detector name stored in OutlierResult.
     * @return Method string, e.g. "IQR", "MAD", "Z-score".
     */
    virtual std::string methodName() const = 0;
};

} // namespace loki::outlier
