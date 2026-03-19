#include <loki/homogeneity/homogenizer.hpp>
#include <loki/homogeneity/medianYearSeries.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <sstream>

using namespace loki;
using namespace loki::homogeneity;

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
    // Step 2 -- Pre-deseasonalization outlier removal (future)
    // ------------------------------------------------------------------
    if (m_cfg.preOutlier.enabled) {
        LOKI_WARNING("Homogenizer: preOutlier.enabled=true but loki_outlier is not yet "
                     "implemented. Skipping.");
    }

    // ------------------------------------------------------------------
    // Step 3 -- Deseasonalization
    // ------------------------------------------------------------------
    Deseasonalizer deseasonalizer{m_cfg.deseasonalizer};
    Deseasonalizer::Result deseasResult = [&]() {
        if (m_cfg.deseasonalizer.strategy == Deseasonalizer::Strategy::MEDIAN_YEAR) {
            MedianYearSeries::Config mysCfg;
            mysCfg.minYears = m_cfg.medianYearMinYears;
            MedianYearSeries mys{working, mysCfg};
            return deseasonalizer.deseasonalize(working, [&mys](const ::TimeStamp& ts) {
                return mys.valueAt(ts);
            });
        }
        return deseasonalizer.deseasonalize(working);
    }();

    const std::vector<double>& residuals = deseasResult.residuals;

    {
        std::ostringstream oss;
        oss << "Homogenizer: deseasonalization complete ("
            << residuals.size() << " residuals).";
        LOKI_INFO(oss.str());
    }

    // ------------------------------------------------------------------
    // Step 4 -- Post-deseasonalization outlier removal (future)
    // ------------------------------------------------------------------
    if (m_cfg.postOutlier.enabled) {
        LOKI_WARNING("Homogenizer: postOutlier.enabled=true but loki_outlier is not yet "
                     "implemented. Skipping.");
    }

    // ------------------------------------------------------------------
    // Step 5 -- Change point detection
    // ------------------------------------------------------------------
    std::vector<double> times;
    times.reserve(working.size());
    for (std::size_t i = 0; i < working.size(); ++i) {
        times.push_back(working[i].time.mjd());
    }

    MultiChangePointDetector detector{m_cfg.detector};
    std::vector<ChangePoint> changePoints = detector.detect(residuals, times);

    {
        std::ostringstream oss;
        oss << "Homogenizer: detected " << changePoints.size() << " change point(s).";
        LOKI_INFO(oss.str());
        for (const auto& cp : changePoints) {
            std::ostringstream cposs;
            cposs << "  index=" << cp.globalIndex
                  << "  mjd="   << cp.mjd
                  << "  shift=" << cp.shift
                  << "  p="     << cp.pValue;
            LOKI_INFO(cposs.str());
        }
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

    // ------------------------------------------------------------------
    // Assemble result
    // ------------------------------------------------------------------
    return Result{
        std::move(changePoints),
        std::move(adjusted),
        residuals
    };
}
