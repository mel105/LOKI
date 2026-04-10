#include "loki/core/logger.hpp"
#include "loki/core/configLoader.hpp"
#include "loki/core/version.hpp"
#include "loki/io/dataManager.hpp"
#include "loki/io/plot.hpp"
#include "loki/stats/descriptive.hpp"
#include "loki/stats/hypothesis.hpp"
#include "loki/timeseries/deseasonalizer.hpp"
#include "loki/timeseries/gapFiller.hpp"
#include "loki/timeseries/medianYearSeries.hpp"
#include "loki/stationarity/stationarityAnalyzer.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <vector>

// ----------------------------------------------------------------------------
//  Help / version
// ----------------------------------------------------------------------------

static void printVersion()
{
    std::cout << "loki_stationarity " << loki::VERSION_STRING << "\n";
}

static void printHelp()
{
    std::cout
        << "loki_stationarity " << loki::VERSION_STRING
        << " -- Stationarity analysis pipeline (ADF, KPSS, PP)\n"
        << "\nUsage:\n"
        << "  loki_stationarity.exe <config.json> [options]\n"
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
    std::filesystem::path configPath  {"config/stationarity.json"};
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
            std::cerr << "[loki_stationarity] Unknown option: " << arg
                      << "  (use --help)\n";
        }
    }
    return args;
}

// ----------------------------------------------------------------------------
//  Deseasonalize -- identical pattern to outlier app
// ----------------------------------------------------------------------------

