#pragma once

#include <loki/evt/evtResult.hpp>

#include <vector>

namespace loki::evt {

/**
 * @brief Generalised Extreme Value (GEV) distribution utilities.
 *
 * Provides CDF, quantile, log-likelihood, MLE fitting, and return level
 * computation for the three-parameter GEV used in Block Maxima analysis.
 *
 * Parameterisation (von Mises form):
 *   F(x) = exp(-(1 + xi*(x-mu)/sigma)^(-1/xi))  for xi != 0
 *   F(x) = exp(-exp(-(x-mu)/sigma))               for xi == 0  (Gumbel limit)
 *
 * Special cases:
 *   xi > 0 : Frechet distribution (heavy tail)
 *   xi = 0 : Gumbel distribution
 *   xi < 0 : Weibull distribution (bounded upper tail)
 *
 * Fitting uses MLE via Nelder-Mead.
 */
class Gev {
public:

    /**
     * @brief GEV cumulative distribution function.
     * @param x     Observation value.
     * @param xi    Shape parameter.
     * @param sigma Scale parameter (must be > 0).
     * @param mu    Location parameter.
     * @return CDF value in [0, 1].
     */
    static double cdf(double x, double xi, double sigma, double mu);

    /**
     * @brief GEV quantile function (inverse CDF).
     * @param p     Probability in (0, 1).
     * @param xi    Shape parameter.
     * @param sigma Scale parameter (must be > 0).
     * @param mu    Location parameter.
     * @return Quantile value.
     */
    static double quantile(double p, double xi, double sigma, double mu);

    /**
     * @brief GEV log-likelihood given a vector of block maxima.
     *
     * Returns -infinity when parameters are inadmissible.
     *
     * @param maxima Vector of block maximum values.
     * @param xi     Shape parameter.
     * @param sigma  Scale parameter.
     * @param mu     Location parameter.
     * @return Log-likelihood value.
     */
    static double logLik(const std::vector<double>& maxima,
                          double xi, double sigma, double mu);

    /**
     * @brief Fit GEV to a vector of block maxima using MLE (Nelder-Mead).
     *
     * @param maxima Block maxima (must contain at least 3 values).
     * @return GevFitResult with xi, sigma, mu, logLik, converged flag.
     * @throws DataException if maxima has fewer than 3 elements.
     */
    static GevFitResult fit(const std::vector<double>& maxima);

    /**
     * @brief Compute the T-period return level for a fitted GEV.
     *
     * @param T     Return period (same units as block length).
     * @param xi    Shape parameter.
     * @param sigma Scale parameter.
     * @param mu    Location parameter.
     * @return Return level estimate.
     */
    static double returnLevel(double T, double xi, double sigma, double mu);
};

} // namespace loki::evt
