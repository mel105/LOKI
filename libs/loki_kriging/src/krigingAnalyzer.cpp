#include <loki/kriging/krigingAnalyzer.hpp>

#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/kriging/plotKriging.hpp>
#include <loki/math/krigingFactory.hpp>
#include <loki/math/krigingVariogram.hpp>
#include <loki/timeseries/gapFiller.hpp>
#include <loki/timeseries/timeStamp.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>

using namespace loki;

namespace loki::kriging {

// =============================================================================
//  Constructor
// =============================================================================

KrigingAnalyzer::KrigingAnalyzer(const AppConfig& cfg)
    : m_cfg(cfg)
{}

// =============================================================================
//  run()
// =============================================================================

void KrigingAnalyzer::run(const TimeSeries& series, const std::string& datasetName)
{
    const KrigingConfig& kcfg      = m_cfg.kriging;
    const std::string    component = series.metadata().componentName;

    LOKI_INFO("Kriging: dataset='" + datasetName
              + "'  component='" + component + "'");
    LOKI_INFO("  mode="   + kcfg.mode
              + "  method=" + kcfg.method
              + "  variogram=" + kcfg.variogram.model);

    // -------------------------------------------------------------------------
    // 1. Gap filling
    // -------------------------------------------------------------------------
    GapFiller::Config gfCfg;
    gfCfg.strategy      = GapFiller::Strategy::LINEAR;
    if      (kcfg.gapFillStrategy == "spline") gfCfg.strategy = GapFiller::Strategy::SPLINE;
    else if (kcfg.gapFillStrategy == "none")   gfCfg.strategy = GapFiller::Strategy::NONE;
    gfCfg.maxFillLength = kcfg.gapFillMaxLength;

    GapFiller        filler(gfCfg);
    const TimeSeries filled = filler.fill(series);

    LOKI_INFO("  gap fill: n_orig=" + std::to_string(series.size())
              + "  n_filled=" + std::to_string(filled.size()));

    std::vector<double> zObs;
    zObs.reserve(filled.size());
    for (std::size_t i = 0; i < filled.size(); ++i)
        if (!std::isnan(filled[i].value)) zObs.push_back(filled[i].value);

    if (zObs.size() < 5) {
        throw DataException(
            "KrigingAnalyzer::run: series '" + component
            + "' has fewer than 5 valid observations after gap filling ("
            + std::to_string(zObs.size()) + "). Cannot proceed.");
    }

    // -------------------------------------------------------------------------
    // 2. Empirical variogram
    // -------------------------------------------------------------------------
    LOKI_INFO("  computing empirical variogram ...");
    const std::vector<loki::math::VariogramPoint> empirical =
        loki::math::computeEmpiricalVariogram(filled, kcfg.variogram);

    LOKI_INFO("  empirical variogram: " + std::to_string(empirical.size())
              + " bins, maxLag="
              + std::to_string(empirical.empty() ? 0.0 : empirical.back().lag)
              + " days");

    // -------------------------------------------------------------------------
    // 3. Fit theoretical variogram
    // -------------------------------------------------------------------------
    LOKI_INFO("  fitting variogram model '" + kcfg.variogram.model + "' ...");
    const loki::math::VariogramFitResult vfit =
        loki::math::fitVariogram(empirical, kcfg.variogram);

    LOKI_INFO("  variogram fit: nugget=" + std::to_string(vfit.nugget)
              + "  sill="   + std::to_string(vfit.sill)
              + "  range="  + std::to_string(vfit.range)
              + "  RMSE="   + std::to_string(vfit.rmse)
              + (vfit.converged ? "" : "  [NOT CONVERGED]"));

    if (!vfit.converged) {
        LOKI_WARNING("KrigingAnalyzer: variogram fitting did not converge for '"
                     + component + "'. Results may be unreliable.");
    }

    // -------------------------------------------------------------------------
    // 4. Create Kriging estimator and fit
    // -------------------------------------------------------------------------
    auto kriging = loki::math::createKriging(kcfg);
    kriging->fit(filled, vfit);
    LOKI_INFO("  Kriging system built (n=" + std::to_string(zObs.size()) + ").");

    // -------------------------------------------------------------------------
    // 5. Predict
    // -------------------------------------------------------------------------
    const std::vector<double> targets = _buildTargetGrid(filled);
    LOKI_INFO("  predicting at " + std::to_string(targets.size()) + " target points ...");

    const std::vector<loki::math::KrigingPrediction> predictions =
        kriging->predictGrid(targets, kcfg.confidenceLevel);

    // -------------------------------------------------------------------------
    // 6. Cross-validation
    // -------------------------------------------------------------------------
    loki::math::CrossValidationResult cv;
    if (kcfg.crossValidate) {
        LOKI_INFO("  running LOO cross-validation ...");
        cv = kriging->crossValidate(kcfg.confidenceLevel);
        LOKI_INFO("  CV RMSE=" + std::to_string(cv.rmse)
                  + "  MAE="    + std::to_string(cv.mae)
                  + "  meanSE=" + std::to_string(cv.meanSE)
                  + "  meanSSE="+ std::to_string(cv.meanSSE));
    }

    // -------------------------------------------------------------------------
    // 7. Assemble result
    // -------------------------------------------------------------------------
    double meanVal = 0.0, sampleVar = 0.0;
    _computeStats(zObs, meanVal, sampleVar);

    KrigingResult result;
    result.mode               = kcfg.mode;
    result.method             = kcfg.method;
    result.nObs               = static_cast<int>(zObs.size());
    result.meanValue          = meanVal;
    result.sampleVariance     = sampleVar;
    result.variogram          = vfit;
    result.empiricalVariogram = empirical;
    result.predictions        = predictions;
    result.crossValidation    = cv;
    result.componentName      = component;

    // -------------------------------------------------------------------------
    // 8. Output
    // -------------------------------------------------------------------------
    _writeProtocol(result, series, datasetName);
    _writeCsv     (result, datasetName, component);

    PlotKriging plotter(m_cfg);
    plotter.plot(result, filled, datasetName);

    LOKI_INFO("Kriging done: '" + component + "'.");
}

// =============================================================================
//  _buildTargetGrid
// =============================================================================

std::vector<double> KrigingAnalyzer::_buildTargetGrid(const TimeSeries& ts) const
{
    std::vector<double> targets;
    targets.reserve(ts.size());
    for (std::size_t i = 0; i < ts.size(); ++i)
        if (!std::isnan(ts[i].value)) targets.push_back(ts[i].time.mjd());

    const KrigingPredictionConfig& pcfg = m_cfg.kriging.prediction;
    if (pcfg.enabled) {
        for (const double mjd : pcfg.targetMjd) targets.push_back(mjd);
        if (pcfg.horizonDays > 0.0 && pcfg.nSteps >= 1) {
            const double lastMjd = targets.empty() ? 0.0
                : *std::max_element(targets.begin(), targets.end());
            const double step = pcfg.horizonDays / static_cast<double>(pcfg.nSteps);
            for (int k = 1; k <= pcfg.nSteps; ++k)
                targets.push_back(lastMjd + static_cast<double>(k) * step);
        }
    }

    std::sort(targets.begin(), targets.end());
    targets.erase(
        std::unique(targets.begin(), targets.end(),
                    [](double a, double b){ return std::abs(a - b) < 1.0e-9; }),
        targets.end());
    return targets;
}

// =============================================================================
//  _computeStats
// =============================================================================

void KrigingAnalyzer::_computeStats(const std::vector<double>& z,
                                     double& mean, double& var)
{
    if (z.empty()) { mean = 0.0; var = 0.0; return; }
    mean = std::accumulate(z.begin(), z.end(), 0.0) / static_cast<double>(z.size());
    double sq = 0.0;
    for (const double v : z) sq += (v - mean) * (v - mean);
    var = (z.size() > 1) ? sq / static_cast<double>(z.size() - 1) : 0.0;
}

// =============================================================================
//  _writeProtocol
// =============================================================================

void KrigingAnalyzer::_writeProtocol(const KrigingResult& result,
                                      const TimeSeries&    ts,
                                      const std::string&   datasetName) const
{
    const std::string fname = datasetName + "_" + result.componentName
                            + "_kriging_protocol.txt";
    const std::filesystem::path path = m_cfg.protocolsDir / fname;

    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        LOKI_WARNING("KrigingAnalyzer: cannot write protocol to '"
                     + path.string() + "'.");
        return;
    }

