#include <loki/filter/filterAnalyzer.hpp>

#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/filter/emaFilter.hpp>
#include <loki/filter/filterWindowAdvisor.hpp>
#include <loki/filter/kernelSmoother.hpp>
#include <loki/filter/loessFilter.hpp>
#include <loki/filter/movingAverageFilter.hpp>
#include <loki/filter/plotFilter.hpp>
#include <loki/filter/savitzkyGolayFilter.hpp>
#include <loki/filter/splineFilter.hpp>
#include <loki/filter/weightedMovingAverageFilter.hpp>
#include <loki/stats/descriptive.hpp>
#include <loki/stats/hypothesis.hpp>
#include <loki/timeseries/gapFiller.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

using namespace loki;

// ---------------------------------------------------------------------------
//  Construction
// ---------------------------------------------------------------------------

FilterAnalyzer::FilterAnalyzer(const AppConfig& cfg)
    : m_cfg{cfg}
{}

// ---------------------------------------------------------------------------
//  Internal helpers
// ---------------------------------------------------------------------------

namespace {

/// Builds a GapFiller::Config from FilterConfig.
GapFiller::Config buildGapFillerConfig(const FilterConfig& fcfg)
{
    GapFiller::Config gfc{};
    const std::string& s = fcfg.gapFilling.strategy;
    if      (s == "linear")       gfc.strategy = GapFiller::Strategy::LINEAR;
    else if (s == "forward_fill") gfc.strategy = GapFiller::Strategy::FORWARD_FILL;
    else if (s == "mean")         gfc.strategy = GapFiller::Strategy::MEAN;
    else if (s == "spline")       gfc.strategy = GapFiller::Strategy::SPLINE;
    else                          gfc.strategy = GapFiller::Strategy::NONE;
    gfc.maxFillLength = static_cast<std::size_t>(
        std::max(0, fcfg.gapFilling.maxFillLength));
    return gfc;
}

/// Builds the selected filter from FilterConfig.
std::unique_ptr<Filter> buildFilter(const FilterConfig& fcfg,
                                    const TimeSeries&   series)
{
    auto makeAdvisorConfig = [&]() -> FilterWindowAdvisor::Config {
        FilterWindowAdvisor::Config ac{};
        const std::string& m = fcfg.autoWindowMethod;
        if      (m == "silverman") ac.method = FilterWindowAdvisor::Method::SILVERMAN;
        else if (m == "acf_peak")  ac.method = FilterWindowAdvisor::Method::ACF_PEAK;
        else                       ac.method = FilterWindowAdvisor::Method::SILVERMAN_MAD;
        return ac;
    };

    switch (fcfg.method) {

        case FilterMethodEnum::MOVING_AVERAGE: {
            int window = fcfg.movingAverage.window;
            if (window == 0) {
                const auto advice = FilterWindowAdvisor::advise(series, makeAdvisorConfig());
                window = advice.windowSamples;
                LOKI_INFO("FilterWindowAdvisor (moving_average): window="
                          + std::to_string(window) + "  [" + advice.rationale + "]");
            }
            MovingAverageFilter::Config c{};
            c.window = window;
            return std::make_unique<MovingAverageFilter>(c);
        }

        case FilterMethodEnum::EMA: {
            EmaFilter::Config c{};
            c.alpha = fcfg.ema.alpha;
            return std::make_unique<EmaFilter>(c);
        }

        case FilterMethodEnum::WEIGHTED_MA: {
            WeightedMovingAverageFilter::Config c{};
            c.weights = fcfg.weightedMa.weights;
            return std::make_unique<WeightedMovingAverageFilter>(c);
        }

        case FilterMethodEnum::KERNEL: {
            double bw = fcfg.kernel.bandwidth;
            if (bw == 0.0) {
                const auto advice = FilterWindowAdvisor::advise(series, makeAdvisorConfig());
                bw = advice.bandwidth;
                LOKI_INFO("FilterWindowAdvisor (kernel): bandwidth="
                          + std::to_string(bw) + "  [" + advice.rationale + "]");
            }
            KernelSmoother::Config c{};
            c.bandwidth      = bw;
            c.gaussianCutoff = fcfg.kernel.gaussianCutoff;
            const std::string& kt = fcfg.kernel.kernelType;
            if      (kt == "gaussian")   c.kernel = KernelSmoother::Kernel::GAUSSIAN;
            else if (kt == "uniform")    c.kernel = KernelSmoother::Kernel::UNIFORM;
            else if (kt == "triangular") c.kernel = KernelSmoother::Kernel::TRIANGULAR;
            else                         c.kernel = KernelSmoother::Kernel::EPANECHNIKOV;
            return std::make_unique<KernelSmoother>(c);
        }

        case FilterMethodEnum::LOESS: {
            double bw = fcfg.loess.bandwidth;
            if (bw == 0.0) {
                const auto advice = FilterWindowAdvisor::advise(series, makeAdvisorConfig());
                bw = advice.bandwidth;
                LOKI_INFO("FilterWindowAdvisor (loess): bandwidth="
                          + std::to_string(bw) + "  [" + advice.rationale + "]");
            }
            LoessFilter::Config c{};
            c.bandwidth        = bw;
            c.degree           = fcfg.loess.degree;
            c.robust           = fcfg.loess.robust;
            c.robustIterations = fcfg.loess.robustIterations;
            const std::string& kt = fcfg.loess.kernelType;
            if      (kt == "epanechnikov") c.kernel = LoessFilter::Kernel::EPANECHNIKOV;
            else if (kt == "gaussian")     c.kernel = LoessFilter::Kernel::GAUSSIAN;
            else                           c.kernel = LoessFilter::Kernel::TRICUBE;
            return std::make_unique<LoessFilter>(c);
        }

        case FilterMethodEnum::SAVITZKY_GOLAY: {
            int window = fcfg.savitzkyGolay.window;
            if (window == 0) {
                const auto advice = FilterWindowAdvisor::advise(series, makeAdvisorConfig());
                window = advice.windowSamples;
                if (window % 2 == 0) window += 1;
                LOKI_INFO("FilterWindowAdvisor (savitzky_golay): window="
                          + std::to_string(window) + "  [" + advice.rationale + "]");
            }
            SavitzkyGolayFilter::Config c{};
            c.window = window;
            c.degree = fcfg.savitzkyGolay.degree;
            return std::make_unique<SavitzkyGolayFilter>(c);
        }

        case FilterMethodEnum::SPLINE: {
            SplineFilter::Config c{};
            c.subsampleStep = fcfg.spline.subsampleStep;
            c.bc            = fcfg.spline.bc;
            return std::make_unique<SplineFilter>(c);
        }
    }

    throw AlgorithmException("FilterAnalyzer: unhandled FilterMethodEnum value.");
}

/// Computes ACF at lag 1 from a residual vector.
double acfLag1(const std::vector<double>& v)
{
    if (v.size() < 2) return 0.0;
    const double n    = static_cast<double>(v.size());
    const double mean = std::accumulate(v.begin(), v.end(), 0.0) / n;

    double var = 0.0, cov = 0.0;
    for (std::size_t i = 0; i < v.size(); ++i) {
        var += (v[i] - mean) * (v[i] - mean);
    }
    for (std::size_t i = 1; i < v.size(); ++i) {
        cov += (v[i] - mean) * (v[i - 1] - mean);
    }
    if (var < 1e-15) return 0.0;
    return cov / var;
}

/// Formats a pass/fail diagnostic line.
std::string diagLine(const std::string& label, const std::string& value,
                     bool pass, const std::string& failMsg)
{
    std::string line = " " + label + value;
    line += pass ? "   [OK]" : ("   [FAIL -- " + failMsg + "]");
    return line;
}

/// Generates method-specific tuning hints based on diagnostics.
std::string tuningHints(const FilterConfig& fcfg, double acf1, double dw)
{
    const bool highAcf = acf1 > 0.5;
    const bool lowAcf  = acf1 < 0.1;
    const bool goodDw  = (dw > 1.5 && dw < 2.5);

    if (lowAcf && goodDw) {
        return " ACF lag-1 < 0.1 and D-W in (1.5, 2.5) -- good fit.\n"
               " No tuning required.\n";
    }

    std::string hints;

    switch (fcfg.method) {
        case FilterMethodEnum::SPLINE:
            if (highAcf) {
                hints += " High ACF in residuals -- subsample_step too small.\n";
                hints += " Current: subsample_step=" + std::to_string(fcfg.spline.subsampleStep)
                      + ". Try 20-100 for smoother output.\n";
            }
            break;

        case FilterMethodEnum::MOVING_AVERAGE:
            if (highAcf) {
                hints += " High ACF in residuals -- window too small.\n";
                hints += " Current: window=" + std::to_string(fcfg.movingAverage.window)
                      + ". Try a larger window.\n";
            }
            break;

        case FilterMethodEnum::EMA:
            if (highAcf) {
                hints += " High ACF in residuals -- alpha too large.\n";
                hints += " Current: alpha=" + std::to_string(fcfg.ema.alpha)
                      + ". Try alpha < 0.05 for stronger smoothing.\n";
            }
            break;

        case FilterMethodEnum::KERNEL:
            if (highAcf) {
                hints += " High ACF in residuals -- bandwidth too small.\n";
                hints += " Current: bandwidth=" + std::to_string(fcfg.kernel.bandwidth)
                      + ". Try a larger value.\n";
            }
            break;

        case FilterMethodEnum::LOESS:
            if (highAcf) {
                hints += " High ACF in residuals -- bandwidth too small.\n";
                hints += " Current: bandwidth=" + std::to_string(fcfg.loess.bandwidth)
                      + ". Try a larger value.\n";
            }
            break;

        case FilterMethodEnum::SAVITZKY_GOLAY:
            if (highAcf) {
                hints += " High ACF in residuals -- window too small.\n";
                hints += " Current: window=" + std::to_string(fcfg.savitzkyGolay.window)
                      + ". Try a larger window.\n";
            }
            break;

        default:
            break;
    }

    if (dw < 1.5) {
        hints += " D-W < 1.5 -- positive autocorrelation remaining in residuals.\n";
    } else if (dw > 2.5) {
        hints += " D-W > 2.5 -- negative autocorrelation remaining in residuals.\n";
    }

    if (hints.empty()) {
        hints = " No specific tuning hints.\n";
    }

    return hints;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
//  run
// ---------------------------------------------------------------------------

void FilterAnalyzer::run(const TimeSeries& ts, const std::string& datasetName) const
{
    const std::string compName = ts.metadata().componentName;
    LOKI_INFO("FilterAnalyzer: processing component '" + compName + "'");

    // Step 1: gap filling.
    const GapFiller::Config gfCfg = buildGapFillerConfig(m_cfg.filter);
    const GapFiller gf{gfCfg};
    const TimeSeries filled = gf.fill(ts);

    LOKI_INFO("FilterAnalyzer: gap filling done  n=" + std::to_string(filled.size())
              + "  strategy=" + m_cfg.filter.gapFilling.strategy);

    if (filled.empty()) {
        throw DataException(
            "FilterAnalyzer: series '" + compName + "' is empty after gap filling.");
    }

    // Step 2: build and apply filter.
    const auto filterPtr = buildFilter(m_cfg.filter, filled);
    LOKI_INFO("FilterAnalyzer: applying filter '" + filterPtr->name() + "'");

    const FilterResult result = filterPtr->apply(filled);

    LOKI_INFO("FilterAnalyzer: filter applied  n=" + std::to_string(result.filtered.size()));

    // Step 3: protocol + CSV.
    std::filesystem::create_directories(m_cfg.protocolsDir);
    std::filesystem::create_directories(m_cfg.csvDir);

    _writeProtocol(filled, result, datasetName, compName);
    _writeCsv(filled, result, datasetName, compName);

    // Step 4: plots.
    loki::filter::PlotFilter plotter{m_cfg};
    plotter.plotAll(ts, filled, result);
}

// ---------------------------------------------------------------------------
//  _writeProtocol
// ---------------------------------------------------------------------------

void FilterAnalyzer::_writeProtocol(const TimeSeries&   ts,
                                     const FilterResult& result,
                                     const std::string&  datasetName,
                                     const std::string&  compName) const
{
    const std::string fname = "filter_" + datasetName + "_" + compName + ".txt";
    const std::filesystem::path outPath = m_cfg.protocolsDir / fname;

    std::ofstream ofs(outPath);
    if (!ofs.is_open()) {
        throw IoException("FilterAnalyzer: cannot write protocol to '"
                          + outPath.string() + "'.");
    }

    const auto& fcfg = m_cfg.filter;
    const int   n    = static_cast<int>(ts.size());

    // Collect residuals (skip NaN).
    std::vector<double> residuals;
    residuals.reserve(static_cast<std::size_t>(n));
    for (std::size_t i = 0; i < result.residuals.size(); ++i) {
        const double v = result.residuals[i].value;
        if (v == v) residuals.push_back(v);  // NaN check
    }

    // Residual statistics via loki::stats::summarize.
    const auto summary = loki::stats::summarize(
        residuals, NanPolicy::SKIP, false);

    // Residual diagnostics.
    const auto   jb   = loki::stats::jarqueBera(residuals, 0.05, NanPolicy::SKIP);
    const double dw   = loki::stats::durbinWatson(residuals, NanPolicy::SKIP);
    const double acf1 = acfLag1(residuals);

    // Header.
    ofs << std::fixed << std::setprecision(4);
    ofs << "============================================================\n";
    ofs << " FILTER PROTOCOL -- " << result.filterName << "\n";
    ofs << "============================================================\n";
    ofs << " Dataset:      " << datasetName << "\n";
    ofs << " Series:       " << compName    << "\n";
    ofs << " Observations: " << n           << "\n";
    ofs << " Filter:       " << result.filterName << "\n";

    // Method-specific parameter lines.
    switch (fcfg.method) {
        case FilterMethodEnum::MOVING_AVERAGE:
            ofs << " Window:       " << result.effectiveWindow << " samples\n";
            break;
        case FilterMethodEnum::EMA:
            ofs << " Alpha:        " << fcfg.ema.alpha << "\n";
            break;
        case FilterMethodEnum::WEIGHTED_MA:
            ofs << " Window:       " << result.effectiveWindow << " samples\n";
            break;
        case FilterMethodEnum::KERNEL:
            ofs << " Bandwidth:    " << fcfg.kernel.bandwidth  << "\n";
            ofs << " Kernel type:  " << fcfg.kernel.kernelType << "\n";
            ofs << " Window:       " << result.effectiveWindow << " samples\n";
            break;
        case FilterMethodEnum::LOESS:
            ofs << " Bandwidth:    " << fcfg.loess.bandwidth  << "\n";
            ofs << " Degree:       " << fcfg.loess.degree     << "\n";
            ofs << " Robust:       " << (fcfg.loess.robust ? "yes" : "no") << "\n";
            ofs << " Window:       " << result.effectiveWindow << " samples\n";
            break;
        case FilterMethodEnum::SAVITZKY_GOLAY:
            ofs << " Window:       " << result.effectiveWindow << " samples\n";
            ofs << " Degree:       " << fcfg.savitzkyGolay.degree << "\n";
            break;
        case FilterMethodEnum::SPLINE:
            ofs << " Knots:        " << (result.effectiveWindow > 0
                                         ? std::to_string(n / result.effectiveWindow)
                                         : "N/A") << "\n";
            ofs << " Subsample step: " << result.effectiveWindow << " samples\n";
            ofs << " Boundary cond.: " << fcfg.spline.bc << "\n";
            break;
    }

    // Auto-window flag.
    const bool autoWindow =
        (fcfg.method == FilterMethodEnum::MOVING_AVERAGE && fcfg.movingAverage.window == 0) ||
        (fcfg.method == FilterMethodEnum::KERNEL         && fcfg.kernel.bandwidth     == 0.0) ||
        (fcfg.method == FilterMethodEnum::LOESS          && fcfg.loess.bandwidth      == 0.0) ||
        (fcfg.method == FilterMethodEnum::SAVITZKY_GOLAY && fcfg.savitzkyGolay.window == 0) ||
        (fcfg.method == FilterMethodEnum::SPLINE         && fcfg.spline.subsampleStep == 0);
    ofs << " Auto-param:   " << (autoWindow ? "yes" : "no") << "\n";

    // Residual statistics.
    ofs << "\n RESIDUAL STATISTICS\n";
    ofs << " -------------------------------------------------------\n";
    ofs << " Mean:    " << std::setw(12) << summary.mean
        << "    Std dev: " << std::setw(12) << summary.stddev  << "\n";
    ofs << " MAD:     " << std::setw(12) << summary.mad
        << "    RMSE:    " << std::setw(12) << std::sqrt(summary.variance) << "\n";
    ofs << " Min:     " << std::setw(12) << summary.min
        << "    Max:     " << std::setw(12) << summary.max     << "\n";

    // Residual diagnostics.
    ofs << "\n RESIDUAL DIAGNOSTICS\n";
    ofs << " -------------------------------------------------------\n";

    {
        std::ostringstream jbLine;
        jbLine << std::fixed << std::setprecision(3) << "p = " << jb.pValue;
        ofs << diagLine(" Normality (J-B):   ", jbLine.str(),
                        !jb.rejected, "non-normal residuals") << "\n";
    }
    {
        std::ostringstream dwLine;
        dwLine << std::fixed << std::setprecision(2) << dw;
        const bool dwOk = (dw > 1.5 && dw < 2.5);
        const std::string dwMsg = (dw < 1.5) ? "positive autocorrelation"
                                              : "negative autocorrelation";
        ofs << diagLine(" Autocorr. (D-W):   ", dwLine.str(), dwOk, dwMsg) << "\n";
    }
    {
        std::ostringstream acfLine;
        acfLine << std::fixed << std::setprecision(3) << acf1;
        ofs << " ACF lag-1:         " << acfLine.str() << "\n";
    }

    // Tuning hints.
    ofs << "\n TUNING HINTS\n";
    ofs << " -------------------------------------------------------\n";
    ofs << tuningHints(fcfg, acf1, dw);

    ofs << "============================================================\n";
    ofs.close();

    LOKI_INFO("FilterAnalyzer: protocol written -> " + outPath.string());
}

// ---------------------------------------------------------------------------
//  _writeCsv
// ---------------------------------------------------------------------------

void FilterAnalyzer::_writeCsv(const TimeSeries&   ts,
                                const FilterResult& result,
                                const std::string&  datasetName,
                                const std::string&  compName) const
{
    const std::string fname = "filter_" + datasetName + "_" + compName + ".csv";
    const std::filesystem::path outPath = m_cfg.csvDir / fname;

    std::ofstream ofs(outPath);
    if (!ofs.is_open()) {
        throw IoException("FilterAnalyzer: cannot write CSV to '"
                          + outPath.string() + "'.");
    }

    ofs << std::fixed << std::setprecision(9);
    ofs << "time_mjd;original;filtered;residual\n";

    const std::size_t n = ts.size();
    for (std::size_t i = 0; i < n; ++i) {
        const double filtered = (i < result.filtered.size())
                                ? result.filtered[i].value
                                : std::numeric_limits<double>::quiet_NaN();
        const double residual = (i < result.residuals.size())
                                ? result.residuals[i].value
                                : std::numeric_limits<double>::quiet_NaN();
        ofs << ts[i].time.mjd() << ";"
            << ts[i].value      << ";"
            << filtered         << ";"
            << residual         << "\n";
    }

    LOKI_INFO("FilterAnalyzer: CSV written -> " + outPath.string());
}
