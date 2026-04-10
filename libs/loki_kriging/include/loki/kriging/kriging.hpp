#pragma once

#include <loki/core/config.hpp>
#include <loki/kriging/krigingResult.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <memory>
#include <vector>

namespace loki::kriging {

// =============================================================================
//  KrigingBase
// =============================================================================

/**
 * @brief Abstract base class for all Kriging estimators.
 *
 * Subclasses implement the specific Kriging system assembly
 * (_assembleSystem / _assembleRHS) while all share the prediction loop,
 * Kriging variance computation, and cross-validation logic.
 *
 * Temporal mode (v1): observations are (t_i, z_i) pairs.
 * The covariance between two points is derived from the variogram:
 *   C(h) = sill - gamma(h)   for stationary models (spherical, exponential, gaussian)
 *   C(h) = -gamma(h)         for intrinsic models (power)
 *
 * FUTURE (spatial mode): observations will be (x_i, y_i, z_i) triples and
 * the lag will be Euclidean distance. The base class interface will remain
 * unchanged; only the lag computation and the fit() input type will change.
 *
 * FUTURE (space-time mode): separate variograms for temporal and spatial
 * components; product-sum or metric model to be decided at design time.
 */
class KrigingBase {
public:

    virtual ~KrigingBase() = default;

    /**
     * @brief Fit the Kriging model to an observation series.
     *
     * Stores the observation values and times, builds the covariance matrix,
     * and factorises the Kriging system (LLT or LU decomposition of K).
     *
     * @param ts           Gap-filled time series (NaN values are skipped).
     * @param variogram    Fitted variogram parameters.
     * @throws DataException      if fewer than 2 valid observations are present.
     * @throws AlgorithmException on singular Kriging system.
     */
    virtual void fit(const TimeSeries&       ts,
                     const VariogramFitResult& variogram) = 0;

    /**
     * @brief Estimate the value at a single target time (MJD).
     *
     * @param mjd Target time in MJD.
     * @return KrigingPrediction with value, variance, and CI.
     */
    virtual KrigingPrediction predictAt(double mjd) const = 0;

    /**
     * @brief Estimate values at a set of target times.
     *
     * @param mjdPoints Target times in MJD (may include observation times).
     * @param confidenceLevel Confidence level for CI computation (e.g. 0.95).
     * @return Vector of KrigingPrediction, one per target point.
     */
    std::vector<KrigingPrediction> predictGrid(
        const std::vector<double>& mjdPoints,
        double confidenceLevel) const;

    /**
     * @brief Leave-one-out cross-validation.
     *
     * Removes each observation in turn, re-solves the Kriging system without
     * it, and records the prediction error.
     *
     * @param confidenceLevel Confidence level (used for variance scaling).
     * @return CrossValidationResult with RMSE, MAE, standardised errors etc.
     */
    CrossValidationResult crossValidate(double confidenceLevel) const;

protected:

    // -- Shared state (populated in fit()) ------------------------------------
    std::vector<double> m_mjd;       ///< Observation times (MJD).
    std::vector<double> m_z;         ///< Observation values.
    VariogramFitResult  m_variogram; ///< Fitted variogram.
    double              m_ciZ;       ///< Standard normal quantile for CI bounds.

    /**
     * @brief Compute covariance C(h) from variogram gamma(h).
     *
     * For stationary models: C(h) = C(0) - gamma(h) = (sill - nugget) - gamma(h)
     *                               plus nugget at h=0, so C(0) = sill - nugget.
     * For power model:      C(h) is replaced by -gamma(h) (intrinsic model).
     * This convention ensures the Kriging system remains positive semi-definite.
     */
    double _cov(double h) const;

    /**
     * @brief Compute the z quantile for the configured confidence level.
     *
     * Uses the probit function: z = sqrt(2) * erfinv(level).
     */
    static double _zQuantile(double confidenceLevel);
};

// =============================================================================
//  SimpleKriging
// =============================================================================

/**
 * @brief Simple Kriging estimator (known constant mean).
 *
 * The estimator is:
 *   Z*(t) = mu + sum_i [ lambda_i * (Z(t_i) - mu) ]
 *
 * Kriging weights lambda solve:
 *   K * lambda = k(t)
 *
 * where K_ij = C(|t_i - t_j|) and k_i(t) = C(|t - t_i|).
 *
 * Kriging variance:
 *   sigma^2_K(t) = C(0) - k(t)^T * lambda
 *
 * Advantages: unconstrained system, numerically most stable.
 * Limitation: requires reliable knowledge of the global mean.
 * Appropriate for deseasonalized residuals where mu = 0 is known.
 *
 * FUTURE: extend to locally varying known mean for spatial non-stationarity.
 */
class SimpleKriging : public KrigingBase {
public:

    /**
     * @brief Construct with known mean and confidence level.
     * @param knownMean Known global mean mu.
     * @param confidenceLevel Confidence level for prediction intervals.
     */
    explicit SimpleKriging(double knownMean, double confidenceLevel);

