#pragma once

#include <loki/outlier/outlierDetector.hpp>

#include <string>
#include <vector>

namespace loki::outlier {

/**
 * @brief Detects outliers using the Interquartile Range (IQR) method.
 *
 * A point x_i is flagged as an outlier when:
 *   |x_i - median(x)| / IQR(x) > multiplier
 *
 * The default multiplier of 1.5 corresponds to the classic Tukey fence.
 * The input series must be deseasonalized residuals; the detector does not
 * account for seasonal structure.
 *
 * Location estimate : median
 * Scale estimate    : IQR = Q3 - Q1
 */
class IqrDetector final : public OutlierDetector {
public:

    /**
     * @brief Constructs an IqrDetector with the given fence multiplier.
     * @param multiplier IQR multiplier for the outlier fence (default 1.5).
     * @throws AlgorithmException if multiplier <= 0.
     */
    explicit IqrDetector(double multiplier = 1.5);

    /// @brief Returns the configured IQR multiplier.
    double multiplier() const { return m_multiplier; }

protected:

    std::size_t minSeriesLength() const override { return 4; }

    double computeLocation(const std::vector<double>& x) const override;
    double computeScale   (const std::vector<double>& x) const override;
    double computeScore   (double value, double location, double scale) const override;
    double threshold      () const override { return m_multiplier; }
    std::string methodName() const override { return "IQR"; }

private:

    double m_multiplier;
};

} // namespace loki::outlier
