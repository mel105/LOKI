#include <loki/homogeneity/homogenizer.hpp>
#include <loki/timeseries/medianYearSeries.hpp>
#include <loki/outlier/outlierCleaner.hpp>
#include <loki/outlier/iqrDetector.hpp>
#include <loki/outlier/madDetector.hpp>
#include <loki/outlier/zScoreDetector.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <memory>
#include <sstream>

using namespace loki;
using namespace loki::homogeneity;

// ----------------------------------------------------------------------------
//  Internal helpers
// ----------------------------------------------------------------------------

namespace {

std::unique_ptr<loki::outlier::OutlierDetector>
buildDetector(const OutlierConfig& cfg)
{
    const std::string& m = cfg.method;

    if (m == "iqr")
        return std::make_unique<loki::outlier::IqrDetector>(cfg.iqrMultiplier);
    if (m == "mad" || m == "mad_bounds")
        return std::make_unique<loki::outlier::MadDetector>(cfg.madMultiplier);
    if (m == "zscore")
        return std::make_unique<loki::outlier::ZScoreDetector>(cfg.zscoreThreshold);

    LOKI_WARNING("Homogenizer: unknown outlier method '" + m + "' -- falling back to 'mad'.");
    return std::make_unique<loki::outlier::MadDetector>(cfg.madMultiplier);
}

loki::outlier::OutlierCleaner::Config
buildCleanerConfig(const OutlierConfig& cfg)
{
    loki::outlier::OutlierCleaner::Config c;

    const std::string& rs = cfg.replacementStrategy;
    if      (rs == "forward_fill") { c.fillStrategy = GapFiller::Strategy::FORWARD_FILL; }
    else if (rs == "mean")         { c.fillStrategy = GapFiller::Strategy::MEAN;         }
    else                           { c.fillStrategy = GapFiller::Strategy::LINEAR;        }

    c.maxFillLength = static_cast<std::size_t>(std::max(0, cfg.maxFillLength));
    return c;
}

} // anonymous namespace

// ----------------------------------------------------------------------------
//  Homogenizer
// ----------------------------------------------------------------------------

Homogenizer::Homogenizer(Config cfg)
    : m_cfg{std::move(cfg)}
{}

// ----------------------------------------------------------------------------

