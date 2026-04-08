#pragma once

#include <loki/homogeneity/changePointResult.hpp>

#include <cstddef>
#include <vector>

namespace loki::homogeneity {

/**
 * @brief Configuration for BocpdDetector.
 *
 * Defined outside the class to avoid a GCC 13 bug with nested structs
 * that have member initializers used as default constructor arguments.
 */
struct BocpdDetectorConfig {
    /// Expected run length (average number of observations between change points).
    /// Corresponds to the mean of the geometric hazard prior.
    /// For climatological data with ~3-year segments at 6h resolution: lambda ~ 4383.
    double hazardLambda{250.0};

    /// Prior mean of the segment mean (Normal-InverseGamma model).
    double priorMean{0.0};

    /// Prior variance on the segment mean (kappa_0 in NIX parameterisation).
    /// Larger values = less confident prior on the mean.
    double priorVar{1.0};

    /// InverseGamma shape prior (alpha_0). Controls confidence in prior variance.
    double priorAlpha{1.0};

    /// InverseGamma scale prior (beta_0). Controls the scale of prior variance.
    double priorBeta{1.0};

    /// Probability threshold for declaring a change point.
    /// A CP is declared at time t when the posterior P(run_length=0 | data) >= threshold.
    double threshold{0.5};

    /// Minimum number of observations between consecutive change points.
    /// Prevents closely spaced spurious detections.
    int minSegmentLength{30};
};

/**
 * @brief Detects change points using Bayesian Online Change Point Detection
 *        (BOCPD, Adams & MacKay 2007).
 *
 * Maintains a posterior distribution over "run length" r_t -- the number of
 * observations since the last change point. At each time step t:
 *
 *   1. Compute the predictive probability of z[t] under each hypothesised
 *      run length using a Normal-InverseGamma (NIX) conjugate model.
 *   2. Update the run-length posterior via Bayes' rule.
 *   3. Apply the hazard function: with probability 1/lambda the run resets
 *      to 0 (change point); with probability 1 - 1/lambda it grows by 1.
 *
 * A change point is declared at time t when the probability of a run-length
 * reset (r_t = 0) exceeds cfg.threshold, subject to cfg.minSegmentLength.
 *
 * Unlike PELT, BOCPD operates online (left-to-right) and returns a smooth
 * posterior probability trace via posteriorChangeProb(). This can be plotted
 * as a separate diagnostic panel.
 *
 * MultiChangePointDetector calls detectAll() once and bypasses split().
 *
 * Reference: Adams, R. P., & MacKay, D. J. C. (2007).
 * Bayesian online changepoint detection. arXiv:0710.3742.
 */
class BocpdDetector {
public:

    /// Configuration type alias.
    using Config = BocpdDetectorConfig;

    /**
     * @brief Constructs a BocpdDetector with the given configuration.
     * @param cfg Detection parameters.
     */
    explicit BocpdDetector(Config cfg = Config{});

    /**
     * @brief Detects all change points and returns them as a sorted vector.
     *
     * Internally calls posteriorChangeProb() and applies threshold + minSegmentLength
     * to convert the continuous posterior into discrete change point positions.
     *
     * @param z     Deseasonalised, gap-free time series values.
     * @param times Optional MJD timestamps (same length as z).
     * @return      Sorted vector of detected change points.
     * @throws loki::DataException if z has fewer than MIN_SERIES points,
     *         or if times is non-empty and times.size() != z.size().
     */
    [[nodiscard]]
    std::vector<ChangePoint> detectAll(const std::vector<double>& z,
                                       const std::vector<double>& times = {}) const;

    /**
     * @brief Returns the raw posterior P(change point at t | data_1..t) for each t.
     *
     * The returned vector has the same length as z. Values are in [0, 1].
     * This can be used for plotting a continuous CP probability trace.
     *
     * @param z Deseasonalised, gap-free time series values.
     * @return  Vector of posterior change point probabilities.
     * @throws loki::DataException if z has fewer than MIN_SERIES points.
     */
    [[nodiscard]]
    std::vector<double> posteriorChangeProb(const std::vector<double>& z) const;

    /// Minimum series length required for detection.
    static constexpr std::size_t MIN_SERIES = 10;

private:

    Config m_cfg;

    // -----------------------------------------------------------------------
    //  NIX (Normal-InverseGamma) sufficient statistics
    // -----------------------------------------------------------------------

    /**
     * @brief Sufficient statistics for one run under the NIX model.
     *
     * Updated incrementally as new observations arrive within a run.
     * Each candidate run length maintains its own NixStats instance.
     */
    struct NixStats {
        double kappa;  ///< Precision weight on the mean (grows with observations).
        double mu;     ///< Posterior mean estimate.
        double alpha;  ///< InvGamma shape (grows with observations).
        double beta;   ///< InvGamma scale (accumulates squared deviations).
        int    count;  ///< Number of observations in this run.
    };

    /**
     * @brief Returns the initial NIX statistics from the prior.
     */
    NixStats priorStats() const;

    /**
     * @brief Updates NIX statistics with one new observation x.
     * @param s Existing statistics.
     * @param x New observation.
     * @return  Updated statistics.
     */
    static NixStats updateStats(const NixStats& s, double x);

    /**
     * @brief Computes the log predictive probability of x under the NIX model.
     *
     * This is the log Student-t predictive density:
     *
     *   log p(x | s) = log T_{2*alpha}(x; mu, beta*(kappa+1)/(alpha*kappa))
     *
     * @param s NIX statistics before observing x.
     * @param x New observation.
     * @return  Log predictive probability.
     */
    static double logPredictive(const NixStats& s, double x);
};

} // namespace loki::homogeneity