    const auto line = [&](const std::string& s){ ofs << s << "\n"; };
    const auto sep  = [&](){ ofs << std::string(70, '-') << "\n"; };

    sep();
    line("KRIGING ANALYSIS PROTOCOL");
    line("Generated by loki_kriging");
    sep();
    line("Dataset       : " + datasetName);
    line("Component     : " + result.componentName);
    line("Mode          : " + result.mode);
    line("Method        : " + result.method);
    line("Observations  : " + std::to_string(result.nObs));
    line("Mean          : " + std::to_string(result.meanValue));
    line("Sample Var    : " + std::to_string(result.sampleVariance));
    line("Sample StdDev : " + std::to_string(std::sqrt(result.sampleVariance)));
    if (!ts.empty()) {
        line("Series start  : " + ts[0].time.utcString());
        line("Series end    : " + ts[ts.size() - 1].time.utcString());
    }
    sep();

    line("VARIOGRAM");
    line("  Model     : " + result.variogram.model);
    line("  Nugget    : " + std::to_string(result.variogram.nugget));
    line("  Sill      : " + std::to_string(result.variogram.sill));
    line("  Range     : " + std::to_string(result.variogram.range) + " days");
    line("  Fit RMSE  : " + std::to_string(result.variogram.rmse));
    line("  Converged : " + std::string(result.variogram.converged ? "yes" : "NO"));
    line("  Empirical bins: " + std::to_string(result.empiricalVariogram.size()));
    sep();

