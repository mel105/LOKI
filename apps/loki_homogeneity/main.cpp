#include "loki/core/logger.hpp"
#include "loki/core/configLoader.hpp"
#include "loki/core/version.hpp"
#include "loki/io/dataManager.hpp"
#include "loki/stats/descriptive.hpp"
#include "loki/homogeneity/homogenizer.hpp"
#include "loki/homogeneity/plotHomogeneity.hpp"
#include "loki/timeseries/deseasonalizer.hpp"
#include "loki/timeseries/gapFiller.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string_view>

// ----------------------------------------------------------------------------
//  Help / version
// ----------------------------------------------------------------------------

static void printVersion()
{
    std::cout << "loki_homogeneity " << loki::VERSION_STRING << "\n";
}

static void printHelp()
{
    std::cout
        << "loki_homogeneity " << loki::VERSION_STRING
        << " -- Homogeneity analysis pipeline\n"
        << "\n"
        << "Usage:\n"
        << "  loki_homogeneity.exe <config.json> [options]\n"
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
    std::filesystem::path configPath {"config/loki_homogeneity.json"};
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
            std::cerr << "[loki_homogeneity] Unknown option: " << arg << "  (use --help)\n";
        }
    }
    return args;
}

// ----------------------------------------------------------------------------
//  Build HomogenizerConfig from AppConfig
// ----------------------------------------------------------------------------

static loki::homogeneity::HomogenizerConfig
buildHomogenizerConfig(const loki::AppConfig& cfg)
{
    const auto& h = cfg.homogeneity;
    loki::homogeneity::HomogenizerConfig hcfg{};

    hcfg.applyGapFilling = h.applyGapFilling;
    {
        const std::string& s = h.gapFilling.strategy;
        if      (s == "linear")       hcfg.gapFiller.strategy = loki::GapFiller::Strategy::LINEAR;
        else if (s == "forward_fill") hcfg.gapFiller.strategy = loki::GapFiller::Strategy::FORWARD_FILL;
        else if (s == "mean")         hcfg.gapFiller.strategy = loki::GapFiller::Strategy::MEAN;
        else                          hcfg.gapFiller.strategy = loki::GapFiller::Strategy::NONE;
        hcfg.gapFiller.maxFillLength = static_cast<std::size_t>(
            std::max(0, h.gapFilling.maxFillLength));
    }

    auto mapOutlier = [](const loki::OutlierFilterConfig& src,
                         double defaultMadMultiplier)
        -> loki::homogeneity::OutlierConfig
    {
        loki::homogeneity::OutlierConfig o;
        o.enabled             = src.enabled;
        o.method              = src.method;
        o.madMultiplier       = (src.madMultiplier > 0.0)
                                    ? src.madMultiplier
                                    : defaultMadMultiplier;
        o.iqrMultiplier       = src.iqrMultiplier;
        o.zscoreThreshold     = src.zscoreThreshold;
        o.replacementStrategy = src.replacementStrategy;
        o.maxFillLength       = src.maxFillLength;
        return o;
    };

    hcfg.preOutlier  = mapOutlier(h.preOutlier,  5.0);
    hcfg.postOutlier = mapOutlier(h.postOutlier,  3.0);

    {
        const std::string& s = h.deseasonalization.strategy;
        if      (s == "median_year")    hcfg.deseasonalizer.strategy =
            loki::Deseasonalizer::Strategy::MEDIAN_YEAR;
        else if (s == "moving_average") hcfg.deseasonalizer.strategy =
            loki::Deseasonalizer::Strategy::MOVING_AVERAGE;
        else                            hcfg.deseasonalizer.strategy =
            loki::Deseasonalizer::Strategy::NONE;
        hcfg.deseasonalizer.maWindowSize = h.deseasonalization.maWindowSize;
    }

    hcfg.detector.minSegmentPoints         = static_cast<std::size_t>(
        std::max(0, h.detection.minSegmentPoints));
    hcfg.detector.minSegmentSeconds        = h.detection.minSegmentSeconds;
    hcfg.detector.detectorConfig.significanceLevel  = h.detection.significanceLevel;
    hcfg.detector.detectorConfig.acfDependenceLimit = h.detection.acfDependenceLimit;

    hcfg.applyAdjustment = h.applyAdjustment;

    return hcfg;
}

