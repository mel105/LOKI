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
#include "loki/arima/arimaAnalyzer.hpp"
#include "loki/arima/arimaForecaster.hpp"
#include "loki/arima/plotArima.hpp"

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
    std::cout << "loki_arima " << loki::VERSION_STRING << "\n";
}

static void printHelp()
{
    std::cout
        << "loki_arima " << loki::VERSION_STRING
        << " -- ARIMA/SARIMA modelling pipeline\n"
        << "\nUsage:\n"
        << "  loki_arima.exe <config.json> [options]\n"
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
    std::filesystem::path configPath  {"config/arima.json"};
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
            std::cerr << "[loki_arima] Unknown option: " << arg << "  (use --help)\n";
        }
    }
    return args;
}

// ----------------------------------------------------------------------------
//  Deseasonalize (same pattern as stationarity app)
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
//  Extract raw values from TimeSeries
// ----------------------------------------------------------------------------

static std::vector<double> extractValues(const loki::TimeSeries& ts)
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
//  Columns: mjd;original;residual;fitted;forecast;lower95;upper95
// ----------------------------------------------------------------------------

static void writeCsv(const std::filesystem::path&         csvDir,
                     const loki::TimeSeries&               ts,
                     const std::vector<double>&            residuals,
                     const std::vector<double>&            fitted,
                     const std::vector<double>&            forecastVals,
                     const std::vector<double>&            lower95,
                     const std::vector<double>&            upper95)
{
    const auto& m    = ts.metadata();
    std::string base = m.stationId;
    if (!m.componentName.empty()) {
        if (!base.empty()) base += "_";
        base += m.componentName;
    }
    if (base.empty()) base = "series";

    const std::filesystem::path outPath = csvDir / (base + "_arima.csv");
    std::ofstream csv(outPath);
    if (!csv.is_open()) {
        LOKI_WARNING("writeCsv: cannot open: " + outPath.string());
        return;
    }

    csv << "mjd;original;residual;fitted;forecast;lower95;upper95\n"
        << std::fixed << std::setprecision(10);

    constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
    const std::size_t n = ts.size();

    for (std::size_t i = 0; i < n; ++i) {
        const double orig  = ts[i].value;
        const double resid = (i < residuals.size())  ? residuals[i]  : NaN;
        const double fit   = (i < fitted.size())     ? fitted[i]     : NaN;
        csv << ts[i].time.mjd() << ";" << orig << ";"
            << resid << ";" << fit << ";nan;nan;nan\n";
    }

    // Append forecast rows beyond observed period
    for (std::size_t i = 0; i < forecastVals.size(); ++i) {
        // No MJD for forecast steps; use NaN for time
        csv << "nan;nan;nan;nan;"
            << forecastVals[i] << ";"
            << lower95[i] << ";"
            << upper95[i] << "\n";
    }

    LOKI_INFO("CSV written: " + outPath.string());
}

// ----------------------------------------------------------------------------
//  Protocol
// ----------------------------------------------------------------------------

