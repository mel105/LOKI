#include "loki/core/logger.hpp"
#include "loki/core/configLoader.hpp"
#include "loki/core/version.hpp"
#include "loki/io/dataManager.hpp"
#include "loki/stats/descriptive.hpp"
#include "loki/timeseries/gapFiller.hpp"
#include "loki/filter/filter.hpp"
#include "loki/filter/filterResult.hpp"
#include "loki/filter/movingAverageFilter.hpp"
#include "loki/filter/emaFilter.hpp"
#include "loki/filter/weightedMovingAverageFilter.hpp"
#include "loki/filter/kernelSmoother.hpp"
#include "loki/filter/loessFilter.hpp"
#include "loki/filter/savitzkyGolayFilter.hpp"
#include "loki/filter/filterWindowAdvisor.hpp"
#include "loki/filter/plotFilter.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string_view>

using namespace loki;

// ----------------------------------------------------------------------------
//  Help / version
// ----------------------------------------------------------------------------

static void printVersion()
{
    std::cout << "loki_filter " << loki::VERSION_STRING << "\n";
}

static void printHelp()
{
    std::cout
        << "loki_filter " << loki::VERSION_STRING
        << " -- Signal filtering pipeline\n"
        << "\n"
        << "Usage:\n"
        << "  loki_filter.exe <config.json> [options]\n"
        << "\n"
        << "Options:\n"
        << "  --help     Show this message and exit.\n"
        << "  --version  Show version string and exit.\n";
}

// ----------------------------------------------------------------------------
//  CLI
// ----------------------------------------------------------------------------

struct CliArgs {
    bool                  showHelp   {false};
    bool                  showVersion{false};
    std::filesystem::path configPath {"config/filter.json"};
};

static CliArgs parseArgs(int argc, char* argv[])
{
    CliArgs args;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if      (arg == "--help"    || arg == "-h") { args.showHelp    = true; }
        else if (arg == "--version" || arg == "-v") { args.showVersion = true; }
        else if (arg[0] != '-') { args.configPath = argv[i]; }
        else {
            std::cerr << "[loki_filter] Unknown option: " << arg << "  (use --help)\n";
        }
    }
    return args;
}

// ----------------------------------------------------------------------------
//  Build GapFiller::Config from FilterConfig
// ----------------------------------------------------------------------------

static loki::GapFiller::Config buildGapFillerConfig(const loki::FilterConfig& fcfg)
{
    loki::GapFiller::Config gfc{};
    const std::string& s = fcfg.gapFilling.strategy;
    if      (s == "linear")       gfc.strategy = loki::GapFiller::Strategy::LINEAR;
    else if (s == "forward_fill") gfc.strategy = loki::GapFiller::Strategy::FORWARD_FILL;
    else if (s == "mean")         gfc.strategy = loki::GapFiller::Strategy::MEAN;
    else                          gfc.strategy = loki::GapFiller::Strategy::NONE;
    gfc.maxFillLength = static_cast<std::size_t>(
        std::max(0, fcfg.gapFilling.maxFillLength));
    return gfc;
}

// ----------------------------------------------------------------------------
//  Build filter from config
// ----------------------------------------------------------------------------

static std::unique_ptr<Filter>
buildFilter(const FilterConfig& fcfg,
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
    }

    throw AlgorithmException("buildFilter: unhandled FilterMethodEnum value.");
}

// ----------------------------------------------------------------------------
//  CSV export
// ----------------------------------------------------------------------------

static void writeCsv(const std::filesystem::path& csvDir,
                     const TimeSeries&             original,
                     const FilterResult&           result)
{
    const auto& m    = original.metadata();
    std::string base = m.stationId;
    if (!m.componentName.empty()) {
        if (!base.empty()) base += "_";
        base += m.componentName;
    }
    if (base.empty()) base = "series";

    const std::filesystem::path outPath = csvDir / (base + "_filter.csv");
    std::ofstream csv(outPath);
    if (!csv.is_open()) {
        LOKI_WARNING("writeCsv: cannot open file: " + outPath.string());
        return;
    }

    csv << "mjd;original;filtered;residual\n";
    csv << std::fixed << std::setprecision(10);

    const std::size_t n = original.size();
    for (std::size_t i = 0; i < n; ++i) {
        const double mjd      = original[i].time.mjd();
        const double origVal  = original[i].value;
        const double filtVal  = (i < result.filtered.size())
                                    ? result.filtered[i].value
                                    : std::numeric_limits<double>::quiet_NaN();
        const double residual = (i < result.residuals.size())
                                    ? result.residuals[i].value
                                    : std::numeric_limits<double>::quiet_NaN();

        csv << mjd      << ";"
            << origVal  << ";"
            << filtVal  << ";"
            << residual << "\n";
    }

    LOKI_INFO("CSV written: " + outPath.string());
}

