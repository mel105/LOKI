#include "loki/core/logger.hpp"
#include "loki/core/configLoader.hpp"
#include "loki/core/version.hpp"
#include "loki/io/dataManager.hpp"
#include "loki/stats/descriptive.hpp"
#include "loki/timeseries/deseasonalizer.hpp"
#include "loki/timeseries/gapFiller.hpp"
#include "loki/timeseries/medianYearSeries.hpp"
#include "loki/ssa/ssaAnalyzer.hpp"
#include "loki/ssa/plotSsa.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string_view>
#include <vector>

// ----------------------------------------------------------------------------
//  Help / version
// ----------------------------------------------------------------------------

static void printVersion()
{
    std::cout << "loki_ssa " << loki::VERSION_STRING << "\n";
}

static void printHelp()
{
    std::cout
        << "loki_ssa " << loki::VERSION_STRING
        << " -- Singular Spectrum Analysis pipeline\n"
        << "\nUsage:\n"
        << "  loki_ssa.exe <config.json> [options]\n"
        << "\nOptions:\n"
        << "  --help     Show this message and exit.\n"
        << "  --version  Show version string and exit.\n";
}

// ----------------------------------------------------------------------------
//  CLI
// ----------------------------------------------------------------------------

struct CliArgs {
    bool                  showHelp    {false};
    bool                  showVersion {false};
    std::filesystem::path configPath  {"config/ssa.json"};
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
            std::cerr << "[loki_ssa] Unknown option: " << arg << "  (use --help)\n";
        }
    }
    return args;
}

// ----------------------------------------------------------------------------
//  Deseasonalize (same pattern as loki_arima)
// ----------------------------------------------------------------------------

static loki::Deseasonalizer::Result
runDeseasonalize(const loki::TimeSeries&              ts,
                 const loki::DeseasonalizationConfig& dsCfg)
{
    loki::Deseasonalizer::Config cfg;
    cfg.maWindowSize = dsCfg.maWindowSize;

    if      (dsCfg.strategy == "median_year")    { cfg.strategy = loki::Deseasonalizer::Strategy::MEDIAN_YEAR;    }
    else if (dsCfg.strategy == "moving_average") { cfg.strategy = loki::Deseasonalizer::Strategy::MOVING_AVERAGE; }
    else                                          { cfg.strategy = loki::Deseasonalizer::Strategy::NONE;           }

    loki::Deseasonalizer::Result result;

    if (cfg.strategy == loki::Deseasonalizer::Strategy::NONE) {
        result.series    = ts;
        result.seasonal  = std::vector<double>(ts.size(), 0.0);
        result.residuals = std::vector<double>(ts.size());
        for (std::size_t i = 0; i < ts.size(); ++i) {
            result.residuals[i] = ts[i].value;
        }
        return result;
    }

    loki::Deseasonalizer ds(cfg);

    if (cfg.strategy == loki::Deseasonalizer::Strategy::MEDIAN_YEAR) {
        loki::MedianYearSeries::Config myCfg;
        myCfg.minYears = dsCfg.medianYearMinYears;
        const loki::MedianYearSeries profile(ts, myCfg);
        auto lookup = [&profile](const ::TimeStamp& t) { return profile.valueAt(t); };
        result = ds.deseasonalize(ts, lookup);
    } else {
        result = ds.deseasonalize(ts);
    }

    return result;
}

// ----------------------------------------------------------------------------
//  CSV export
//  Columns: mjd;original;trend;noise;[per-group...]
// ----------------------------------------------------------------------------

