#include <loki/gnss/gnssTypes.hpp>
#include <loki/gnss/rinexNavParser.hpp>
#include <loki/gnss/rinexObsParser.hpp>
#include <loki/core/config.hpp>
#include <loki/core/configLoader.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/core/version.hpp>
#include <loki/io/gnuplot.hpp>

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
        << "  parse  -- Parse NAV + OBS files, produce summary and plots.\n"
        << "  spp    -- Single Point Positioning (broadcast ephemeris).\n"
        << "  ppp    -- Precise Point Positioning (SP3 + CLK).\n";
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
        else {
            std::cerr << "[loki_gnss] Unknown option: " << arg
                      << "  (use --help)\n";
        }
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
    if (nav.nequickAi[0] != 0.0) {
        std::cout << "  NeQuick ai     : "
                  << nav.nequickAi[0] << "  "
                  << nav.nequickAi[1] << "  "
                  << nav.nequickAi[2] << "\n";
    }

    // Unique GPS PRNs
    if (!nav.gpsEph.empty()) {
        std::map<int, bool> seen;
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
              << t0.utcString() << "  -->  "
              << t1.utcString() << "\n";

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

    std::cout << "  Observations per constellation:\n";
    for (const auto& [sys, cnt] : sysCounts)
        std::cout << "    " << std::setw(8) << std::left
                  << gnssSystemName(sys) << ": " << cnt << "\n";

    // First epoch detail
    separator();
    std::cout << "FIRST EPOCH DETAIL\n";
    separator();
    const auto& ep0 = obs.epochs.front();
    std::cout << "  Time  : " << ep0.time.toTimeStamp().utcString() << "\n";
    std::cout << "  nSat  : " << ep0.satellites.size() << "\n";
    for (const auto& sat : ep0.satellites) {
        std::cout << "    " << gnssSystemName(sat.system)
                  << std::setw(2) << std::setfill('0') << sat.prn
                  << std::setfill(' ') << "  [";
        bool first = true;
        int shown = 0;
        for (const auto& [code, ov] : sat.obs) {
            if (!first) std::cout << ", ";
            std::cout << code << "=" << std::fixed
                      << std::setprecision(3) << ov.value;
            first = false;
            if (++shown >= 4) { std::cout << ", ..."; break; }
        }
        std::cout << "]\n";
    }
}

// =============================================================================
//  Satellite count plot
// =============================================================================
void plotSatCount(const loki::gnss::ObsFile& obs,
                  const std::string& imgDir,
                  const std::string& csvDir,
                  const std::string& station) {
    using G = loki::gnss::GnssSystem;

    struct Row {
        double mjd{0.0};
        int nGps{0}, nGal{0}, nGlo{0}, nBds{0}, nTotal{0};
    };
    std::vector<Row> rows;
    rows.reserve(obs.epochs.size());

    for (const auto& ep : obs.epochs) {
        Row r;
        r.mjd = ep.time.toTimeStamp().mjd();
        for (const auto& sat : ep.satellites) {
            ++r.nTotal;
            switch (sat.system) {
                case G::GPS:     ++r.nGps; break;
                case G::GALILEO: ++r.nGal; break;
                case G::GLONASS: ++r.nGlo; break;
                case G::BEIDOU:  ++r.nBds; break;
                default: break;
            }
        }
        rows.push_back(r);
    }

    // CSV
    const std::string csvPath = csvDir + "/gnss_" + station + "_satcount.csv";
    {
        std::ofstream ofs(csvPath);
        ofs << "mjd;gps;galileo;glonass;beidou;total\n";
        for (const auto& r : rows)
            ofs << std::fixed << std::setprecision(8) << r.mjd << ";"
                << r.nGps << ";" << r.nGal << ";"
                << r.nGlo << ";" << r.nBds << ";" << r.nTotal << "\n";
    }
    LOKI_INFO("CSV: " + csvPath);

    // gnuplot
    const std::string pngPath =
        fwdSlash(imgDir + "/gnss_" + station + "_satcount.png");

    loki::Gnuplot gp;
    std::string datablock = "$data << EOD\n";
    for (const auto& r : rows)
        datablock += std::to_string(r.mjd) + " "
                   + std::to_string(r.nTotal) + " "
                   + std::to_string(r.nGps)   + " "
                   + std::to_string(r.nGal)   + " "
                   + std::to_string(r.nGlo)   + " "
                   + std::to_string(r.nBds)   + "\n";
    datablock += "EOD";
    gp(datablock);

    gp("set terminal pngcairo noenhanced font 'Sans,12' size 1200,500");
    gp("set output '" + pngPath + "'");
    gp("set title 'Visible satellites per epoch -- " + station + "'");
    gp("set xlabel 'MJD'");
    gp("set ylabel 'Number of satellites'");
    gp("set key top right");
    gp("set grid");
    gp("set yrange [0:*]");
    gp("plot $data u 1:2 w l lw 2 lc rgb '#222222' title 'Total', "
       "     $data u 1:3 w l lw 1 lc rgb '#1f77b4' title 'GPS', "
       "     $data u 1:4 w l lw 1 lc rgb '#ff7f0e' title 'Galileo', "
       "     $data u 1:5 w l lw 1 lc rgb '#2ca02c' title 'GLONASS', "
       "     $data u 1:6 w l lw 1 lc rgb '#d62728' title 'BeiDou'");
    gp("unset output");

    LOKI_INFO("PNG: " + pngPath);
}

} // namespace

// =============================================================================
//  main
// =============================================================================
int main(int argc, char* argv[]) {
    const CliArgs args = parseArgs(argc, argv);
    if (args.showHelp)    { printHelp();
                           std::cout << "loki_gnss "
                                     << loki::VERSION_STRING << "\n";
                           return EXIT_SUCCESS; }
    if (args.showVersion) { std::cout << "loki_gnss "
                                      << loki::VERSION_STRING << "\n";
                            return EXIT_SUCCESS; }

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
        obsCfg.verbose     = false;
        loki::gnss::RinexObsParser obsParser(obsCfg);
        LOKI_INFO("Parsing OBS: " + gcfg.obsFile);
        obs = obsParser.parseGz(gcfg.obsFile);
        printObsSummary(obs);
    } catch (const loki::LOKIException& ex) {
        LOKI_ERROR(std::string("OBS parsing failed: ") + ex.what());
        return EXIT_FAILURE;
    }

    // ── Plots ─────────────────────────────────────────────────────────────────
    try {
        plotSatCount(obs,
                     cfg.imgDir.string(),
                     cfg.csvDir.string(),
                     gcfg.station);
    } catch (const loki::LOKIException& ex) {
        LOKI_WARNING(std::string("Plot failed: ") + ex.what());
    }

    // ── SPP (placeholder) ────────────────────────────────────────────────────
    if (gcfg.task == "spp" || gcfg.spp.enabled) {
        LOKI_WARNING("SPP not yet implemented -- coming in next iteration.");
    }

    LOKI_INFO("loki_gnss finished successfully.");
    return EXIT_SUCCESS;
}
