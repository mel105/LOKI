#include <loki/stats/bootstrap.hpp>
#include <loki/stats/sampling.hpp>
#include <loki/stats/descriptive.hpp>
#include <loki/io/gnuplot.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
//  Paths
// ---------------------------------------------------------------------------

static const std::filesystem::path DEMO_DIR =
    std::filesystem::path(__FILE__).parent_path();
static const std::filesystem::path INPUT_DIR   = DEMO_DIR / "input";
static const std::filesystem::path PNG_DIR     = DEMO_DIR / "png";
static const std::filesystem::path PROTOCOL_DIR = DEMO_DIR / "protocol";

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------

static std::string fwdSlash(const std::filesystem::path& p)
{
    std::string s = p.string();
    std::replace(s.begin(), s.end(), '\\', '/');
    return s;
}

/// Loads a CLIM_DATA_EX1 style file: "YYYY-MM-DD HH:MM:SS value"
static std::vector<double> loadClimData(const std::filesystem::path& path)
{
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        throw std::runtime_error("Cannot open: " + path.string());
    }

    std::vector<double> values;
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '%' || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string date, time;
        double v = 0.0;
        if (!(ss >> date >> time >> v)) continue;
        values.push_back(v);
    }
    return values;
}

/// Plots bootstrap CI as a horizontal band with the estimate marked.
static void plotBootstrapCI(const std::string&           label,
                             const loki::stats::BootstrapResult& pct,
                             const loki::stats::BootstrapResult& bca,
                             const loki::stats::BootstrapResult& blk,
                             const std::filesystem::path& outPng)
{
    loki::Gnuplot gp;
    gp("set terminal pngcairo noenhanced font 'Sans,12' size 900,400");
    gp("set output '" + fwdSlash(outPng) + "'");
    gp("set title 'Bootstrap CI comparison -- " + label + "'");
    gp("set xlabel 'Value'");
    gp("set yrange [0:4]");
    gp("set ytics ('Percentile' 1, 'BCa' 2, 'Block' 3)");
    gp("set grid x");
    gp("unset key");

    // Draw CI bars as horizontal error bars via arrows.
    auto arrow = [&](double y, double lo, double hi, double est, const std::string& color) {
        gp("set arrow from " + std::to_string(lo) + "," + std::to_string(y)
           + " to " + std::to_string(hi) + "," + std::to_string(y)
           + " nohead lc rgb '" + color + "' lw 3");
        gp("set arrow from " + std::to_string(lo) + "," + std::to_string(y - 0.15)
           + " to " + std::to_string(lo) + "," + std::to_string(y + 0.15)
           + " nohead lc rgb '" + color + "' lw 2");
        gp("set arrow from " + std::to_string(hi) + "," + std::to_string(y - 0.15)
           + " to " + std::to_string(hi) + "," + std::to_string(y + 0.15)
           + " nohead lc rgb '" + color + "' lw 2");
        // Estimate point via label.
        gp("set label at " + std::to_string(est) + "," + std::to_string(y)
           + " '+' center tc rgb '" + color + "'");
    };

    arrow(1.0, pct.lower, pct.upper, pct.estimate, "#4477AA");
    arrow(2.0, bca.lower, bca.upper, bca.estimate, "#AA4444");
    arrow(3.0, blk.lower, blk.upper, blk.estimate, "#44AA44");

    gp("plot NaN notitle");
}

// ---------------------------------------------------------------------------
//  main
// ---------------------------------------------------------------------------