// ----------------------------------------------------------------------------
//  main
// ----------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    const CliArgs args = parseArgs(argc, argv);
    if (args.showHelp)    { printHelp();    return EXIT_SUCCESS; }
    if (args.showVersion) { printVersion(); return EXIT_SUCCESS; }

    loki::AppConfig cfg;
    try {
        cfg = loki::ConfigLoader::load(args.configPath);
    } catch (const loki::LOKIException& ex) {
        std::cerr << "[LOKI ERROR] " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    try {
        loki::Logger::initDefault(cfg.logDir, "loki_filter", cfg.output.logLevel);
    } catch (const loki::LOKIException& ex) {
        std::cerr << "[LOKI ERROR] Cannot init logger: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    LOKI_INFO("loki_filter " + std::string(loki::VERSION_STRING) + " started.");
    LOKI_INFO("Config:    " + args.configPath.string());
    LOKI_INFO("Workspace: " + cfg.workspace.string());

    // Create output directories.
    try {
        std::filesystem::create_directories(cfg.logDir);
        std::filesystem::create_directories(cfg.csvDir);
        std::filesystem::create_directories(cfg.imgDir);
    } catch (const std::exception& ex) {
        LOKI_ERROR(std::string("Cannot create output directories: ") + ex.what());
        return EXIT_FAILURE;
    }

    // Load data.
    std::vector<loki::LoadResult> loadResults;
    try {
        loki::DataManager dm(cfg);
        loadResults = dm.load();
        for (const auto& r : loadResults) {
            LOKI_INFO("Loaded: " + r.filePath.filename().string()
                      + "  lines=" + std::to_string(r.linesRead)
                      + "  skipped=" + std::to_string(r.linesSkipped));
            for (std::size_t i = 0; i < r.series.size(); ++i) {
                LOKI_INFO("  Series[" + std::to_string(i) + "] "
                          + r.columnNames[i]
                          + "  n=" + std::to_string(r.series[i].size()));
            }
        }
    } catch (const loki::LOKIException& ex) {
        LOKI_ERROR(std::string("Data loading failed: ") + ex.what());
        return EXIT_FAILURE;
    }

    // Descriptive stats on raw series.
    if (cfg.stats.enabled) {
        try {
            for (const auto& r : loadResults) {
                for (const auto& ts : r.series) {
                    std::vector<double> vals;
                    vals.reserve(ts.size());
                    for (const auto& obs : ts) vals.push_back(obs.value);
                    const auto st = loki::stats::summarize(
                        vals, cfg.stats.nanPolicy, cfg.stats.hurst);
                    LOKI_INFO(loki::stats::formatSummary(st, ts.metadata().componentName));
                }
            }
        } catch (const loki::LOKIException& ex) {
            LOKI_ERROR(std::string("Statistics failed: ") + ex.what());
        }
    }

    const loki::GapFiller::Config gapFillerCfg = buildGapFillerConfig(cfg.filter);
    loki::filter::PlotFilter plotter{cfg};

    for (const auto& r : loadResults) {
        for (const auto& ts : r.series) {
            const std::string name = ts.metadata().stationId + "_"
                                   + ts.metadata().componentName;
            LOKI_INFO("--- Processing series: " + name + " ---");

            // Step 1: gap filling.
            loki::TimeSeries filled = ts;
            try {
                loki::GapFiller gapFiller{gapFillerCfg};
                filled = gapFiller.fill(ts);
                const std::size_t nFilled = filled.size() - ts.size();
                // Count NaN before and after to report gap fill statistics.
                std::size_t nanBefore = 0;
                std::size_t nanAfter  = 0;
                for (const auto& obs : ts)     if (!loki::isValid(obs)) ++nanBefore;
                for (const auto& obs : filled) if (!loki::isValid(obs)) ++nanAfter;
                LOKI_INFO("GapFiller: NaN before=" + std::to_string(nanBefore)
                          + "  after=" + std::to_string(nanAfter)
                          + "  strategy=" + cfg.filter.gapFilling.strategy);
                (void)nFilled;
            } catch (const loki::LOKIException& ex) {
                LOKI_ERROR("GapFiller failed for " + name + ": " + ex.what());
                continue;
            }

            // Step 2: build and apply filter.
            FilterResult result;
            try {
                const auto filterPtr = buildFilter(cfg.filter, filled);
                LOKI_INFO("Filter: " + filterPtr->name());
                result = filterPtr->apply(filled);
            } catch (const LOKIException& ex) {
                LOKI_ERROR("Filter failed for " + name + ": " + ex.what());
                continue;
            }

            {
                std::string winfo = "Filter applied: " + result.filterName
                    + "  n=" + std::to_string(result.filtered.size());
                if (result.effectiveWindow > 0) {
                    winfo += "  window=" + std::to_string(result.effectiveWindow) + " samples";
                    winfo += "  bandwidth=" + std::to_string(result.effectiveBandwidth);
                    // Hint: if residuals still show periodicity, try smaller bandwidth.
                    winfo += "  (to reduce smoothing: lower bandwidth in filter.json)";
                }
                LOKI_INFO(winfo);
            }

            // Step 3: CSV export.
            try {
                writeCsv(cfg.csvDir, ts, result);
            } catch (const loki::LOKIException& ex) {
                LOKI_ERROR(std::string("CSV export failed: ") + ex.what());
            }

            // Step 4: plots.
            try {
                plotter.plotAll(ts, filled, result);
            } catch (const loki::LOKIException& ex) {
                LOKI_ERROR(std::string("Plotting failed: ") + ex.what());
            }
        }
    }

    LOKI_INFO("loki_filter finished successfully.");
    return EXIT_SUCCESS;
}
