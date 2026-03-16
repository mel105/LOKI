#include "loki/core/logger.hpp"
#include "loki/core/configLoader.hpp"
#include "loki/io/dataManager.hpp"
#include "loki/io/plot.hpp"

#include <filesystem>
#include <iostream>

int main(int argc, char* argv[])
{
    // Locate config file
    const std::filesystem::path configPath =
        (argc > 1) ? argv[1] : "config/loki_homogeneity.json";

    // Load configuration
    loki::AppConfig cfg;
    try {
        cfg = loki::ConfigLoader::load(configPath);
    } catch (const loki::LOKIException& ex) {
        std::cerr << "[LOKI ERROR] " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    // Initialise logger
    try {
        loki::Logger::initDefault(cfg.logDir, "loki", cfg.output.logLevel);
    } catch (const loki::LOKIException& ex) {
        std::cerr << "[LOKI ERROR] Cannot initialise logger: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    LOKI_INFO("LOKI started.");
    LOKI_INFO("Config:    " + configPath.string());
    LOKI_INFO("Workspace: " + cfg.workspace.string());

    // Load data
    try {
        loki::DataManager dm(cfg);
        const auto results = dm.load();

        for (const auto& r : results) {
            LOKI_INFO("File: " + r.filePath.filename().string()
                      + "  |  lines read: "  + std::to_string(r.linesRead)
                      + "  |  skipped: "     + std::to_string(r.linesSkipped)
                      + "  |  records: "     + std::to_string(
                            r.series.empty() ? 0 : r.series[0].size()));

            for (std::size_t i = 0; i < r.series.size(); ++i) {
                LOKI_INFO("  Series [" + std::to_string(i) + "] "
                          + r.columnNames[i]
                          + "  size=" + std::to_string(r.series[i].size()));
            }
        }
    } catch (const loki::LOKIException& ex) {
        LOKI_ERROR(std::string("Data loading failed: ") + ex.what());
        return EXIT_FAILURE;
    }

    // Test plotting
    try {
        loki::Plot plot(cfg);
        
        for (const auto& r : results) {
            for (const auto& ts : r.series) {
                if (cfg.plots.timeSeries) plot.timeSeries(ts);
                if (cfg.plots.histogram)  plot.histogram(ts);
                if (cfg.plots.qqPlot)     plot.qqPlot(ts);
                if (cfg.plots.boxplot)    plot.boxplot(ts);
            }
        }
    } catch (const loki::LOKIException& ex) {
        LOKI_ERROR(std::string("Plotting failed: ") + ex.what());
        return EXIT_FAILURE;
    }

    LOKI_INFO("LOKI finished successfully.");
    return EXIT_SUCCESS;
}