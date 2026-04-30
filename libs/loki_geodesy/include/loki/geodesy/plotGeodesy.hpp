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
 *   1. Combined covariance matrix panel (NxN grid) with MC samples,
 *      analytical covariance (Jacobian) and empirical covariance (MC)
 *      overlaid in a single plot.
 *   2. Geodesic distance bar chart (when distance task is used).
 *
 * Panel layout (N=3 example):
 *   Diagonal      : horizontal histogram + analytical normal (red) +
 *                   empirical normal (green). Title shows both sigmas.
 *   Lower triangle: scatter of MC samples + analytical error ellipse (blue) +
 *                   empirical error ellipse (green) +
 *                   Helmert curve for analytical cov (red dashed).
 *                   Title shows covariance value.
 *   Upper triangle: scatter of MC samples + regression line +
 *                   Pearson R and p-value in title.
 *
 * All plots follow the LOKI naming convention:
 *   geodesy_<dataset>_<tag>_covariance.png
 */
class PlotGeodesy {
public:

    /**
     * @brief Construct with config and dataset identifier.
     * @param cfg       GeodesyConfig (provides imgDir, ellipsoid name).
     * @param datasetId Stem of the input filename, used in output file names.
     */
    explicit PlotGeodesy(const GeodesyConfig& cfg,
                         const std::string&    datasetId);

    /**
     * @brief Plot NxN covariance matrix panel.
     *
     * Diagonal     : horizontal histogram of MC samples +
     *                analytical normal density (red) +
     *                empirical normal density (green).
     * Lower triangle: scatter + analytical ellipse (blue solid) +
     *                 empirical ellipse (green solid) +
     *                 Helmert curve of analytical cov (red dashed).
     * Upper triangle: scatter + regression line + R[p] in title.
     *
     * @param samples        MC sample matrix, size N x nSamples.
     * @param analyticalCov  Analytical covariance (Jacobian propagation), NxN.
     * @param empiricalCov   Empirical covariance (computed from samples), NxN.
     * @param mean           Mean of MC samples, size N.
     * @param prob           Probability level for error ellipses (e.g. 0.95).
     * @param tag            Short tag appended to output file name.
     * @param panelTitle     Title shown above the full panel.
     */
    void plotCovariancePanel(const Eigen::MatrixXd& samples,
                             const Eigen::MatrixXd& analyticalCov,
                             const Eigen::MatrixXd& empiricalCov,
                             const Eigen::VectorXd& mean,
                             double                  prob,
                             const std::string&      tag,
                             const std::string&      panelTitle) const;

    /**
     * @brief Plot bar chart of geodesic distances between consecutive points.
     * @param lines  Vector of GeodLineResult (one per point pair).
     */
    void plotDistances(const std::vector<GeodLineResult>& lines) const;

private:

    GeodesyConfig m_cfg;
    std::string   m_datasetId;
    std::string   m_outDir;

    /**
     * @brief Compute error ellipse polygon from 2x2 covariance subblock.
     *
     * Eigendecomposition + chi2 scaling gives semi-axes a, b.
     * Parametric ellipse: (a cos t, b sin t) rotated by eigenvector angle.
     * Centred at (0,0); caller adds mean offset.
     * xHelm/yHelm are unused placeholders kept for API compatibility.
     */
    static void computeEllipseAndHelmert(const Eigen::Matrix2d& cov2x2,
                                          double                  prob,
                                          std::vector<double>&    xEll,
                                          std::vector<double>&    yEll,
                                          std::vector<double>&    xHelm,
                                          std::vector<double>&    yHelm);

    /// Chi-squared quantile for 2 degrees of freedom: -2 ln(1 - prob).
    static double chi2Quantile2dof(double prob) noexcept;

    /// Build output file path: m_outDir/geodesy_<datasetId>_<tag>_<suffix>.png
    std::string outPath(const std::string& tag, const std::string& suffix) const;
};

} // namespace loki::geodesy
