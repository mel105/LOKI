#include "loki/core/logger.hpp"
#include "loki/core/configLoader.hpp"
#include "loki/core/version.hpp"
#include "loki/io/dataManager.hpp"
#include "loki/stats/descriptive.hpp"
#include "loki/homogeneity/deseasonalizer.hpp"
#include "loki/homogeneity/medianYearSeries.hpp"
#include "loki/outlier/plotOutlier.hpp"
#include "loki/outlier/outlierCleaner.hpp"
#include "loki/outlier/iqrDetector.hpp"
#include "loki/outlier/madDetector.hpp"
#include "loki/outlier/zScoreDetector.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string_view>

// ----------------------------------------------------------------------------
//  Help / version
// ----------------------------------------------------------------------------

static void printVersion()
{
    std::cout << "loki_outlier " << loki::VERSION_STRING << "\n";
}

static void printHelp()
{
    std::cout
        << "loki_outlier " << loki::VERSION_STRING
        << " -- Outlier detection and removal pipeline\n"
        << "\n"
        << "Usage:\n"
        << "  loki_outlier.exe <config.json> [options]\n"
        << "\n"
        << "Arguments:\n"
        << "  <config.json>   Path to the JSON configuration file.\n"
        << "                  Defaults to config/outlier.json\n"
        << "\n"
        << "Options:\n"
        << "  --help          Show this message and exit.\n"
        << "  --version       Show version string and exit.\n"
        << "\n"
        << "Pipeline steps (controlled via JSON 'outlier' section):\n"
        << "  1. (optional) Deseasonalizer  -- subtract seasonal component\n"
        << "  2. Outlier detector           -- iqr | mad | zscore | mad_bounds\n"
        << "  3. GapFiller                  -- replace outliers via linear | forward_fill | mean\n"
        << "  4. (optional) Reconstruct     -- add seasonal component back\n";
}

// ----------------------------------------------------------------------------
//  CLI
// ----------------------------------------------------------------------------

struct CliArgs {
    bool                  showHelp   {false};
    bool                  showVersion{false};
    std::filesystem::path configPath {"config/outlier.json"};
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
            std::cerr << "[loki_outlier] Unknown option: " << arg
                      << "  (use --help)\n";
        }
    }
    return args;
}

// ----------------------------------------------------------------------------
//  Build detector from OutlierConfig::DetectionSection
// ----------------------------------------------------------------------------

static std::unique_ptr<loki::outlier::OutlierDetector>
buildDetector(const loki::OutlierConfig::DetectionSection& det)
{
    const std::string& m = det.method;

    if (m == "iqr") {
        return std::make_unique<loki::outlier::IqrDetector>(det.iqrMultiplier);
    }
    if (m == "mad" || m == "mad_bounds") {
        return std::make_unique<loki::outlier::MadDetector>(det.madMultiplier);
    }
    if (m == "zscore") {
        return std::make_unique<loki::outlier::ZScoreDetector>(det.zscoreThreshold);
    }

    LOKI_WARNING("outlier.detection.method '" + m + "' unknown -- falling back to 'mad'.");
    return std::make_unique<loki::outlier::MadDetector>(det.madMultiplier);
}

// ----------------------------------------------------------------------------
//  Build OutlierCleaner::Config from OutlierConfig
// ----------------------------------------------------------------------------

static loki::outlier::OutlierCleaner::Config
buildCleanerConfig(const loki::OutlierConfig& cfg)
{
    loki::outlier::OutlierCleaner::Config c;

    const std::string& rs = cfg.replacement.strategy;
    if      (rs == "forward_fill") { c.fillStrategy = loki::GapFiller::Strategy::FORWARD_FILL; }
    else if (rs == "mean")         { c.fillStrategy = loki::GapFiller::Strategy::MEAN;         }
    else                           { c.fillStrategy = loki::GapFiller::Strategy::LINEAR;        }

    c.maxFillLength = static_cast<std::size_t>(std::max(0, cfg.replacement.maxFillLength));

    return c;
}

// ----------------------------------------------------------------------------
//  Deseasonalize -- returns seasonal vector and populates dsResult
// ----------------------------------------------------------------------------

