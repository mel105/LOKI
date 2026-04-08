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
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
//  Paths
// ---------------------------------------------------------------------------

static const std::filesystem::path DEMO_DIR =
    std::filesystem::path(__FILE__).parent_path();
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

/// Computes a simple histogram of values into nBins bins.
struct Bin {
    double lo, hi, count;
};

static std::vector<Bin> makeHistogram(const std::vector<double>& data, int nBins)
{
    const double mn = *std::min_element(data.begin(), data.end());
    const double mx = *std::max_element(data.begin(), data.end());
    const double w  = (mx - mn) / static_cast<double>(nBins);

    std::vector<Bin> bins(static_cast<std::size_t>(nBins));
    for (int i = 0; i < nBins; ++i) {
        bins[static_cast<std::size_t>(i)] = {mn + i * w, mn + (i + 1) * w, 0.0};
    }
    for (double v : data) {
        int idx = static_cast<int>((v - mn) / w);
        if (idx < 0)     idx = 0;
        if (idx >= nBins) idx = nBins - 1;
        bins[static_cast<std::size_t>(idx)].count += 1.0;
    }
    return bins;
}

/// Plots a histogram of samples with gnuplot inline data.
static void plotHistogram(const std::vector<double>& samples,
                          const std::string&         title,
                          const std::string&         xlabel,
                          const std::filesystem::path& outPng)
{
    const auto bins = makeHistogram(samples, 40);
    const double n  = static_cast<double>(samples.size());

    loki::Gnuplot gp;
    gp("set terminal pngcairo noenhanced font 'Sans,12' size 800,500");
    gp("set output '" + fwdSlash(outPng) + "'");
    gp("set title '" + title + "'");
    gp("set xlabel '" + xlabel + "'");
    gp("set ylabel 'Density'");
    gp("set style fill solid 0.7");
    gp("set boxwidth 0.9 relative");
    gp("plot '-' using 1:2 with boxes lc rgb '#4477AA' notitle");

    for (const auto& b : bins) {
        const double mid     = (b.lo + b.hi) / 2.0;
        const double density = b.count / (n * (b.hi - b.lo));
        gp(std::to_string(mid) + " " + std::to_string(density));
    }
    gp("e");
}

// ---------------------------------------------------------------------------
//  main
// ---------------------------------------------------------------------------

int main()
{
    std::filesystem::create_directories(PNG_DIR);
    std::filesystem::create_directories(PROTOCOL_DIR);

    constexpr int    N_SAMPLES = 5000;
    constexpr uint64_t SEED    = 42;

    loki::stats::Sampler sampler(SEED);

    std::cout << "=== demo_sampling ===\n\n";

    // -----------------------------------------------------------------------
    //  1. Normal distribution
    // -----------------------------------------------------------------------
    const auto normalSamples = sampler.normalVector(N_SAMPLES, 0.0, 1.0);
    plotHistogram(normalSamples, "N(0,1) -- " + std::to_string(N_SAMPLES) + " samples",
                  "x", PNG_DIR / "sampling_normal.png");
    std::cout << "[1] Normal N(0,1):  histogram -> sampling_normal.png\n";

    // -----------------------------------------------------------------------
    //  2. Student-t distribution (df=5)
    // -----------------------------------------------------------------------
    std::vector<double> tSamples(static_cast<std::size_t>(N_SAMPLES));
    for (auto& v : tSamples) v = sampler.studentT(5.0);
    plotHistogram(tSamples, "Student-t (df=5) -- " + std::to_string(N_SAMPLES) + " samples",
                  "x", PNG_DIR / "sampling_studentt.png");
    std::cout << "[2] Student-t(5):   histogram -> sampling_studentt.png\n";

    // -----------------------------------------------------------------------
    //  3. Gamma distribution (shape=2, scale=1)
    // -----------------------------------------------------------------------
    std::vector<double> gammaSamples(static_cast<std::size_t>(N_SAMPLES));
    for (auto& v : gammaSamples) v = sampler.gamma(2.0, 1.0);
    plotHistogram(gammaSamples, "Gamma(shape=2, scale=1) -- " + std::to_string(N_SAMPLES) + " samples",
                  "x", PNG_DIR / "sampling_gamma.png");
    std::cout << "[3] Gamma(2,1):     histogram -> sampling_gamma.png\n";

    // -----------------------------------------------------------------------
    //  4. Laplace distribution (mu=0, scale=1)
    // -----------------------------------------------------------------------
    std::vector<double> laplaceSamples(static_cast<std::size_t>(N_SAMPLES));
    for (auto& v : laplaceSamples) v = sampler.laplace(0.0, 1.0);
    plotHistogram(laplaceSamples, "Laplace(0,1) -- " + std::to_string(N_SAMPLES) + " samples",
                  "x", PNG_DIR / "sampling_laplace.png");
    std::cout << "[4] Laplace(0,1):   histogram -> sampling_laplace.png\n";

    // -----------------------------------------------------------------------
    //  5. Bootstrap indices demo
    // -----------------------------------------------------------------------
    {
        constexpr std::size_t BSERIES_LEN  = 100;
        constexpr std::size_t BLOCK_LEN    = 10;
        const auto iidIdx   = sampler.bootstrapIndices(BSERIES_LEN);
        const auto blockIdx = sampler.blockBootstrapIndices(BSERIES_LEN, BLOCK_LEN);

        std::cout << "[5] Bootstrap indices (n=100):\n";
        std::cout << "    iid   first 10: ";
        for (std::size_t i = 0; i < 10; ++i)
            std::cout << iidIdx[i] << " ";
        std::cout << "\n";
        std::cout << "    block first 10 (blockLen=10): ";
        for (std::size_t i = 0; i < 10; ++i)
            std::cout << blockIdx[i] << " ";
        std::cout << "\n\n";
    }

    // -----------------------------------------------------------------------
    //  Protocol
    // -----------------------------------------------------------------------
    {
        const std::filesystem::path protoPath = PROTOCOL_DIR / "sampling_demo.txt";
        std::ofstream ofs(protoPath);

        const auto normStats = loki::stats::summarize(normalSamples, loki::NanPolicy::SKIP, false);

        ofs << "============================================================\n";
        ofs << " SAMPLING DEMO PROTOCOL\n";
        ofs << "============================================================\n";
        ofs << " Seed:       " << SEED     << "\n";
        ofs << " N samples:  " << N_SAMPLES << "\n\n";
        ofs << " Normal N(0,1) summary:\n";
        ofs << "   mean    = " << std::fixed << std::setprecision(6) << normStats.mean   << "\n";
        ofs << "   std dev = " << normStats.stddev  << "\n";
        ofs << "   min     = " << normStats.min     << "\n";
        ofs << "   max     = " << normStats.max     << "\n\n";
        ofs << " Distributions plotted:\n";
        ofs << "   sampling_normal.png\n";
        ofs << "   sampling_studentt.png\n";
        ofs << "   sampling_gamma.png\n";
        ofs << "   sampling_laplace.png\n";
        ofs << "============================================================\n";

        std::cout << "Protocol written -> " << protoPath.string() << "\n";
    }

    std::cout << "\ndemo_sampling finished.\n";
    return EXIT_SUCCESS;
}