int main()
{
    std::filesystem::create_directories(PNG_DIR);
    std::filesystem::create_directories(PROTOCOL_DIR);

    constexpr uint64_t SEED       = 123;
    constexpr int      N_RESAMPLE = 1999;

    // Load climate data.
    const std::filesystem::path dataPath = INPUT_DIR / "CLIM_DATA_EX1.txt";
    std::vector<double> data;
    try {
        data = loadClimData(dataPath);
    } catch (const std::exception& ex) {
        std::cerr << "[ERROR] " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    std::cout << "=== demo_bootstrap ===\n\n";
    std::cout << "Loaded " << data.size() << " observations from "
              << dataPath.filename().string() << "\n\n";

    // Use the first 2000 observations for speed (full 36k would be slow with BCa jackknife).
    constexpr std::size_t SUBSET = 2000;
    if (data.size() > SUBSET) {
        data.resize(SUBSET);
        std::cout << "Using first " << SUBSET << " observations for demo.\n\n";
    }

    loki::stats::Sampler sampler(SEED);

    // Statistic: sample mean.
    loki::stats::StatFn meanFn = [](const std::vector<double>& v) -> double {
        if (v.empty()) return 0.0;
        return std::accumulate(v.begin(), v.end(), 0.0)
               / static_cast<double>(v.size());
    };

    // Statistic: sample median.
    loki::stats::StatFn medianFn = [](const std::vector<double>& v) -> double {
        if (v.empty()) return 0.0;
        std::vector<double> s = v;
        std::sort(s.begin(), s.end());
        const std::size_t m = s.size() / 2;
        return (s.size() % 2 == 0) ? (s[m - 1] + s[m]) / 2.0 : s[m];
    };

    loki::stats::BootstrapConfig cfg;
    cfg.nResamples  = N_RESAMPLE;
    cfg.alpha       = 0.05;
    cfg.blockLength = 40;   // ~10 days at 6h resolution

    // -----------------------------------------------------------------------
    //  Mean CI
    // -----------------------------------------------------------------------
    std::cout << "Computing CI for mean (n=" << data.size()
              << ", nResample=" << N_RESAMPLE << ")...\n";

    const auto pctMean = loki::stats::percentileCI(data, meanFn, sampler, cfg);
    const auto bcaMean = loki::stats::bcaCI      (data, meanFn, sampler, cfg);
    const auto blkMean = loki::stats::blockCI    (data, meanFn, sampler, cfg);

    std::cout << "  Estimate:   " << std::fixed << std::setprecision(6) << pctMean.estimate << "\n";
    std::cout << "  Percentile: [" << pctMean.lower << ", " << pctMean.upper << "]\n";
    std::cout << "  BCa:        [" << bcaMean.lower << ", " << bcaMean.upper << "]\n";
    std::cout << "  Block:      [" << blkMean.lower << ", " << blkMean.upper << "]\n\n";

    plotBootstrapCI("mean", pctMean, bcaMean, blkMean,
                    PNG_DIR / "bootstrap_mean_ci.png");

    // -----------------------------------------------------------------------
    //  Median CI
    // -----------------------------------------------------------------------
    std::cout << "Computing CI for median...\n";

    const auto pctMed = loki::stats::percentileCI(data, medianFn, sampler, cfg);
    const auto bcaMed = loki::stats::bcaCI       (data, medianFn, sampler, cfg);
    const auto blkMed = loki::stats::blockCI     (data, medianFn, sampler, cfg);

    std::cout << "  Estimate:   " << std::fixed << std::setprecision(6) << pctMed.estimate << "\n";
    std::cout << "  Percentile: [" << pctMed.lower << ", " << pctMed.upper << "]\n";
    std::cout << "  BCa:        [" << bcaMed.lower << ", " << bcaMed.upper << "]\n";
    std::cout << "  Block:      [" << blkMed.lower << ", " << blkMed.upper << "]\n\n";

    plotBootstrapCI("median", pctMed, bcaMed, blkMed,
                    PNG_DIR / "bootstrap_median_ci.png");

    // -----------------------------------------------------------------------
    //  Protocol
    // -----------------------------------------------------------------------
    {
        const std::filesystem::path protoPath = PROTOCOL_DIR / "bootstrap_demo.txt";
        std::ofstream ofs(protoPath);

        ofs << std::fixed << std::setprecision(6);
        ofs << "============================================================\n";
        ofs << " BOOTSTRAP DEMO PROTOCOL\n";
        ofs << "============================================================\n";
        ofs << " Dataset:     " << dataPath.filename().string() << "\n";
        ofs << " N:           " << data.size()  << "\n";
        ofs << " N resamples: " << N_RESAMPLE   << "\n";
        ofs << " Block length:" << cfg.blockLength << " (auto=no)\n";
        ofs << " Alpha:       " << cfg.alpha    << " (95% CI)\n";
        ofs << " Seed:        " << SEED         << "\n\n";

        auto writeCI = [&](const std::string& stat,
                           const loki::stats::BootstrapResult& p,
                           const loki::stats::BootstrapResult& b,
                           const loki::stats::BootstrapResult& bl) {
            ofs << " Statistic: " << stat << "\n";
            ofs << "   Estimate  = " << p.estimate << "\n";
            ofs << "   SE        = " << p.se       << "\n";
            ofs << "   Bias      = " << p.bias     << "\n";
            ofs << "   Percentile CI: [" << p.lower  << ", " << p.upper  << "]\n";
            ofs << "   BCa CI:        [" << b.lower  << ", " << b.upper  << "]\n";
            ofs << "   Block CI:      [" << bl.lower << ", " << bl.upper << "]\n\n";
        };

        writeCI("mean",   pctMean, bcaMean, blkMean);
        writeCI("median", pctMed,  bcaMed,  blkMed);

        ofs << " Plots:\n";
        ofs << "   bootstrap_mean_ci.png\n";
        ofs << "   bootstrap_median_ci.png\n";
        ofs << "============================================================\n";

        std::cout << "Protocol written -> " << protoPath.string() << "\n";
    }

    std::cout << "\ndemo_bootstrap finished.\n";
    return EXIT_SUCCESS;
}
