#pragma once

#include <loki/geodesy/geodesyResult.hpp>
#include <loki/geodesy/geodesyAnalyzer.hpp>

#include <Eigen/Dense>
#include <string>
#include <vector>

namespace loki::geodesy {

/**
 * @brief Gnuplot-based plotter for geodesy results.
 *
 * Produces:
 *   1. Error ellipse + Helmert curve for each 2x2 subblock of the covariance
 *      matrix (all pairs of coordinate components).
 *   2. Covariance matrix heatmap (diagonal: histogram; off-diagonal: scatter
 *      with ellipse overlay) when Monte Carlo samples are available.
 *   3. Geodesic distance bar chart (when distance task is used).
 *
 * All plots follow the LOKI naming convention:
 *   geodesy_<dataset>_<component>_<plottype>.png
 */
class PlotGeodesy {
public:
    /**
     * @brief Construct with config and output directory.
     * @param cfg       GeodesyConfig (provides output_dir and ellipsoid name).
     * @param datasetId Stem of the input filename, used in output file names.
     */
    explicit PlotGeodesy(const GeodesyConfig& cfg,
                         const std::string&    datasetId);

    /**
     * @brief Plot error ellipse + Helmert curve for a 2x2 covariance subblock.
     *
     * Eigendecomposition of the 2x2 block yields the principal axes.
     * Chi-squared quantile scales the axes to the requested probability level.
     *
     * @param cov2x2   2x2 covariance subblock.
     * @param prob     Probability level for chi2 scaling (e.g. 0.95).
     * @param labelX   Axis label for first component (e.g. "E [m]").
     * @param labelY   Axis label for second component.
     * @param tag      Short tag for file name (e.g. "EN", "EU").
     */
    void plotErrorEllipse(const Eigen::Matrix2d& cov2x2,
                          double                  prob,
                          const std::string&      labelX,
                          const std::string&      labelY,
                          const std::string&      tag) const;

    /**
     * @brief Plot covariance matrix panel (NxN grid):
     *   - Diagonal: histogram of MC samples + fitted normal density.
     *   - Off-diagonal: scatter plot of MC sample pairs + error ellipse.
     *
     * @param mcResult    Monte Carlo result containing empirical samples.
     * @param labels      Component labels (size N).
     * @param prob        Probability level for ellipses.
     */
    void plotCovariancePanel(const MonteCarloResult&          mcResult,
                             const Eigen::MatrixXd&           samples,
                             const std::vector<std::string>&  labels,
                             double                            prob) const;

    /**
     * @brief Plot bar chart of geodesic distances.
     * @param lines  Vector of GeodLineResult (one per point pair).
     */
    void plotDistances(const std::vector<GeodLineResult>& lines) const;

private:
    GeodesyConfig m_cfg;
    std::string   m_datasetId;
    std::string   m_outDir;

    // Compute error ellipse polygon from 2x2 covariance + chi2 scaling.
    // Returns (x_vec, y_vec) of the ellipse boundary.
    static void computeErrorEllipse(const Eigen::Matrix2d& cov2x2,
                                    double                  prob,
                                    std::vector<double>&    xEll,
                                    std::vector<double>&    yEll,
                                    std::vector<double>&    xHelm,
                                    std::vector<double>&    yHelm);

    // Chi-squared quantile approximation for 2 degrees of freedom.
    static double chi2Quantile2dof(double prob) noexcept;

    std::string outPath(const std::string& tag, const std::string& suffix) const;
};

} // namespace loki::geodesy
