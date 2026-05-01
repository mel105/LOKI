#include "loki/core/logger.hpp"
#include "loki/core/configLoader.hpp"
#include "loki/core/version.hpp"
#include "loki/io/loader.hpp"
#include "loki/stats/descriptive.hpp"
#include "loki/multivariate/multivariateAssembler.hpp"
#include "loki/multivariate/multivariateAnalyzer.hpp"
#include "loki/multivariate/plotMultivariate.hpp"
#include "loki/multivariate/multivariateProtocol.hpp"

#include <filesystem>
#include <iostream>
#include <string_view>

// ----------------------------------------------------------------------------
//  Help / version
// ----------------------------------------------------------------------------

static void printVersion()
{
    std::cout << "loki_multivariate " << loki::VERSION_STRING << "\n";
}

static void printHelp()
{
    std::cout
        << "loki_multivariate " << loki::VERSION_STRING
        << " -- Multivariate time series analysis pipeline\n"
        << "\n"
        << "Usage:\n"
        << "  loki_multivariate.exe <config.json> [options]\n"
        << "\n"
        << "Options:\n"
        << "  --help     Show this message and exit.\n"
        << "  --version  Show version string and exit.\n"
        << "\n"
        << "Methods (configured per JSON):\n"
        << "  ccf          Cross-correlation function matrix\n"
        << "  pca          Principal Component Analysis\n"
        << "  mssa         Multivariate Singular Spectrum Analysis\n"
        << "  var          Vector Autoregression + Granger causality\n"
        << "  factor       Factor Analysis (Varimax rotation)\n"
        << "  cca          Canonical Correlation Analysis\n"
        << "  lda          Linear / Quadratic Discriminant Analysis\n"
        << "  mahalanobis  Multivariate outlier detection\n"
        << "  manova       One-way MANOVA\n";
}

// ----------------------------------------------------------------------------
//  CLI
// ----------------------------------------------------------------------------

struct CliArgs {
    bool                  showHelp    {false};
    bool                  showVersion {false};
    std::filesystem::path configPath  {"config/multivariate.json"};
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
            std::cerr << "[loki_multivariate] Unknown option: " << arg
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

    // -- Load config ----------------------------------------------------------
    loki::AppConfig cfg;
    try {
        cfg = loki::ConfigLoader::load(args.configPath);
    } catch (const loki::LOKIException& ex) {
        std::cerr << "[LOKI ERROR] " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    // -- Init logger ----------------------------------------------------------
    try {
        loki::Logger::initDefault(cfg.logDir, "loki_multivariate",
                                  cfg.output.logLevel);
    } catch (const loki::LOKIException& ex) {
        std::cerr << "[LOKI ERROR] Cannot init logger: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    LOKI_INFO("loki_multivariate " + std::string(loki::VERSION_STRING) + " started.");
    LOKI_INFO("Config:    " + args.configPath.string());
    LOKI_INFO("Workspace: " + cfg.workspace.string());

    // -- Create output directories --------------------------------------------
    try {
        std::filesystem::create_directories(cfg.logDir);
        std::filesystem::create_directories(cfg.csvDir);
        std::filesystem::create_directories(cfg.imgDir);
        std::filesystem::create_directories(cfg.protocolsDir);
    } catch (const std::exception& ex) {
        LOKI_ERROR(std::string("Cannot create output directories: ") + ex.what());
        return EXIT_FAILURE;
    }

    // -- Assemble multivariate series -----------------------------------------
    // MultivariateAssembler loads each file via Loader, synchronises timestamps,
    // gap-fills, and optionally standardises all channels into one matrix.
    loki::multivariate::MultivariateSeries mvSeries;
    try {
        loki::multivariate::MultivariateAssembler assembler(cfg);
        mvSeries = assembler.assemble();

        LOKI_INFO("Assembly complete: "
                  + std::to_string(mvSeries.nObs()) + " obs x "
                  + std::to_string(mvSeries.nChannels()) + " channels.");

        // Log channel names.
        for (int j = 0; j < static_cast<int>(mvSeries.nChannels()); ++j) {
            LOKI_INFO("  Channel[" + std::to_string(j) + "] = "
                      + mvSeries.channelName(j));
        }
    } catch (const loki::LOKIException& ex) {
        LOKI_ERROR(std::string("Assembly failed: ") + ex.what());
        return EXIT_FAILURE;
    }

    // -- Descriptive statistics -----------------------------------------------
    if (cfg.stats.enabled) {
        try {
            for (int j = 0; j < static_cast<int>(mvSeries.nChannels()); ++j) {
                const Eigen::VectorXd col = mvSeries.data().col(j);
                std::vector<double> vals(static_cast<std::size_t>(col.size()));
                for (Eigen::Index i = 0; i < col.size(); ++i) {
                    vals[static_cast<std::size_t>(i)] = col(i);
                }
                const auto st = loki::stats::summarize(
                    vals, cfg.stats.nanPolicy, cfg.stats.hurst);
                LOKI_INFO(loki::stats::formatSummary(st, mvSeries.channelName(j)));
            }
        } catch (const loki::LOKIException& ex) {
            LOKI_ERROR(std::string("Descriptive statistics failed: ") + ex.what());
        }
    }

    // -- Build dataset stem for output naming ---------------------------------
    // Use first file stem as dataset identifier.
    std::string stem = "multivariate";
    if (!cfg.multivariate.input.files.empty()) {
        stem = std::filesystem::path(
            cfg.multivariate.input.files[0].path).stem().string();
    }

    // -- Run analysis ---------------------------------------------------------
    loki::multivariate::MultivariateResult result;
    try {
        loki::multivariate::MultivariateAnalyzer analyzer(cfg);
        result = analyzer.run(mvSeries, stem);
    } catch (const loki::LOKIException& ex) {
        LOKI_ERROR(std::string("Analysis failed: ") + ex.what());
        return EXIT_FAILURE;
    }

    // -- Plots ----------------------------------------------------------------
    try {
        loki::multivariate::PlotMultivariate plotter(cfg);
        plotter.plotAll(mvSeries, result, stem);
    } catch (const loki::LOKIException& ex) {
        LOKI_ERROR(std::string("Plotting failed: ") + ex.what());
        // Non-fatal -- continue to finish.
    }

    // -- Protocol -------------------------------------------------------------
    try {
        loki::multivariate::MultivariateProtocol protocol(cfg);
        protocol.write(mvSeries, result, stem);
    } catch (const loki::LOKIException& ex) {
        LOKI_ERROR(std::string("Protocol writing failed: ") + ex.what());
    }

    LOKI_INFO("loki_multivariate finished successfully.");
    return EXIT_SUCCESS;
}
