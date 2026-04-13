#include <loki/spline/splineAnalyzer.hpp>

#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/math/bspline.hpp>
#include <loki/math/bsplineFit.hpp>
#include <loki/spline/plotSpline.hpp>
#include <loki/timeseries/gapFiller.hpp>
#include <loki/timeseries/timeStamp.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <numbers>
#include <iomanip>
#include <numeric>
#include <sstream>

using namespace loki;

namespace loki::spline {

// =============================================================================
//  Constructor
// =============================================================================

SplineAnalyzer::SplineAnalyzer(const AppConfig& cfg)
    : m_cfg(cfg)
{}

// =============================================================================
//  run()
// =============================================================================

void SplineAnalyzer::run(const TimeSeries& series, const std::string& datasetName)
{
    const SplineConfig&   scfg      = m_cfg.spline;
    const BSplineConfig&  bcfg      = scfg.bspline;
    const std::string     component = series.metadata().componentName;

    LOKI_INFO("Spline: dataset='" + datasetName
              + "'  component='" + component + "'");
    LOKI_INFO("  method=" + scfg.method
              + "  fitMode=" + bcfg.fitMode
              + "  degree=" + std::to_string(bcfg.degree));

    // -------------------------------------------------------------------------
    // 0. NURBS guard
    // -------------------------------------------------------------------------
    if (scfg.method == "nurbs") {
        throw AlgorithmException(
            "SplineAnalyzer: method='nurbs' is not yet implemented. "
            "Use method='bspline'.");
    }

    // -------------------------------------------------------------------------
    // 1. Gap filling
    // -------------------------------------------------------------------------
    GapFiller::Config gfCfg;
    gfCfg.strategy      = GapFiller::Strategy::LINEAR;
    if      (scfg.gapFillStrategy == "spline") gfCfg.strategy = GapFiller::Strategy::SPLINE;
    else if (scfg.gapFillStrategy == "none")   gfCfg.strategy = GapFiller::Strategy::NONE;
    gfCfg.maxFillLength = scfg.gapFillMaxLength;

    GapFiller        filler(gfCfg);
    const TimeSeries filled = filler.fill(series);

    LOKI_INFO("  gap fill: n_orig=" + std::to_string(series.size())
              + "  n_filled=" + std::to_string(filled.size()));

    // Collect valid (non-NaN) observations.
    std::vector<double> tObs, zObs;
    tObs.reserve(filled.size());
    zObs.reserve(filled.size());
    for (std::size_t i = 0; i < filled.size(); ++i) {
        if (!std::isnan(filled[i].value)) {
            tObs.push_back(static_cast<double>(i)); // sample index
            zObs.push_back(filled[i].value);
        }
    }

    const int nObs = static_cast<int>(tObs.size());
    if (nObs < bcfg.degree + 2) {
        throw DataException(
            "SplineAnalyzer::run: series '" + component
            + "' has only " + std::to_string(nObs)
            + " valid observations after gap filling. Need at least "
            + std::to_string(bcfg.degree + 2) + " for degree-"
            + std::to_string(bcfg.degree) + " B-spline.");
    }

    // -------------------------------------------------------------------------
    // 2. Knot placement: auto-detect non-uniform sampling
    // -------------------------------------------------------------------------
    std::string knotPlacement = bcfg.knotPlacement;
    bool        autoKnot      = false;
    if (knotPlacement == "uniform" && _isNonUniform(filled)) {
        knotPlacement = "chord_length";
        autoKnot      = true;
        LOKI_INFO("  non-uniform sampling detected -- switching to chord_length knots.");
    }

    // -------------------------------------------------------------------------
    // 3. Exact interpolation guard
    // -------------------------------------------------------------------------
    if (bcfg.fitMode == "exact_interpolation") {
        if (nObs > bcfg.exactInterpolationMaxN) {
            throw ConfigException(
                "SplineAnalyzer::run: fitMode='exact_interpolation' requested "
                "for series '" + component + "' with nObs=" + std::to_string(nObs)
                + " > exactInterpolationMaxN=" + std::to_string(bcfg.exactInterpolationMaxN)
                + ". The LSQ system would be ill-conditioned. Use fitMode='approximation' "
                "or reduce exactInterpolationMaxN (at your own risk).");
        }
        LOKI_INFO("  exact_interpolation mode: nCtrl=" + std::to_string(nObs));
    }

    // -------------------------------------------------------------------------
    // 4. Determine nCtrl and optionally run CV
    // -------------------------------------------------------------------------
    int                  finalNCtrl    = 0;
    int                  optimalNCtrl  = 0;
    int                  manualNCtrl   = 0;
    std::vector<CvPoint> cvCurve;

    if (bcfg.fitMode == "exact_interpolation") {
        // Exact mode: nCtrl == nObs always.
        finalNCtrl  = nObs;
        manualNCtrl = nObs;
    } else if (bcfg.nControlPoints > 0) {
        // Manual mode.
        manualNCtrl = bcfg.nControlPoints;
        if (manualNCtrl < bcfg.degree + 1) {
            throw ConfigException(
                "SplineAnalyzer::run: nControlPoints=" + std::to_string(manualNCtrl)
                + " < degree+1=" + std::to_string(bcfg.degree + 1) + ".");
        }
        if (manualNCtrl > nObs) {
            throw ConfigException(
                "SplineAnalyzer::run: nControlPoints=" + std::to_string(manualNCtrl)
                + " > nObs=" + std::to_string(nObs) + ".");
        }
        finalNCtrl = manualNCtrl;
        LOKI_INFO("  manual nCtrl=" + std::to_string(finalNCtrl) + " (CV skipped).");
    } else {
        // Automatic CV mode.
        const int nCtrlMin = std::max(bcfg.nControlMin, bcfg.degree + 1);
        int nCtrlMax = bcfg.nControlMax;
        if (nCtrlMax <= 0) {
            nCtrlMax = std::min(nObs / 5, 200);
            nCtrlMax = std::max(nCtrlMax, nCtrlMin + 1);
        }
        nCtrlMax = std::min(nCtrlMax, nObs);

        LOKI_INFO("  CV mode: nCtrlMin=" + std::to_string(nCtrlMin)
                  + "  nCtrlMax=" + std::to_string(nCtrlMax)
                  + "  folds=" + std::to_string(bcfg.cvFolds));

        cvCurve = loki::math::crossValidateBSpline(
            tObs, zObs, bcfg.degree,
            nCtrlMin, nCtrlMax,
            knotPlacement, bcfg.cvFolds);

        if (cvCurve.empty()) {
            throw AlgorithmException(
                "SplineAnalyzer::run: cross-validation returned no results for '"
                + component + "'. Check nControlMin/nControlMax settings.");
        }

        optimalNCtrl = loki::math::selectOptimalNCtrl(cvCurve);
        finalNCtrl   = optimalNCtrl;

        LOKI_INFO("  CV complete: optimal nCtrl=" + std::to_string(finalNCtrl)
                  + " from " + std::to_string(cvCurve.size()) + " candidates.");
    }

    // -------------------------------------------------------------------------
    // 5. Final B-spline fit on full series
    // -------------------------------------------------------------------------
    LOKI_INFO("  fitting B-spline: degree=" + std::to_string(bcfg.degree)
              + "  nCtrl=" + std::to_string(finalNCtrl)
              + "  knots=" + knotPlacement
              + "  nObs=" + std::to_string(nObs));

    const loki::math::BSplineFitResult fitResult =
        loki::math::fitBSpline(tObs, zObs, bcfg.degree, finalNCtrl, knotPlacement);

    LOKI_INFO("  fit: RMSE=" + std::to_string(fitResult.rmse)
              + "  R2="      + std::to_string(fitResult.rSquared)
              + "  residStd="+ std::to_string(fitResult.residualStd));

    // -------------------------------------------------------------------------
    // 6. Residuals and CI band
    // -------------------------------------------------------------------------
    const std::vector<double> fitted =
        loki::math::evalBSpline(fitResult.tNorm, fitResult.controlPoints,
                                fitResult.degree, fitResult.knots);

    std::vector<double> residuals(static_cast<std::size_t>(nObs));
    for (int i = 0; i < nObs; ++i) {
        residuals[static_cast<std::size_t>(i)] =
            zObs[static_cast<std::size_t>(i)] - fitted[static_cast<std::size_t>(i)];
    }

    // Residual-based CI: fitted +/- z * residualStd
    const double zScore    = _zQuantile(scfg.confidenceLevel);
    const double halfWidth = zScore * fitResult.residualStd;

    std::vector<double> ciLower(static_cast<std::size_t>(nObs));
    std::vector<double> ciUpper(static_cast<std::size_t>(nObs));
    for (int i = 0; i < nObs; ++i) {
        const std::size_t si = static_cast<std::size_t>(i);
        ciLower[si] = fitted[si] - halfWidth;
        ciUpper[si] = fitted[si] + halfWidth;
    }

    // -------------------------------------------------------------------------
    // 7. Assemble result
    // -------------------------------------------------------------------------
    SplineResult result;
    result.componentName = component;
    result.method        = scfg.method;
    result.fitMode       = bcfg.fitMode;
    result.knotPlacement = knotPlacement;
    result.fit           = fitResult;
    result.tObs          = tObs;
    result.zObs          = zObs;
    result.fitted        = fitted;
    result.residuals     = residuals;
    result.ciLower       = ciLower;
    result.ciUpper       = ciUpper;
    result.cvCurve       = cvCurve;
    result.optimalNCtrl  = optimalNCtrl;
    result.manualNCtrl   = manualNCtrl;
    result.rmse          = fitResult.rmse;
    result.rSquared      = fitResult.rSquared;
    result.residualStd   = fitResult.residualStd;
    result.nObs          = nObs;
    result.nCtrl         = finalNCtrl;
    result.degree        = bcfg.degree;
    result.autoKnot      = autoKnot;

    // -------------------------------------------------------------------------
    // 8. Output
    // -------------------------------------------------------------------------
    _writeProtocol(result, series, datasetName);
    _writeCsv(result, filled, datasetName, component);

    PlotSpline plotter(m_cfg);
    plotter.plot(result, filled, datasetName);

    LOKI_INFO("Spline done: '" + component + "'.");
}

// =============================================================================
//  _isNonUniform
//
//  Compute the coefficient of variation (CV = stddev / mean) of time step
//  differences. A CV > 0.1 indicates non-uniform sampling.
// =============================================================================

bool SplineAnalyzer::_isNonUniform(const TimeSeries& ts)
{
    if (ts.size() < 3) return false;

    std::vector<double> diffs;
    diffs.reserve(ts.size() - 1);

    for (std::size_t i = 1; i < ts.size(); ++i) {
        const double dt = ts[i].time.gpsTotalSeconds()
                        - ts[i - 1].time.gpsTotalSeconds();
        if (dt > 0.0) diffs.push_back(dt);
    }
    if (diffs.empty()) return false;

    const double mean = std::accumulate(diffs.begin(), diffs.end(), 0.0)
                      / static_cast<double>(diffs.size());
    if (mean <= 0.0) return false;

    double var = 0.0;
    for (const double d : diffs) {
        const double e = d - mean;
        var += e * e;
    }
    const double stdDev = std::sqrt(var / static_cast<double>(diffs.size()));
    return (stdDev / mean) > 0.1;
}

// =============================================================================
//  _zQuantile  --  bisection on erf, accurate for levels in [0.80, 0.999]
// =============================================================================

double SplineAnalyzer::_zQuantile(double confidenceLevel)
{
    double lo = 0.0, hi = 5.0;
    for (int i = 0; i < 60; ++i) {
        const double mid = 0.5 * (lo + hi);
        if (std::erf(mid / std::numbers::sqrt2) < confidenceLevel) lo = mid;
        else hi = mid;
    }
    return 0.5 * (lo + hi);
}

// =============================================================================
//  _writeProtocol
// =============================================================================

void SplineAnalyzer::_writeProtocol(const SplineResult& result,
                                    const TimeSeries&   ts,
                                    const std::string&  datasetName) const
{
    const std::string fname = datasetName + "_" + result.componentName
                            + "_spline_protocol.txt";
    const std::filesystem::path path = m_cfg.protocolsDir / fname;

    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        LOKI_WARNING("SplineAnalyzer: cannot write protocol to '"
                     + path.string() + "'.");
        return;
    }

