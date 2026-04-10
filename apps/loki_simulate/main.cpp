#include <loki/simulate/simulateAnalyzer.hpp>

#include <loki/core/config.hpp>
#include <loki/core/configLoader.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/core/version.hpp>
#include <loki/io/dataManager.hpp>

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

// ----------------------------------------------------------------------------
//  Help / version
// ----------------------------------------------------------------------------

static void printVersion()
{
    std::cout << "loki_simulate " << loki::VERSION_STRING << "\n";
}

static void printHelp()
{
    std::cout
        << "loki_simulate " << loki::VERSION_STRING
        << " -- Synthetic time series generation and parametric bootstrap\n"
        << "\nUsage:\n"
        << "  loki_simulate.exe <config.json> [options]\n"
        << "\nOptions:\n"
        << "  --help     Show this message and exit.\n"
        << "  --version  Show version string and exit.\n"
        << "\nModes (set via config JSON 'simulate.mode'):\n"
        << "  synthetic  -- Generate realizations from ARIMA/Kalman parameters.\n"
        << "                No input data required.\n"
        << "  bootstrap  -- Fit model on input data, generate B replicas,\n"
        << "                compute bootstrap CIs for key parameters.\n";
}

// ----------------------------------------------------------------------------
//  CLI
// ----------------------------------------------------------------------------

struct CliArgs {
    bool                  showHelp   {false};
    bool                  showVersion{false};
    std::filesystem::path configPath {"config/simulate.json"};
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
            std::cerr << "[loki_simulate] Unknown option: " << arg
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
        loki::Logger::initDefault(cfg.logDir, "loki_simulate", cfg.output.logLevel);
    } catch (const loki::LOKIException& ex) {
        std::cerr << "[LOKI ERROR] Cannot init logger: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    LOKI_INFO("loki_simulate " + std::string(loki::VERSION_STRING) + " started.");
    LOKI_INFO("Config:    " + args.configPath.string());
    LOKI_INFO("Workspace: " + cfg.workspace.string());
    LOKI_INFO("Mode:      " + cfg.simulate.mode);
    LOKI_INFO("Model:     " + cfg.simulate.model);

    // Create output directories.
    std::filesystem::create_directories(cfg.logDir);
    std::filesystem::create_directories(cfg.csvDir);
    std::filesystem::create_directories(cfg.imgDir);
    std::filesystem::create_directories(cfg.protocolsDir);

    loki::simulate::SimulateAnalyzer analyzer{cfg};

    // ---- Synthetic mode: no input data needed. ------------------------------
    if (cfg.simulate.mode == "synthetic") {
        try {
            analyzer.runSynthetic("synthetic");
        } catch (const loki::LOKIException& ex) {
            LOKI_ERROR(std::string("Simulation failed: ") + ex.what());
            return EXIT_FAILURE;
        }
        LOKI_INFO("loki_simulate finished successfully.");
        return EXIT_SUCCESS;
    }

    // ---- Bootstrap mode: load input data and run for each series. -----------
    if (cfg.simulate.mode != "bootstrap") {
        std::cerr << "[LOKI ERROR] simulate.mode must be 'synthetic' or 'bootstrap', got '"
                  << cfg.simulate.mode << "'.\n";
        return EXIT_FAILURE;
    }

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

    for (const auto& r : loadResults) {
        const std::string datasetName = r.filePath.stem().string().empty()
            ? "data" : r.filePath.stem().string();

        for (const loki::TimeSeries& ts : r.series) {
            try {
                analyzer.run(ts, datasetName);
            } catch (const loki::LOKIException& ex) {
                LOKI_ERROR("Simulation failed for '"
                           + ts.metadata().componentName + "': " + ex.what());
                // Continue with remaining series.
            }
        }
    }

    LOKI_INFO("loki_simulate finished successfully.");
    return EXIT_SUCCESS;
}
