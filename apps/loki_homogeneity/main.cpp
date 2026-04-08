#include "loki/core/logger.hpp"
#include "loki/core/configLoader.hpp"
#include "loki/core/version.hpp"
#include "loki/io/dataManager.hpp"
#include "loki/stats/descriptive.hpp"
#include "loki/homogeneity/homogeneityAnalyzer.hpp"

#include <filesystem>
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
    std::filesystem::path configPath {"config/homogenization.json"};
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
            std::cerr << "[loki_homogeneity] Unknown option: " << arg
                      << "  (use --help)\n";
        }
    }
    return args;
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

    // Create output directories.
    try {
        std::filesystem::create_directories(cfg.logDir);
        std::filesystem::create_directories(cfg.csvDir);
        std::filesystem::create_directories(cfg.imgDir);
        std::filesystem::create_directories(cfg.protocolsDir);
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

    // Descriptive statistics on raw series.
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

    // Run homogeneity analysis.
    loki::homogeneity::HomogeneityAnalyzer analyzer{cfg};

    for (const auto& r : loadResults) {
        for (const auto& ts : r.series) {
            try {
                analyzer.run(ts, r.filePath.stem().string());
            } catch (const loki::LOKIException& ex) {
                LOKI_ERROR(std::string("HomogeneityAnalyzer failed: ") + ex.what());
            }
        }
    }

    LOKI_INFO("loki_homogeneity finished successfully.");
    return EXIT_SUCCESS;
}