#include "loki/core/logger.hpp"
#include "loki/core/configLoader.hpp"
#include "loki/core/version.hpp"
#include "loki/io/dataManager.hpp"
#include "loki/io/plot.hpp"
#include "loki/stats/descriptive.hpp"
#include "loki/timeseries/deseasonalizer.hpp"
#include "loki/timeseries/gapFiller.hpp"
#include "loki/timeseries/medianYearSeries.hpp"
#include "loki/outlier/plotOutlier.hpp"
#include "loki/outlier/outlierCleaner.hpp"
#include "loki/outlier/outlierResult.hpp"
#include "loki/outlier/iqrDetector.hpp"
#include "loki/outlier/madDetector.hpp"
#include "loki/outlier/zScoreDetector.hpp"
#include "loki/outlier/hatMatrixDetector.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <string_view>

// ----------------------------------------------------------------------------
//  Help / version
// ----------------------------------------------------------------------------

static void printVersion()
{
    std::cout << "loki_outlier " << loki::VERSION_STRING << "\n";
}

static void printHelp()
{
    std::cout
        << "loki_outlier " << loki::VERSION_STRING
        << " -- Outlier detection and removal pipeline\n"
        << "\nUsage:\n"
        << "  loki_outlier.exe <config.json> [options]\n"
        << "\nOptions:\n"
        << "  --help     Show this message and exit.\n"
        << "  --version  Show version string and exit.\n";
}

// ----------------------------------------------------------------------------
//  CLI
// ----------------------------------------------------------------------------

struct CliArgs {
    bool                  showHelp   {false};
    bool                  showVersion{false};
    std::filesystem::path configPath {"config/outlier.json"};
};

static CliArgs parseArgs(int argc, char* argv[])
{
    CliArgs args;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if      (arg == "--help"    || arg == "-h") { args.showHelp    = true; }
        else if (arg == "--version" || arg == "-v") { args.showVersion = true; }
        else if (arg[0] != '-') { args.configPath = argv[i]; }
        else { std::cerr << "[loki_outlier] Unknown option: " << arg << "  (use --help)\n"; }
    }
    return args;
}

// ----------------------------------------------------------------------------
//  Build detector (O1-O3 methods)
// ----------------------------------------------------------------------------

static std::unique_ptr<loki::outlier::OutlierDetector>
buildDetector(const loki::OutlierConfig::DetectionSection& det)
{
    const std::string& m = det.method;
    if (m == "iqr")
        return std::make_unique<loki::outlier::IqrDetector>(det.iqrMultiplier);
    if (m == "mad" || m == "mad_bounds")
        return std::make_unique<loki::outlier::MadDetector>(det.madMultiplier);
    if (m == "zscore")
        return std::make_unique<loki::outlier::ZScoreDetector>(det.zscoreThreshold);
    if (m == "hat_matrix") return nullptr;
    LOKI_WARNING("outlier.detection.method '" + m + "' unknown -- falling back to 'mad'.");
    return std::make_unique<loki::outlier::MadDetector>(det.madMultiplier);
}

// ----------------------------------------------------------------------------
//  Build OutlierCleaner::Config
// ----------------------------------------------------------------------------

static loki::outlier::OutlierCleaner::Config
buildCleanerConfig(const loki::OutlierConfig& cfg)
{
    loki::outlier::OutlierCleaner::Config c;
    const std::string& rs = cfg.replacement.strategy;
    if      (rs == "forward_fill") { c.fillStrategy = loki::GapFiller::Strategy::FORWARD_FILL; }
    else if (rs == "mean")         { c.fillStrategy = loki::GapFiller::Strategy::MEAN;         }
    else if (rs == "spline")       { c.fillStrategy = loki::GapFiller::Strategy::SPLINE;       }
    else                           { c.fillStrategy = loki::GapFiller::Strategy::LINEAR;       }
    c.maxFillLength = static_cast<std::size_t>(std::max(0, cfg.replacement.maxFillLength));
    return c;
}

// ----------------------------------------------------------------------------
//  Build GapFiller::Strategy from replacement config string
// ----------------------------------------------------------------------------

