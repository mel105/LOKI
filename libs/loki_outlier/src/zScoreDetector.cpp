#include <loki/outlier/zScoreDetector.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/stats/descriptive.hpp>

using namespace loki;

namespace loki::outlier {

ZScoreDetector::ZScoreDetector(double threshold)
    : m_threshold(threshold)
{
    if (threshold <= 0.0) {
        throw AlgorithmException(
            "ZScoreDetector: threshold must be positive, got " +
            std::to_string(threshold) + ".");
    }
}

double ZScoreDetector::computeLocation(const std::vector<double>& x) const
{
    return loki::stats::mean(x, loki::NanPolicy::SKIP);
}

double ZScoreDetector::computeScale(const std::vector<double>& x) const
{
    // sample std dev (denominator n-1)
    return loki::stats::stddev(x, /*population=*/false, loki::NanPolicy::SKIP);
}

double ZScoreDetector::computeScore(double value, double location, double scale) const
{
    // scale > 0 is guaranteed by OutlierDetector::detect() before calling this
    return (value - location) / scale;
}

} // namespace loki::outlier