struct DeseasonalizeResult {
    std::vector<double> seasonal;
    bool                hasComponent;
};

static DeseasonalizeResult
runDeseasonalize(const loki::TimeSeries&                              ts,
                 const loki::OutlierConfig::DeseasonalizationSection& dsCfg,
                 loki::homogeneity::Deseasonalizer::Result&           dsResult)
{
    using Strategy = loki::homogeneity::Deseasonalizer::Strategy;

    loki::homogeneity::Deseasonalizer::Config cfg;
    cfg.maWindowSize = dsCfg.maWindowSize;

    if      (dsCfg.strategy == "median_year")    { cfg.strategy = Strategy::MEDIAN_YEAR;    }
    else if (dsCfg.strategy == "moving_average") { cfg.strategy = Strategy::MOVING_AVERAGE; }
    else                                         { cfg.strategy = Strategy::NONE;            }

    if (cfg.strategy == Strategy::NONE) {
        dsResult.residuals = {};
        dsResult.seasonal  = std::vector<double>(ts.size(), 0.0);
        dsResult.series    = ts;
        return {std::vector<double>(ts.size(), 0.0), false};
    }

    loki::homogeneity::Deseasonalizer ds(cfg);

    if (cfg.strategy == Strategy::MEDIAN_YEAR) {
        loki::homogeneity::MedianYearSeries::Config myCfg;
        myCfg.minYears = dsCfg.medianYearMinYears;

        const loki::homogeneity::MedianYearSeries profile(ts, myCfg);
        auto lookup = [&profile](const ::TimeStamp& t) {
            return profile.valueAt(t);
        };
        dsResult = ds.deseasonalize(ts, lookup);
    } else {
        dsResult = ds.deseasonalize(ts);
    }

    return {dsResult.seasonal, true};
}

// ----------------------------------------------------------------------------
//  CSV export
// ----------------------------------------------------------------------------