Homogenizer::Result Homogenizer::process(const TimeSeries& input) const
{
    if (input.size() == 0) {
        throw DataException("Homogenizer::process: input series is empty.");
    }

    HomogenizerResult result;

    // ------------------------------------------------------------------
    // Step 1 -- Gap filling
    // ------------------------------------------------------------------
    TimeSeries working = input;

    if (m_cfg.applyGapFilling) {
        LOKI_INFO("Homogenizer: running GapFiller (strategy="
                  + std::to_string(static_cast<int>(m_cfg.gapFiller.strategy)) + ")");
        GapFiller filler{m_cfg.gapFiller};

        if (m_cfg.gapFiller.strategy == GapFiller::Strategy::MEDIAN_YEAR) {
            MedianYearSeries::Config mysCfg;
            mysCfg.minYears = m_cfg.medianYearMinYears;
            MedianYearSeries mys{working, mysCfg};
            working = filler.fill(working, [&mys](const ::TimeStamp& ts) {
                return mys.valueAt(ts);
            });
        } else {
            working = filler.fill(working);
        }
    }

    // ------------------------------------------------------------------
    // Step 2 -- Pre-deseasonalization outlier removal
    // ------------------------------------------------------------------
    if (m_cfg.preOutlier.enabled) {
        LOKI_INFO("Homogenizer: pre-outlier removal (method="
                  + m_cfg.preOutlier.method + ")");

        auto detector   = buildDetector(m_cfg.preOutlier);
        auto cleanerCfg = buildCleanerConfig(m_cfg.preOutlier);
        loki::outlier::OutlierCleaner cleaner(cleanerCfg, *detector);

        auto cleanResult = cleaner.clean(working);

        result.preOutlierDetection = cleanResult.detection;
        result.preOutlierCleaned   = cleanResult.cleaned;
        working                    = cleanResult.cleaned;

        LOKI_INFO("Homogenizer: pre-outlier removed "
                  + std::to_string(cleanResult.detection.nOutliers) + " point(s).");
    } else {
        result.preOutlierCleaned = working;
    }

    // ------------------------------------------------------------------
    // Step 3 -- Deseasonalization
    // ------------------------------------------------------------------
    loki::Deseasonalizer deseasonalizer{m_cfg.deseasonalizer};
    loki::Deseasonalizer::Result deseasResult = [&]() {
        if (m_cfg.deseasonalizer.strategy == loki::Deseasonalizer::Strategy::MEDIAN_YEAR) {
            MedianYearSeries::Config mysCfg;
            mysCfg.minYears = m_cfg.medianYearMinYears;
            MedianYearSeries mys{working, mysCfg};
            return deseasonalizer.deseasonalize(working, [&mys](const ::TimeStamp& ts) {
                return mys.valueAt(ts);
            });
        }
        return deseasonalizer.deseasonalize(working);
    }();

    LOKI_INFO("Homogenizer: deseasonalization complete ("
              + std::to_string(deseasResult.residuals.size()) + " residuals).");

    // ------------------------------------------------------------------
    // Step 4 -- Post-deseasonalization outlier removal
    // ------------------------------------------------------------------
    std::vector<double> detectionResiduals = deseasResult.residuals;

    if (m_cfg.postOutlier.enabled) {
        LOKI_INFO("Homogenizer: post-outlier removal (method="
                  + m_cfg.postOutlier.method + ")");

        auto detector   = buildDetector(m_cfg.postOutlier);
        auto cleanerCfg = buildCleanerConfig(m_cfg.postOutlier);
        loki::outlier::OutlierCleaner cleaner(cleanerCfg, *detector);

        auto cleanResult = cleaner.clean(deseasResult.series, deseasResult.seasonal);

        result.postOutlierDetection = cleanResult.detection;

        detectionResiduals.clear();
        detectionResiduals.reserve(cleanResult.residuals.size());
        for (std::size_t i = 0; i < cleanResult.residuals.size(); ++i) {
            detectionResiduals.push_back(cleanResult.residuals[i].value);
        }

        LOKI_INFO("Homogenizer: post-outlier removed "
                  + std::to_string(cleanResult.detection.nOutliers) + " point(s).");
    }

    result.deseasonalizedValues = detectionResiduals;

    // ------------------------------------------------------------------
    // Step 5 -- Change point detection
    // ------------------------------------------------------------------
    std::vector<double> times;
    times.reserve(working.size());
    for (std::size_t i = 0; i < working.size(); ++i) {
        times.push_back(working[i].time.mjd());
    }

    MultiChangePointDetector detector{m_cfg.detector};
    std::vector<ChangePoint> changePoints = detector.detect(detectionResiduals, times);

    LOKI_INFO("Homogenizer: detected " + std::to_string(changePoints.size()) + " change point(s).");
    for (const auto& cp : changePoints) {
        std::ostringstream oss;
        oss << "  index=" << cp.globalIndex
            << "  mjd="   << cp.mjd
            << "  shift=" << cp.shift
            << "  p="     << cp.pValue;
        LOKI_INFO(oss.str());
    }

    // ------------------------------------------------------------------
    // Step 6 -- Series adjustment
    // ------------------------------------------------------------------
    TimeSeries adjusted = working;

    if (m_cfg.applyAdjustment && !changePoints.empty()) {
        SeriesAdjuster adjuster{};
        adjusted = adjuster.adjust(working, changePoints);
        LOKI_INFO("Homogenizer: series adjustment applied.");
    } else if (changePoints.empty()) {
        LOKI_INFO("Homogenizer: no change points -- series unchanged.");
    } else {
        LOKI_INFO("Homogenizer: applyAdjustment=false -- series unchanged.");
    }

    result.changePoints   = std::move(changePoints);
    result.adjustedSeries = std::move(adjusted);

    return result;
}