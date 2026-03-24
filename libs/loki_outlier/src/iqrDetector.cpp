#include <loki/outlier/iqrDetector.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/stats/descriptive.hpp>

using namespace loki;

namespace loki::outlier {

IqrDetector::IqrDetector(double multiplier)
    : m_multiplier(multiplier)
{
    if (multiplier <= 0.0) {
        throw AlgorithmException(
            "IqrDetector: multiplier must be positive, got " +
            std::to_string(multiplier) + ".");
    }
}

double IqrDetector::computeLocation(const std::vector<double>& x) const
{
    return loki::stats::median(x, loki::NanPolicy::SKIP);
}

double IqrDetector::computeScale(const std::vector<double>& x) const
{
    return loki::stats::iqr(x, loki::NanPolicy::SKIP);
}

double IqrDetector::computeScore(double value, double location, double scale) const
{
    // scale > 0 is guaranteed by OutlierDetector::detect() before calling this
    return (value - location) / scale;
}

} // namespace loki::outlier
