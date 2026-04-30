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

static loki::geodesy::GeodesyConfig buildAnalyzerConfig(const loki::AppConfig& cfg)
{
    loki::geodesy::GeodesyConfig gcfg;

    gcfg.task           = loki::geodesy::geodesyTaskFromString(cfg.geodesy.task);
    gcfg.inputSystem    = cfg.geodesy.input.coordSystem;
    gcfg.outputSystem   = cfg.geodesy.outputSystem;
    gcfg.ellipsoidModel = loki::math::ellipsoidFromString(cfg.geodesy.ellipsoid);
    gcfg.ellipsoidName  = cfg.geodesy.ellipsoid;
    gcfg.refBody        = cfg.geodesy.refBody;
    gcfg.stateSize      = cfg.geodesy.input.stateSize;
    gcfg.distMethod     = loki::geodesy::distanceMethodFromString(
                              cfg.geodesy.distanceMethod);
    gcfg.mcSamples      = cfg.geodesy.mcSamples;
    gcfg.mcTolerance    = cfg.geodesy.mcTolerance;
    gcfg.enuOriginLat   = cfg.geodesy.enuOrigin.lat;
    gcfg.enuOriginLon   = cfg.geodesy.enuOrigin.lon;
    gcfg.enuOriginH     = cfg.geodesy.enuOrigin.h;
    gcfg.protocolDir    = cfg.protocolsDir.string();
    gcfg.imgDir         = cfg.imgDir.string();
    gcfg.csvDir         = cfg.csvDir.string();

    return gcfg;
}

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

    loki::geodesy::GeodesyConfig   gcfg     = buildAnalyzerConfig(cfg);
    loki::geodesy::GeodesyAnalyzer analyzer(gcfg);

    loki::geodesy::GeodesyResult result;
    try {
        result = analyzer.run(loadResult);
    } catch (const loki::LOKIException& ex) {
        LOKI_ERROR(std::string("Geodesy analysis failed: ") + ex.what());
        return EXIT_FAILURE;
    }

    loki::geodesy::PlotGeodesy plotter(gcfg, datasetName);

    // Monte Carlo: one combined panel per point showing both covariance matrices.
    // Scatter = MC samples (empirical reality)
    // Blue ellipse   = analytical cov (Jacobian propagation)
    // Green ellipse  = empirical cov (computed from MC samples)
    // Red dashed     = Helmert curve (analytical)
    if (gcfg.task == loki::geodesy::GeodesyTask::MONTE_CARLO) {
        for (std::size_t i = 0; i < result.transforms.size(); ++i) {
            const auto& tr = result.transforms[i];
            if (tr.mcSamples.cols() < 2) continue;

            // Skip points where MC was not run (no empirical cov)
            if (tr.empiricalCov.rows() == 0) continue;

            std::string ptTag = gcfg.inputSystem + "_to_" + gcfg.outputSystem
                                + "_pt" + std::to_string(i);

            std::string panelTitle = gcfg.inputSystem + " -> " + gcfg.outputSystem
                                     + "  pt" + std::to_string(i)
                                     + "  (blue=analytical, green=empirical)";

            plotter.plotCovariancePanel(
                tr.mcSamples,
                tr.outputCov,       // analytical: Jacobian-propagated
                tr.empiricalCov,    // empirical:  cov(MC samples)
                tr.empiricalMean,
                cfg.geodesy.confidenceLevel,
                ptTag,
                panelTitle);
        }
    }

    if (result.hasLines)
        plotter.plotDistances(result.lines);

    LOKI_INFO("loki_geodesy finished successfully.");
    return EXIT_SUCCESS;
}
