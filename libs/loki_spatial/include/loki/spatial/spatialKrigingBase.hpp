#pragma once

#include <loki/math/krigingTypes.hpp>
#include <loki/math/spatialTypes.hpp>
#include <loki/math/spatialVariogram.hpp>

#include <memory>
#include <Eigen/Dense>

#include <vector>

namespace loki::spatial {

// =============================================================================
//  SpatialKrigingBase
// =============================================================================

/**
 * @brief Abstract base class for 2-D spatial Kriging estimators.
 *
 * Mirrors loki::math::KrigingBase but operates on SpatialPoint scatter data
 * rather than TimeSeries. The lag is the Euclidean distance between points:
 *   h_ij = sqrt( (xi-xj)^2 + (yi-yj)^2 )
 *
 * Subclasses implement fit() and predictAt() for each Kriging variant
 * (Simple, Ordinary, Universal).
 *
 * Performance:
 *   fit()          : O(n^3) -- Kriging matrix inversion.
 *   predictAt()    : O(n)   -- cached inverse.
 *   predictGrid()  : O(m*n) -- m calls to predictAt().
 *   crossValidate(): O(n^2) -- LOO shortcut (Dubrule 1983).
 *
 * LOO shortcut:
 *   e_i = alpha_i / [K^{-1}]_{ii}   where alpha = K^{-1} * z
 */
class SpatialKrigingBase {
public:

    virtual ~SpatialKrigingBase() = default;

    /**
     * @brief Fit the Kriging model to scatter observations.
     *
     * Builds and inverts the Kriging system. Result cached in m_Kinv.
     *
     * @param points    Input scatter observations (x, y, value). NaN values skipped.
     * @param variogram Fitted variogram parameters.
     * @throws DataException      if fewer than 2 valid points.
     * @throws AlgorithmException on singular Kriging system.
     */
    /**
     * @brief Create a new empty instance of the same derived type for LOO CV.
     *
     * Each subclass returns a fresh instance with the same constructor
     * parameters (knownMean, confidenceLevel, trendDegree as applicable).
     * Used by crossValidate() to refit n times without virtual dispatch issues.
     */
    virtual std::unique_ptr<SpatialKrigingBase> _cloneEmpty() const = 0;

    virtual void fit(const std::vector<loki::math::SpatialPoint>& points,
                     const loki::math::VariogramFitResult&        variogram) = 0;

    /**
     * @brief Estimate value at a single query location. O(n).
     */
    virtual loki::math::SpatialPrediction predictAt(double x, double y) const = 0;

    /**
     * @brief Estimate values on a regular grid. O(m*n).
     *
     * @param extent          Grid geometry.
     * @param confidenceLevel Confidence level for CI bounds.
     * @return                SpatialGrid with values, variance, CI bands.
     */
    loki::math::SpatialGrid predictGrid(
        const loki::math::GridExtent& extent,
        double                        confidenceLevel) const;

    /**
     * @brief LOO cross-validation via O(n^2) shortcut.
     *
     * @param confidenceLevel Confidence level (for variance scaling).
     * @return                SpatialCrossValidationResult.
     */
    loki::math::SpatialCrossValidationResult crossValidate(double confidenceLevel) const;

protected:

    std::vector<loki::math::SpatialPoint> m_pts;      ///< Input scatter data.
    std::vector<double>                   m_x;         ///< X coordinates of valid points.
    std::vector<double>                   m_y;         ///< Y coordinates of valid points.
    std::vector<double>                   m_z;         ///< Observed values of valid points.
    loki::math::VariogramFitResult        m_variogram; ///< Fitted variogram.
    double                                m_ciZ = 0.0; ///< N(0,1) quantile for CI.

    /**
     * @brief Cached inverse of the Kriging system matrix.
     *
     *   SimpleKriging:    K^{-1}     (n x n)
     *   OrdinaryKriging:  K_ext^{-1} ((n+1) x (n+1))
     *   UniversalKriging: K_ext^{-1} ((n+p+1) x (n+p+1))
     */
    Eigen::MatrixXd m_Kinv;
    double          m_c0 = 0.0; ///< C(0) = sill (cached in fit()).

    // -- Helpers --------------------------------------------------------------

    /** @brief Covariance C(h) from fitted variogram. */
    double _cov(double h) const;

    /** @brief N(0,1) quantile via bisection. */
    static double _zQuantile(double confidenceLevel);
};

} // namespace loki::spatial