static void writeCsv(const std::filesystem::path&   csvDir,
                     const loki::TimeSeries&         ts,
                     const loki::ssa::SsaResult&     result)
{
    const auto& m    = ts.metadata();
    std::string base = m.stationId;
    if (!m.componentName.empty()) {
        if (!base.empty()) base += "_";
        base += m.componentName;
    }
    if (base.empty()) base = "series";

    const std::filesystem::path outPath = csvDir / (base + "_ssa.csv");
    std::ofstream csv(outPath);
    if (!csv.is_open()) {
        LOKI_WARNING("writeCsv: cannot open: " + outPath.string());
        return;
    }

    // Collect extra groups (not trend, not noise) in order
    std::vector<const loki::ssa::SsaGroup*> extraGroups;
    for (const auto& g : result.groups) {
        if (g.name != "trend" && g.name != "noise") {
            extraGroups.push_back(&g);
        }
    }

    // Header
    csv << "mjd;original;trend;noise";
    for (const auto* g : extraGroups) csv << ";" << g->name;
    csv << "\n";

    csv << std::fixed << std::setprecision(10);

    constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
    const std::size_t n = ts.size();

    auto safeVal = [&](const std::vector<double>& v, std::size_t k) -> double {
        return (k < v.size()) ? v[k] : NaN;
    };

    for (std::size_t i = 0; i < n; ++i) {
        csv << ts[i].time.mjd() << ";"
            << ts[i].value << ";"
            << safeVal(result.trend, i) << ";"
            << safeVal(result.noise, i);
        for (const auto* g : extraGroups) {
            csv << ";" << safeVal(g->reconstruction, i);
        }
        csv << "\n";
    }

    LOKI_INFO("CSV written: " + outPath.string());
}

// ----------------------------------------------------------------------------
//  Protocol
// ----------------------------------------------------------------------------

static void writeProtocol(const std::filesystem::path& protDir,
                           const loki::TimeSeries&       ts,
                           const loki::ssa::SsaResult&   result)
{
    std::filesystem::create_directories(protDir);

    const auto& m    = ts.metadata();
    std::string base = m.stationId;
    if (!m.componentName.empty()) {
        if (!base.empty()) base += "_";
        base += m.componentName;
    }
    if (base.empty()) base = "series";

    const std::filesystem::path outPath = protDir / (base + "_ssa.txt");
    std::ofstream prot(outPath);
    if (!prot.is_open()) {
        LOKI_WARNING("writeProtocol: cannot open: " + outPath.string());
        return;
    }

    auto line = [&](const std::string& s) { prot << s << "\n"; };
    auto fmtD = [](double v) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6) << v;
        return oss.str();
    };

    line("============================================================");
    line("LOKI SSA PROTOCOL");
    line("============================================================");
    line("Series  : " + base);
    line("n       : " + std::to_string(result.n));
    line("L       : " + std::to_string(result.L));
    line("K       : " + std::to_string(result.K));
    line("Method  : " + result.groupingMethod);
    line("");

    line("--- EIGENVALUES (first 20) ---");
    const int nEig = std::min(static_cast<int>(result.eigenvalues.size()), 20);
    double cumVar = 0.0;
    for (int i = 0; i < nEig; ++i) {
        cumVar += result.varianceFractions[static_cast<std::size_t>(i)];
        std::ostringstream oss;
        oss << "  [" << std::setw(3) << i << "]"
            << "  eigenvalue=" << fmtD(result.eigenvalues[static_cast<std::size_t>(i)])
            << "  varFrac=" << fmtD(result.varianceFractions[static_cast<std::size_t>(i)])
            << "  cumul=" << fmtD(cumVar);
        line(oss.str());
    }
    line("");

    line("--- GROUPS ---");
    for (const auto& g : result.groups) {
        std::ostringstream oss;
        oss << "  " << g.name
            << "  varFrac=" << fmtD(g.varianceFraction)
            << "  indices=[";
        for (std::size_t j = 0; j < g.indices.size(); ++j) {
            if (j > 0) oss << ",";
            oss << g.indices[j];
        }
        oss << "]";
        line(oss.str());
    }
    line("");
    line("============================================================");

    LOKI_INFO("Protocol written: " + outPath.string());
}

// ----------------------------------------------------------------------------
//  Log result summary
// ----------------------------------------------------------------------------

