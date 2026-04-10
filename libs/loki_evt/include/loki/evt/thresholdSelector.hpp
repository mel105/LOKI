#pragma once

#include <vector>

namespace loki::evt {

/**
 * @brief Automatic and manual threshold selection for Peaks-Over-Threshold analysis.
 *
 * Two strategies are provided:
 *
 * autoSelect():
 *   Evaluates a grid of candidate thresholds. For each candidate, the mean
 *   excess (mean of exceedances) and the GPD stability parameters (sigma, xi)
 *   are computed. The selected threshold is found via elbow detection on the
 *   mean excess plot: the point where the plot begins to behave linearly,
 *   indicating that the GPD approximation is valid above that level.
 *
 * manual():
 *   Accepts a user-specified threshold and validates that at least
 *   minExceedances observations fall above it.
 */
class ThresholdSelector {
public:

    /**
     * @brief Output of threshold selection.
     */
    struct Result {
        double              selected;        ///< Selected threshold value.
        std::vector<double> candidates;      ///< All evaluated candidate values.
        std::vector<double> meanExcess;      ///< Mean excess at each candidate.
        std::vector<double> sigmaStability;  ///< Fitted GPD sigma at each candidate.
        std::vector<double> xiStability;     ///< Fitted GPD xi at each candidate.
    };

    /**
     * @brief Automatic threshold selection via mean excess elbow detection.
     *
     * Candidate thresholds span from the (1 - minExceedances/n) quantile down
     * to the median of the data, evaluated at nCandidates evenly-spaced levels.
     * The elbow is the candidate with the largest normalised second-difference
     * on the mean excess curve.
     *
     * @param data           Full data vector (raw, not pre-filtered).
     * @param nCandidates    Number of candidate thresholds to evaluate (default: 50).
     * @param minExceedances Minimum exceedances required per candidate (default: 30).
     * @return Result with selected threshold and diagnostic arrays.
     * @throws DataException if data is empty or no valid candidate is found.
     */
    static Result autoSelect(const std::vector<double>& data,
                              int nCandidates    = 50,
                              int minExceedances = 30);

    /**
     * @brief Manual threshold with validation.
     *
     * @param data           Full data vector.
     * @param threshold      User-specified threshold value.
     * @param minExceedances Minimum required exceedances above threshold.
     * @return Result with selected = threshold and diagnostic arrays computed.
     * @throws DataException if fewer than minExceedances observations exceed threshold.
     */
    static Result manual(const std::vector<double>& data,
                          double threshold,
                          int    minExceedances = 30);

private:

    /**
     * @brief Compute mean excess and GPD stability at a single threshold.
     *
     * @param sorted      Data sorted in ascending order.
     * @param u           Threshold value.
     * @param meanExc     Output: mean of exceedances.
     * @param sigma       Output: fitted GPD sigma (NaN if too few exceedances).
     * @param xi          Output: fitted GPD xi.
     * @param minExc      Minimum exceedance count needed for GPD fit.
     */
    static void _evalCandidate(const std::vector<double>& sorted,
                                double u,
                                double& meanExc,
                                double& sigma,
                                double& xi,
                                int    minExc);

    /**
     * @brief Return the index of the elbow in a curve y(x).
     *
     * Uses the maximum of the normalised second difference (curvature proxy).
     */
    static std::size_t _elbowIndex(const std::vector<double>& y);
};

} // namespace loki::evt