static void writeCsv(const std::filesystem::path&                         csvDir,
                     const loki::TimeSeries&                              original,
                     const loki::outlier::OutlierCleaner::CleanResult&   result,
                     const std::vector<double>&                           seasonal)
{
    const auto& m    = original.metadata();
    std::string base = m.stationId;
    if (!m.componentName.empty()) {
        if (!base.empty()) base += "_";
        base += m.componentName;
    }
    if (base.empty()) base = "series";

    const std::filesystem::path outPath = csvDir / (base + "_outlier.csv");
    std::ofstream csv(outPath);
    if (!csv.is_open()) {
        LOKI_WARNING("writeCsv: cannot open file: " + outPath.string());
        return;
    }

    csv << "mjd;original;residual;cleaned;seasonal;outlier_flag\n";
    csv << std::fixed << std::setprecision(10);

    const std::size_t n = original.size();
    for (std::size_t i = 0; i < n; ++i) {
        const double mjd      = original[i].time.mjd();
        const double origVal  = original[i].value;
        const double residual = (i < result.residuals.size())
                                    ? result.residuals[i].value
                                    : std::numeric_limits<double>::quiet_NaN();
        const double cleaned  = (i < result.cleaned.size())
                                    ? result.cleaned[i].value
                                    : std::numeric_limits<double>::quiet_NaN();
        const double seas     = (i < seasonal.size()) ? seasonal[i] : 0.0;

        bool isOutlier = false;
        for (const auto& pt : result.detection.points) {
            if (pt.index == i) { isOutlier = true; break; }
        }

        csv << mjd      << ";"
            << origVal  << ";"
            << residual << ";"
            << cleaned  << ";"
            << seas     << ";"
            << (isOutlier ? 1 : 0) << "\n";
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

    // -- Load config ----------------------------------------------------------
    loki::AppConfig cfg;
    try {
        cfg = loki::ConfigLoader::load(args.configPath);
    } catch (const loki::LOKIException& ex) {
        std::cerr << "[LOKI ERROR] " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    // -- Logger ---------------------------------------------------------------
    try {
        loki::Logger::initDefault(cfg.logDir, "loki_outlier", cfg.output.logLevel);
    } catch (const loki::LOKIException& ex) {
        std::cerr << "[LOKI ERROR] Cannot init logger: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    LOKI_INFO("loki_outlier " + std::string(loki::VERSION_STRING) + " started.");
    LOKI_INFO("Config:    " + args.configPath.string());
    LOKI_INFO("Workspace: " + cfg.workspace.string());

    // -- Load data ------------------------------------------------------------
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

    // -- Descriptive stats (optional) -----------------------------------------
    if (cfg.stats.enabled) {
        try {
            for (const auto& r : loadResults) {
                for (const auto& ts : r.series) {
                    std::vector<double> vals;
                    vals.reserve(ts.size());
                    for (const auto& obs : ts) vals.push_back(obs.value);
                    const auto st = loki::stats::summarize(
                        vals, cfg.stats.nanPolicy, cfg.stats.hurst);
                    LOKI_INFO(loki::stats::formatSummary(
                        st, ts.metadata().componentName));
                }
            }
        } catch (const loki::LOKIException& ex) {
            LOKI_ERROR(std::string("Statistics failed: ") + ex.what());
        }
    }

    // -- Build detector and cleaner once --------------------------------------
    auto detector = buildDetector(cfg.outlier.detection);
    const auto cleanerCfg = buildCleanerConfig(cfg.outlier);
    const loki::outlier::OutlierCleaner cleaner(cleanerCfg, *detector);
    const loki::outlier::PlotOutlier    plotter(cfg, "outlier");

    // -- Outlier pipeline per series ------------------------------------------
    for (const auto& r : loadResults) {
        for (const auto& ts : r.series) {
            const std::string name = ts.metadata().stationId.empty()
                ? ts.metadata().componentName
                : ts.metadata().stationId + "_" + ts.metadata().componentName;
            LOKI_INFO("--- Processing series: " + name
                      + "  n=" + std::to_string(ts.size()) + " ---");

            try {
                // Step 1: deseasonalize (or pass-through if strategy == none).
                loki::homogeneity::Deseasonalizer::Result dsResult;
                const auto [seasonal, hasComponent] =
                    runDeseasonalize(ts, cfg.outlier.deseasonalization, dsResult);

                LOKI_INFO("Deseasonalization: strategy=" + cfg.outlier.deseasonalization.strategy);

                // Step 2+3: detect and replace.
                loki::outlier::OutlierCleaner::CleanResult result =
                    hasComponent
                        ? cleaner.clean(ts, seasonal)
                        : cleaner.clean(ts);

                // Log detection summary.
                const std::size_t nOut = result.detection.nOutliers;
                LOKI_INFO("Outliers detected: " + std::to_string(nOut)
                          + " / " + std::to_string(ts.size())
                          + "  method=" + cfg.outlier.detection.method
                          + "  location=" + std::to_string(result.detection.location)
                          + "  scale="    + std::to_string(result.detection.scale));

                if (nOut > 0) {
                    for (const auto& pt : result.detection.points) {
                        LOKI_INFO("  outlier idx=" + std::to_string(pt.index)
                                  + "  mjd=" + std::to_string(ts[pt.index].time.mjd())
                                  + "  orig=" + std::to_string(pt.originalValue)
                                  + "  score=" + std::to_string(pt.score)
                                  + "  threshold=" + std::to_string(pt.threshold));
                    }
                }

                // Step 4: CSV export.
                try {
                    writeCsv(cfg.csvDir, ts, result, seasonal);
                } catch (const loki::LOKIException& ex) {
                    LOKI_ERROR(std::string("CSV export failed: ") + ex.what());
                }

                // Step 5: Plots.
                try {
                    plotter.plotAll(ts,
                                    result.cleaned,
                                    result.residuals,
                                    result.detection,
                                    hasComponent);
                } catch (const loki::LOKIException& ex) {
                    LOKI_ERROR(std::string("Plotting failed: ") + ex.what());
                }

            } catch (const loki::LOKIException& ex) {
                LOKI_ERROR("Outlier pipeline failed for " + name + ": " + ex.what());
                continue;
            }
        }
    }

    LOKI_INFO("loki_outlier finished successfully.");
    return EXIT_SUCCESS;
}