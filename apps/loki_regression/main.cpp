#include <loki/core/logger.hpp>
#include <loki/core/configLoader.hpp>
#include <loki/core/version.hpp>
#include <loki/io/dataManager.hpp>
#include <loki/stats/descriptive.hpp>
#include <loki/stats/hypothesis.hpp>
#include <loki/timeseries/gapFiller.hpp>
#include <loki/regression/regressor.hpp>
#include <loki/regression/regressionResult.hpp>
#include <loki/regression/regressionDiagnostics.hpp>
#include <loki/regression/linearRegressor.hpp>
#include <loki/regression/polynomialRegressor.hpp>
#include <loki/regression/harmonicRegressor.hpp>
#include <loki/regression/trendEstimator.hpp>
#include <loki/regression/robustRegressor.hpp>
#include <loki/regression/calibrationRegressor.hpp>
#include <loki/regression/plotRegression.hpp>
#include <loki/regression/nonlinearRegressor.hpp>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string_view>

using namespace loki;
using namespace loki::regression;

// Maximum number of prediction points before skipping with a warning.
// Prevents accidental generation of millions of points for high-rate data.
static constexpr std::size_t MAX_PREDICTION_POINTS = 100'000;

// ----------------------------------------------------------------------------
//  Help / version
// ----------------------------------------------------------------------------

static void printVersion()
{
    std::cout << "loki_regression " << loki::VERSION_STRING << "\n";
}

static void printHelp()
{
    std::cout
        << "loki_regression " << loki::VERSION_STRING
        << " -- Regression analysis pipeline\n"
        << "\n"
        << "Usage:\n"
        << "  loki_regression.exe <config.json> [options]\n"
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
    std::filesystem::path configPath {"config/regression.json"};
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
            std::cerr << "[loki_regression] Unknown option: " << arg << "  (use --help)\n";
        }
    }
    return args;
}

// ----------------------------------------------------------------------------
//  GapFiller config helper
// ----------------------------------------------------------------------------

static loki::GapFiller::Config buildGapFillerConfig(const loki::RegressionConfig& rcfg)
{
    loki::GapFiller::Config gfc{};
    const std::string& s = rcfg.gapFilling.strategy;
    if      (s == "linear")       gfc.strategy = loki::GapFiller::Strategy::LINEAR;
    else if (s == "forward_fill") gfc.strategy = loki::GapFiller::Strategy::FORWARD_FILL;
    else if (s == "mean")         gfc.strategy = loki::GapFiller::Strategy::MEAN;
    else if (s == "spline")       gfc.strategy = loki::GapFiller::Strategy::SPLINE;
    else                          gfc.strategy = loki::GapFiller::Strategy::NONE;
    gfc.maxFillLength = static_cast<std::size_t>(
        std::max(0, rcfg.gapFilling.maxFillLength));
    return gfc;
}

// ----------------------------------------------------------------------------
//  Regressor factory
// ----------------------------------------------------------------------------

static std::unique_ptr<Regressor> buildRegressor(const RegressionConfig& rcfg)
{
    switch (rcfg.method) {
        case RegressionMethodEnum::LINEAR:
            return std::make_unique<LinearRegressor>(rcfg);
        case RegressionMethodEnum::POLYNOMIAL:
            return std::make_unique<PolynomialRegressor>(rcfg);
        case RegressionMethodEnum::HARMONIC:
            return std::make_unique<HarmonicRegressor>(rcfg);
        case RegressionMethodEnum::TREND:
            return nullptr; // handled separately in main loop
        case RegressionMethodEnum::ROBUST:
            return std::make_unique<RobustRegressor>(rcfg);
        case RegressionMethodEnum::CALIBRATION:
            return std::make_unique<CalibrationRegressor>(rcfg);
        case RegressionMethodEnum::NONLINEAR:
            return nullptr; // handled separately in main loop
    }
    throw AlgorithmException("buildRegressor: unhandled RegressionMethodEnum value.");
}

// ----------------------------------------------------------------------------
//  CSV export -- fitted values
// ----------------------------------------------------------------------------

