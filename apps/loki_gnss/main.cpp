#include <loki/gnss/gnssTypes.hpp>
#include <loki/gnss/rinexNavParser.hpp>
#include <loki/gnss/rinexObsParser.hpp>
#include <loki/gnss/keplerOrbit.hpp>
#include <loki/gnss/satVisibility.hpp>
#include <loki/gnss/sppSolver.hpp>
#include <loki/core/config.hpp>
#include <loki/core/configLoader.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/core/version.hpp>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

// =============================================================================
//  CLI
// =============================================================================

static void printHelp() {
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
        << "  ppp    -- Precise Point Positioning (SP3 + CLK). [planned]\n";
}

struct CliArgs {
    bool                  showHelp   {false};
    bool                  showVersion{false};
    std::filesystem::path configPath {"config/gnss.json"};
};

static CliArgs parseArgs(int argc, char* argv[]) {
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
//  Display helpers
// =============================================================================
namespace {

std::string gnssSystemName(loki::gnss::GnssSystem sys) {
    using G = loki::gnss::GnssSystem;
    switch (sys) {
        case G::GPS:     return "GPS";
        case G::GLONASS: return "GLONASS";
        case G::GALILEO: return "Galileo";
        case G::BEIDOU:  return "BeiDou";
        case G::QZSS:    return "QZSS";
        case G::SBAS:    return "SBAS";
        default:         return "Unknown";
    }
}

std::string fwdSlash(const std::string& p) {
    std::string s = p;
    for (char& c : s) { if (c == '\\') c = '/'; }
    return s;
}

void separator() { std::cout << std::string(70, '-') << "\n"; }

// =============================================================================
//  NAV summary
// =============================================================================
void printNavSummary(const loki::gnss::NavFile& nav) {
    separator();
    std::cout << "NAV FILE SUMMARY\n";
    separator();
    std::cout << "  RINEX version : " << nav.rinexVersion << "\n";
    std::cout << "  GPS  ephemeris: " << nav.gpsEph.size()  << " records\n";
    std::cout << "  GAL  ephemeris: " << nav.galEph.size()  << " records\n";
    std::cout << "  GLO  ephemeris: " << nav.gloEph.size()  << " records\n";
    std::cout << "  BDS  ephemeris: " << nav.bdsEph.size()  << " records\n";
    std::cout << "  SBAS ephemeris: " << nav.sbasEph.size() << " records\n";

    const auto& a = nav.ionoAlpha;
    const auto& b = nav.ionoBeta;
    if (a[0] != 0.0 || a[1] != 0.0) {
        std::cout << "  Klobuchar alpha: "
                  << a[0] << "  " << a[1] << "  "
                  << a[2] << "  " << a[3] << "\n";
        std::cout << "  Klobuchar beta : "
                  << b[0] << "  " << b[1] << "  "
                  << b[2] << "  " << b[3] << "\n";
    }

    // Unique GPS PRNs
    if (!nav.gpsEph.empty()) {
        std::map<int,bool> seen;
        std::cout << "  GPS PRNs: ";
        for (const auto& e : nav.gpsEph) {
            if (!seen[e.prn]) {
                std::cout << "G" << std::setw(2) << std::setfill('0')
                          << e.prn << " ";
                seen[e.prn] = true;
            }
        }
        std::cout << "\n";
    }
}

// =============================================================================
//  OBS summary
// =============================================================================
void printObsSummary(const loki::gnss::ObsFile& obs) {
    separator();
    std::cout << "OBS FILE SUMMARY\n";
    separator();
    std::cout << "  RINEX version : " << obs.rinexVersion  << "\n";
    std::cout << "  Station       : " << obs.receiver.markerName << "\n";
    std::cout << "  Receiver type : " << obs.receiver.receiverType << "\n";
    std::cout << "  Antenna type  : " << obs.receiver.antennaType << "\n";
    std::cout << "  Approx XYZ    : "
              << std::fixed << std::setprecision(3)
              << obs.receiver.approxX << "  "
              << obs.receiver.approxY << "  "
              << obs.receiver.approxZ << " [m]\n";
    std::cout << "  Interval      : " << obs.interval << " s\n";
    std::cout << "  Epochs        : " << obs.epochs.size() << "\n";

    if (obs.epochs.empty()) return;

    const auto t0 = obs.epochs.front().time.toTimeStamp();
    const auto t1 = obs.epochs.back().time.toTimeStamp();
    std::cout << "  Time span     : "
              << t0.utcString() << "  -->  " << t1.utcString() << "\n";

    std::size_t minSat = SIZE_MAX, maxSat = 0;
    double sumSat = 0.0;
    std::map<loki::gnss::GnssSystem, std::size_t> sysCounts;

    for (const auto& ep : obs.epochs) {
        const std::size_t n = ep.satellites.size();
        minSat  = std::min(minSat, n);
        maxSat  = std::max(maxSat, n);
        sumSat += static_cast<double>(n);
        for (const auto& sat : ep.satellites)
            sysCounts[sat.system]++;
    }

    std::cout << "  Satellites/epoch: min=" << minSat
              << "  max=" << maxSat
              << "  mean=" << std::fixed << std::setprecision(1)
              << sumSat / static_cast<double>(obs.epochs.size()) << "\n";
    std::cout << "  Per constellation:\n";
    for (const auto& [sys, cnt] : sysCounts)
        std::cout << "    " << std::setw(8) << std::left
                  << gnssSystemName(sys) << ": " << cnt << "\n";
}

// =============================================================================
//  SPP summary
// =============================================================================
void printSppSummary(const std::vector<loki::gnss::SppResult>& results,
                     const loki::GnssConfig& gcfg) {
    separator();
    std::cout << "SPP SUMMARY\n";
    separator();

    int nValid = 0;
    double sumX = 0, sumY = 0, sumZ = 0;

    for (const auto& r : results) {
        if (!r.valid) continue;
        ++nValid;
        sumX += r.x; sumY += r.y; sumZ += r.z;
    }

    std::cout << "  Valid epochs  : " << nValid << " / "
              << results.size() << "\n";

    if (nValid == 0) return;

    // Also accumulate clock bias
    double sumClk = 0.0;
    for (const auto& r : results) {
        if (!r.valid) continue;
        sumClk += r.clkBiasM;
    }
    const double meanClk = sumClk / nValid;
    std::cout << "  Mean clk bias : " << std::fixed << std::setprecision(3)
              << meanClk << " [m]  = "
              << meanClk / 299792458.0 * 1e6 << " [us]\n";

    const double meanX = sumX / nValid;
    const double meanY = sumY / nValid;
    const double meanZ = sumZ / nValid;
    std::cout << "  Mean ECEF X   : " << std::fixed << std::setprecision(3)
              << meanX << " [m]\n";
    std::cout << "  Mean ECEF Y   : " << meanY << " [m]\n";
    std::cout << "  Mean ECEF Z   : " << meanZ << " [m]\n";

    if (gcfg.referencePosition.enabled) {
        const double dX = meanX - gcfg.referencePosition.x;
        const double dY = meanY - gcfg.referencePosition.y;
        const double dZ = meanZ - gcfg.referencePosition.z;
        const double dR = std::sqrt(dX*dX + dY*dY + dZ*dZ);
        std::cout << "  3D error (vs " << gcfg.referencePosition.source
                  << "): " << std::setprecision(3) << dR << " [m]\n";
        std::cout << "  dX=" << dX << "  dY=" << dY << "  dZ=" << dZ
                  << " [m]\n";
    }
}

// =============================================================================
//  SPP CSV export
// =============================================================================
void exportSppCsv(const std::vector<loki::gnss::SppResult>& results,
                  const std::string& csvDir,
                  const std::string& station) {
    const std::string path = csvDir + "/gnss_" + station + "_spp.csv";
    std::ofstream ofs(path);
    ofs << "mjd;x;y;z;clk_bias_m;pdop;n_sats;converged\n";
    for (const auto& r : results) {
        if (!r.valid) continue;
        const double mjd = r.time.toTimeStamp().mjd();
        ofs << std::fixed << std::setprecision(8) << mjd << ";"
            << std::setprecision(4)
            << r.x << ";" << r.y << ";" << r.z << ";"
            << r.clkBiasM << ";"
            << std::setprecision(2) << r.pdop << ";"
            << r.nSats << ";"
            << (r.converged ? 1 : 0) << "\n";
    }
    LOKI_INFO("SPP CSV: " + path);
}

} // namespace

// =============================================================================
//  main
// =============================================================================
int main(int argc, char* argv[]) {
    const CliArgs args = parseArgs(argc, argv);
    if (args.showHelp)    { printHelp();    return EXIT_SUCCESS; }
    if (args.showVersion) {
        std::cout << "loki_gnss " << loki::VERSION_STRING << "\n";
        return EXIT_SUCCESS;
    }

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
    LOKI_INFO("Config:    " + args.configPath.string());
    LOKI_INFO("Workspace: " + cfg.workspace.string());
    LOKI_INFO("Task:      " + cfg.gnss.task);
    LOKI_INFO("Station:   " + cfg.gnss.station);

    fs::create_directories(cfg.logDir);
    fs::create_directories(cfg.csvDir);
    fs::create_directories(cfg.imgDir);
    fs::create_directories(cfg.protocolsDir);

    const loki::GnssConfig& gcfg = cfg.gnss;

    // ── Load NAV ─────────────────────────────────────────────────────────────
    loki::gnss::NavFile nav;
    try {
        loki::gnss::RinexNavParser navParser;
        LOKI_INFO("Parsing NAV: " + gcfg.navFile);
        nav = navParser.parseGz(gcfg.navFile);
        printNavSummary(nav);
    } catch (const loki::LOKIException& ex) {
        LOKI_ERROR(std::string("NAV parsing failed: ") + ex.what());
        return EXIT_FAILURE;
    }

    // ── Load OBS ─────────────────────────────────────────────────────────────
    loki::gnss::ObsFile obs;
    try {
        loki::gnss::RinexObsParser::Config obsCfg;
        obsCfg.crx2rnxPath = gcfg.crx2rnxPath;
        loki::gnss::RinexObsParser obsParser(obsCfg);
        LOKI_INFO("Parsing OBS: " + gcfg.obsFile);
        obs = obsParser.parseGz(gcfg.obsFile);
        printObsSummary(obs);
    } catch (const loki::LOKIException& ex) {
        LOKI_ERROR(std::string("OBS parsing failed: ") + ex.what());
        return EXIT_FAILURE;
    }

    // ── SPP ──────────────────────────────────────────────────────────────────
    std::vector<loki::gnss::SppResult> sppResults;

    if (gcfg.task == "spp" || gcfg.spp.enabled) {
        try {
            loki::gnss::SppSolver::Config solverCfg;
            solverCfg.maxIterations         = gcfg.spp.maxIterations;
            solverCfg.convergenceThresholdM = gcfg.spp.convergenceThresholdM;
            solverCfg.weighting             = gcfg.spp.weighting;
            solverCfg.ionosphere            = gcfg.corrections.ionosphere;
            solverCfg.troposphere           = gcfg.corrections.troposphere;
            solverCfg.sagnac                = gcfg.corrections.sagnac;
            solverCfg.elevMaskDeg           = gcfg.elevationMaskDeg;
            solverCfg.constellations        = gcfg.constellations;

            loki::gnss::SppSolver solver(solverCfg);
            LOKI_INFO("Running SPP for " + std::to_string(obs.epochs.size())
                      + " epochs...");
            sppResults = solver.solveAll(obs, nav, gcfg);

            printSppSummary(sppResults, gcfg);
            exportSppCsv(sppResults, cfg.csvDir.string(), gcfg.station);

        } catch (const loki::LOKIException& ex) {
            LOKI_ERROR(std::string("SPP failed: ") + ex.what());
        }
    }

    // ── PlotGnss (placeholder -- implemented in next iteration) ──────────────
    LOKI_INFO("Plots: PlotGnss will be implemented in the next iteration.");

    LOKI_INFO("loki_gnss finished successfully.");
    return EXIT_SUCCESS;
}
