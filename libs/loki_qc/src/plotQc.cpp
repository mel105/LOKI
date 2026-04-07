#include <loki/qc/plotQc.hpp>

#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/io/gnuplot.hpp>
#include <loki/io/plot.hpp>
#include <loki/qc/qcFlags.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>
#include <string>

using namespace loki;
using namespace loki::qc;

// =============================================================================
//  Construction
// =============================================================================

PlotQc::PlotQc(const AppConfig& cfg)
    : m_cfg(cfg)
{}

// =============================================================================
//  plotAll
// =============================================================================

void PlotQc::plotAll(const TimeSeries& series, const QcResult& result) const
{
    if (m_cfg.plots.qcCoverage) {
        try { plotCoverage(series, result); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotQc: coverage plot failed: " + std::string(ex.what()));
        }
    }
    if (m_cfg.plots.qcHistogram) {
        try { plotHistogram(series, result); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotQc: histogram plot failed: " + std::string(ex.what()));
        }
    }
    if (m_cfg.plots.qcAcf) {
        try { plotAcf(series, result); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotQc: ACF plot failed: " + std::string(ex.what()));
        }
    }
}

// =============================================================================
//  plotCoverage
// =============================================================================

void PlotQc::plotCoverage(const TimeSeries& series, const QcResult& result) const
{
    const std::size_t n = series.size();
    if (n == 0) return;

    const std::string stem = _stem(result, "coverage");
    const auto outPath     = _outPath(stem);

    // Decide per-epoch or per-day aggregation.
    const bool perEpoch = (n < 1000);

    // For per-day: aggregate worst flag per calendar day.
    // Key: MJD floor (integer day), value: worst flag byte.
    // Priority: GAP(1) > OUTLIER_*(2,4,8) > VALID(0)
    // We define "worst" as: if any GAP -> red; elif any outlier -> orange; else green.

    auto worstFlag = [](uint8_t current, uint8_t incoming) -> uint8_t {
        if ((current & FLAG_GAP) || (incoming & FLAG_GAP)) return current | FLAG_GAP;
        return current | incoming;
    };

    // Build data: (mjd, colour_index) where 0=green, 1=orange, 2=red
    // colour_index used as gnuplot palette index.
    struct Bar {
        double mjd;
        int    colour; // 0=green, 1=orange, 2=red
    };
    std::vector<Bar> bars;

    if (perEpoch) {
        bars.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            const uint8_t f = result.flags[i];
            int col = 0;
            if (f & FLAG_GAP)         col = 2;
            else if (f & (FLAG_OUTLIER_IQR | FLAG_OUTLIER_MAD | FLAG_OUTLIER_ZSC)) col = 1;
            bars.push_back({series[i].time.mjd(), col});
        }
    } else {
        // Aggregate per calendar day.
        std::map<int, uint8_t> dayFlag; // key = floor(mjd)
        for (std::size_t i = 0; i < n; ++i) {
            const int day = static_cast<int>(std::floor(series[i].time.mjd()));
            auto it = dayFlag.find(day);
            if (it == dayFlag.end()) {
                dayFlag[day] = result.flags[i];
            } else {
                it->second = worstFlag(it->second, result.flags[i]);
            }
        }
        bars.reserve(dayFlag.size());
        for (const auto& [day, f] : dayFlag) {
            int col = 0;
            if (f & FLAG_GAP)         col = 2;
            else if (f & (FLAG_OUTLIER_IQR | FLAG_OUTLIER_MAD | FLAG_OUTLIER_ZSC)) col = 1;
            bars.push_back({static_cast<double>(day) + 0.5, col}); // centre of day
        }
    }

    // Send inline to gnuplot.
    Gnuplot gp;
    gp(_terminal(1400, 300));
    gp("set output '" + _fwdSlash(outPath) + "'");
    gp("set title 'Coverage: " + result.datasetName + " / " + result.componentName + "' noenhanced");
    gp("set xlabel 'MJD'");
    gp("set ylabel ''");
    gp("set yrange [0:1]");
    gp("set ytics ('' 0)");
    gp("set xrange [" + std::to_string(result.startMjd - 1.0)
       + ":" + std::to_string(result.endMjd + 1.0) + "]");
    gp("set palette defined (0 'green', 1 'orange', 2 'red')");
    gp("unset colorbox");
    gp("set cbrange [0:2]");

    // Bar width: 1 day for per-day, medianStep for per-epoch.
    const double barWidth = perEpoch
        ? std::max(result.medianStepSeconds / 86400.0, 0.001)
        : 1.0;

    gp("set boxwidth " + std::to_string(barWidth) + " absolute");
    gp("set style fill solid 1.0 noborder");

    // Build inline data string.
    std::ostringstream data;
    for (const auto& b : bars) {
        data << b.mjd << " 0.5 " << b.colour << "\n";
    }
    data << "e\n";

    gp("plot '-' using 1:2:3 with boxes palette notitle\n" + data.str());

    LOKI_INFO("PlotQc: coverage plot -> " + outPath.string());
}