    const auto line = [&](const std::string& s){ ofs << s << "\n"; };
    const auto sep  = [&](){ ofs << std::string(70, '-') << "\n"; };

    sep();
    line("B-SPLINE APPROXIMATION PROTOCOL");
    line("Generated by loki_spline");
    sep();
    line("Dataset           : " + datasetName);
    line("Component         : " + result.componentName);
    line("Method            : " + result.method);
    line("Fit mode          : " + result.fitMode);
    line("Knot placement    : " + result.knotPlacement
         + (result.autoKnot ? "  (auto-detected non-uniform sampling)" : ""));
    line("B-spline degree   : " + std::to_string(result.degree));
    line("Control points    : " + std::to_string(result.nCtrl));
    line("Observations      : " + std::to_string(result.nObs));
    line("Exact fit         : " + std::string(result.fit.isExact ? "yes" : "no"));
    if (!ts.empty()) {
        line("Series start      : " + ts[0].time.utcString());
        line("Series end        : " + ts[ts.size() - 1].time.utcString());
    }
    sep();

    line("FIT QUALITY");
    line("  RMSE            : " + std::to_string(result.rmse));
    line("  R^2             : " + std::to_string(result.rSquared));
    line("  Residual StdDev : " + std::to_string(result.residualStd));
    sep();