static void writeProtocol(const std::filesystem::path&        protDir,
                           const loki::TimeSeries&              ts,
                           const loki::arima::ArimaResult&      result,
                           const loki::arima::ForecastResult&   fcResult,
                           const std::string&                   deseasonStrategy)
{
    std::filesystem::create_directories(protDir);

    const auto& m    = ts.metadata();
    std::string base = m.stationId;
    if (!m.componentName.empty()) {
        if (!base.empty()) base += "_";
        base += m.componentName;
    }
    if (base.empty()) base = "series";

    const std::filesystem::path outPath = protDir / (base + "_arima.txt");
    std::ofstream prot(outPath);
    if (!prot.is_open()) {
        LOKI_WARNING("writeProtocol: cannot open: " + outPath.string());
        return;
    }

    auto fmtD = [](double v) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6) << v;
        return oss.str();
    };

    prot << "=== LOKI ARIMA Protocol ===\n\n";
    prot << "Station:           " << m.stationId << "\n";
    prot << "Component:         " << m.componentName << "\n";
    prot << "Deseasonalization: " << deseasonStrategy << "\n\n";

    prot << "--- MODEL ORDER ---\n";
    prot << "ARIMA(" << result.order.p << "," << result.order.d << "," << result.order.q << ")";
    if (result.seasonal.s > 0) {
        prot << "(" << result.seasonal.P << "," << result.seasonal.D
             << "," << result.seasonal.Q << ")[" << result.seasonal.s << "]";
    }
    prot << "\n";
    prot << "Method: " << result.method << "\n";
    prot << "n (fit): " << result.n << "\n\n";

    prot << "--- COEFFICIENTS ---\n";
    prot << "Intercept: " << fmtD(result.intercept) << "\n";
    for (std::size_t i = 0; i < result.arCoeffs.size(); ++i) {
        prot << "AR(lag=" << result.arLags[i] << "): " << fmtD(result.arCoeffs[i]) << "\n";
    }
    for (std::size_t i = 0; i < result.maCoeffs.size(); ++i) {
        prot << "MA(lag=" << result.maLags[i] << "): " << fmtD(result.maCoeffs[i]) << "\n";
    }
    prot << "\n";

    prot << "--- DIAGNOSTICS ---\n";
    prot << "sigma2:  " << fmtD(result.sigma2) << "\n";
    prot << "logLik:  " << fmtD(result.logLik) << "\n";
    prot << "AIC:     " << fmtD(result.aic) << "\n";
    prot << "BIC:     " << fmtD(result.bic) << "\n\n";

    if (!fcResult.forecast.empty()) {
        prot << "--- FORECAST ---\n";
        prot << "Horizon: " << fcResult.horizon << " steps\n";
        for (int h = 0; h < fcResult.horizon; ++h) {
            prot << "h+" << (h + 1)
                 << ":  " << fmtD(fcResult.forecast[static_cast<std::size_t>(h)])
                 << "  [" << fmtD(fcResult.lower95[static_cast<std::size_t>(h)])
                 << ", "  << fmtD(fcResult.upper95[static_cast<std::size_t>(h)])
                 << "]\n";
        }
    }

    LOKI_INFO("Protocol written: " + outPath.string());
}

// ----------------------------------------------------------------------------
//  Log result to console
// ----------------------------------------------------------------------------

static void logResult(const loki::arima::ArimaResult& result)
{
    auto fmtD = [](double v) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6) << v;
        return oss.str();
    };

    LOKI_INFO("ARIMA(" + std::to_string(result.order.p)
              + "," + std::to_string(result.order.d)
              + "," + std::to_string(result.order.q) + ")"
              + (result.seasonal.s > 0
                  ? ("(" + std::to_string(result.seasonal.P)
                     + "," + std::to_string(result.seasonal.D)
                     + "," + std::to_string(result.seasonal.Q)
                     + ")[" + std::to_string(result.seasonal.s) + "]")
                  : "")
              + "  n=" + std::to_string(result.n)
              + "  sigma2=" + fmtD(result.sigma2)
              + "  AIC=" + fmtD(result.aic)
              + "  BIC=" + fmtD(result.bic));

    for (std::size_t i = 0; i < result.arCoeffs.size(); ++i) {
        LOKI_INFO("  AR(lag=" + std::to_string(result.arLags[i])
                  + ") = " + fmtD(result.arCoeffs[i]));
    }
    for (std::size_t i = 0; i < result.maCoeffs.size(); ++i) {
        LOKI_INFO("  MA(lag=" + std::to_string(result.maLags[i])
                  + ") = " + fmtD(result.maCoeffs[i]));
    }
}

// ----------------------------------------------------------------------------
//  Determine forecast horizon in samples
// ----------------------------------------------------------------------------

