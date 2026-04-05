#include <loki/decomposition/decompositionAnalyzer.hpp>
#include <loki/decomposition/classicalDecomposer.hpp>
#include <loki/decomposition/stlDecomposer.hpp>
#include <loki/decomposition/plotDecomposition.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/stats/descriptive.hpp>
#include <loki/timeseries/gapFiller.hpp>

#include <cmath>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <filesystem>
#include <string>

using namespace loki;

// -----------------------------------------------------------------------------

DecompositionAnalyzer::DecompositionAnalyzer(const AppConfig& cfg)
    : m_cfg{cfg}
{}

// -----------------------------------------------------------------------------

void DecompositionAnalyzer::run(const TimeSeries& ts,
                                 const std::string& datasetName) const
{
    const auto& dcfg = m_cfg.decomposition;
    const std::string compName = ts.metadata().componentName;

    LOKI_INFO("DecompositionAnalyzer: processing component '" + compName + "'");

    // Step 1: gap filling.
    GapFiller::Config gfCfg;
    gfCfg.strategy      = GapFiller::Strategy::LINEAR;
    gfCfg.maxFillLength = static_cast<std::size_t>(
        dcfg.gapFillMaxLength > 0 ? dcfg.gapFillMaxLength : 0);

    if (dcfg.gapFillStrategy == "forward_fill") {
        gfCfg.strategy = GapFiller::Strategy::FORWARD_FILL;
    } else if (dcfg.gapFillStrategy == "mean") {
        gfCfg.strategy = GapFiller::Strategy::MEAN;
    }

    const GapFiller gf{gfCfg};
    const TimeSeries filled = gf.fill(ts);

    const int n      = static_cast<int>(filled.size());
    const int period = dcfg.period;

    if (n < 2 * period) {
        throw DataException(
            "DecompositionAnalyzer: series '" + compName + "' has " +
            std::to_string(n) + " points after gap filling, need at least 2 * period = " +
            std::to_string(2 * period) + ".");
    }

    // Step 2: decompose.
    DecompositionResult result;

    if (dcfg.method == DecompositionMethodEnum::CLASSICAL) {
        LOKI_INFO("DecompositionAnalyzer: using Classical decomposition, period="
                  + std::to_string(period));
        ClassicalDecomposer decomposer{dcfg.classical};
        result = decomposer.decompose(filled, period);
    } else {
        LOKI_INFO("DecompositionAnalyzer: using STL decomposition, period="
                  + std::to_string(period)
                  + ", n_inner=" + std::to_string(dcfg.stl.nInner)
                  + ", n_outer=" + std::to_string(dcfg.stl.nOuter));
        StlDecomposer decomposer{dcfg.stl};
        result = decomposer.decompose(filled, period);
    }

    LOKI_INFO("DecompositionAnalyzer: decomposition complete");

    // Step 3: protocol + CSV.
    std::filesystem::create_directories(m_cfg.protocolsDir);
    std::filesystem::create_directories(m_cfg.csvDir);

    writeProtocol(filled, result, datasetName, compName);
    writeCsv(filled, result, datasetName, compName);

    // Step 4: plots.
    PlotDecomposition plotter{m_cfg};

    if (m_cfg.plots.decompOverlay) {
        plotter.plotOverlay(filled, result, datasetName, compName);
    }
    if (m_cfg.plots.decompPanels) {
        plotter.plotPanels(filled, result, datasetName, compName);
    }
    if (m_cfg.plots.decompDiagnostics) {
        plotter.plotDiagnostics(result, datasetName, compName);
    }
}

// -----------------------------------------------------------------------------

