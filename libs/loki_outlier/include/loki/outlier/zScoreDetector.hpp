#pragma once

#include <loki/outlier/outlierDetector.hpp>

#include <string>
#include <vector>

namespace loki::outlier {

/**
 * @brief Detects outliers using the classical Z-score method.
 *
 * A point x_i is flagged as an outlier when:
 *   |x_i - mean(x)| / stddev(x) > threshold
 *
 * The default threshold of 3.0 rejects points more than 3 standard
 * deviations from the mean. This method assumes an approximately normal
 * distribution and is less robust to heavy-tailed data than IQR or MAD.
 * Prefer MadDetector or IqrDetector when the distribution is unknown.
 *
 * Location estimate : arithmetic mean
 * Scale estimate    : sample standard deviation (denominator n-1)
 */
class ZScoreDetector final : public OutlierDetector {
public:

    /**
     * @brief Constructs a ZScoreDetector with the given sigma threshold.
     * @param threshold Z-score threshold for outlier flagging (default 3.0).
     * @throws AlgorithmException if threshold <= 0.
     */
    explicit ZScoreDetector(double threshold = 3.0);

    /// @brief Returns the configured Z-score threshold.
    double zThreshold() const { return m_threshold; }

protected:

    std::size_t minSeriesLength() const override { return 3; }

    double computeLocation(const std::vector<double>& x) const override;
    double computeScale   (const std::vector<double>& x) const override;
    double computeScore   (double value, double location, double scale) const override;
    double threshold      () const override { return m_threshold; }
    std::string methodName() const override { return "Z-score"; }

private:

    double m_threshold;
};

} // namespace loki::outlier
