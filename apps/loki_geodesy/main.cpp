#include <loki/core/config.hpp>
#include <loki/core/configLoader.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/core/version.hpp>
#include <loki/io/geodesyLoader.hpp>
#include <loki/geodesy/geodesyAnalyzer.hpp>
#include <loki/geodesy/geodesyResult.hpp>
#include <loki/geodesy/plotGeodesy.hpp>

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

static void printVersion()
{
    std::cout << "loki_geodesy " << loki::VERSION_STRING << "\n";
}

static void printHelp()
{
    std::cout
        << "loki_geodesy " << loki::VERSION_STRING
        << " -- geodetic coordinate transformations with covariance propagation\n"
        << "\nUsage:\n"
        << "  loki_geodesy.exe <config.json> [options]\n"
        << "\nOptions:\n"
        << "  --help     Show this message and exit.\n"
        << "  --version  Show version string and exit.\n"
        << "\nTasks (geodesy.task in config):\n"
        << "  transform    -- Transform coordinates + propagate covariance.\n"
        << "  distance     -- Compute geodesic distance between point pairs.\n"
        << "  monte_carlo  -- Transform + Monte Carlo covariance validation.\n"
        << "\nCoordinate systems (geodesy.input.coord_system):\n"
        << "  ecef   -- Earth-Centred Earth-Fixed: X, Y, Z [m].\n"
        << "  geod   -- Geodetic: lat [deg], lon [deg], h [m].\n"
        << "  sphere -- Spherical: lat [deg], lon [deg], radius [m].\n"
        << "  enu    -- Local topocentric: E [m], N [m], U [m].\n"
        << "\nEllipsoid models:\n"
        << "  WGS84, GRS80, Bessel, Krasovsky, Clarke1866\n";
}

struct CliArgs {
    bool                  showHelp   { false };
    bool                  showVersion{ false };
    std::filesystem::path configPath { "config/geodesy.json" };
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
            std::cerr << "[loki_geodesy] Unknown option: " << arg
                      << "  (use --help)\n";
        }
    }
    return args;
}

// ---------------------------------------------------------------------------
// Build GeodesyAnalyzer config from AppConfig
// ---------------------------------------------------------------------------

static loki::geodesy::GeodesyConfig buildAnalyzerConfig(const loki::AppConfig& cfg)
{
    loki::geodesy::GeodesyConfig gcfg;

    gcfg.task         = loki::geodesy::geodesyTaskFromString(cfg.geodesy.task);
    gcfg.inputSystem  = loki::geodesy::inputCoordSystemFromString(
                            cfg.geodesy.input.coordSystem);
    gcfg.outputSystem = loki::geodesy::inputCoordSystemFromString(
                            cfg.geodesy.outputSystem);
    gcfg.ellipsoidModel  = loki::math::ellipsoidFromString(cfg.geodesy.ellipsoid);
    gcfg.ellipsoidName   = cfg.geodesy.ellipsoid;
    gcfg.refBody = (cfg.geodesy.refBody == "sphere")
                   ? loki::geodesy::RefBody::SPHERE
                   : loki::geodesy::RefBody::ELLIPSOID;
    gcfg.stateSize       = cfg.geodesy.input.stateSize;
    gcfg.distMethod      = loki::geodesy::distanceMethodFromString(
                               cfg.geodesy.distanceMethod);
    gcfg.mcSamples       = cfg.geodesy.mcSamples;
    gcfg.mcTolerance     = cfg.geodesy.mcTolerance;
    gcfg.enuOrigin.lat   = cfg.geodesy.enuOrigin.lat;
    gcfg.enuOrigin.lon   = cfg.geodesy.enuOrigin.lon;
    gcfg.enuOrigin.h     = cfg.geodesy.enuOrigin.h;
    gcfg.outputDir       = cfg.csvDir.string() + "/";

    return gcfg;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

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
        loki::Logger::initDefault(cfg.logDir, "loki_geodesy", cfg.output.logLevel);
    } catch (const loki::LOKIException& ex) {
        std::cerr << "[LOKI ERROR] Cannot init logger: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    LOKI_INFO("loki_geodesy " + std::string(loki::VERSION_STRING) + " started.");
    LOKI_INFO("Config:    " + args.configPath.string());
    LOKI_INFO("Workspace: " + cfg.workspace.string());

    std::filesystem::create_directories(cfg.logDir);
    std::filesystem::create_directories(cfg.csvDir);
    std::filesystem::create_directories(cfg.imgDir);
    std::filesystem::create_directories(cfg.protocolsDir);

    // Load geodetic data
    loki::io::GeodesyLoadResult loadResult;
    try {
        loki::io::GeodesyLoader loader(cfg);
        loadResult = loader.load();
    } catch (const loki::LOKIException& ex) {
        LOKI_ERROR(std::string("Data loading failed: ") + ex.what());
        return EXIT_FAILURE;
    }

    if (loadResult.positions.empty()) {
        LOKI_WARNING("No points found -- nothing to do.");
        return EXIT_SUCCESS;
    }

    const std::string datasetName = loadResult.filePath.stem().string().empty()
        ? "data" : loadResult.filePath.stem().string();

    LOKI_INFO("Loaded " + std::to_string(loadResult.positions.size())
              + " points from '" + loadResult.filePath.string() + "'");

    // Build analyzer config and run
    loki::geodesy::GeodesyConfig  gcfg     = buildAnalyzerConfig(cfg);
    loki::geodesy::GeodesyAnalyzer analyzer(gcfg);

    loki::geodesy::GeodesyResult result;
    try {
        result = analyzer.run(loadResult);
    } catch (const loki::LOKIException& ex) {
        LOKI_ERROR(std::string("Geodesy analysis failed: ") + ex.what());
        return EXIT_FAILURE;
    }

    // Plots
    loki::geodesy::PlotGeodesy plotter(gcfg, datasetName);

    // Error ellipse for first point (if covariance available)
    if (!result.transforms.empty()) {
        const auto& tr = result.transforms[0];
        int N = static_cast<int>(tr.outputCov.rows());
        if (N >= 2 && tr.outputCov.norm() > 1e-30) {
            Eigen::Matrix2d sub;
            sub << tr.outputCov(0, 0), tr.outputCov(0, 1),
                   tr.outputCov(1, 0), tr.outputCov(1, 1);
            plotter.plotErrorEllipse(sub, cfg.geodesy.confidenceLevel,
                                     "Component 0", "Component 1", "c01");
        }
    }

    // Monte Carlo covariance panel
    if (result.hasMonteCarlo) {
        int N = static_cast<int>(result.monteCarlo.analyticalCov.rows());
        std::vector<std::string> labels;
        for (int i = 0; i < N; ++i)
            labels.push_back("c" + std::to_string(i));
        Eigen::MatrixXd dummy = Eigen::MatrixXd::Zero(N, 0);
        plotter.plotCovariancePanel(result.monteCarlo, dummy, labels,
                                    cfg.geodesy.confidenceLevel);
    }

    // Geodesic distances chart
    if (result.hasLines)
        plotter.plotDistances(result.lines);

    LOKI_INFO("loki_geodesy finished successfully.");
    return EXIT_SUCCESS;
}