static void writeCsv(const std::filesystem::path& csvDir,
                     const TimeSeries&             original,
                     const RegressionResult&       result,
                     bool                          isTrend)
{
    const auto& m    = original.metadata();
    std::string base = m.stationId;
    if (!m.componentName.empty()) {
        if (!base.empty()) base += "_";
        base += m.componentName;
    }
    if (base.empty()) base = "series";

    const std::filesystem::path outPath = csvDir / (base + "_regression.csv");
    std::ofstream csv(outPath);
    if (!csv.is_open()) {
        LOKI_WARNING("writeCsv: cannot open file: " + outPath.string());
        return;
    }

    if (isTrend) {
        csv << "mjd;original;fitted;trend;seasonal;residual\n";
    } else {
        csv << "mjd;original;fitted;residual\n";
    }

    csv << std::fixed << std::setprecision(10);

    const std::size_t n = original.size();
    for (std::size_t i = 0; i < n; ++i) {
        const double mjd     = original[i].time.mjd();
        const double origVal = original[i].value;

        double fittedVal = std::numeric_limits<double>::quiet_NaN();
        double residual  = std::numeric_limits<double>::quiet_NaN();

        for (std::size_t j = 0; j < result.fitted.size(); ++j) {
            if (std::fabs(result.fitted[j].time.mjd() - mjd) < 1e-6) {
                fittedVal = result.fitted[j].value;
                residual  = (j < static_cast<std::size_t>(result.residuals.size()))
                            ? result.residuals[static_cast<int>(j)]
                            : std::numeric_limits<double>::quiet_NaN();
                break;
            }
        }

        if (isTrend) {
            csv << mjd       << ";"
                << origVal   << ";"
                << fittedVal << ";"
                << std::numeric_limits<double>::quiet_NaN() << ";"
                << std::numeric_limits<double>::quiet_NaN() << ";"
                << residual  << "\n";
        } else {
            csv << mjd       << ";"
                << origVal   << ";"
                << fittedVal << ";"
                << residual  << "\n";
        }
    }

    LOKI_INFO("CSV written: " + outPath.string());
}

// ----------------------------------------------------------------------------
//  CSV export -- prediction
// ----------------------------------------------------------------------------

static void writePredictionCsv(const std::filesystem::path&        csvDir,
                                const TimeSeries&                   original,
                                const RegressionResult&             result,
                                const std::vector<PredictionPoint>& prediction)
{
    if (prediction.empty()) return;

    const auto& m    = original.metadata();
    std::string base = m.stationId;
    if (!m.componentName.empty()) {
        if (!base.empty()) base += "_";
        base += m.componentName;
    }
    if (base.empty()) base = "series";

    const std::filesystem::path outPath = csvDir / (base + "_prediction.csv");
    std::ofstream csv(outPath);
    if (!csv.is_open()) {
        LOKI_WARNING("writePredictionCsv: cannot open file: " + outPath.string());
        return;
    }

    // conf_low/conf_high: where the mean prediction lies (narrow for large n).
    // pred_low/pred_high: where future observations are expected (includes sigma0).
    csv << "mjd;predicted;conf_low;conf_high;pred_low;pred_high\n";
    csv << std::fixed << std::setprecision(10);

    for (const auto& pt : prediction) {
        const double mjd = pt.x + result.tRef;
        csv << mjd          << ";"
            << pt.predicted << ";"
            << pt.confLow   << ";"
            << pt.confHigh  << ";"
            << pt.predLow   << ";"
            << pt.predHigh  << "\n";
    }

    LOKI_INFO("Prediction CSV written: " + outPath.string());
}

// ----------------------------------------------------------------------------
//  Protocol writer
// ----------------------------------------------------------------------------

