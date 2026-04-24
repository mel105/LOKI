#include <loki/core/config.hpp>
#include <loki/core/configLoader.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/core/version.hpp>
#include <loki/spatial/spatialAnalyzer.hpp>
#include <loki/io/spatialLoader.hpp>
#include <loki/spatial/plotSpatial.hpp>

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

static void printVersion()
{
    std::cout << "loki_spatial " << loki::VERSION_STRING << "\n";
}

static void printHelp()
{
    std::cout
        << "loki_spatial " << loki::VERSION_STRING
        << " -- 2-D spatial interpolation\n"
        << "\nUsage:\n"
        << "  loki_spatial.exe <config.json> [options]\n"
        << "\nOptions:\n"
        << "  --help     Show this message and exit.\n"
        << "  --version  Show version string and exit.\n"
        << "\nMethods (spatial.method in config):\n"
        << "  kriging          -- Spatial Kriging (simple/ordinary/universal).\n"
        << "  idw              -- Inverse Distance Weighting.\n"
        << "  rbf              -- Radial Basis Function interpolation.\n"
        << "  natural_neighbor -- Sibson Natural Neighbor interpolation.\n"
        << "  bspline_surface  -- Tensor product B-spline surface.\n"
        << "  nurbs            -- PLACEHOLDER (not implemented in v1).\n"
        << "\nInput file format:\n"
        << "  One observation per line: x y v1 [v2 ...]\n"
        << "  Comment lines start with comment_prefix (#, %, !, //, ;, *).\n"
        << "  Column names extracted from comment lines: # NAME unit desc\n"
        << "  No-data values filtered per column (no_data_value in config).\n";
}

struct CliArgs {
    bool                  showHelp    {false};
    bool                  showVersion {false};
    std::filesystem::path configPath  {"config/spatial.json"};
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
            std::cerr << "[loki_spatial] Unknown option: " << arg
                      << "  (use --help)\n";
        }
    }
    return args;
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
        loki::Logger::initDefault(cfg.logDir, "loki_spatial", cfg.output.logLevel);
    } catch (const loki::LOKIException& ex) {
        std::cerr << "[LOKI ERROR] Cannot init logger: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    LOKI_INFO("loki_spatial " + std::string(loki::VERSION_STRING) + " started.");
    LOKI_INFO("Config:    " + args.configPath.string());
    LOKI_INFO("Workspace: " + cfg.workspace.string());

    std::filesystem::create_directories(cfg.logDir);
    std::filesystem::create_directories(cfg.csvDir);
    std::filesystem::create_directories(cfg.imgDir);
    std::filesystem::create_directories(cfg.protocolsDir);

    // Load spatial scatter data.
    loki::io::SpatialLoadResult loadResult;
    try {
        loki::io::SpatialLoader loader(cfg);
        loadResult = loader.load();
    } catch (const loki::LOKIException& ex) {
        LOKI_ERROR(std::string("Data loading failed: ") + ex.what());
        return EXIT_FAILURE;
    }

    if (loadResult.variables.empty()) {
        LOKI_WARNING("No variable columns found -- nothing to do.");
        return EXIT_SUCCESS;
    }

    const std::string datasetName = loadResult.filePath.stem().string().empty()
        ? "data" : loadResult.filePath.stem().string();

    loki::spatial::SpatialAnalyzer analyzer(cfg);
    loki::spatial::PlotSpatial     plotter(cfg);

    for (std::size_t v = 0; v < loadResult.variables.size(); ++v) {
        const auto& pts     = loadResult.variables[v];
        const auto& varName = loadResult.varNames[v];

        if (pts.size() < 3) {
            LOKI_WARNING("Variable '" + varName + "' has fewer than 3 valid points ("
                         + std::to_string(pts.size()) + ") -- skipped.");
            continue;
        }

        LOKI_INFO("Processing variable '" + varName
                  + "'  n=" + std::to_string(pts.size()));
        try {
            const auto result = analyzer.run(pts, datasetName, varName);
            plotter.plot(result, datasetName);
        } catch (const loki::LOKIException& ex) {
            LOKI_ERROR("Spatial analysis failed for '" + varName + "': " + ex.what());
        }
    }

    LOKI_INFO("loki_spatial finished successfully.");
    return EXIT_SUCCESS;
}
