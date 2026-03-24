#include <loki/outlier/madDetector.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/stats/descriptive.hpp>

using namespace loki;

namespace loki::outlier {

MadDetector::MadDetector(double multiplier)
    : m_multiplier(multiplier)
{
    if (multiplier <= 0.0) {
        throw AlgorithmException(
            "MadDetector: multiplier must be positive, got " +
            std::to_string(multiplier) + ".");
    }
}

double MadDetector::computeLocation(const std::vector<double>& x) const
{
    return loki::stats::median(x, loki::NanPolicy::SKIP);
}

double MadDetector::computeScale(const std::vector<double>& x) const
{
    // Normalise MAD to be a consistent estimator of std dev under normality
    return loki::stats::mad(x, loki::NanPolicy::SKIP) / CONSISTENCY_FACTOR;
}

double MadDetector::computeScore(double value, double location, double scale) const
{
    // scale > 0 is guaranteed by OutlierDetector::detect() before calling this
    return (value - location) / scale;
}

} // namespace loki::outlier
