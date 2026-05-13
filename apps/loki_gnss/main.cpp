#include <loki/gnss/gnssTypes.hpp>
#include <loki/gnss/rinexNavParser.hpp>
#include <loki/gnss/rinexObsParser.hpp>
#include <loki/gnss/gnssAnalyzer.hpp>
#include <loki/core/config.hpp>
#include <loki/core/configLoader.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/core/version.hpp>

#include <filesystem>
#include <iostream>
#include <string_view>

namespace fs = std::filesystem;

// =============================================================================
//  CLI
// =============================================================================

static void printHelp()
{
    std::cout
        << "loki_gnss " << loki::VERSION_STRING
        << " -- GNSS parsing and positioning pipeline\n"
        << "\nUsage:\n"
        << "  loki_gnss.exe <config.json> [options]\n"
        << "\nOptions:\n"
        << "  --help     Show this message and exit.\n"
        << "  --version  Show version string and exit.\n"
        << "\nTasks (gnss.task in config):\n"
        << "  parse  -- Parse NAV + OBS, summary and sat-count plot.\n"
        << "  spp    -- Single Point Positioning (broadcast ephemeris).\n"
        << "  ppp    -- Precise Point Positioning (SP3 + CLK).\n";
}

struct CliArgs {
    bool                  showHelp   {false};
    bool                  showVersion{false};
    std::filesystem::path configPath {"config/gnss.json"};
};

static CliArgs parseArgs(int argc, char* argv[])
{
    CliArgs args;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if      (arg == "--help"    || arg == "-h") { args.showHelp    = true; }
        else if (arg == "--version" || arg == "-v") { args.showVersion = true; }
        else if (arg[0] != '-') { args.configPath = argv[i]; }
        else std::cerr << "[loki_gnss] Unknown option: " << arg << "\n";
    }
    return args;
}

// =============================================================================
//  main
// =============================================================================

int main(int argc, char* argv[])
{
    const CliArgs args = parseArgs(argc, argv);
    if (args.showHelp)    { printHelp(); return EXIT_SUCCESS; }
    if (args.showVersion) {
        std::cout << "loki_gnss " << loki::VERSION_STRING << "\n";
        return EXIT_SUCCESS;
    }

    // -- Config ----------------------------------------------------------------
    loki::AppConfig cfg;
    try {
        cfg = loki::ConfigLoader::load(args.configPath);
    } catch (const loki::LOKIException& ex) {
        std::cerr << "[LOKI ERROR] " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    try {
        loki::Logger::initDefault(cfg.logDir, "loki_gnss", cfg.output.logLevel);
    } catch (const loki::LOKIException& ex) {
        std::cerr << "[LOKI ERROR] Cannot init logger: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    LOKI_INFO("loki_gnss " + std::string(loki::VERSION_STRING) + " started.");
    LOKI_INFO("Config:  " + args.configPath.string());
    LOKI_INFO("Station: " + cfg.gnss.station);
    LOKI_INFO("Task:    " + cfg.gnss.task);

    // Log which pipeline stages are active.
    if (cfg.gnss.spp.enabled) LOKI_INFO("Stage:   SPP enabled (broadcast ephemeris).");
    if (cfg.gnss.ppp.enabled) LOKI_INFO("Stage:   PPP enabled (SP3 + CLK).");

    fs::create_directories(cfg.logDir);
    fs::create_directories(cfg.csvDir);
    fs::create_directories(cfg.imgDir);
    fs::create_directories(cfg.protocolsDir);

    // -- Parse NAV (always required) ------------------------------------------
    loki::gnss::NavFile nav;
    try {
        loki::gnss::RinexNavParser navParser;
        LOKI_INFO("Parsing NAV: " + cfg.gnss.navFile);
        nav = navParser.parseGz(cfg.gnss.navFile);
        LOKI_INFO("NAV: GPS=" + std::to_string(nav.gpsEph.size())
                  + " GAL=" + std::to_string(nav.galEph.size())
                  + " GLO=" + std::to_string(nav.gloEph.size()));
    } catch (const loki::LOKIException& ex) {
        LOKI_ERROR(std::string("NAV parsing failed: ") + ex.what());
        return EXIT_FAILURE;
    }

    // -- Parse OBS (always required) ------------------------------------------
    loki::gnss::ObsFile obs;
    try {
        loki::gnss::RinexObsParser::Config obsCfg;
        obsCfg.crx2rnxPath = cfg.gnss.crx2rnxPath;
        loki::gnss::RinexObsParser obsParser(obsCfg);
        LOKI_INFO("Parsing OBS: " + cfg.gnss.obsFile);
        obs = obsParser.parseGz(cfg.gnss.obsFile);
        LOKI_INFO("OBS: " + std::to_string(obs.epochs.size()) + " epochs.");
    } catch (const loki::LOKIException& ex) {
        LOKI_ERROR(std::string("OBS parsing failed: ") + ex.what());
        return EXIT_FAILURE;
    }

    // -- Run pipeline ----------------------------------------------------------
    try {
        loki::gnss::GnssAnalyzer analyzer(cfg);
        analyzer.run(nav, obs);
    } catch (const loki::LOKIException& ex) {
        LOKI_ERROR(std::string("Pipeline failed: ") + ex.what());
        return EXIT_FAILURE;
    } catch (const std::exception& ex) {
        LOKI_ERROR(std::string("Unexpected error: ") + ex.what());
        return EXIT_FAILURE;
    }

    LOKI_INFO("loki_gnss finished successfully.");
    return EXIT_SUCCESS;
}