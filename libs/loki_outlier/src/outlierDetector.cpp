#include <loki/outlier/outlierDetector.hpp>
#include <loki/core/exceptions.hpp>

#include <cmath>
#include <limits>

using namespace loki;

namespace loki::outlier {

OutlierResult OutlierDetector::detect(const std::vector<double>& residuals) const
{
    // --- validate length ---
    if (residuals.size() < minSeriesLength()) {
        throw SeriesTooShortException(
            "OutlierDetector::detect: series length " +
            std::to_string(residuals.size()) +
            " is below minimum " +
            std::to_string(minSeriesLength()) + ".");
    }

    // --- validate no NaN ---
    for (std::size_t i = 0; i < residuals.size(); ++i) {
        if (std::isnan(residuals[i])) {
            throw MissingValueException(
                "OutlierDetector::detect: NaN at index " +
                std::to_string(i) + ". Residuals must be NaN-free.");
        }
    }

    // --- robust estimates ---
    const double loc   = computeLocation(residuals);
    const double scale = computeScale(residuals);

    OutlierResult result;
    result.method   = methodName();
    result.location = loc;
    result.scale    = scale;
    result.n        = residuals.size();

    // scale == 0 means all values are identical -- no outliers by definition
    if (scale == 0.0) {
        result.nOutliers = 0;
        return result;
    }

    const double thr = threshold();

    for (std::size_t i = 0; i < residuals.size(); ++i) {
        const double score = computeScore(residuals[i], loc, scale);
        if (std::abs(score) > thr) {
            OutlierPoint pt;
            pt.index         = i;
            pt.originalValue = residuals[i];
            pt.score         = score;
            pt.threshold     = thr;
            pt.flag          = 1;
            result.points.push_back(pt);
        }
    }

    result.nOutliers = result.points.size();
    return result;
}

} // namespace loki::outlier
