#include <loki/outlier/outlierCleaner.hpp>
#include <loki/core/exceptions.hpp>

#include <cmath>
#include <cstdint>
#include <limits>

using namespace loki;

namespace loki::outlier {

// -----------------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------------

OutlierCleaner::OutlierCleaner(Config cfg, const OutlierDetector& detector)
    : m_cfg(cfg)
    , m_detector(&detector)
{
    if (cfg.fillStrategy == GapFiller::Strategy::MEDIAN_YEAR) {
        throw ConfigException(
            "OutlierCleaner: MEDIAN_YEAR fill strategy is not supported. "
            "Use LINEAR, FORWARD_FILL, or MEAN.");
    }
    if (cfg.fillStrategy == GapFiller::Strategy::NONE) {
        throw ConfigException(
            "OutlierCleaner: NONE fill strategy is not permitted. "
            "Outliers must be replaced; use LINEAR, FORWARD_FILL, or MEAN.");
    }
}

// -----------------------------------------------------------------------------
// Public interface
// -----------------------------------------------------------------------------

OutlierCleaner::CleanResult OutlierCleaner::clean(const TimeSeries& series) const
{
    const std::vector<double> noSeasonal(series.size(), 0.0);
    return _runPipeline(series, noSeasonal);
}

OutlierCleaner::CleanResult OutlierCleaner::clean(
    const TimeSeries&          series,
    const std::vector<double>& seasonal) const
{
    if (seasonal.size() != series.size()) {
        throw DataException(
            "OutlierCleaner::clean: seasonal vector length " +
            std::to_string(seasonal.size()) +
            " does not match series length " +
            std::to_string(series.size()) + ".");
    }
    return _runPipeline(series, seasonal);
}

// -----------------------------------------------------------------------------
// Private helpers
// -----------------------------------------------------------------------------

OutlierCleaner::CleanResult OutlierCleaner::_runPipeline(
    const TimeSeries&          series,
    const std::vector<double>& seasonal) const
{
    // Step 1: build residual series (values - seasonal)
    TimeSeries residualSeries = _buildResidualSeries(series, seasonal);

    // Step 2: extract raw residual values for detector
    std::vector<double> residualValues;
    residualValues.reserve(series.size());
    for (std::size_t i = 0; i < series.size(); ++i) {
        residualValues.push_back(residualSeries[i].value);
    }

    // Step 3: detect outliers
    OutlierResult detection = m_detector->detect(residualValues);

    // Step 4: mark outlier positions as NaN in the residual TimeSeries
    TimeSeries markedResiduals = _markOutliers(residualSeries, detection);

    // Step 5: fill NaN positions via GapFiller
    GapFiller::Config gfCfg;
    gfCfg.strategy      = m_cfg.fillStrategy;
    gfCfg.maxFillLength = m_cfg.maxFillLength;

    GapFiller gapFiller(gfCfg);
    TimeSeries filledResiduals = gapFiller.fill(markedResiduals);

    // Step 6: record replacedValue and set flag = 2 on each OutlierPoint
    for (auto& pt : detection.points) {
        pt.replacedValue = filledResiduals[pt.index].value;
        pt.flag          = 2;
    }

    // Step 7: reconstruct cleaned series (filled residuals + seasonal)
    TimeSeries cleaned = _reconstruct(filledResiduals, seasonal, series.metadata());

    // Copy flag = 2 to cleaned series observations at outlier positions
    // (flag propagation: mark the cleaned series so downstream knows what changed)
    // Note: TimeSeries is immutable after construction via append, so we rebuild.
    SeriesMetadata cleanedMeta = series.metadata();
    cleanedMeta.componentName += "_cleaned";

    TimeSeries cleanedFlagged(cleanedMeta);
    cleanedFlagged.reserve(series.size());

    // Build a quick lookup: which indices are outliers?
    // Use a boolean mask for O(n) lookup
    std::vector<uint8_t> outlierFlags(series.size(), 0);
    for (const auto& pt : detection.points) {
        outlierFlags[pt.index] = 2;
    }

    for (std::size_t i = 0; i < series.size(); ++i) {
        const uint8_t flag = outlierFlags[i] > 0 ? 2 : series[i].flag;
        cleanedFlagged.append(series[i].time, cleaned[i].value, flag);
    }

    // Build residuals TimeSeries for output (before replacement, for auditing)
    CleanResult result;
    result.residuals = residualSeries;
    result.detection = std::move(detection);
    result.cleaned   = std::move(cleanedFlagged);

    return result;
}

TimeSeries OutlierCleaner::_buildResidualSeries(
    const TimeSeries&          series,
    const std::vector<double>& seasonal)
{
    SeriesMetadata meta = series.metadata();
    meta.componentName += "_residuals";

    TimeSeries residuals(meta);
    residuals.reserve(series.size());

    for (std::size_t i = 0; i < series.size(); ++i) {
        const double v = series[i].value - seasonal[i];
        residuals.append(series[i].time, v, series[i].flag);
    }
    return residuals;
}

TimeSeries OutlierCleaner::_markOutliers(
    const TimeSeries&    residualSeries,
    const OutlierResult& detection)
{
    // Build a boolean mask for O(n) marking
    std::vector<bool> isOutlier(residualSeries.size(), false);
    for (const auto& pt : detection.points) {
        isOutlier[pt.index] = true;
    }

    SeriesMetadata meta = residualSeries.metadata();
    TimeSeries marked(meta);
    marked.reserve(residualSeries.size());

    for (std::size_t i = 0; i < residualSeries.size(); ++i) {
        if (isOutlier[i]) {
            // NaN signals GapFiller to fill this position
            marked.append(residualSeries[i].time,
                          std::numeric_limits<double>::quiet_NaN(),
                          1);  // flag 1 = detected
        } else {
            marked.append(residualSeries[i].time,
                          residualSeries[i].value,
                          residualSeries[i].flag);
        }
    }
    return marked;
}

TimeSeries OutlierCleaner::_reconstruct(
    const TimeSeries&          filledResiduals,
    const std::vector<double>& seasonal,
    const SeriesMetadata&      originalMetadata)
{
    SeriesMetadata meta = originalMetadata;
    meta.componentName += "_cleaned";

    TimeSeries result(meta);
    result.reserve(filledResiduals.size());

    for (std::size_t i = 0; i < filledResiduals.size(); ++i) {
        const double v = filledResiduals[i].value + seasonal[i];
        result.append(filledResiduals[i].time, v, filledResiduals[i].flag);
    }
    return result;
}

} // namespace loki::outlier
