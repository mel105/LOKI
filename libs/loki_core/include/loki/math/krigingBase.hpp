#pragma once

#include <loki/math/krigingTypes.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <Eigen/Dense>

#include <vector>

namespace loki::math {

// =============================================================================
//  KrigingBase
// =============================================================================

/**
 * @brief Abstract base class for all Kriging estimators.
 *
 * Subclasses implement the specific Kriging system assembly while the base
 * provides shared prediction, variance computation, and cross-validation.
 *
 * Performance:
 *   - Full system inverse K^{-1} computed once in fit() and cached (m_Kinv).
 *   - predictAt()    : O(n)   -- one matrix-vector product using cached inverse.
 *   - predictGrid()  : O(m*n) -- m calls to predictAt().
 *   - crossValidate(): O(n^2) -- LOO shortcut (Dubrule 1983), no re-factorisation.
 *
 * LOO shortcut identity:
 *   e_i = alpha_i / [K^{-1}]_{ii}   where alpha = K^{-1} * z
 *
 * Temporal Kriging (v1): lag = |t_i - t_j| in MJD days.
 *
 * FUTURE (spatial):    lag = Euclidean distance sqrt((x_i-x_j)^2+(y_i-y_j)^2).
 * FUTURE (space-time): product-sum or metric covariance model.
 */
class KrigingBase {
public:

    virtual ~KrigingBase() = default;

    /**
     * @brief Fit the Kriging model to an observation series. O(n^3).
     *
     * Builds and inverts the Kriging system. Result cached in m_Kinv.
     *
     * @param ts        Gap-filled time series (NaN values skipped).
     * @param variogram Fitted variogram parameters.
     * @throws DataException      if fewer than 2 valid observations.
     * @throws AlgorithmException on singular Kriging system.
     */
    virtual void fit(const TimeSeries&        ts,
                     const VariogramFitResult& variogram) = 0;

    /**
     * @brief Estimate value at a single target MJD. O(n).
     */
    virtual KrigingPrediction predictAt(double mjd) const = 0;

    /**
     * @brief Estimate values at a set of target MJD points. O(m*n).
     *
     * @param mjdPoints       Target times in MJD.
     * @param confidenceLevel Confidence level for CI bounds.
     */
    std::vector<KrigingPrediction> predictGrid(
        const std::vector<double>& mjdPoints,
        double                     confidenceLevel) const;

    /**
     * @brief Leave-one-out cross-validation via O(n^2) shortcut.
     *
     * Uses: e_i = alpha_i / [K^{-1}]_{ii}  (no per-point re-factorisation).
     * For Ordinary and Universal Kriging the upper-left n x n block of the
     * extended inverse is used.
     *
     * @param confidenceLevel Confidence level (for variance scaling in stdErrors).
     */
    CrossValidationResult crossValidate(double confidenceLevel) const;

protected:

    // -- Shared state ---------------------------------------------------------
    std::vector<double> m_mjd;       ///< Observation times (MJD).
    std::vector<double> m_z;         ///< Observation values.
    VariogramFitResult  m_variogram; ///< Fitted variogram parameters.
    double              m_ciZ = 0.0; ///< N(0,1) quantile for CI bounds.

    // -- Cached inverse -------------------------------------------------------

    /**
     * @brief Cached inverse of the Kriging system matrix.
     *
     *   SimpleKriging:    K^{-1}     (n x n)
     *   OrdinaryKriging:  K_ext^{-1} ((n+1) x (n+1))
     *   UniversalKriging: K_ext^{-1} ((n+p+1) x (n+p+1))
     *
     * Memory: O(n^2). ~11 MB for n=1200 (double precision).
     * For n > 5000 a neighbourhood search should be added.
     */
    Eigen::MatrixXd m_Kinv;

    /// C(0) = sill for stationary models. Cached in fit().
    double m_c0 = 0.0;

    // -- Helpers --------------------------------------------------------------

    /**
     * @brief Compute covariance C(h) from the fitted variogram.
     *
     * Stationary models: C(h) = sill - gamma(h)
     * Power model:       C(h) = -gamma(h)  (intrinsic, no finite sill)
     */
    double _cov(double h) const;

    /**
     * @brief Standard normal quantile via bisection on erf.
     *
     * Accurate for confidence levels in [0.80, 0.999].
     */
    static double _zQuantile(double confidenceLevel);
};

} // namespace loki::math