void DecompositionAnalyzer::writeProtocol(const TimeSeries&          ts,
                                           const DecompositionResult& result,
                                           const std::string&         datasetName,
                                           const std::string&         compName) const
{
    const std::string fname = "decomposition_" + datasetName + "_" + compName + ".txt";
    const std::filesystem::path outPath = m_cfg.protocolsDir / fname;

    std::ofstream ofs(outPath);
    if (!ofs.is_open()) {
        throw IoException("DecompositionAnalyzer: cannot write protocol to '"
                          + outPath.string() + "'.");
    }

    const auto& dcfg = m_cfg.decomposition;
    const int n = static_cast<int>(ts.size());

    // Helper: compute mean and std from a vector.
    auto stats = [&](const std::vector<double>& v) -> std::pair<double, double> {
        const double mean = std::accumulate(v.begin(), v.end(), 0.0)
                            / static_cast<double>(v.size());
        double sq = 0.0;
        for (double x : v) sq += (x - mean) * (x - mean);
        const double std = std::sqrt(sq / static_cast<double>(v.size()));
        return {mean, std};
    };

    const std::string methodStr =
        (result.method == DecompositionMethod::CLASSICAL) ? "Classical" : "STL";

    ofs << std::fixed << std::setprecision(6);
    ofs << "==========================================================\n";
    ofs << " LOKI Decomposition Protocol\n";
    ofs << "==========================================================\n";
    ofs << " Dataset   : " << datasetName << "\n";
    ofs << " Component : " << compName    << "\n";
    ofs << " Method    : " << methodStr   << "\n";
    ofs << " Period    : " << dcfg.period << " samples\n";
    ofs << " N         : " << n           << " observations\n";
    ofs << "----------------------------------------------------------\n";

    if (result.method == DecompositionMethod::CLASSICAL) {
        ofs << " Trend filter  : " << dcfg.classical.trendFilter  << "\n";
        ofs << " Seasonal type : " << dcfg.classical.seasonalType << "\n";
    } else {
        ofs << " n_inner : " << dcfg.stl.nInner << "\n";
        ofs << " n_outer : " << dcfg.stl.nOuter << "\n";
        ofs << " s_degree: " << dcfg.stl.sDegree << "\n";
        ofs << " t_degree: " << dcfg.stl.tDegree << "\n";
    }
    ofs << "----------------------------------------------------------\n";
    ofs << " Component statistics (mean / std):\n";

    // Original.
    std::vector<double> orig(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
        orig[static_cast<std::size_t>(i)] = ts[static_cast<std::size_t>(i)].value;

    auto [om, os] = stats(orig);
    auto [tm, ts2] = stats(result.trend);
    auto [sm, ss] = stats(result.seasonal);
    auto [rm, rs] = stats(result.residual);

    ofs << "   Original : mean=" << om  << "  std=" << os  << "\n";
    ofs << "   Trend    : mean=" << tm  << "  std=" << ts2 << "\n";
    ofs << "   Seasonal : mean=" << sm  << "  std=" << ss  << "\n";
    ofs << "   Residual : mean=" << rm  << "  std=" << rs  << "\n";
    ofs << "----------------------------------------------------------\n";

    // Variance explained by each component.
    const double varOrig = os * os;
    if (varOrig > 0.0) {
        ofs << " Variance explained:\n";
        ofs << "   Trend    : " << std::setprecision(2) << (ts2 * ts2 / varOrig * 100.0) << " %\n";
        ofs << "   Seasonal : " << (ss * ss / varOrig * 100.0) << " %\n";
        ofs << "   Residual : " << (rs * rs / varOrig * 100.0) << " %\n";
    }
    ofs << "==========================================================\n";

    LOKI_INFO("DecompositionAnalyzer: protocol written to '"
              + outPath.string() + "'");
}

// -----------------------------------------------------------------------------

void DecompositionAnalyzer::writeCsv(const TimeSeries&          ts,
                                      const DecompositionResult& result,
                                      const std::string&         datasetName,
                                      const std::string&         compName) const
{
    const std::string fname = "decomposition_" + datasetName + "_" + compName + ".csv";
    const std::filesystem::path outPath = m_cfg.csvDir / fname;

    std::ofstream ofs(outPath);
    if (!ofs.is_open()) {
        throw IoException("DecompositionAnalyzer: cannot write CSV to '"
                          + outPath.string() + "'.");
    }

    ofs << std::fixed << std::setprecision(9);
    ofs << "time_mjd;original;trend;seasonal;residual\n";

    const int n = static_cast<int>(ts.size());
    for (int i = 0; i < n; ++i) {
        const std::size_t si = static_cast<std::size_t>(i);
        ofs << ts[si].time.mjd()     << ";"
            << ts[si].value          << ";"
            << result.trend[si]      << ";"
            << result.seasonal[si]   << ";"
            << result.residual[si]   << "\n";
    }

    LOKI_INFO("DecompositionAnalyzer: CSV written to '" + outPath.string() + "'");
}
