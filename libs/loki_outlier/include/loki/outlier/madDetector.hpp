#pragma once

#include <loki/outlier/outlierDetector.hpp>

#include <string>
#include <vector>

namespace loki::outlier {

/**
 * @brief Detects outliers using the Median Absolute Deviation (MAD) method.
 *
 * A point x_i is flagged as an outlier when:
 *   |x_i - median(x)| / (MAD(x) / 0.6745) > multiplier
 *
 * Dividing MAD by 0.6745 makes the scale estimate consistent with the
 * standard deviation under normality, so the default multiplier of 3.0
 * corresponds to approximately 3-sigma rejection.
 *
 * MAD is more robust than IQR for heavy-tailed distributions and series
 * with clustered outliers. Requires at least 4 observations.
 *
 * Location estimate : median
 * Scale estimate    : MAD / 0.6745 (normalised MAD)
 */
class MadDetector final : public OutlierDetector {
public:

    /**
     * @brief Constructs a MadDetector with the given sigma multiplier.
     * @param multiplier Normalised-MAD multiplier (default 3.0).
     * @throws AlgorithmException if multiplier <= 0.
     */
    explicit MadDetector(double multiplier = 3.0);

    /// @brief Returns the configured multiplier.
    double multiplier() const { return m_multiplier; }

protected:

    std::size_t minSeriesLength() const override { return 4; }

    double computeLocation(const std::vector<double>& x) const override;
    double computeScale   (const std::vector<double>& x) const override;
    double computeScore   (double value, double location, double scale) const override;
    double threshold      () const override { return m_multiplier; }
    std::string methodName() const override { return "MAD"; }

private:

    double m_multiplier;

    // Consistency factor: MAD / CONSISTENCY_FACTOR -> std dev equivalent under normality
    static constexpr double CONSISTENCY_FACTOR = 0.6745;
};

} // namespace loki::outlier
