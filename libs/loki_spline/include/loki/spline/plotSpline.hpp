#pragma once

#include <loki/core/config.hpp>
#include <loki/spline/splineResult.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <string>

namespace loki::spline {

/**
 * @brief Generates all loki_spline diagnostic plots via gnuplot.
 *
 * All plot types are individually gated by PlotConfig flags.
 * Plots written to OUTPUT/IMG/ following the naming convention:
 *   spline_<dataset>_<component>_<plottype>.<format>
 *
 * Available plots:
 *   splineOverlay     -- Original series + B-spline fit + CI band (filledcurves).
 *   splineResiduals   -- Residuals vs sample index with +/- RMSE and +/- 2*RMSE lines.
 *   splineBasis       -- B-spline basis functions N_{i,p}(t) (default: disabled,
 *                        visually cluttered for large nCtrl).
 *   splineKnots       -- Original series + vertical lines at each interior knot position.
 *   splineCv          -- CV RMSE vs nCtrl with the selected optimum highlighted.
 *                        Only shown when CV was run (auto mode).
 *   splineDiagnostics -- Delegates to Plot::residualDiagnostics() (ACF, histogram,
 *                        QQ, fitted vs residuals). Uses loki::Plot from loki_core.
 */
class PlotSpline {
public:

    /**
     * @brief Construct the plotter.
     * @param cfg  Application configuration (paths, plot flags, format).
     */
    explicit PlotSpline(const AppConfig& cfg);

    /**
     * @brief Generate all enabled plots for one SplineResult.
     *
     * @param result      Populated SplineResult from SplineAnalyzer::run().
     * @param tsFilled    Gap-filled time series (for MJD/UTC axis labelling).
     * @param datasetName Input file stem (used in output file names).
     */
    void plot(const SplineResult& result,
              const TimeSeries&   tsFilled,
              const std::string&  datasetName) const;

private:

    const AppConfig& m_cfg;

    // -- Individual plot generators -------------------------------------------

    /**
     * @brief Original series + B-spline fit + confidence interval band.
     *
     * Panel 1: filled CI band (filledcurves using 1:2:3, pink #ffb3c1).
     *          Original observations (grey points).
     *          Fitted B-spline (blue line).
     * Annotates nCtrl, degree, RMSE, R^2 in key.
     */
    void _plotOverlay(const SplineResult& result,
                      const TimeSeries&   tsFilled,
                      const std::string&  stem) const;

    /**
     * @brief Residuals vs sample index.
     *
     * Points: residuals (coloured by sign: blue=positive, red=negative).
     * Horizontal reference lines at 0, +/-RMSE, +/-2*RMSE.
     */
    void _plotResiduals(const SplineResult& result,
                        const std::string&  stem) const;

    /**
     * @brief B-spline basis functions N_{i,p}(t) for i = 0 .. nCtrl-1.
     *
     * Each basis function plotted as a thin coloured line over [0, 1].
     * Vertical dashed lines at interior knot positions.
     * Disabled by default (splineBasis = false) -- cluttered for large nCtrl.
     */
    void _plotBasis(const SplineResult& result,
                    const std::string&  stem) const;

    /**
     * @brief Original series with vertical lines at knot positions.
     *
     * Interior knots only (first and last are at t=0 and t=n-1).
     * Useful to verify knot density distribution.
     */
    void _plotKnots(const SplineResult& result,
                    const TimeSeries&   tsFilled,
                    const std::string&  stem) const;

    /**
     * @brief CV curve: RMSE vs nCtrl.
     *
     * Solid line with point markers. Vertical dashed line at optimalNCtrl.
     * Title shows the selection method ("one-SE elbow rule").
     * Only called when cvCurve is non-empty.
     */
    void _plotCv(const SplineResult& result,
                 const std::string&  stem) const;

    // -- Utilities ------------------------------------------------------------

    /**
     * @brief Build the output file path for a given plot type.
     *
     * Pattern: OUTPUT/IMG/spline_<dataset>_<component>_<plottype>.<format>
     */
    std::string _makeStem(const std::string& datasetName,
                          const std::string& component,
                          const std::string& plotType) const;

    /**
     * @brief Convert a Windows path to forward-slash form for gnuplot.
     */
    static std::string _fwdSlash(const std::string& path);
};

} // namespace loki::spline
