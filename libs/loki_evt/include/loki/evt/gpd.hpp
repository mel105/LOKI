#pragma once

#include <loki/evt/evtResult.hpp>

#include <vector>

namespace loki::evt {

/**
 * @brief Generalised Pareto Distribution (GPD) utilities.
 *
 * Provides CDF, quantile, log-likelihood, MLE fitting, and return level
 * computation for the two-parameter GPD used in Peaks-Over-Threshold (POT)
 * analysis.
 *
 * Parameterisation:
 *   F(x) = 1 - (1 + xi * x / sigma)^(-1/xi)   for xi != 0
 *   F(x) = 1 - exp(-x / sigma)                 for xi == 0  (exponential limit)
 *
 * where x >= 0 (exceedances above the threshold).
 *
 * Fitting uses MLE via Nelder-Mead with constraint xi > -0.5.
 * For n < 50 exceedances, Probability-Weighted Moments (PWM) is used as a
 * fallback and GpdFitResult::converged is set to false.
 */
class Gpd {
public:

    /**
     * @brief GPD cumulative distribution function.
     * @param x     Exceedance value (must be >= 0).
     * @param xi    Shape parameter.
     * @param sigma Scale parameter (must be > 0).
     * @return CDF value in [0, 1]. Returns 1.0 for invalid (x, xi, sigma) combos.
     */
    static double cdf(double x, double xi, double sigma);

    /**
     * @brief GPD quantile function (inverse CDF).
     * @param p     Probability in (0, 1).
     * @param xi    Shape parameter.
     * @param sigma Scale parameter (must be > 0).
     * @return Quantile value.
     */
    static double quantile(double p, double xi, double sigma);

    /**
     * @brief GPD log-likelihood given a vector of exceedances.
     *
     * Returns -infinity when parameters are inadmissible (sigma <= 0,
     * xi <= -0.5, or any exceedance falls outside the support).
     *
     * @param exc   Vector of exceedance values (values above the threshold).
     * @param xi    Shape parameter.
     * @param sigma Scale parameter.
     * @return Log-likelihood value.
     */
    static double logLik(const std::vector<double>& exc,
                          double xi, double sigma);

    /**
     * @brief Fit GPD to a vector of exceedances using MLE (Nelder-Mead).
     *
     * Falls back to Probability-Weighted Moments (PWM) when n < 50.
     * The constraint xi > -0.5 is enforced via a penalty in the MLE objective.
     *
     * @param exc       Exceedances above the threshold (must not be empty).
     * @param threshold Threshold value u (stored in result for reference).
     * @param sigmaInit Initial sigma for MLE (0.0 = use sample mean as default).
     * @return GpdFitResult with xi, sigma, logLik, converged flag.
     * @throws DataException if exc is empty.
     */
    static GpdFitResult fit(const std::vector<double>& exc,
                             double threshold,
                             double sigmaInit = 0.0);

    /**
     * @brief Compute the T-period return level for a fitted GPD.
     *
     * Return level z_T satisfies P(X > z_T) = 1/T per time_unit.
     * Formula (xi != 0): z_T = threshold + sigma/xi * ((T*lambda)^xi - 1)
     * Formula (xi == 0): z_T = threshold + sigma * log(T*lambda)
     *
     * @param T         Return period (in time_unit units, e.g. hours).
     * @param lambda    Exceedance rate (exceedances per time_unit).
     * @param threshold Threshold value u.
     * @param xi        Shape parameter.
     * @param sigma     Scale parameter.
     * @return Return level estimate.
     */
    static double returnLevel(double T, double lambda,
                               double threshold, double xi, double sigma);

private:

    /// PWM fitting (used for n < 50).
    static GpdFitResult _fitPwm(const std::vector<double>& exc,
                                 double threshold);

    /// MLE fitting via Nelder-Mead.
    static GpdFitResult _fitMle(const std::vector<double>& exc,
                                 double threshold,
                                 double sigmaInit);
};

} // namespace loki::evt