static loki::GapFiller::Strategy
replacementStrategy(const loki::OutlierConfig& cfg)
{
    const std::string& rs = cfg.replacement.strategy;
    if (rs == "forward_fill") return loki::GapFiller::Strategy::FORWARD_FILL;
    if (rs == "mean")         return loki::GapFiller::Strategy::MEAN;
    if (rs == "spline")       return loki::GapFiller::Strategy::SPLINE;
    return loki::GapFiller::Strategy::LINEAR;
}

// ----------------------------------------------------------------------------
//  Deseasonalize
// ----------------------------------------------------------------------------

struct DeseasonalizeResult {
    std::vector<double> seasonal;
    bool                hasComponent;
};

static DeseasonalizeResult
runDeseasonalize(const loki::TimeSeries&                              ts,
                 const loki::OutlierConfig::DeseasonalizationSection& dsCfg,
                 loki::Deseasonalizer::Result&                        dsResult)
{
    loki::Deseasonalizer::Config cfg;
    cfg.maWindowSize = dsCfg.maWindowSize;

    if      (dsCfg.strategy == "median_year")    { cfg.strategy = loki::Deseasonalizer::Strategy::MEDIAN_YEAR;    }
    else if (dsCfg.strategy == "moving_average") { cfg.strategy = loki::Deseasonalizer::Strategy::MOVING_AVERAGE; }
    else                                         { cfg.strategy = loki::Deseasonalizer::Strategy::NONE;            }

    if (cfg.strategy == loki::Deseasonalizer::Strategy::NONE) {
        dsResult.residuals = {};
        dsResult.seasonal  = std::vector<double>(ts.size(), 0.0);
        dsResult.series    = ts;
        return {std::vector<double>(ts.size(), 0.0), false};
    }

    loki::Deseasonalizer ds(cfg);

    if (cfg.strategy == loki::Deseasonalizer::Strategy::MEDIAN_YEAR) {
        loki::MedianYearSeries::Config myCfg;
        myCfg.minYears = dsCfg.medianYearMinYears;
        const loki::MedianYearSeries profile(ts, myCfg);
        auto lookup = [&profile](const ::TimeStamp& t) { return profile.valueAt(t); };
        dsResult = ds.deseasonalize(ts, lookup);
    } else {
        dsResult = ds.deseasonalize(ts);
    }

    return {dsResult.seasonal, true};
}

// ----------------------------------------------------------------------------
//  Build residual TimeSeries: values = original - seasonal, suffix "_residuals"
// ----------------------------------------------------------------------------

static loki::TimeSeries
buildResidualSeries(const loki::TimeSeries&    original,
                    const std::vector<double>& seasonal)
{
    loki::SeriesMetadata meta = original.metadata();
    meta.componentName += "_residuals";
    loki::TimeSeries res(meta);
    res.reserve(original.size());
    for (std::size_t i = 0; i < original.size(); ++i)
        res.append(original[i].time, original[i].value - seasonal[i], original[i].flag);
    return res;
}

// ----------------------------------------------------------------------------
//  Replace detected positions with NaN then fill via GapFiller.
//  Returns the reconstructed cleaned series (filled residuals + seasonal).
//  Mirrors the logic in OutlierCleaner::_runPipeline() for the O4 branch.
// ----------------------------------------------------------------------------