// =============================================================================
//  plotHistogram  (delegate to loki::Plot)
// =============================================================================

void PlotQc::plotHistogram(const TimeSeries& series, const QcResult& result) const
{
    // Build a TimeSeries with only valid values so loki::Plot::histogram works cleanly.
    // We pass the original series directly -- Plot::histogram handles NaN via validValues.
    // We need a custom stem, so we generate the file via Plot with a pre-set output path.
    // The simplest approach: use Plot::histogram which writes to imgDir with its own stem.
    // To match our naming convention we rename post-hoc -- but that is fragile.
    // Better: build a minimal TimeSeries copy with the correct componentName.

    // Clone metadata with desired plot stem as componentName for filename generation.
    // loki::Plot uses componentName in the filename. We set it to match our convention.
    const std::string stem = _stem(result, "histogram");

    // We cannot control Plot's output filename directly, so we build a temporary
    // TimeSeries whose componentName encodes the desired stem.
    // Plot will write to: imgDir / "histogram_" + stationId + "_" + componentName + ".fmt"
    // We want:            imgDir / stem + ".fmt"
    // Workaround: pass the series as-is and accept Plot's naming scheme,
    // OR: use the vector overload which takes an explicit title.

    // Use vector overload with title set to stem -- Plot uses title as filename stem.
    std::vector<double> vals;
    vals.reserve(series.size());
    for (std::size_t i = 0; i < series.size(); ++i) {
        if (!std::isnan(series[i].value)) {
            vals.push_back(series[i].value);
        }
    }
    if (vals.empty()) return;

    const int bins = (m_cfg.plots.options.histogramBins > 0)
                     ? m_cfg.plots.options.histogramBins : 30;

    Plot corePlot(m_cfg);
    corePlot.histogram(vals, bins, stem);

    LOKI_INFO("PlotQc: histogram plot -> " + stem + "." + m_cfg.plots.outputFormat);
}

// =============================================================================
//  plotAcf  (delegate to loki::Plot)
// =============================================================================

void PlotQc::plotAcf(const TimeSeries& series, const QcResult& result) const
{
    std::vector<double> vals;
    vals.reserve(series.size());
    for (std::size_t i = 0; i < series.size(); ++i) {
        if (!std::isnan(series[i].value)) {
            vals.push_back(series[i].value);
        }
    }
    if (vals.size() < 10) return;

    const int maxLag = (m_cfg.plots.options.acfMaxLag > 0)
                       ? m_cfg.plots.options.acfMaxLag : 40;

    const std::string stem = _stem(result, "acf");

    Plot corePlot(m_cfg);
    corePlot.acf(vals, maxLag, stem);

    LOKI_INFO("PlotQc: ACF plot -> " + stem + "." + m_cfg.plots.outputFormat);
}

// =============================================================================
//  Helpers
// =============================================================================

std::string PlotQc::_stem(const QcResult& result, const std::string& plotType) const
{
    return "qc_" + result.datasetName + "_" + result.componentName + "_" + plotType;
}

std::filesystem::path PlotQc::_outPath(const std::string& stem) const
{
    return m_cfg.imgDir / (stem + "." + m_cfg.plots.outputFormat);
}

std::string PlotQc::_terminal(int widthPx, int heightPx) const
{
    return "set terminal pngcairo noenhanced font 'Sans,12' size "
           + std::to_string(widthPx) + "," + std::to_string(heightPx);
}

std::string PlotQc::_fwdSlash(const std::filesystem::path& p)
{
    std::string s = p.string();
    std::replace(s.begin(), s.end(), '\\', '/');
    return s;
}