// ----------------------------------------------------------------------------
//  CSV export
// ----------------------------------------------------------------------------

static void writeCsv(const std::filesystem::path&                csvDir,
                     const loki::TimeSeries&                     original,
                     const loki::homogeneity::HomogenizerResult& result)
{
    const auto& m    = original.metadata();
    std::string base = m.stationId;
    if (!m.componentName.empty()) {
        if (!base.empty()) base += "_";
        base += m.componentName;
    }
    if (base.empty()) base = "series";

    const std::filesystem::path outPath = csvDir / (base + "_homogeneity.csv");
    std::ofstream csv(outPath);
    if (!csv.is_open()) {
        LOKI_WARNING("writeCsv: cannot open file: " + outPath.string());
        return;
    }

    csv << "mjd;original;seasonal;deseasonalized;adjusted;flag;change_point\n";
    csv << std::fixed << std::setprecision(10);

    const std::size_t n = original.size();
    for (std::size_t i = 0; i < n; ++i) {
        const double mjd     = original[i].time.mjd();
        const double origVal = original[i].value;
        const double seas    = (i < result.seasonal.size())
                                   ? result.seasonal[i]
                                   : std::numeric_limits<double>::quiet_NaN();
        const double deseas  = (i < result.deseasonalizedValues.size())
                                   ? result.deseasonalizedValues[i]
                                   : std::numeric_limits<double>::quiet_NaN();
        const double adj     = (i < result.adjustedSeries.size())
                                   ? result.adjustedSeries[i].value
                                   : std::numeric_limits<double>::quiet_NaN();
        const int    flag    = original[i].flag;

        bool isCp = false;
        for (const auto& cp : result.changePoints) {
            if (cp.globalIndex == i) { isCp = true; break; }
        }

        csv << mjd     << ";"
            << origVal << ";"
            << seas    << ";"
            << deseas  << ";"
            << adj     << ";"
            << flag    << ";"
            << (isCp ? 1 : 0) << "\n";
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
        loki::Logger::initDefault(cfg.logDir, "loki_homogeneity", cfg.output.logLevel);
    } catch (const loki::LOKIException& ex) {
        std::cerr << "[LOKI ERROR] Cannot init logger: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    LOKI_INFO("loki_homogeneity " + std::string(loki::VERSION_STRING) + " started.");
    LOKI_INFO("Config:    " + args.configPath.string());
    LOKI_INFO("Workspace: " + cfg.workspace.string());

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

    const auto hcfg = buildHomogenizerConfig(cfg);
    const loki::homogeneity::Homogenizer homogenizer{hcfg};

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

    loki::homogeneity::PlotHomogeneity plotter{cfg};

    for (const auto& r : loadResults) {
        for (const auto& ts : r.series) {
            const std::string name = ts.metadata().stationId + "_"
                                   + ts.metadata().componentName;
            LOKI_INFO("--- Processing series: " + name + " ---");

            loki::homogeneity::HomogenizerResult result;
            try {
                result = homogenizer.process(ts);
            } catch (const loki::LOKIException& ex) {
                LOKI_ERROR("Homogenization failed for " + name + ": " + ex.what());
                continue;
            }

            LOKI_INFO("Change points detected: "
                      + std::to_string(result.changePoints.size()));
            for (const auto& cp : result.changePoints) {
                LOKI_INFO("  index=" + std::to_string(cp.globalIndex)
                          + "  mjd="   + std::to_string(cp.mjd)
                          + "  shift=" + std::to_string(cp.shift)
                          + "  p="     + std::to_string(cp.pValue));
            }

            try {
                writeCsv(cfg.csvDir, ts, result);
            } catch (const loki::LOKIException& ex) {
                LOKI_ERROR(std::string("CSV export failed: ") + ex.what());
            }

            try {
                // Use result.seasonal directly -- set by Homogenizer from
                // Deseasonalizer::Result. Do not recompute here.
                plotter.plotAll(ts, result, result.seasonal);
            } catch (const loki::LOKIException& ex) {
                LOKI_ERROR(std::string("Plotting failed: ") + ex.what());
            }
        }
    }

    LOKI_INFO("loki_homogeneity finished successfully.");
    return EXIT_SUCCESS;
}