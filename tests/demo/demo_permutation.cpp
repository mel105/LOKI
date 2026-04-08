#include <loki/stats/permutation.hpp>
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
static const std::filesystem::path INPUT_DIR    = DEMO_DIR / "input";
static const std::filesystem::path PNG_DIR      = DEMO_DIR / "png";
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

/// Plots the permutation distribution as histogram with observed statistic marked.
static void plotPermDist(const std::vector<double>& permDist,
                          double                     observed,
                          const std::string&         title,
                          const std::string&         xlabel,
                          const std::filesystem::path& outPng)
{
    // Histogram of permutation distribution.
    const double mn = *std::min_element(permDist.begin(), permDist.end());
    const double mx = *std::max_element(permDist.begin(), permDist.end());
    const int    nBins = 40;
    const double w = (mx - mn) / static_cast<double>(nBins);
    const double n = static_cast<double>(permDist.size());

    std::vector<std::pair<double,double>> bins(static_cast<std::size_t>(nBins));
    for (int i = 0; i < nBins; ++i) {
        bins[static_cast<std::size_t>(i)] = {mn + (i + 0.5) * w, 0.0};
    }
    for (double v : permDist) {
        int idx = static_cast<int>((v - mn) / w);
        if (idx < 0)      idx = 0;
        if (idx >= nBins) idx = nBins - 1;
        bins[static_cast<std::size_t>(idx)].second += 1.0;
    }

    loki::Gnuplot gp;
    gp("set terminal pngcairo noenhanced font 'Sans,12' size 800,500");
    gp("set output '" + fwdSlash(outPng) + "'");
    gp("set title '" + title + "'");
    gp("set xlabel '" + xlabel + "'");
    gp("set ylabel 'Count'");
    gp("set style fill solid 0.7");
    gp("set boxwidth 0.9 relative");

    // Vertical line at observed statistic.
    gp("set arrow from " + std::to_string(observed) + ", graph 0"
       + " to " + std::to_string(observed) + ", graph 1"
       + " nohead lc rgb '#CC0000' lw 2 dt 2");
    gp("set label 'observed' at " + std::to_string(observed)
       + ", graph 0.95 left tc rgb '#CC0000'");

    gp("plot '-' using 1:2 with boxes lc rgb '#4477AA' title 'permutation dist'");
    for (const auto& [mid, cnt] : bins) {
        gp(std::to_string(mid) + " " + std::to_string(cnt / n));
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

    constexpr uint64_t SEED          = 99;
    constexpr int      N_PERMS       = 9999;
    constexpr std::size_t SUBSET     = 1000;

    // Load climate data.
    const std::filesystem::path dataPath = INPUT_DIR / "CLIM_DATA_EX1.txt";
    std::vector<double> data;
    try {
        data = loadClimData(dataPath);
    } catch (const std::exception& ex) {
        std::cerr << "[ERROR] " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    std::cout << "=== demo_permutation ===\n\n";
    std::cout << "Loaded " << data.size() << " observations.\n";

    if (data.size() > SUBSET) {
        data.resize(SUBSET);
        std::cout << "Using first " << SUBSET << " observations for demo.\n\n";
    }

    loki::stats::Sampler sampler(SEED);

    loki::stats::PermutationConfig cfg;
    cfg.nPermutations = N_PERMS;
    cfg.alpha         = 0.05;
    cfg.alternative   = loki::stats::Alternative::TWO_SIDED;

    // -----------------------------------------------------------------------
    //  1. One-sample test: H0: mean = 0.1 (plausible IWV reference value)
    // -----------------------------------------------------------------------
    const double refMean = 0.1;

    loki::stats::StatFn meanFn = [](const std::vector<double>& v) -> double {
        return std::accumulate(v.begin(), v.end(), 0.0)
               / static_cast<double>(v.size());
    };

    std::cout << "Test 1: one-sample  H0: mean = " << refMean << "\n";
    const auto r1 = loki::stats::oneSampleTest(data, meanFn, refMean, sampler, cfg);
    std::cout << "  statistic = " << std::fixed << std::setprecision(6) << r1.statistic << "\n";
    std::cout << "  p-value   = " << r1.pValue  << "\n";
    std::cout << "  rejected  = " << (r1.rejected ? "YES" : "NO") << "\n\n";

    // -----------------------------------------------------------------------
    //  2. Two-sample test: H0: mean(first half) = mean(second half)
    // -----------------------------------------------------------------------
    const std::size_t half = data.size() / 2;
    const std::vector<double> groupA(data.begin(), data.begin()
                                     + static_cast<std::ptrdiff_t>(half));
    const std::vector<double> groupB(data.begin()
                                     + static_cast<std::ptrdiff_t>(half), data.end());

    loki::stats::StatFn2 diffMeanFn = [](const std::vector<double>& a,
                                          const std::vector<double>& b) -> double {
        const double ma = std::accumulate(a.begin(), a.end(), 0.0)
                          / static_cast<double>(a.size());
        const double mb = std::accumulate(b.begin(), b.end(), 0.0)
                          / static_cast<double>(b.size());
        return ma - mb;
    };

    std::cout << "Test 2: two-sample  H0: mean(first half) = mean(second half)\n";
    const auto r2 = loki::stats::twoSampleTest(groupA, groupB, diffMeanFn, sampler, cfg);
    std::cout << "  statistic = " << std::fixed << std::setprecision(6) << r2.statistic << "\n";
    std::cout << "  p-value   = " << r2.pValue  << "\n";
    std::cout << "  rejected  = " << (r2.rejected ? "YES" : "NO") << "\n\n";

    // -----------------------------------------------------------------------
    //  3. Correlation test: H0: corr(x, lag-1(x)) = 0
    //     Uses data[0..n-2] vs data[1..n-1] to test for lag-1 autocorrelation.
    // -----------------------------------------------------------------------
    const std::size_t nc = data.size() - 1;
    const std::vector<double> xLag0(data.begin(),
                                    data.begin() + static_cast<std::ptrdiff_t>(nc));
    const std::vector<double> xLag1(data.begin() + 1,
                                    data.begin() + static_cast<std::ptrdiff_t>(nc) + 1);

    std::cout << "Test 3: correlation  H0: corr(x[t], x[t-1]) = 0\n";
    const auto r3 = loki::stats::correlationTest(xLag0, xLag1, sampler, cfg);
    std::cout << "  Pearson r = " << std::fixed << std::setprecision(6) << r3.statistic << "\n";
    std::cout << "  p-value   = " << r3.pValue  << "\n";
    std::cout << "  rejected  = " << (r3.rejected ? "YES" : "NO") << "\n\n";

    // -----------------------------------------------------------------------
    //  Plots -- permutation distributions
    // -----------------------------------------------------------------------
    // Regenerate permutation distributions for plotting (separate sampler run).
    // For the two-sample test we plot the distribution visually.
    {
        // Quick re-run with fewer perms just to get the distribution vector.
        // We reuse the same sampler (different sequence is fine for illustration).
        loki::stats::PermutationConfig plotCfg = cfg;
        plotCfg.nPermutations = 999;

        // Two-sample distribution plot.
        // Manually generate the distribution for plotting purposes.
        const std::size_t ntot = groupA.size() + groupB.size();
        std::vector<double> pooled;
        pooled.reserve(ntot);
        for (double v : groupA) pooled.push_back(v);
        for (double v : groupB) pooled.push_back(v);

        std::vector<std::size_t> idx(ntot);
        std::iota(idx.begin(), idx.end(), 0);

        std::vector<double> permDist2;
        permDist2.reserve(static_cast<std::size_t>(plotCfg.nPermutations));
        std::vector<double> pA(groupA.size()), pB(groupB.size());
        for (int r = 0; r < plotCfg.nPermutations; ++r) {
            std::shuffle(idx.begin(), idx.end(), sampler.rng());
            for (std::size_t i = 0; i < groupA.size(); ++i) pA[i] = pooled[idx[i]];
            for (std::size_t i = 0; i < groupB.size(); ++i) pB[i] = pooled[idx[groupA.size() + i]];
            permDist2.push_back(diffMeanFn(pA, pB));
        }

        plotPermDist(permDist2, r2.statistic,
                     "Two-sample permutation -- mean difference",
                     "mean(A) - mean(B)",
                     PNG_DIR / "permutation_twosample.png");

        // Correlation test distribution.
        std::vector<double> yPerm(xLag1.begin(), xLag1.end());
        auto pearsonR = [](const std::vector<double>& x,
                           const std::vector<double>& y) -> double {
            const double n    = static_cast<double>(x.size());
            const double mx   = std::accumulate(x.begin(), x.end(), 0.0) / n;
            const double my   = std::accumulate(y.begin(), y.end(), 0.0) / n;
            double num = 0.0, vx = 0.0, vy = 0.0;
            for (std::size_t i = 0; i < x.size(); ++i) {
                num += (x[i] - mx) * (y[i] - my);
                vx  += (x[i] - mx) * (x[i] - mx);
                vy  += (y[i] - my) * (y[i] - my);
            }
            const double denom = std::sqrt(vx * vy);
            return (denom < 1e-15) ? 0.0 : num / denom;
        };

        std::vector<double> permDistCorr;
        permDistCorr.reserve(static_cast<std::size_t>(plotCfg.nPermutations));
        for (int r = 0; r < plotCfg.nPermutations; ++r) {
            std::shuffle(yPerm.begin(), yPerm.end(), sampler.rng());
            permDistCorr.push_back(pearsonR(xLag0, yPerm));
        }

        plotPermDist(permDistCorr, r3.statistic,
                     "Permutation correlation -- ACF lag-1",
                     "Pearson r",
                     PNG_DIR / "permutation_correlation.png");
    }

    // -----------------------------------------------------------------------
    //  Protocol
    // -----------------------------------------------------------------------
    {
        const std::filesystem::path protoPath = PROTOCOL_DIR / "permutation_demo.txt";
        std::ofstream ofs(protoPath);

        ofs << std::fixed << std::setprecision(6);
        ofs << "============================================================\n";
        ofs << " PERMUTATION TEST DEMO PROTOCOL\n";
        ofs << "============================================================\n";
        ofs << " Dataset:       " << dataPath.filename().string() << "\n";
        ofs << " N:             " << data.size()  << "\n";
        ofs << " N permutations:" << N_PERMS      << "\n";
        ofs << " Alpha:         " << cfg.alpha    << "\n";
        ofs << " Seed:          " << SEED         << "\n\n";

        auto writeResult = [&](const std::string& desc,
                               const loki::stats::PermutationResult& r) {
            ofs << " " << desc << "\n";
            ofs << "   Test:       " << r.testName  << "\n";
            ofs << "   Statistic:  " << r.statistic << "\n";
            ofs << "   p-value:    " << r.pValue    << "\n";
            ofs << "   Rejected:   " << (r.rejected ? "YES" : "NO") << "\n\n";
        };

        writeResult("Test 1: one-sample (H0: mean = " + std::to_string(refMean) + ")", r1);
        writeResult("Test 2: two-sample (H0: mean(A) = mean(B), first vs second half)", r2);
        writeResult("Test 3: correlation (H0: ACF lag-1 = 0)", r3);

        ofs << " Plots:\n";
        ofs << "   permutation_twosample.png\n";
        ofs << "   permutation_correlation.png\n";
        ofs << "============================================================\n";

        std::cout << "Protocol written -> " << protoPath.string() << "\n";
    }

    std::cout << "\ndemo_permutation finished.\n";
    return EXIT_SUCCESS;
}