static int forecastHorizonSamples(double horizonDays,
                                   const loki::TimeSeries& ts)
{
    if (ts.size() < 2) { return 0; }
    // Estimate step from first two observations
    const double stepDays = ts[1].time.mjd() - ts[0].time.mjd();
    if (stepDays <= 0.0) { return 0; }
    return std::max(1, static_cast<int>(std::round(horizonDays / stepDays)));
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

    try { loki::Logger::initDefault(cfg.logDir, "loki_arima", cfg.output.logLevel); }
    catch (const loki::LOKIException& ex) {
        std::cerr << "[LOKI ERROR] Cannot init logger: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    LOKI_INFO("loki_arima " + std::string(loki::VERSION_STRING) + " started.");
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

    // Descriptive statistics on raw data
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

    const loki::ArimaConfig& arimaCfg = cfg.arima;
    loki::arima::ArimaAnalyzer analyzer(arimaCfg);

    for (const auto& r : loadResults) {
        for (const auto& ts : r.series) {
            const std::string name = ts.metadata().stationId.empty()
                ? ts.metadata().componentName
                : ts.metadata().stationId + "_" + ts.metadata().componentName;

            LOKI_INFO("--- Processing series: " + name
                      + "  n=" + std::to_string(ts.size()) + " ---");

            try {
                // Step 1: Gap filling
                loki::GapFiller::Config gfCfg;
                gfCfg.strategy      = loki::GapFiller::Strategy::LINEAR;
                gfCfg.maxFillLength = arimaCfg.gapFillMaxLength;
                loki::GapFiller gapFiller(gfCfg);
                const loki::TimeSeries filled = gapFiller.fill(ts);

                // Step 2: Deseasonalize
                const loki::Deseasonalizer::Result dsResult =
                    runDeseasonalize(filled, arimaCfg.deseasonalization);

                const std::vector<double>& residuals = dsResult.residuals;

                LOKI_INFO("Deseasonalization: strategy="
                          + arimaCfg.deseasonalization.strategy
                          + "  residuals n=" + std::to_string(residuals.size()));

                // Step 3: ARIMA analyze (differencing + order selection + fit + LjungBox)
                const loki::arima::ArimaResult arimaResult = analyzer.analyze(residuals);

                logResult(arimaResult);

                // Step 4: Forecast (optional)
                loki::arima::ForecastResult fcResult;
                if (arimaCfg.computeForecast && arimaCfg.forecastHorizon > 0.0) {
                    const int horizon = forecastHorizonSamples(
                        arimaCfg.forecastHorizon, filled);
                    if (horizon >= 1) {
                        loki::arima::ArimaForecaster forecaster(arimaResult);
                        fcResult = forecaster.forecast(horizon);
                        LOKI_INFO("Forecast: horizon=" + std::to_string(horizon)
                                  + " steps");
                    }
                }

                // Step 5: CSV
                try {
                    writeCsv(cfg.csvDir, ts,
                             arimaResult.residuals,
                             arimaResult.fitted,
                             fcResult.forecast,
                             fcResult.lower95,
                             fcResult.upper95);
                } catch (const loki::LOKIException& ex) {
                    LOKI_ERROR(std::string("CSV export failed: ") + ex.what());
                }

                // Step 6: Protocol
                try {
                    writeProtocol(cfg.protocolsDir, ts,
                                  arimaResult, fcResult,
                                  arimaCfg.deseasonalization.strategy);
                } catch (const loki::LOKIException& ex) {
                    LOKI_ERROR(std::string("Protocol failed: ") + ex.what());
                }

                // Step 7: Plots
                try {
                    // Generic plots on deseasonalized residuals via loki::Plot
                    loki::Plot corePlot(cfg);

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
                    if (cfg.plots.pacfPlot)   corePlot.pacf(resSeries);

                    // ARIMA-specific plots (overlay, forecast, diagnostics panel)
                    loki::arima::PlotArima arimaPlot(cfg);
                    arimaPlot.plotAll(filled, residuals, arimaResult, fcResult, arimaCfg.forecastTail);

                } catch (const loki::LOKIException& ex) {
                    LOKI_ERROR(std::string("Plotting failed: ") + ex.what());
                }

            } catch (const loki::LOKIException& ex) {
                LOKI_ERROR("ARIMA pipeline failed for " + name + ": " + ex.what());
                continue;
            }
        }
    }

    LOKI_INFO("loki_arima finished successfully.");
    return EXIT_SUCCESS;
}