static loki::Deseasonalizer::Result
runDeseasonalize(const loki::TimeSeries&          ts,
                 const loki::DeseasonalizationConfig& dsCfg)
{
    loki::Deseasonalizer::Config cfg;
    cfg.maWindowSize = dsCfg.maWindowSize;

    if      (dsCfg.strategy == "median_year")    { cfg.strategy = loki::Deseasonalizer::Strategy::MEDIAN_YEAR;    }
    else if (dsCfg.strategy == "moving_average") { cfg.strategy = loki::Deseasonalizer::Strategy::MOVING_AVERAGE; }
    else                                         { cfg.strategy = loki::Deseasonalizer::Strategy::NONE;            }

    loki::Deseasonalizer::Result result;

    if (cfg.strategy == loki::Deseasonalizer::Strategy::NONE) {
        // No deseasonalization: residuals == original values.
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
//  Apply first-order or seasonal differencing
// ----------------------------------------------------------------------------

static std::vector<double>
applyDifferencing(const std::vector<double>& vals, int order)
{
    std::vector<double> d = vals;
    for (int pass = 0; pass < order; ++pass) {
        d = loki::stats::diff(d, loki::NanPolicy::SKIP);
    }
    return d;
}

// ----------------------------------------------------------------------------
//  Extract raw values from TimeSeries
// ----------------------------------------------------------------------------

static std::vector<double>
extractValues(const loki::TimeSeries& ts)
{
    std::vector<double> v;
    v.reserve(ts.size());
    for (std::size_t i = 0; i < ts.size(); ++i) {
        v.push_back(ts[i].value);
    }
    return v;
}

// ----------------------------------------------------------------------------
//  CSV export
//  Columns: mjd;original;residual;diff1;diff2
// ----------------------------------------------------------------------------

static void writeCsv(const std::filesystem::path&      csvDir,
                     const loki::TimeSeries&            ts,
                     const std::vector<double>&         residuals,
                     const std::vector<double>&         diff1,
                     const std::vector<double>&         diff2)
{
    const auto& m    = ts.metadata();
    std::string base = m.stationId;
    if (!m.componentName.empty()) {
        if (!base.empty()) base += "_";
        base += m.componentName;
    }
    if (base.empty()) base = "series";

    const std::filesystem::path outPath = csvDir / (base + "_stationarity.csv");
    std::ofstream csv(outPath);
    if (!csv.is_open()) {
        LOKI_WARNING("writeCsv: cannot open: " + outPath.string());
        return;
    }

    csv << "mjd;original;residual;diff1;diff2\n"
        << std::fixed << std::setprecision(10);

    const std::size_t n = ts.size();
    for (std::size_t i = 0; i < n; ++i) {
        const double orig  = ts[i].value;
        const double resid = (i < residuals.size())
            ? residuals[i] : std::numeric_limits<double>::quiet_NaN();
        const double d1 = (i < diff1.size())
            ? diff1[i] : std::numeric_limits<double>::quiet_NaN();
        const double d2 = (i < diff2.size())
            ? diff2[i] : std::numeric_limits<double>::quiet_NaN();

        csv << ts[i].time.mjd() << ";" << orig << ";"
            << resid << ";" << d1 << ";" << d2 << "\n";
    }
    LOKI_INFO("CSV written: " + outPath.string());
}

// ----------------------------------------------------------------------------
//  Protocol
// ----------------------------------------------------------------------------

static void writeProtocol(const std::filesystem::path&                protDir,
                          const loki::TimeSeries&                     ts,
                          const loki::stationarity::StationarityResult& result,
                          const std::string&                          deseasonStrategy,
                          bool                                        differencingApplied,
                          int                                         differencingOrder)
{
    std::filesystem::create_directories(protDir);

    const auto& m    = ts.metadata();
    std::string base = m.stationId;
    if (!m.componentName.empty()) {
        if (!base.empty()) base += "_";
        base += m.componentName;
    }
    if (base.empty()) base = "series";

    const std::filesystem::path outPath =
        protDir / ("stationarity_" + base + "_protocol.txt");
    std::ofstream prot(outPath);
    if (!prot.is_open()) {
        LOKI_WARNING("writeProtocol: cannot open: " + outPath.string());
        return;
    }

    auto fmtD = [](double v, int prec = 6) -> std::string {
        if (std::isnan(v)) return "       NaN";
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(prec) << v;
        return oss.str();
    };

    prot << "=======================================================\n"
         << "  LOKI -- Stationarity Analysis Protocol\n"
         << "=======================================================\n\n"
         << "Series:               " << base << "\n"
         << "Observations (n):     " << result.n << "\n"
         << "Deseasonalization:    " << deseasonStrategy << "\n"
         << "Differencing applied: " << (differencingApplied ? "yes" : "no") << "\n";
    if (differencingApplied) {
        prot << "Differencing order:   " << differencingOrder << "\n";
    }
    prot << "\n";

    // ADF
    if (result.adf.has_value()) {
        const auto& a = *result.adf;
        prot << "--- ADF Test (Augmented Dickey-Fuller) ---\n"
             << "  H0: unit root present (non-stationary)\n"
             << "  Trend type:   " << a.trendType << "\n"
             << "  Lags used:    " << a.lags << "\n"
             << "  Statistic:    " << fmtD(a.statistic) << "\n"
             << "  CV 1%:        " << fmtD(a.critVal1pct) << "\n"
             << "  CV 5%:        " << fmtD(a.critVal5pct) << "\n"
             << "  CV 10%:       " << fmtD(a.critVal10pct) << "\n"
             << "  Verdict:      " << (a.rejected ? "REJECT H0 (stationary)" : "fail to reject H0 (unit root)") << "\n\n";
    }

    // PP
    if (result.pp.has_value()) {
        const auto& p = *result.pp;
        prot << "--- PP Test (Phillips-Perron) ---\n"
             << "  H0: unit root present (non-stationary)\n"
             << "  Trend type:   " << p.trendType << "\n"
             << "  NW lags:      " << p.lags << "\n"
             << "  Statistic:    " << fmtD(p.statistic) << "\n"
             << "  CV 1%:        " << fmtD(p.critVal1pct) << "\n"
             << "  CV 5%:        " << fmtD(p.critVal5pct) << "\n"
             << "  CV 10%:       " << fmtD(p.critVal10pct) << "\n"
             << "  Verdict:      " << (p.rejected ? "REJECT H0 (stationary)" : "fail to reject H0 (unit root)") << "\n\n";
    }

    // KPSS
    if (result.kpss.has_value()) {
        const auto& k = *result.kpss;
        prot << "--- KPSS Test (Kwiatkowski-Phillips-Schmidt-Shin) ---\n"
             << "  H0: series is stationary\n"
             << "  Trend type:   " << k.trendType << "\n"
             << "  NW lags:      " << k.lags << "\n"
             << "  Statistic:    " << fmtD(k.statistic) << "\n"
             << "  CV 1%:        " << fmtD(k.critVal1pct) << "\n"
             << "  CV 5%:        " << fmtD(k.critVal5pct) << "\n"
             << "  CV 10%:       " << fmtD(k.critVal10pct) << "\n"
             << "  Verdict:      " << (k.rejected ? "REJECT H0 (non-stationary)" : "fail to reject H0 (stationary)") << "\n\n";
    }

    // Runs test
    if (result.runsTest.has_value()) {
        const auto& rt = *result.runsTest;
        prot << "--- Runs Test (supplementary) ---\n"
             << "  H0: series is random\n"
             << "  Statistic:    " << fmtD(rt.statistic) << "\n"
             << "  p-value:      " << fmtD(rt.pValue) << "\n"
             << "  Verdict:      " << (rt.rejected ? "REJECT H0 (serial dependence)" : "fail to reject H0 (random)") << "\n\n";
    }

    // Joint conclusion
    prot << "=======================================================\n"
         << "  JOINT CONCLUSION\n"
         << "=======================================================\n"
         << "  Stationary:           " << (result.isStationary ? "YES" : "NO") << "\n"
         << "  Recommended d:        " << result.recommendedDiff << "\n\n"
         << "  " << result.conclusion << "\n\n"
         << "=======================================================\n";

    LOKI_INFO("Protocol written: " + outPath.string());
}

// ----------------------------------------------------------------------------
//  Log test results
// ----------------------------------------------------------------------------

static void logResult(const loki::stationarity::StationarityResult& result)
{
    auto fmtD = [](double v) -> std::string {
        if (std::isnan(v)) return "NaN";
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(5) << v;
        return oss.str();
    };

    if (result.adf.has_value()) {
        const auto& a = *result.adf;
        LOKI_INFO("ADF(" + a.trendType + ", lags=" + std::to_string(a.lags)
                  + "): tau=" + fmtD(a.statistic)
                  + "  cv5%=" + fmtD(a.critVal5pct)
                  + (a.rejected ? "  -> REJECT H0" : "  -> fail to reject H0"));
    }
    if (result.pp.has_value()) {
        const auto& p = *result.pp;
        LOKI_INFO("PP(" + p.trendType + ", lags=" + std::to_string(p.lags)
                  + "): Zt=" + fmtD(p.statistic)
                  + "  cv5%=" + fmtD(p.critVal5pct)
                  + (p.rejected ? "  -> REJECT H0" : "  -> fail to reject H0"));
    }
    if (result.kpss.has_value()) {
        const auto& k = *result.kpss;
        LOKI_INFO("KPSS(" + k.trendType + ", lags=" + std::to_string(k.lags)
                  + "): eta=" + fmtD(k.statistic)
                  + "  cv5%=" + fmtD(k.critVal5pct)
                  + (k.rejected ? "  -> REJECT H0" : "  -> fail to reject H0"));
    }
    if (result.runsTest.has_value()) {
        const auto& rt = *result.runsTest;
        LOKI_INFO("Runs test: z=" + fmtD(rt.statistic)
                  + "  p=" + fmtD(rt.pValue)
                  + (rt.rejected ? "  -> REJECT H0" : "  -> fail to reject H0"));
    }
    LOKI_INFO("Conclusion: " + result.conclusion);
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

    try { loki::Logger::initDefault(cfg.logDir, "loki_stationarity", cfg.output.logLevel); }
    catch (const loki::LOKIException& ex) {
        std::cerr << "[LOKI ERROR] Cannot init logger: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    LOKI_INFO("loki_stationarity " + std::string(loki::VERSION_STRING) + " started.");
    LOKI_INFO("Config:    " + args.configPath.string());
    LOKI_INFO("Workspace: " + cfg.workspace.string());

    // Load data.
    std::vector<loki::LoadResult> loadResults;
    try {
        loki::DataManager dm(cfg);
        loadResults = dm.load();
        for (const auto& r : loadResults) {
            LOKI_INFO("Loaded: " + r.filePath.filename().string()
                      + "  lines=" + std::to_string(r.linesRead)
                      + "  skipped=" + std::to_string(r.linesSkipped));
            for (std::size_t i = 0; i < r.series.size(); ++i)
                LOKI_INFO("  Series[" + std::to_string(i) + "] " + r.columnNames[i]
                          + "  n=" + std::to_string(r.series[i].size()));
        }
    } catch (const loki::LOKIException& ex) {
        LOKI_ERROR(std::string("Data loading failed: ") + ex.what());
        return EXIT_FAILURE;
    }

    // Descriptive statistics on raw data.
    if (cfg.stats.enabled) {
        try {
            for (const auto& r : loadResults) {
                for (const auto& ts : r.series) {
                    const std::vector<double> vals = extractValues(ts);
                    LOKI_INFO(loki::stats::formatSummary(
                        loki::stats::summarize(vals, cfg.stats.nanPolicy, cfg.stats.hurst),
                        ts.metadata().componentName));
                }
            }
        } catch (const loki::LOKIException& ex) {
            LOKI_ERROR(std::string("Statistics failed: ") + ex.what());
        }
    }

    const loki::StationarityConfig& stCfg = cfg.stationarity;
    loki::stationarity::StationarityAnalyzer analyzer(stCfg);

    for (const auto& r : loadResults) {
        for (const auto& ts : r.series) {
            const std::string name = ts.metadata().stationId.empty()
                ? ts.metadata().componentName
                : ts.metadata().stationId + "_" + ts.metadata().componentName;

            LOKI_INFO("--- Processing series: " + name
                      + "  n=" + std::to_string(ts.size()) + " ---");

            try {
                // Step 1: Gap filling.
                loki::GapFiller::Config gfCfg;
                {
                    const std::string& s = stCfg.gapFillStrategy;
                    if      (s == "forward_fill") gfCfg.strategy = loki::GapFiller::Strategy::FORWARD_FILL;
                    else if (s == "mean")         gfCfg.strategy = loki::GapFiller::Strategy::MEAN;
                    else if (s == "spline")       gfCfg.strategy = loki::GapFiller::Strategy::SPLINE;
                    else if (s == "none")         gfCfg.strategy = loki::GapFiller::Strategy::NONE;
                    else                          gfCfg.strategy = loki::GapFiller::Strategy::LINEAR;
                }
                gfCfg.maxFillLength = static_cast<std::size_t>(
                    std::max(0, stCfg.gapFillMaxLength));
                loki::GapFiller gapFiller(gfCfg);
                const loki::TimeSeries filled = gapFiller.fill(ts);

                // Step 2: Deseasonalize.
                const loki::Deseasonalizer::Result dsResult =
                    runDeseasonalize(filled, stCfg.deseasonalization);

                const std::vector<double>& residuals = dsResult.residuals;

                LOKI_INFO("Deseasonalization: strategy="
                          + stCfg.deseasonalization.strategy
                          + "  residuals n=" + std::to_string(residuals.size()));

                // Step 3: Optional differencing on residuals.
                std::vector<double> analysisVals = residuals;
                if (stCfg.differencing.apply && stCfg.differencing.order >= 1) {
                    analysisVals = applyDifferencing(residuals, stCfg.differencing.order);
                    LOKI_INFO("Differencing applied: order=" +
                              std::to_string(stCfg.differencing.order) +
                              "  n after=" + std::to_string(analysisVals.size()));
                }

                // Precompute diff1 and diff2 for CSV export.
                std::vector<double> diff1, diff2;
                try {
                    diff1 = loki::stats::diff(residuals, loki::NanPolicy::SKIP);
                } catch (const loki::LOKIException&) {}
                try {
                    if (!diff1.empty())
                        diff2 = loki::stats::diff(diff1, loki::NanPolicy::SKIP);
                } catch (const loki::LOKIException&) {}

                // Step 4: Stationarity tests.
                if (analysisVals.size() < 10) {
                    LOKI_WARNING("Series too short for stationarity tests after preprocessing (n="
                                 + std::to_string(analysisVals.size()) + "). Skipping.");
                    continue;
                }

                const loki::stationarity::StationarityResult stResult =
                    analyzer.analyze(analysisVals);

                logResult(stResult);

                // Step 5: CSV.
                try {
                    writeCsv(cfg.csvDir, ts, residuals, diff1, diff2);
                } catch (const loki::LOKIException& ex) {
                    LOKI_ERROR(std::string("CSV export failed: ") + ex.what());
                }

                // Step 6: Protocol.
                try {
                    writeProtocol(cfg.protocolsDir, ts, stResult,
                                  stCfg.deseasonalization.strategy,
                                  stCfg.differencing.apply,
                                  stCfg.differencing.order);
                } catch (const loki::LOKIException& ex) {
                    LOKI_ERROR(std::string("Protocol failed: ") + ex.what());
                }

                // Step 7: Plots on residuals.
                try {
                    loki::Plot corePlot(cfg);
                    // Build a residual TimeSeries for the plot functions.
                    loki::SeriesMetadata resMeta = ts.metadata();
                    resMeta.componentName += "_residuals";
                    loki::TimeSeries resSeries(resMeta);
                    resSeries.reserve(filled.size());
                    for (std::size_t i = 0; i < filled.size(); ++i) {
                        const double v = (i < residuals.size())
                            ? residuals[i] : std::numeric_limits<double>::quiet_NaN();
                        resSeries.append(filled[i].time, v, filled[i].flag);
                    }

                    if (cfg.plots.timeSeries) corePlot.timeSeries(resSeries);
                    if (cfg.plots.acf)        corePlot.acf(resSeries);
                    if (cfg.plots.histogram)  corePlot.histogram(resSeries);
                    if (cfg.plots.qqPlot)     corePlot.qqPlot(resSeries);

                    // PACF plot (new -- stationarity-specific).
                    if (cfg.plots.pacfPlot) {
                        corePlot.pacf(resSeries);
                    }

                } catch (const loki::LOKIException& ex) {
                    LOKI_ERROR(std::string("Plotting failed: ") + ex.what());
                }

            } catch (const loki::LOKIException& ex) {
                LOKI_ERROR("Stationarity pipeline failed for " + name + ": " + ex.what());
                continue;
            }
        }
    }

    LOKI_INFO("loki_stationarity finished successfully.");
    return EXIT_SUCCESS;
}