    void              fit(const TimeSeries&        ts,
                          const VariogramFitResult& variogram) override;
    KrigingPrediction predictAt(double mjd) const override;

private:

    double              m_mu;   ///< Known constant mean.
    // Factored covariance matrix K = L * L^T (Cholesky)
    // Stored column-major for Eigen compatibility (set in fit())
    std::vector<double> m_Kdata;  ///< Row-major K values (n x n).
    std::vector<double> m_L;      ///< Lower Cholesky factor (row-major).
};

// =============================================================================
//  OrdinaryKriging
// =============================================================================

/**
 * @brief Ordinary Kriging estimator (unknown local mean, sum-of-weights = 1).
 *
 * The estimator is:
 *   Z*(t) = sum_i [ lambda_i * Z(t_i) ]
 *
 * Kriging weights solve the extended system with Lagrange multiplier mu:
 *   [ K   1 ] [ lambda ]   [ k(t) ]
 *   [ 1^T 0 ] [   mu   ] = [  1   ]
 *
 * Kriging variance:
 *   sigma^2_K(t) = C(0) - k(t)^T * lambda - mu
 *
 * This is the standard default. No assumption on the mean is required.
 * The mean is estimated implicitly as the weighted average of neighbours.
 *
 * FUTURE: neighbourhood search (only use k nearest neighbours) to avoid
 * ill-conditioning for large datasets (n > 500).
 */
class OrdinaryKriging : public KrigingBase {
public:

    /**
     * @brief Construct with confidence level.
     * @param confidenceLevel Confidence level for prediction intervals.
     */
    explicit OrdinaryKriging(double confidenceLevel);

    void              fit(const TimeSeries&        ts,
                          const VariogramFitResult& variogram) override;
    KrigingPrediction predictAt(double mjd) const override;

private:

    // Extended system matrix (n+1) x (n+1) stored row-major
    std::vector<double> m_Kext;  ///< Extended covariance matrix with constraint row/col.
    std::vector<double> m_Linv;  ///< LU factors (via Eigen, stored flat).
    std::vector<int>    m_piv;   ///< Pivot indices for LU decomposition.
};

// =============================================================================
//  UniversalKriging
// =============================================================================

/**
 * @brief Universal Kriging estimator (mean is a polynomial trend in time).
 *
 * The mean is modelled as:
 *   mu(t) = beta_0 + beta_1 * t + ... + beta_p * t^p
 * where t = mjd - tRef for numerical stability and p = trendDegree.
 *
 * Kriging weights solve the extended system:
 *   [ K   F ] [ lambda ]   [ k(t) ]
 *   [ F^T 0 ] [  beta  ] = [ f(t) ]
 *
 * where F is the (n x (p+1)) matrix of basis function evaluations at observations
 * and f(t) is the (p+1)-vector of basis evaluations at the prediction point.
 *
 * Kriging variance:
 *   sigma^2_K(t) = C(0) - k(t)^T * lambda - f(t)^T * beta
 *
 * Appropriate when a deterministic trend is present in the data
 * (e.g. velocity trend, altitude-dependent temperature gradient).
 *
 * FUTURE: replace polynomial drift with arbitrary user-defined basis
 * functions (harmonic, piecewise linear) for greater flexibility.
 */
class UniversalKriging : public KrigingBase {
public:

    /**
     * @brief Construct with trend degree and confidence level.
     * @param trendDegree Polynomial degree of the drift function (1 or 2 recommended).
     * @param confidenceLevel Confidence level for prediction intervals.
     */
    explicit UniversalKriging(int trendDegree, double confidenceLevel);

    void              fit(const TimeSeries&        ts,
                          const VariogramFitResult& variogram) override;
    KrigingPrediction predictAt(double mjd) const override;

private:

    int    m_degree; ///< Polynomial degree of drift.
    double m_tRef;   ///< Reference time for numerical stability (first obs MJD).

    // Extended system matrix (n + p + 1) x (n + p + 1)
    std::vector<double> m_Kext;
    std::vector<double> m_Linv;
    std::vector<int>    m_piv;

    /**
     * @brief Evaluate the drift basis vector at a given (shifted) time.
     *
     * Returns [1, t, t^2, ..., t^degree] where t = mjd - m_tRef.
     */
    std::vector<double> _driftBasis(double mjd) const;
};

// =============================================================================
//  Factory
// =============================================================================

/**
 * @brief Create the appropriate Kriging estimator from configuration.
 *
 * @param cfg      Kriging configuration (method, knownMean, trendDegree, etc.)
 * @return         Owning pointer to the created KrigingBase subclass.
 * @throws ConfigException if cfg.method is not recognised.
 * @throws AlgorithmException if cfg.mode is "spatial" or "space_time"
 *         (not yet implemented -- placeholder throws with descriptive message).
 */
std::unique_ptr<KrigingBase> createKriging(const KrigingConfig& cfg);

} // namespace loki::kriging
