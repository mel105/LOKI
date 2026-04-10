#include <loki/core/config.hpp>
#include <loki/core/configLoader.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/core/version.hpp>
#include <loki/io/dataManager.hpp>
#include <loki/kriging/krigingAnalyzer.hpp>

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

static void printVersion()
{
    std::cout << "loki_kriging " << loki::VERSION_STRING << "\n";
}

static void printHelp()
{
    std::cout
        << "loki_kriging " << loki::VERSION_STRING
        << " -- Kriging interpolation and prediction for time series\n"
        << "\nUsage:\n"
        << "  loki_kriging.exe <config.json> [options]\n"
        << "\nOptions:\n"
        << "  --help     Show this message and exit.\n"
        << "  --version  Show version string and exit.\n"
        << "\nModes (kriging.mode in config):\n"
        << "  temporal   -- 1-D Kriging in time (implemented).\n"
        << "  spatial    -- 2-D spatial Kriging (placeholder, not yet implemented).\n"
        << "  space_time -- Space-time Kriging (placeholder, not yet implemented).\n"
        << "\nMethods (kriging.method in config):\n"
        << "  simple     -- known constant mean.\n"
        << "  ordinary   -- unknown local mean (default).\n"
        << "  universal  -- polynomial drift in time.\n";
}

struct CliArgs {
    bool                  showHelp   {false};
    bool                  showVersion{false};
    std::filesystem::path configPath {"config/kriging.json"};
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
            std::cerr << "[loki_kriging] Unknown option: " << arg
                      << "  (use --help)\n";
        }
    }
    return args;
}

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
        loki::Logger::initDefault(cfg.logDir, "loki_kriging", cfg.output.logLevel);
    } catch (const loki::LOKIException& ex) {
        std::cerr << "[LOKI ERROR] Cannot init logger: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    LOKI_INFO("loki_kriging " + std::string(loki::VERSION_STRING) + " started.");
    LOKI_INFO("Config:    " + args.configPath.string());
    LOKI_INFO("Workspace: " + cfg.workspace.string());

    std::filesystem::create_directories(cfg.logDir);
    std::filesystem::create_directories(cfg.csvDir);
    std::filesystem::create_directories(cfg.imgDir);
    std::filesystem::create_directories(cfg.protocolsDir);

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

    if (loadResults.empty()) {
        LOKI_WARNING("No data loaded -- nothing to do.");
        return EXIT_SUCCESS;
    }

    loki::kriging::KrigingAnalyzer analyzer{cfg};

    for (const auto& r : loadResults) {
        const std::string datasetName = r.filePath.stem().string().empty()
            ? "data" : r.filePath.stem().string();

        for (const loki::TimeSeries& ts : r.series) {
            try {
                analyzer.run(ts, datasetName);
            } catch (const loki::LOKIException& ex) {
                LOKI_ERROR("Kriging analysis failed for '"
                           + ts.metadata().componentName + "': " + ex.what());
            }
        }
    }

    LOKI_INFO("loki_kriging finished successfully.");
    return EXIT_SUCCESS;
}
