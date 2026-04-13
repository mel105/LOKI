#pragma once

#include <loki/math/bsplineFit.hpp>

#include <string>
#include <vector>

namespace loki::spline {

// Re-export math types so module code uses the loki::spline namespace.
using BSplineFitResult = loki::math::BSplineFitResult;
using CvPoint          = loki::math::CvPoint;

// =============================================================================
//  SplineResult
// =============================================================================

/**
 * @brief Top-level result struct for the loki_spline pipeline.
 *
 * Populated by SplineAnalyzer::run() and consumed by PlotSpline and the
 * protocol/CSV writers.
 */
struct SplineResult {
    std::string componentName;     ///< Series metadata component name.
    std::string method;            ///< "bspline" (currently always).
    std::string fitMode;           ///< "approximation" | "exact_interpolation"
    std::string knotPlacement;     ///< "uniform" | "chord_length"

    BSplineFitResult fit;          ///< Full B-spline fit result.

    // -- Observation-domain arrays (length nObs after gap fill) ---------------
    std::vector<double> tObs;      ///< Sample indices (0, 1, 2, ...) -- uniform parameter.
    std::vector<double> zObs;      ///< Observed values at tObs.
    std::vector<double> fitted;    ///< Fitted B-spline values at tObs.
    std::vector<double> residuals; ///< zObs - fitted.
    std::vector<double> ciLower;   ///< Confidence interval lower bound at tObs.
    std::vector<double> ciUpper;   ///< Confidence interval upper bound at tObs.

    // -- Cross-validation (empty when nControlPoints > 0 i.e. manual mode) ---
    std::vector<CvPoint> cvCurve;  ///< CV RMSE vs nCtrl.
    int    optimalNCtrl = 0;       ///< Selected nCtrl from CV (0 = manual).
    int    manualNCtrl  = 0;       ///< User-specified nCtrl (0 = auto).

    // -- Summary statistics ---------------------------------------------------
    double rmse        = 0.0;      ///< Training RMSE.
    double rSquared    = 0.0;      ///< R^2 on training data.
    double residualStd = 0.0;      ///< Std dev of residuals (used for CI).
    int    nObs        = 0;        ///< Number of valid observations used.
    int    nCtrl       = 0;        ///< Final number of control points.
    int    degree      = 3;        ///< B-spline degree used.
    bool   autoKnot    = false;    ///< True if chord_length was auto-detected.
};

} // namespace loki::spline