static loki::TimeSeries
buildCleanedSeries(const loki::TimeSeries&               original,
                   const loki::TimeSeries&               residualSeries,
                   const std::vector<double>&            seasonal,
                   const loki::outlier::HatMatrixResult& hmResult,
                   loki::GapFiller::Strategy             strategy,
                   std::size_t                           maxFillLength)
{
    // Step 1: mark detected positions as NaN in residual series
    std::vector<bool> isOutlier(residualSeries.size(), false);
    for (const std::size_t idx : hmResult.outlierIndices)
        if (idx < residualSeries.size()) isOutlier[idx] = true;

    loki::SeriesMetadata markedMeta = residualSeries.metadata();
    loki::TimeSeries marked(markedMeta);
    marked.reserve(residualSeries.size());
    for (std::size_t i = 0; i < residualSeries.size(); ++i) {
        if (isOutlier[i])
            marked.append(residualSeries[i].time,
                          std::numeric_limits<double>::quiet_NaN(), 1);
        else
            marked.append(residualSeries[i].time,
                          residualSeries[i].value, residualSeries[i].flag);
    }

    // Step 2: fill NaN positions via GapFiller
    loki::GapFiller::Config gfCfg;
    gfCfg.strategy      = strategy;
    gfCfg.maxFillLength = maxFillLength;

    loki::GapFiller gapFiller(gfCfg);
    const loki::TimeSeries filledResiduals = gapFiller.fill(marked);

    // Step 3: reconstruct: filled residuals + seasonal component
    loki::SeriesMetadata cleanedMeta = original.metadata();
    cleanedMeta.componentName += "_cleaned";

    loki::TimeSeries cleaned(cleanedMeta);
    cleaned.reserve(original.size());
    for (std::size_t i = 0; i < original.size(); ++i) {
        const double v    = filledResiduals[i].value + seasonal[i];
        const uint8_t flg = isOutlier[i] ? 2 : original[i].flag;
        cleaned.append(original[i].time, v, flg);
    }
    return cleaned;
}

// ----------------------------------------------------------------------------
//  Convert HatMatrixResult -> OutlierResult for reuse of existing plot methods.
//  score = leverage value, threshold = DEH threshold.
// ----------------------------------------------------------------------------

static loki::outlier::OutlierResult
toOutlierResult(const loki::TimeSeries&               series,
                const loki::outlier::HatMatrixResult& hmResult)
{
    loki::outlier::OutlierResult out;
    out.method   = "hat_matrix";
    out.n        = hmResult.n;
    out.location = 0.0;
    out.scale    = 0.0;

    const std::size_t offset = static_cast<std::size_t>(hmResult.arOrder);
    const std::size_t levN   = static_cast<std::size_t>(hmResult.leverages.size());

    for (const std::size_t idx : hmResult.outlierIndices) {
        loki::outlier::OutlierPoint pt;
        pt.index         = idx;
        pt.originalValue = (idx < series.size()) ? series[idx].value
                                                  : std::numeric_limits<double>::quiet_NaN();
        pt.threshold     = hmResult.threshold;
        pt.flag          = 1;
        if (idx >= offset) {
            const std::size_t li = idx - offset;
            if (li < levN)
                pt.score = hmResult.leverages(static_cast<Eigen::Index>(li));
        }
        out.points.push_back(pt);
    }
    out.nOutliers = out.points.size();
    return out;
}

// ----------------------------------------------------------------------------
//  CSV export -- standard pipeline (O1-O3)
// ----------------------------------------------------------------------------

static void writeCsv(const std::filesystem::path&                       csvDir,
                     const loki::TimeSeries&                            original,
                     const loki::outlier::OutlierCleaner::CleanResult&  result,
                     const std::vector<double>&                         seasonal)
{
    const auto& m    = original.metadata();
    std::string base = m.stationId;
    if (!m.componentName.empty()) { if (!base.empty()) base += "_"; base += m.componentName; }
    if (base.empty()) base = "series";

    const std::filesystem::path outPath = csvDir / (base + "_outlier.csv");
    std::ofstream csv(outPath);
    if (!csv.is_open()) { LOKI_WARNING("writeCsv: cannot open: " + outPath.string()); return; }

    csv << "mjd;original;residual;cleaned;seasonal;outlier_flag\n"
        << std::fixed << std::setprecision(10);

    for (std::size_t i = 0; i < original.size(); ++i) {
        const double residual = (i < result.residuals.size())
            ? result.residuals[i].value : std::numeric_limits<double>::quiet_NaN();
        const double cleaned  = (i < result.cleaned.size())
            ? result.cleaned[i].value  : std::numeric_limits<double>::quiet_NaN();
        const double seas = (i < seasonal.size()) ? seasonal[i] : 0.0;
        bool isOutlier = false;
        for (const auto& pt : result.detection.points)
            if (pt.index == i) { isOutlier = true; break; }

        csv << original[i].time.mjd() << ";" << original[i].value << ";"
            << residual << ";" << cleaned << ";" << seas << ";"
            << (isOutlier ? 1 : 0) << "\n";
    }
    LOKI_INFO("CSV written: " + outPath.string());
}