static void logResult(const loki::ssa::SsaResult& result)
{
    LOKI_INFO("SSA result: n=" + std::to_string(result.n)
              + "  L=" + std::to_string(result.L)
              + "  K=" + std::to_string(result.K)
              + "  r=" + std::to_string(result.eigenvalues.size())
              + "  groups=" + std::to_string(result.groups.size())
              + "  method=" + result.groupingMethod);

    double cumVar = 0.0;
    for (std::size_t i = 0;
         i < std::min(result.varianceFractions.size(), std::size_t{10});
         ++i)
    {
        cumVar += result.varianceFractions[i];
    }
    LOKI_INFO("  cumulative variance (first 10 eigentriples): "
              + std::to_string(cumVar * 100.0) + "%");
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
    try { cfg = loki::ConfigLoader::load(args.configPath); }
    catch (const loki::LOKIException& ex) {
        std::cerr << "[LOKI ERROR] " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    try { loki::Logger::initDefault(cfg.logDir, "loki_ssa", cfg.output.logLevel); }
    catch (const loki::LOKIException& ex) {
        std::cerr << "[LOKI ERROR] Cannot init logger: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    LOKI_INFO("loki_ssa " + std::string(loki::VERSION_STRING) + " started.");
    LOKI_INFO("Config:    " + args.configPath.string());
    LOKI_INFO("Workspace: " + cfg.workspace.string());

    // Load data
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

    // Descriptive statistics
    if (cfg.stats.enabled) {
        try {
            for (const auto& r : loadResults) {
                for (const auto& ts : r.series) {
                    std::vector<double> vals;
                    vals.reserve(ts.size());
                    for (std::size_t i = 0; i < ts.size(); ++i) {
                        vals.push_back(ts[i].value);
                    }
                    LOKI_INFO(loki::stats::formatSummary(
                        loki::stats::summarize(vals, cfg.stats.nanPolicy, cfg.stats.hurst),
                        ts.metadata().componentName));
                }
            }
        } catch (const loki::LOKIException& ex) {
            LOKI_ERROR(std::string("Statistics failed: ") + ex.what());
        }
    }

    const loki::SsaConfig& ssaCfg = cfg.ssa;
    loki::ssa::SsaAnalyzer analyzer(cfg);

    for (const auto& r : loadResults) {
        for (const auto& ts : r.series) {
            const std::string name =
                ts.metadata().stationId.empty()
                ? ts.metadata().componentName
                : ts.metadata().stationId + "_" + ts.metadata().componentName;

            LOKI_INFO("--- Processing series: " + name
                      + "  n=" + std::to_string(ts.size()) + " ---");

            try {
                // Step 1: Gap filling
                loki::GapFiller::Config gfCfg;
                gfCfg.strategy      = loki::GapFiller::Strategy::LINEAR;
                gfCfg.maxFillLength = ssaCfg.gapFillMaxLength;
                loki::GapFiller gapFiller(gfCfg);
                const loki::TimeSeries filled = gapFiller.fill(ts);

                // Step 2: Deseasonalize (usually "none" for SSA)
                const loki::Deseasonalizer::Result dsResult =
                    runDeseasonalize(filled, ssaCfg.deseasonalization);
                const std::vector<double>& values = dsResult.residuals;

                LOKI_INFO("Deseasonalization: strategy="
                          + ssaCfg.deseasonalization.strategy
                          + "  n=" + std::to_string(values.size()));

                // Step 3: SSA analysis
                const loki::ssa::SsaResult ssaResult = analyzer.analyze(values);
                logResult(ssaResult);

                // Step 4: CSV
                try {
                    std::filesystem::create_directories(cfg.csvDir);
                    writeCsv(cfg.csvDir, filled, ssaResult);
                } catch (const loki::LOKIException& ex) {
                    LOKI_ERROR(std::string("CSV export failed: ") + ex.what());
                }

                // Step 5: Protocol
                try {
                    writeProtocol(cfg.protocolsDir, filled, ssaResult);
                } catch (const loki::LOKIException& ex) {
                    LOKI_ERROR(std::string("Protocol failed: ") + ex.what());
                }

                // Step 6: Plots
                try {
                    std::filesystem::create_directories(cfg.imgDir);
                    loki::ssa::PlotSsa ssaPlot(cfg);
                    ssaPlot.plotAll(filled, ssaResult);
                } catch (const loki::LOKIException& ex) {
                    LOKI_ERROR(std::string("Plotting failed: ") + ex.what());
                }

            } catch (const loki::LOKIException& ex) {
                LOKI_ERROR("SSA pipeline failed for " + name + ": " + ex.what());
                continue;
            }
        }
    }

    LOKI_INFO("loki_ssa finished successfully.");
    return EXIT_SUCCESS;
}
