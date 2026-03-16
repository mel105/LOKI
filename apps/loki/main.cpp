#include "loki/core/logger.hpp"
#include "loki/core/configLoader.hpp"
#include "loki/core/version.hpp"
#include "loki/io/dataManager.hpp"
#include "loki/io/plot.hpp"
#include "loki/stats/descriptive.hpp"

#include <filesystem>
#include <iostream>
#include <string_view>

// -----------------------------------------------------------------------------
// Help / version output
// -----------------------------------------------------------------------------

static void printVersion()
{
    std::cout << "LOKI " << loki::VERSION_STRING << "\n";
}

static void printHelp()
{
    std::cout
        << "LOKI " << loki::VERSION_STRING << " -- Statistical Analysis Toolkit\n"
        << "\n"
        << "Usage:\n"
        << "  loki.exe <config.json> [options]\n"
        << "\n"
        << "Arguments:\n"
        << "  <config.json>   Path to the JSON configuration file.\n"
        << "                  Defaults to config/loki_homogeneity.json\n"
        << "                  if not supplied.\n"
        << "\n"
        << "Options:\n"
        << "  --help          Show this help message and exit.\n"
        << "  --version       Show version string and exit.\n"
        << "\n"
        << "Configuration keys (JSON):\n"
        << "  workspace               Absolute path to workspace root.\n"
        << "  input.file              Input data file (relative to INPUT/).\n"
        << "  input.time_format       gpst_seconds | mjd | utc | unix | index\n"
        << "  input.delimiter         Column separator character.\n"
        << "  input.comment_char      Comment line prefix character.\n"
        << "  input.columns           Value column indices (empty = all).\n"
        << "  input.merge_strategy    separate | merge\n"
        << "  output.log_level        debug | info | warning | error\n"
        << "  stats.enabled           true | false\n"
        << "  stats.nan_policy        skip | throw | propagate\n"
        << "  stats.hurst             true | false\n"
        << "  plots.output_format     png | eps | svg\n"
        << "  plots.enabled.*         time_series | histogram | acf | qq_plot | boxplot\n"
        << "  homogeneity.method      snht | pettitt | buishand\n"
        << "  homogeneity.significance_level   e.g. 0.05\n"
        << "\n"
        << "Examples:\n"
        << "  loki.exe config/loki_homogeneity.json\n"
        << "  loki.exe --help\n"
        << "  loki.exe --version\n";
}

// -----------------------------------------------------------------------------
// Argument parsing
// -----------------------------------------------------------------------------

struct CliArgs {
    bool                  showHelp    {false};
    bool                  showVersion {false};
    std::filesystem::path configPath  {"config/loki_homogeneity.json"};
};

static CliArgs parseArgs(int argc, char* argv[])
{
    CliArgs args;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--help"    || arg == "-h") { args.showHelp    = true; }
        else if (arg == "--version" || arg == "-v") { args.showVersion = true; }
        else if (arg[0] != '-') { args.configPath = argv[i]; }
        else {
            std::cerr << "[LOKI] Unknown option: " << arg
                      << "  (use --help for usage)\n";
        }
    }
    return args;
}

// -----------------------------------------------------------------------------
// main
// -----------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    const CliArgs args = parseArgs(argc, argv);

    if (args.showHelp) {
        printHelp();
        return EXIT_SUCCESS;
    }

    if (args.showVersion) {
        printVersion();
        return EXIT_SUCCESS;
    }

    // Load configuration
    loki::AppConfig cfg;
    try {
        cfg = loki::ConfigLoader::load(args.configPath);
    } catch (const loki::LOKIException& ex) {
        std::cerr << "[LOKI ERROR] " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    // Initialise logger
    try {
        loki::Logger::initDefault(cfg.logDir, "loki", cfg.output.logLevel);
    } catch (const loki::LOKIException& ex) {
        std::cerr << "[LOKI ERROR] Cannot initialise logger: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    LOKI_INFO("LOKI " + std::string(loki::VERSION_STRING) + " started.");
    LOKI_INFO("Config:    " + args.configPath.string());
    LOKI_INFO("Workspace: " + cfg.workspace.string());

    // Load data
    std::vector<loki::LoadResult> results;
    try {
        loki::DataManager dm(cfg);
        results = dm.load();

        for (const auto& r : results) {
            LOKI_INFO("File: " + r.filePath.filename().string()
                      + "  |  lines read: "  + std::to_string(r.linesRead)
                      + "  |  skipped: "     + std::to_string(r.linesSkipped)
                      + "  |  records: "     + std::to_string(
                            r.series.empty() ? 0 : r.series[0].size()));

            for (std::size_t i = 0; i < r.series.size(); ++i) {
                LOKI_INFO("  Series [" + std::to_string(i) + "] "
                          + r.columnNames[i]
                          + "  size=" + std::to_string(r.series[i].size()));
            }
        }
    } catch (const loki::LOKIException& ex) {
        LOKI_ERROR(std::string("Data loading failed: ") + ex.what());
        return EXIT_FAILURE;
    }

    // Descriptive statistics
    try {
        if (cfg.stats.enabled) {
            for (const auto& r : results) {
                for (const auto& ts : r.series) {
                    std::vector<double> vals;
                    vals.reserve(ts.size());
                    for (const auto& obs : ts) {
                        vals.push_back(obs.value);
                    }
                    const auto st = loki::stats::summarize(
                        vals,
                        cfg.stats.nanPolicy,
                        cfg.stats.hurst);
                    LOKI_INFO(loki::stats::formatSummary(
                        st, ts.metadata().componentName));
                }
            }
        }
    } catch (const loki::LOKIException& ex) {
        LOKI_ERROR(std::string("Statistics failed: ") + ex.what());
        return EXIT_FAILURE;
    }

    // Plotting
    try {
        loki::Plot plot(cfg);
        for (const auto& r : results) {
            for (const auto& ts : r.series) {
                const std::string name = ts.metadata().stationId + "_"
                                       + ts.metadata().componentName;
                if (cfg.plots.timeSeries) {
                    LOKI_INFO("Plotting timeSeries: " + name);
                    plot.timeSeries(ts);
                }
                if (cfg.plots.histogram) {
                    LOKI_INFO("Plotting histogram: " + name);
                    plot.histogram(ts);
                }
                if (cfg.plots.qqPlot) {
                    LOKI_INFO("Plotting qqPlot: " + name);
                    plot.qqPlot(ts);
                }
                if (cfg.plots.boxplot) {
                    LOKI_INFO("Plotting boxplot: " + name);
                    plot.boxplot(ts);
                }
            }
        }
        LOKI_INFO("Plotting complete.");
    } catch (const loki::LOKIException& ex) {
        LOKI_ERROR(std::string("Plotting failed: ") + ex.what());
        return EXIT_FAILURE;
    }

    LOKI_INFO("LOKI finished successfully.");
    return EXIT_SUCCESS;
}