static void writeProtocol(const std::filesystem::path& protocolsDir,
                           const TimeSeries&             original,
                           const RegressionResult&       result,
                           const AnovaTable&             anova,
                           const InfluenceMeasures&      influence,
                           const VifResult&              vif,
                           const BreuschPaganResult&     bp,
                           const RegressionConfig&       rcfg)
{
    const auto& m    = original.metadata();
    std::string dataset = m.stationId.empty()  ? "data"   : m.stationId;
    std::string param   = m.componentName.empty() ? "series" : m.componentName;

    const std::string fname = "regression_" + dataset + "_" + param + "_protocol.txt";
    const std::filesystem::path outPath = protocolsDir / fname;

    std::ofstream prot(outPath);
    if (!prot.is_open()) {
        LOKI_WARNING("writeProtocol: cannot open file: " + outPath.string());
        return;
    }

    auto pVal = [](double p) -> std::string {
        if (p < 0.001) return "<0.001";
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(4) << p;
        return ss.str();
    };

    auto fmtD = [](double v, int prec = 4) -> std::string {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(prec) << v;
        return ss.str();
    };

    const int nObs   = static_cast<int>(result.residuals.size());
    const int nParam = static_cast<int>(result.coefficients.size());

    prot << "============================================================\n";
    prot << " REGRESSION PROTOCOL -- " << result.modelName << "\n";
    prot << "============================================================\n";
    prot << " Dataset:      " << dataset << "    Series: " << param << "\n";
    prot << " Observations: " << nObs
         << "    Parameters: " << nParam
         << "    DOF: " << result.dof << "\n";
    prot << " Robust: " << (rcfg.robust ? "yes" : "no");
    if (rcfg.robust)
        prot << "    Weight fn: " << rcfg.robustWeightFn
             << "    Converged: " << (result.converged ? "yes" : "NO");
    prot << "\n\n";

    prot << " COEFFICIENTS\n";
    prot << " -------------------------------------------------------\n";
    prot << " Parameter    Estimate        Std.Err\n";

    auto coeffName = [&](int i) -> std::string {
        switch (rcfg.method) {
            case RegressionMethodEnum::LINEAR:
                return (i == 0) ? "a0 (intercept)" : "a1 (slope)";
            case RegressionMethodEnum::POLYNOMIAL:
                return "a" + std::to_string(i);
            case RegressionMethodEnum::HARMONIC:
            case RegressionMethodEnum::TREND: {
                if (i == 0) return "a0 (mean)";
                if (rcfg.method == RegressionMethodEnum::TREND && i == 1)
                    return "a1 (trend)";
                const int base = (rcfg.method == RegressionMethodEnum::TREND) ? 2 : 1;
                const int k    = (i - base) / 2 + 1;
                return ((i - base) % 2 == 0)
                    ? "s" + std::to_string(k) + " (sin K=" + std::to_string(k) + ")"
                    : "c" + std::to_string(k) + " (cos K=" + std::to_string(k) + ")";
            }
            case RegressionMethodEnum::ROBUST:
                return "a" + std::to_string(i);
            case RegressionMethodEnum::CALIBRATION:
                return (i == 0) ? "a0 (intercept)" : "a1 (slope, TLS)";
            case RegressionMethodEnum::NONLINEAR:
                return "p" + std::to_string(i);
        }
        return "a" + std::to_string(i);
    };

    for (int i = 0; i < nParam; ++i) {
        const double coeff = result.coefficients[i];
        std::string stdErrStr = "N/A";
        if (result.cofactorX.rows() == nParam && i < result.cofactorX.rows()) {
            const double stdErr = result.sigma0 * std::sqrt(result.cofactorX(i, i));
            stdErrStr = fmtD(stdErr, 6);
        }
        prot << " " << std::left << std::setw(22) << coeffName(i)
             << std::right << std::setw(14) << fmtD(coeff, 6)
             << std::setw(14) << stdErrStr << "\n";
    }
    prot << "\n";

    prot << " MODEL FIT\n";
    prot << " -------------------------------------------------------\n";
    prot << " sigma0:          " << fmtD(result.sigma0, 6) << "\n";
    prot << " R^2:             " << fmtD(result.rSquared, 6)
         << "    Adjusted R^2: " << fmtD(result.rSquaredAdj, 6) << "\n";
    prot << " AIC:             " << fmtD(result.aic, 2)
         << "    BIC: "          << fmtD(result.bic, 2) << "\n\n";

    prot << " ANOVA\n";
    prot << " -------------------------------------------------------\n";
    prot << " Source         SS              df     MS\n";
    const double msr = anova.ssr / static_cast<double>(anova.dfRegression);
    const double mse = anova.sse / static_cast<double>(anova.dfError);
    prot << " Regression  " << std::setw(16) << fmtD(anova.ssr, 4)
         << std::setw(6)  << anova.dfRegression
         << std::setw(16) << fmtD(msr, 4) << "\n";
    prot << " Error       " << std::setw(16) << fmtD(anova.sse, 4)
         << std::setw(6)  << anova.dfError
         << std::setw(16) << fmtD(mse, 4) << "\n";
    prot << " Total       " << std::setw(16) << fmtD(anova.sst, 4)
         << std::setw(6)  << anova.dfTotal << "\n";
    prot << " F-statistic: " << fmtD(anova.fStatistic, 4)
         << "    p-value: " << pVal(anova.pValue)
         << (anova.pValue < rcfg.significanceLevel ? "  [SIGNIFICANT]" : "  [NOT SIGNIFICANT]")
         << "\n\n";

    prot << " RESIDUAL DIAGNOSTICS\n";
    prot << " -------------------------------------------------------\n";

    const Eigen::VectorXd& e = result.residuals;
    const double resMean = e.mean();
    const double resStd  = std::sqrt((e.array() - resMean).square().sum()
                                     / static_cast<double>(nObs - 1));
    prot << " Mean:            " << fmtD(resMean, 6)
         << "    Std dev: "      << fmtD(resStd, 6) << "\n";
    prot << " Breusch-Pagan:   LM=" << fmtD(bp.testStatistic, 4)
         << "    p=" << pVal(bp.pValue)
         << (bp.rejected ? "  [HETEROSCEDASTIC]" : "  [HOMOSCEDASTIC]") << "\n";

    const int nFlaggedCook = static_cast<int>(
        (influence.cooksDistance.array() > influence.cooksThreshold).count());
    prot << " Cook's D > 4/n:  " << nFlaggedCook << " observation(s)"
         << "  (threshold=" << fmtD(influence.cooksThreshold, 4) << ")\n";

    const int nFlaggedLev = static_cast<int>(
        (influence.leverages.array() > influence.leverageThreshold).count());
    prot << " High leverage:   " << nFlaggedLev << " observation(s)"
         << "  (threshold=" << fmtD(influence.leverageThreshold, 4) << ")\n\n";

    if (vif.vifValues.size() > 0) {
        prot << " MULTICOLLINEARITY (VIF)\n";
        prot << " -------------------------------------------------------\n";
        for (int j = 0; j < static_cast<int>(vif.vifValues.size()); ++j) {
            const bool flagged = std::find(vif.flaggedIndices.begin(),
                                           vif.flaggedIndices.end(), j)
                                 != vif.flaggedIndices.end();
            prot << " Predictor " << (j + 1) << ":  VIF=" << fmtD(vif.vifValues[j], 3)
                 << (flagged ? "  [HIGH]" : "") << "\n";
        }
        prot << "\n";
    }

    prot << "============================================================\n";
    LOKI_INFO("Protocol written: " + outPath.string());
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
        loki::Logger::initDefault(cfg.logDir, "loki_regression", cfg.output.logLevel);
    } catch (const loki::LOKIException& ex) {
        std::cerr << "[LOKI ERROR] Cannot init logger: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    LOKI_INFO("loki_regression " + std::string(loki::VERSION_STRING) + " started.");
    LOKI_INFO("Config:    " + args.configPath.string());
    LOKI_INFO("Workspace: " + cfg.workspace.string());

    try {
        std::filesystem::create_directories(cfg.logDir);
        std::filesystem::create_directories(cfg.csvDir);
        std::filesystem::create_directories(cfg.imgDir);
        std::filesystem::create_directories(cfg.protocolsDir);
    } catch (const std::exception& ex) {
        LOKI_ERROR(std::string("Cannot create output directories: ") + ex.what());
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
            LOKI_WARNING(std::string("Statistics failed: ") + ex.what());
        }
    }

    const loki::GapFiller::Config gapFillerCfg = buildGapFillerConfig(cfg.regression);
    PlotRegression plotter{cfg};
    RegressionDiagnostics diag{cfg.regression.significanceLevel};

    for (const auto& r : loadResults) {
        for (const auto& ts : r.series) {
            const std::string seriesName = ts.metadata().stationId + "_"
                                         + ts.metadata().componentName;
            LOKI_INFO("--- Processing series: " + seriesName + " ---");

            // Step 1: gap filling.
            loki::TimeSeries filled = ts;
            try {
                loki::GapFiller gapFiller{gapFillerCfg};
                filled = gapFiller.fill(ts);
                std::size_t nanBefore = 0;
                std::size_t nanAfter  = 0;
                for (const auto& obs : ts)     if (!loki::isValid(obs)) ++nanBefore;
                for (const auto& obs : filled) if (!loki::isValid(obs)) ++nanAfter;
                LOKI_INFO("GapFiller: NaN before=" + std::to_string(nanBefore)
                          + "  after=" + std::to_string(nanAfter)
                          + "  strategy=" + cfg.regression.gapFilling.strategy);
            } catch (const loki::LOKIException& ex) {
                LOKI_ERROR("GapFiller failed for " + seriesName + ": " + ex.what());
                continue;
            }

            // Step 2: fit regressor.
            RegressionResult result;
            try {
                if (cfg.regression.method == RegressionMethodEnum::TREND) {
                    TrendEstimator te{cfg.regression};
                    LOKI_INFO("Regressor: " + te.name());
                    const auto decomp = te.fit(filled);
                    result = decomp.regression;
                } else if (cfg.regression.method == RegressionMethodEnum::NONLINEAR) {
                    NonlinearRegressor nlr{cfg.regression};
                    LOKI_INFO("Regressor: " + nlr.name());
                    result = nlr.fit(filled);
                    if (!result.converged)
                        LOKI_WARNING("LM did not converge for series: " + seriesName);
                } else {
                    const auto regressorPtr = buildRegressor(cfg.regression);
                    LOKI_INFO("Regressor: " + regressorPtr->name());
                    result = regressorPtr->fit(filled);
                }
            } catch (const loki::LOKIException& ex) {
                LOKI_ERROR("Regression failed for " + seriesName + ": " + ex.what());
                continue;
            }

            LOKI_INFO("Model: " + result.modelName);
            LOKI_INFO("sigma0=" + std::to_string(result.sigma0)
                      + "  R2=" + std::to_string(result.rSquared)
                      + "  R2adj=" + std::to_string(result.rSquaredAdj)
                      + "  AIC=" + std::to_string(result.aic)
                      + "  BIC=" + std::to_string(result.bic)
                      + "  DOF=" + std::to_string(result.dof));

            if (!result.converged)
                LOKI_WARNING("IRLS did not converge for series: " + seriesName);

            // Step 3: diagnostics.
            AnovaTable         anova;
            InfluenceMeasures  influence;
            VifResult          vif;
            BreuschPaganResult bp;

            try {
                anova = diag.computeAnova(result);
                LOKI_INFO("ANOVA: F=" + std::to_string(anova.fStatistic)
                          + "  p=" + std::to_string(anova.pValue)
                          + "  SSR=" + std::to_string(anova.ssr)
                          + "  SSE=" + std::to_string(anova.sse));
            } catch (const loki::LOKIException& ex) {
                LOKI_WARNING("ANOVA failed: " + std::string(ex.what()));
            }

            try {
                influence = diag.computeInfluence(result);
                const int nCook = static_cast<int>(
                    (influence.cooksDistance.array() > influence.cooksThreshold).count());
                LOKI_INFO("Influence: " + std::to_string(nCook)
                          + " high-influence observations (Cook's D > "
                          + std::to_string(influence.cooksThreshold) + ")");
            } catch (const loki::LOKIException& ex) {
                LOKI_WARNING("Influence diagnostics failed: " + std::string(ex.what()));
            }

            try {
                vif = diag.computeVif(result);
                if (!vif.flaggedIndices.empty())
                    LOKI_WARNING("VIF: " + std::to_string(vif.flaggedIndices.size())
                                 + " predictor(s) with VIF > " + std::to_string(vif.threshold));
            } catch (const loki::LOKIException& ex) {
                LOKI_WARNING("VIF failed: " + std::string(ex.what()));
            }

            try {
                bp = diag.computeBreuschPagan(result);
                LOKI_INFO("Breusch-Pagan: LM=" + std::to_string(bp.testStatistic)
                          + "  p=" + std::to_string(bp.pValue)
                          + (bp.rejected ? "  [HETEROSCEDASTIC]" : "  [OK]"));
            } catch (const loki::LOKIException& ex) {
                LOKI_WARNING("Breusch-Pagan failed: " + std::string(ex.what()));
            }

            // Step 3b: prediction (optional).
            std::vector<PredictionPoint> predictionPts;
            if (cfg.regression.computePrediction && cfg.regression.predictionHorizon > 0.0) {
                try {
                    // Estimate time step from first two valid observations.
                    double stepDays   = 1.0;
                    std::size_t found = 0;
                    double prevMjd    = 0.0;
                    for (std::size_t i = 0; i < filled.size(); ++i) {
                        if (!loki::isValid(filled[i])) continue;
                        if (found == 1) {
                            stepDays = filled[i].time.mjd() - prevMjd;
                            break;
                        }
                        prevMjd = filled[i].time.mjd();
                        ++found;
                    }
                    if (stepDays <= 0.0) stepDays = 1.0;

                    const std::size_t nPred = static_cast<std::size_t>(
                        std::ceil(cfg.regression.predictionHorizon / stepDays));

                    if (nPred > MAX_PREDICTION_POINTS) {
                        LOKI_WARNING(
                            "Prediction skipped: estimated " + std::to_string(nPred)
                            + " points (step=" + std::to_string(stepDays)
                            + " days, horizon=" + std::to_string(cfg.regression.predictionHorizon)
                            + " days) exceeds limit of "
                            + std::to_string(MAX_PREDICTION_POINTS)
                            + ". Reduce predictionHorizon or increase step.");
                    } else if (cfg.regression.method == RegressionMethodEnum::TREND) {
                        LOKI_WARNING("Prediction not supported for TrendEstimator method.");
                    } else {
                        // Build xNew -- shared by both NONLINEAR and linear branches.
                        const double lastFittedMjd =
                            result.fitted.size() > 0
                            ? result.fitted[result.fitted.size() - 1].time.mjd()
                            : 0.0;
                        const double x0 = lastFittedMjd - result.tRef + stepDays;

                        LOKI_INFO("Prediction details: tRef=" + std::to_string(result.tRef)
                            + "  lastFittedMjd=" + std::to_string(lastFittedMjd)
                            + "  x0=" + std::to_string(x0)
                            + "  nPred=" + std::to_string(nPred));

                        std::vector<double> xNew;
                        xNew.reserve(nPred);
                        for (std::size_t k = 0; k < nPred; ++k)
                            xNew.push_back(x0 + static_cast<double>(k) * stepDays);

                        if (cfg.regression.method == RegressionMethodEnum::NONLINEAR) {
                            NonlinearRegressor nlrPred{cfg.regression};
                            nlrPred.fit(filled);
                            predictionPts = nlrPred.predict(xNew);
                        } else {
                            // Re-fit to restore internal state, then predict.
                            const auto regressorPtr = buildRegressor(cfg.regression);
                            regressorPtr->fit(filled);
                            predictionPts = regressorPtr->predict(xNew);
                        }

                        LOKI_INFO("Prediction: " + std::to_string(predictionPts.size())
                                  + " points (horizon="
                                  + std::to_string(cfg.regression.predictionHorizon)
                                  + " days, step=" + std::to_string(stepDays) + " days)");
                        LOKI_INFO(
                            "Prediction intervals: conf=[confLow, confHigh] is the CI"
                            " for the mean response (narrow for large n)."
                            " pred=[predLow, predHigh] is the PI for future observations"
                            " (wider, includes sigma0=" + std::to_string(result.sigma0) + ")."
                            " Both exported to CSV and shown in the forecast plot.");
                    }
                } catch (const loki::LOKIException& ex) {
                    LOKI_WARNING("Prediction failed: " + std::string(ex.what()));
                }
            }

            // Step 4: CSV export.
            try {
                const bool isTrend = (cfg.regression.method == RegressionMethodEnum::TREND);
                writeCsv(cfg.csvDir, ts, result, isTrend);
            } catch (const loki::LOKIException& ex) {
                LOKI_ERROR(std::string("CSV export failed: ") + ex.what());
            }

            // Step 4b: prediction CSV.
            if (!predictionPts.empty()) {
                try {
                    writePredictionCsv(cfg.csvDir, ts, result, predictionPts);
                } catch (const loki::LOKIException& ex) {
                    LOKI_ERROR(std::string("Prediction CSV export failed: ") + ex.what());
                }
            }

            // Step 5: protocol.
            try {
                writeProtocol(cfg.protocolsDir, ts, result,
                              anova, influence, vif, bp, cfg.regression);
            } catch (const loki::LOKIException& ex) {
                LOKI_ERROR(std::string("Protocol failed: ") + ex.what());
            }

            // Step 6: plots.
            try {
                plotter.plotAll(ts, result, influence, predictionPts);
            } catch (const loki::LOKIException& ex) {
                LOKI_ERROR(std::string("Plotting failed: ") + ex.what());
            }
        }
    }

    LOKI_INFO("loki_regression finished successfully.");
    return EXIT_SUCCESS;
}