    if (m_cfg.kriging.crossValidate) {
        const auto& cv = result.crossValidation;
        line("CROSS-VALIDATION (Leave-One-Out)");
        line("  RMSE            : " + std::to_string(cv.rmse));
        line("  MAE             : " + std::to_string(cv.mae));
        line("  Mean Std Error  : " + std::to_string(cv.meanSE)  + "  (ideal: 0)");
        line("  Mean Sq Std Err : " + std::to_string(cv.meanSSE) + "  (ideal: 1)");
        sep();
    }

    line("PREDICTIONS");
    line("  Total target points : " + std::to_string(result.predictions.size()));
    const int nForecast = static_cast<int>(std::count_if(
        result.predictions.begin(), result.predictions.end(),
        [](const KrigingPrediction& p){ return !p.isObserved; }));
    line("  Forecast points     : " + std::to_string(nForecast));
    sep();

    LOKI_INFO("  Protocol written: " + path.string());
}

// =============================================================================
//  _writeCsv
// =============================================================================

void KrigingAnalyzer::_writeCsv(const KrigingResult& result,
                                 const std::string&   datasetName,
                                 const std::string&   component) const
{
    const std::string fname = datasetName + "_" + component
                            + "_kriging_predictions.csv";
    const std::filesystem::path path = m_cfg.csvDir / fname;

    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        LOKI_WARNING("KrigingAnalyzer: cannot write CSV to '"
                     + path.string() + "'.");
        return;
    }

    ofs << std::fixed << std::setprecision(9);
    ofs << "mjd;utc;value;variance;ci_lower;ci_upper;is_observed\n";
    for (const auto& p : result.predictions) {
        const ::TimeStamp ts = ::TimeStamp::fromMjd(p.mjd);
        ofs << p.mjd         << ";"
            << ts.utcString() << ";"
            << p.value       << ";"
            << p.variance    << ";"
            << p.ciLower     << ";"
            << p.ciUpper     << ";"
            << (p.isObserved ? 1 : 0) << "\n";
    }

    LOKI_INFO("  CSV written: " + path.string());
}

} // namespace loki::kriging