    if (!result.cvCurve.empty()) {
        line("CROSS-VALIDATION (k-fold, k=" + std::to_string(m_cfg.spline.bspline.cvFolds) + ")");
        line("  Candidates tested : " + std::to_string(result.cvCurve.size()));
        line("  Optimal nCtrl     : " + std::to_string(result.optimalNCtrl)
             + "  (one-SE elbow rule)");
        // Find best and worst CV RMSE.
        const auto minIt = std::min_element(
            result.cvCurve.begin(), result.cvCurve.end(),
            [](const CvPoint& a, const CvPoint& b){ return a.rmse < b.rmse; });
        line("  Best CV RMSE      : " + std::to_string(minIt->rmse)
             + "  (nCtrl=" + std::to_string(minIt->nCtrl) + ")");
        sep();
    } else {
        line("CONTROL POINT SELECTION");
        line("  Mode   : manual");
        line("  nCtrl  : " + std::to_string(result.manualNCtrl));
        sep();
    }

    LOKI_INFO("  Protocol written: " + path.string());
}

// =============================================================================
//  _writeCsv
// =============================================================================

void SplineAnalyzer::_writeCsv(const SplineResult& result,
                                const TimeSeries&   tsFilled,
                                const std::string&  datasetName,
                                const std::string&  component) const
{
    const std::string fname = datasetName + "_" + component
                            + "_spline_fit.csv";
    const std::filesystem::path path = m_cfg.csvDir / fname;

    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        LOKI_WARNING("SplineAnalyzer: cannot write CSV to '"
                     + path.string() + "'.");
        return;
    }

    ofs << std::fixed << std::setprecision(9);
    ofs << "index;mjd;utc;observed;fitted;residual;ci_lower;ci_upper\n";

    const int n = result.nObs;
    for (int i = 0; i < n; ++i) {
        const std::size_t si    = static_cast<std::size_t>(i);
        const int         idx   = static_cast<int>(result.tObs[si]);
        double            mjd   = 0.0;
        std::string       utcStr;

        // Map sample index back to the filled series for MJD/UTC.
        if (idx >= 0 && static_cast<std::size_t>(idx) < tsFilled.size()) {
            mjd    = tsFilled[static_cast<std::size_t>(idx)].time.mjd();
            utcStr = tsFilled[static_cast<std::size_t>(idx)].time.utcString();
        }

        ofs << idx                    << ";"
            << mjd                    << ";"
            << utcStr                 << ";"
            << result.zObs[si]        << ";"
            << result.fitted[si]      << ";"
            << result.residuals[si]   << ";"
            << result.ciLower[si]     << ";"
            << result.ciUpper[si]     << "\n";
    }

    LOKI_INFO("  CSV written: " + path.string());
}

} // namespace loki::spline