// ----------------------------------------------------------------------------
//  CSV export -- hat matrix (O4)
// ----------------------------------------------------------------------------

static void writeCsvHatMatrix(const std::filesystem::path&              csvDir,
                               const loki::TimeSeries&                  series,
                               const loki::TimeSeries&                  cleaned,
                               const loki::outlier::HatMatrixResult&    result,
                               const std::vector<double>&               residuals,
                               const std::vector<double>&               seasonal)
{
    const auto& m    = series.metadata();
    std::string base = m.stationId;
    if (!m.componentName.empty()) { if (!base.empty()) base += "_"; base += m.componentName; }
    if (base.empty()) base = "series";

    const std::filesystem::path outPath = csvDir / (base + "_hat_matrix.csv");
    std::ofstream csv(outPath);
    if (!csv.is_open()) { LOKI_WARNING("writeCsvHatMatrix: cannot open: " + outPath.string()); return; }

    std::vector<bool> isOutlier(series.size(), false);
    for (const std::size_t idx : result.outlierIndices)
        if (idx < series.size()) isOutlier[idx] = true;

    const std::size_t offset = static_cast<std::size_t>(result.arOrder);
    const std::size_t levN   = static_cast<std::size_t>(result.leverages.size());

    csv << "mjd;original;residual;cleaned;seasonal;leverage;threshold;outlier_flag\n"
        << std::fixed << std::setprecision(10);

    for (std::size_t i = 0; i < series.size(); ++i) {
        const double residual = (i < residuals.size()) ? residuals[i]
                                                       : std::numeric_limits<double>::quiet_NaN();
        const double cleanedVal = (i < cleaned.size()) ? cleaned[i].value
                                                       : std::numeric_limits<double>::quiet_NaN();
        const double seas = (i < seasonal.size()) ? seasonal[i] : 0.0;
        double leverage = std::numeric_limits<double>::quiet_NaN();
        if (i >= offset) {
            const std::size_t li = i - offset;
            if (li < levN) leverage = result.leverages(static_cast<Eigen::Index>(li));
        }
        csv << series[i].time.mjd() << ";" << series[i].value << ";"
            << residual << ";" << cleanedVal << ";" << seas << ";"
            << leverage << ";" << result.threshold << ";"
            << (isOutlier[i] ? 1 : 0) << "\n";
    }
    LOKI_INFO("Hat matrix CSV written: " + outPath.string());
}

// ----------------------------------------------------------------------------
//  Protocol -- hat matrix (O4)
// ----------------------------------------------------------------------------

