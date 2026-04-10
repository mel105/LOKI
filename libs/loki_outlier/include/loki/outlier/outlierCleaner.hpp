#pragma once

#include <loki/outlier/outlierDetector.hpp>
#include <loki/outlier/outlierResult.hpp>
#include <loki/timeseries/gapFiller.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <vector>

namespace loki::outlier {

/**
 * @brief Orchestrates the full outlier removal pipeline for a single TimeSeries.
 *
 * Pipeline steps:
 *   1. Subtract seasonal component (if provided by caller).
 *   2. Detect outliers in the residuals using the injected OutlierDetector.
 *   3. Set detected positions to NaN in the residual series.
 *   4. Fill the NaN positions via GapFiller.
 *   5. Add the seasonal component back to reconstruct the cleaned series.
 *
 * OutlierCleaner does NOT perform deseasonalization itself. The caller is
 * responsible for computing the seasonal component (e.g. via Deseasonalizer
 * from loki_homogeneity) and passing it as a std::vector<double>. This keeps
 * loki_outlier free of any dependency on loki_homogeneity.
 *
 * The detector is supplied by reference at construction time, enabling
 * polymorphic use of IqrDetector, MadDetector, ZScoreDetector, or any future
 * OutlierDetector subclass.
 *
 * GapFiller::Strategy::MEDIAN_YEAR is not supported here; use LINEAR,
 * FORWARD_FILL, MEAN, or SPLINE for outlier replacement.
 */
class OutlierCleaner {
public:

    // -------------------------------------------------------------------------
    // Config
    // -------------------------------------------------------------------------

    /**
     * @brief Configuration for OutlierCleaner.
     */
    struct Config {
        /// Gap-filling strategy used to replace outlier positions.
        /// MEDIAN_YEAR and NONE are not permitted; ConfigException is thrown.
        /// Supported: LINEAR, FORWARD_FILL, MEAN, SPLINE.
        GapFiller::Strategy fillStrategy{GapFiller::Strategy::LINEAR};

        /// Maximum number of consecutive outlier positions to replace.
        /// 0 = no limit. Positions exceeding this limit are left as NaN.
        std::size_t maxFillLength{0};
    };

    // -------------------------------------------------------------------------
    // Result
    // -------------------------------------------------------------------------

    /**
     * @brief Result of one OutlierCleaner::clean() call.
     */
    struct CleanResult {
        /// Reconstructed series: cleaned residuals + seasonal component.
        /// Timestamps and metadata are copied from the input series;
        /// metadata name receives the suffix "_cleaned".
        TimeSeries cleaned;

        /// Deseasonalized series before outlier replacement.
        /// Useful for plotting and auditing detection decisions.
        /// Name suffix: "_residuals".
        TimeSeries residuals;

        /// Full detection report from the OutlierDetector.
        /// OutlierPoint::replacedValue is filled in by OutlierCleaner
        /// after GapFiller runs. OutlierPoint::flag is set to 2.
        OutlierResult detection;
    };

    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    /**
     * @brief Constructs an OutlierCleaner.
     *
     * @param cfg      Configuration.
     * @param detector Outlier detector to use. Must outlive this object.
     * @throws ConfigException    if cfg.fillStrategy is MEDIAN_YEAR or NONE.
     *                            SPLINE is supported for outlier replacement.
     */
    OutlierCleaner(Config cfg, const OutlierDetector& detector);

    // -------------------------------------------------------------------------
    // Interface
    // -------------------------------------------------------------------------

    /**
     * @brief Runs the outlier removal pipeline without seasonal correction.
     *
     * Use this overload when no seasonal component is present (e.g. GNSS
     * series, or when the caller has chosen Strategy::NONE for deseasonalization).
     * The detector operates directly on the series values.
     *
     * @param series Input series. Must be sorted and NaN-free.
     * @return CleanResult. If no outliers are detected, cleaned == series (copy).
     * @throws SeriesTooShortException if series is too short for the detector.
     * @throws MissingValueException   if series contains NaN values.
     * @throws AlgorithmException      if the series is not sorted.
     */
    [[nodiscard]]
    CleanResult clean(const TimeSeries& series) const;

    /**
     * @brief Runs the outlier removal pipeline with a precomputed seasonal component.
     *
     * The caller must provide a seasonal vector of the same length as the series.
     * Residuals are computed as series[i].value - seasonal[i]. After replacement,
     * the seasonal component is added back to reconstruct the cleaned series.
     *
     * Typical usage:
     * @code
     *   Deseasonalizer deseas(deseasonCfg);
     *   auto dr = deseas.deseasonalize(series, profileLookup);
     *   OutlierCleaner cleaner(cleanerCfg, detector);
     *   auto result = cleaner.clean(series, dr.seasonal);
     * @endcode
     *
     * @param series   Input series. Must be sorted and NaN-free.
     * @param seasonal Seasonal component vector. Must have the same length as series.
     * @return CleanResult with reconstructed cleaned series.
     * @throws DataException           if seasonal.size() != series.size().
     * @throws SeriesTooShortException if series is too short for the detector.
     * @throws MissingValueException   if series contains NaN values.
     * @throws AlgorithmException      if the series is not sorted.
     */
    [[nodiscard]]
    CleanResult clean(const TimeSeries& series,
                      const std::vector<double>& seasonal) const;

private:

    Config                 m_cfg;
    const OutlierDetector* m_detector; // non-owning

    // -------------------------------------------------------------------------
    // Internal helpers
    // -------------------------------------------------------------------------

    /**
     * @brief Core pipeline shared by both clean() overloads.
     *
     * @param series   Input series (sorted, NaN-free).
     * @param seasonal Seasonal component (same length as series; all zeros if NONE).
     * @return Populated CleanResult.
     */
    [[nodiscard]]
    CleanResult _runPipeline(const TimeSeries&          series,
                             const std::vector<double>& seasonal) const;

    /**
     * @brief Builds a TimeSeries of residuals from series values and seasonal vector.
     *
     * The returned series has the same timestamps and metadata as @p series,
     * with name suffix "_residuals". Values = series[i].value - seasonal[i].
     */
    [[nodiscard]]
    static TimeSeries _buildResidualSeries(const TimeSeries&          series,
                                           const std::vector<double>& seasonal);

    /**
     * @brief Marks outlier positions as NaN in @p residualSeries.
     *
     * Returns a new TimeSeries with outlier values replaced by NaN.
     * Timestamps and flags are preserved; outlier positions receive flag = 1.
     */
    [[nodiscard]]
    static TimeSeries _markOutliers(const TimeSeries&  residualSeries,
                                    const OutlierResult& detection);

    /**
     * @brief Reconstructs the cleaned series from filled residuals + seasonal.
     *
     * Returns a new TimeSeries where value[i] = filledResiduals[i].value + seasonal[i].
     * Name suffix: "_cleaned".
     */
    [[nodiscard]]
    static TimeSeries _reconstruct(const TimeSeries&          filledResiduals,
                                   const std::vector<double>& seasonal,
                                   const SeriesMetadata&      originalMetadata);
};

} // namespace loki::outlier