static void writeProtocolHatMatrix(const std::filesystem::path&           protDir,
                                    const loki::TimeSeries&               series,
                                    const loki::outlier::HatMatrixResult& result,
                                    const std::string&                    replacementStrategy)
{
    std::filesystem::create_directories(protDir);

    const auto& m    = series.metadata();
    std::string base = m.stationId;
    if (!m.componentName.empty()) { if (!base.empty()) base += "_"; base += m.componentName; }
    if (base.empty()) base = "series";

    const std::filesystem::path outPath =
        protDir / ("outlier_" + base + "_hat_matrix_protocol.txt");
    std::ofstream prot(outPath);
    if (!prot.is_open()) {
        LOKI_WARNING("writeProtocolHatMatrix: cannot open: " + outPath.string()); return;
    }

    prot << "=======================================================\n"
         << "  LOKI -- Hat Matrix Outlier Detection Protocol\n"
         << "  Method: DEH (Hau & Tong, 1989)\n"
         << "=======================================================\n\n"
         << "Series:               " << base << "\n"
         << "Total points:         " << result.n << "\n"
         << "AR order (p):         " << result.arOrder << "\n"
         << "Threshold:            " << std::fixed << std::setprecision(8)
                                     << result.threshold << "\n"
         << "Outliers found:       " << result.nOutliers << "\n"
         << "Replacement strategy: " << replacementStrategy << "\n\n";

    if (result.nOutliers == 0) {
        prot << "No outliers detected.\n";
    } else {
        prot << "Detected outlier positions:\n"
             << std::setw(10) << "Index"
             << std::setw(16) << "MJD"
             << std::setw(16) << "Value"
             << std::setw(16) << "Leverage h_ii" << "\n"
             << std::string(58, '-') << "\n"
             << std::fixed << std::setprecision(6);

        const std::size_t offset = static_cast<std::size_t>(result.arOrder);
        const std::size_t levN   = static_cast<std::size_t>(result.leverages.size());

        for (const std::size_t idx : result.outlierIndices) {
            const double mjd = (idx < series.size()) ? series[idx].time.mjd()
                                                     : std::numeric_limits<double>::quiet_NaN();
            const double val = (idx < series.size()) ? series[idx].value
                                                     : std::numeric_limits<double>::quiet_NaN();
            double lev = std::numeric_limits<double>::quiet_NaN();
            if (idx >= offset) {
                const std::size_t li = idx - offset;
                if (li < levN) lev = result.leverages(static_cast<Eigen::Index>(li));
            }
            prot << std::setw(10) << idx << std::setw(16) << mjd
                 << std::setw(16) << val  << std::setw(16) << lev << "\n";
        }
    }
    prot << "\n=======================================================\n";
    LOKI_INFO("Hat matrix protocol written: " + outPath.string());
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
        std::cerr << "[LOKI ERROR] " << ex.what() << "\n"; return EXIT_FAILURE;
    }

    try { loki::Logger::initDefault(cfg.logDir, "loki_outlier", cfg.output.logLevel); }
    catch (const loki::LOKIException& ex) {
        std::cerr << "[LOKI ERROR] Cannot init logger: " << ex.what() << "\n"; return EXIT_FAILURE;
    }

    LOKI_INFO("loki_outlier " + std::string(loki::VERSION_STRING) + " started.");
    LOKI_INFO("Config:    " + args.configPath.string());
    LOKI_INFO("Workspace: " + cfg.workspace.string());

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
        LOKI_ERROR(std::string("Data loading failed: ") + ex.what()); return EXIT_FAILURE;
    }

    if (cfg.stats.enabled) {
        try {
            for (const auto& r : loadResults) {
                for (const auto& ts : r.series) {
                    std::vector<double> vals;
                    vals.reserve(ts.size());
                    for (const auto& obs : ts) vals.push_back(obs.value);
                    LOKI_INFO(loki::stats::formatSummary(
                        loki::stats::summarize(vals, cfg.stats.nanPolicy, cfg.stats.hurst),
                        ts.metadata().componentName));
                }
            }
        } catch (const loki::LOKIException& ex) {
            LOKI_ERROR(std::string("Statistics failed: ") + ex.what());
        }
    }

    const bool useHatMatrix = (cfg.outlier.detection.method == "hat_matrix")
                               && cfg.outlier.hatMatrix.enabled;

    std::unique_ptr<loki::outlier::OutlierDetector> detector;
    std::unique_ptr<loki::outlier::OutlierCleaner>  cleaner;

    if (!useHatMatrix) {
        detector = buildDetector(cfg.outlier.detection);
        if (detector) {
            const auto cleanerCfg = buildCleanerConfig(cfg.outlier);
            cleaner = std::make_unique<loki::outlier::OutlierCleaner>(cleanerCfg, *detector);
        }
    }

    const loki::outlier::PlotOutlier plotter(cfg, "outlier");

    for (const auto& r : loadResults) {
        for (const auto& ts : r.series) {
            const std::string name = ts.metadata().stationId.empty()
                ? ts.metadata().componentName
                : ts.metadata().stationId + "_" + ts.metadata().componentName;
            LOKI_INFO("--- Processing series: " + name
                      + "  n=" + std::to_string(ts.size()) + " ---");

            try {
                loki::Deseasonalizer::Result dsResult;
                const auto [seasonal, hasComponent] =
                    runDeseasonalize(ts, cfg.outlier.deseasonalization, dsResult);

                LOKI_INFO("Deseasonalization: strategy="
                          + cfg.outlier.deseasonalization.strategy);

                std::vector<double> residualValues;
                residualValues.reserve(ts.size());
                for (std::size_t i = 0; i < ts.size(); ++i)
                    residualValues.push_back(ts[i].value - seasonal[i]);

                const loki::TimeSeries residualSeries = buildResidualSeries(ts, seasonal);

                // =============================================================
                //  O4 branch: HatMatrixDetector
                // =============================================================
                if (useHatMatrix) {
                    loki::outlier::HatMatrixDetector::Config hmCfg;
                    hmCfg.arOrder           = cfg.outlier.hatMatrix.arOrder;
                    hmCfg.significanceLevel = cfg.outlier.hatMatrix.significanceLevel;

                    const loki::outlier::HatMatrixDetector hmDetector(hmCfg);
                    const loki::outlier::HatMatrixResult   hmResult =
                        hmDetector.detect(residualValues);

                    LOKI_INFO("HatMatrix outliers detected: "
                              + std::to_string(hmResult.nOutliers)
                              + " / " + std::to_string(hmResult.n)
                              + "  arOrder=" + std::to_string(hmResult.arOrder)
                              + "  threshold=" + std::to_string(hmResult.threshold));

                    {
                        const std::size_t offset = static_cast<std::size_t>(hmResult.arOrder);
                        const std::size_t levN   = static_cast<std::size_t>(hmResult.leverages.size());
                        for (const std::size_t idx : hmResult.outlierIndices) {
                            const std::size_t li = (idx >= offset) ? (idx - offset) : 0;
                            const double lev = (li < levN)
                                ? hmResult.leverages(static_cast<Eigen::Index>(li))
                                : std::numeric_limits<double>::quiet_NaN();
                            LOKI_INFO("  outlier idx=" + std::to_string(idx)
                                      + "  mjd=" + std::to_string(ts[idx].time.mjd())
                                      + "  value=" + std::to_string(ts[idx].value)
                                      + "  leverage=" + std::to_string(lev));
                        }
                    }

                    // Convert for reuse of existing plot methods
                    const loki::outlier::OutlierResult asOutlierResult =
                        toOutlierResult(ts, hmResult);

                    // Build cleaned series: mark outliers as NaN, fill, reconstruct
                    const loki::GapFiller::Strategy fillStrat = replacementStrategy(cfg.outlier);
                    const std::size_t maxFill = static_cast<std::size_t>(
                        std::max(0, cfg.outlier.replacement.maxFillLength));

                    loki::TimeSeries cleanedSeries;
                    bool cleaningOk = true;
                    try {
                        cleanedSeries = buildCleanedSeries(
                            ts, residualSeries, seasonal, hmResult, fillStrat, maxFill);
                        LOKI_INFO("Replacement strategy: " + cfg.outlier.replacement.strategy
                                  + "  positions replaced: " + std::to_string(hmResult.nOutliers));
                    } catch (const loki::LOKIException& ex) {
                        LOKI_ERROR(std::string("Outlier replacement failed: ") + ex.what());
                        cleaningOk = false;
                    }

                    // CSV (includes cleaned column)
                    try {
                        writeCsvHatMatrix(cfg.csvDir, ts,
                                          cleaningOk ? cleanedSeries : ts,
                                          hmResult, residualValues, seasonal);
                    } catch (const loki::LOKIException& ex) {
                        LOKI_ERROR(std::string("Hat matrix CSV export failed: ") + ex.what());
                    }

                    // Protocol
                    try {
                        writeProtocolHatMatrix(cfg.protocolsDir, ts, hmResult,
                                               cfg.outlier.replacement.strategy);
                    } catch (const loki::LOKIException& ex) {
                        LOKI_ERROR(std::string("Hat matrix protocol failed: ") + ex.what());
                    }

                    // --- Plots ---

                    // Leverage plot: h_ii vs time with threshold line
                    if (cfg.plots.leveragePlot) {
                        try {
                            plotter.plotLeverages(ts, hmResult.leverages,
                                                  hmResult.threshold, hmResult.arOrder,
                                                  hmResult.nOutliers);
                        } catch (const loki::LOKIException& ex) {
                            LOKI_ERROR(std::string("Leverage plot failed: ") + ex.what());
                        }
                    }

                    // Original series with outlier markers
                    if (cfg.plots.originalSeries) {
                        try { plotter.plotOriginalWithOutliers(ts, asOutlierResult); }
                        catch (const loki::LOKIException& ex) {
                            LOKI_ERROR(std::string("Original+outliers plot failed: ") + ex.what());
                        }
                    }

                    // Cleaned series
                    if (cleaningOk && cfg.plots.adjustedSeries) {
                        try { plotter.plotCleaned(cleanedSeries); }
                        catch (const loki::LOKIException& ex) {
                            LOKI_ERROR(std::string("Cleaned plot failed: ") + ex.what());
                        }
                    }

                    // Original vs cleaned overlay
                    if (cleaningOk && cfg.plots.homogComparison) {
                        try { plotter.plotComparison(ts, cleanedSeries); }
                        catch (const loki::LOKIException& ex) {
                            LOKI_ERROR(std::string("Comparison plot failed: ") + ex.what());
                        }
                    }

                    // Residuals with outlier markers
                    if (cfg.plots.deseasonalized && hasComponent) {
                        try { plotter.plotResiduals(ts, residualSeries, asOutlierResult); }
                        catch (const loki::LOKIException& ex) {
                            LOKI_ERROR(std::string("Residuals plot failed: ") + ex.what());
                        }
                    }

                    // Seasonal overlay on original
                    if (cfg.plots.seasonalOverlay && hasComponent) {
                        try { plotter.plotSeasonalOverlay(ts, seasonal); }
                        catch (const loki::LOKIException& ex) {
                            LOKI_ERROR(std::string("Seasonal overlay plot failed: ") + ex.what());
                        }
                    }

                    // Core diagnostic plots on residuals --
                    // ACF of residuals is the correct tool for AR order selection:
                    // choose p where ACF first crosses the 1.96/sqrt(n) confidence band.
                    try {
                        loki::Plot corePlot(cfg);
                        if (cfg.plots.timeSeries) corePlot.timeSeries(residualSeries);
                        if (cfg.plots.histogram)  corePlot.histogram(residualSeries);
                        if (cfg.plots.acf)        corePlot.acf(residualSeries);
                        if (cfg.plots.qqPlot)     corePlot.qqPlot(residualSeries);
                        if (cfg.plots.boxplot)    corePlot.boxplot(residualSeries);
                    } catch (const loki::LOKIException& ex) {
                        LOKI_ERROR(std::string("Core plots failed: ") + ex.what());
                    }

                    continue;
                }

                // =============================================================
                //  O1-O3 branch: standard OutlierCleaner pipeline (unchanged)
                // =============================================================
                if (!cleaner) {
                    LOKI_WARNING("No detector configured -- skipping series " + name);
                    continue;
                }

                loki::outlier::OutlierCleaner::CleanResult result =
                    hasComponent ? cleaner->clean(ts, seasonal) : cleaner->clean(ts);

                LOKI_INFO("Outliers detected: " + std::to_string(result.detection.nOutliers)
                          + " / " + std::to_string(ts.size())
                          + "  method=" + cfg.outlier.detection.method
                          + "  location=" + std::to_string(result.detection.location)
                          + "  scale="    + std::to_string(result.detection.scale));

                for (const auto& pt : result.detection.points) {
                    LOKI_INFO("  outlier idx=" + std::to_string(pt.index)
                              + "  mjd=" + std::to_string(ts[pt.index].time.mjd())
                              + "  orig=" + std::to_string(pt.originalValue)
                              + "  score=" + std::to_string(pt.score)
                              + "  threshold=" + std::to_string(pt.threshold));
                }

                try { writeCsv(cfg.csvDir, ts, result, seasonal); }
                catch (const loki::LOKIException& ex) {
                    LOKI_ERROR(std::string("CSV export failed: ") + ex.what());
                }

                try {
                    plotter.plotAll(ts, result.cleaned, result.residuals,
                                    result.detection, hasComponent, seasonal);
                } catch (const loki::LOKIException& ex) {
                    LOKI_ERROR(std::string("Plotting failed: ") + ex.what());
                }

            } catch (const loki::LOKIException& ex) {
                LOKI_ERROR("Outlier pipeline failed for " + name + ": " + ex.what());
                continue;
            }
        }
    }

    LOKI_INFO("loki_outlier finished successfully.");
    return EXIT_SUCCESS;
}