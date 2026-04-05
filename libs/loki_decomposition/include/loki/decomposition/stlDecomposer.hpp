#pragma once

#include <loki/decomposition/decompositionResult.hpp>
#include <loki/core/config.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <vector>

namespace loki {

/**
 * @brief STL decomposition: Seasonal-Trend decomposition using LOESS.
 *
 * Implements the algorithm of Cleveland, Cleveland, McRae & Terpenning (1990).
 * The series is decomposed additively as Y[t] = T[t] + S[t] + R[t] via an
 * inner loop (seasonal smoothing + trend smoothing) iterated nInner times,
 * and an outer robustness loop (nOuter times) that computes bisquare weights
 * from the current residuals and passes them into the inner loop LOESS fits.
 *
 * Inner loop per iteration:
 *   1. Detrend:    W[t] = Y[t] - T[t]  (T[t] = 0 on first pass)
 *   2. Subseries:  for each slot s, smooth the subsequence W[s], W[s+p], ...
 *                  with LOESS (sBandwidth, sDegree) -> C[s] (cycle-subseries)
 *   3. Low-pass:   L[t] = MA(period, MA(period, MA(3, C[t]))) -> L
 *   4. Seasonal:   S[t] = C[t] - L[t]
 *   5. Deseason:   V[t] = Y[t] - S[t]
 *   6. Trend:      T[t] = LOESS(V, tBandwidth, tDegree)
 *
 * Outer loop robustness weights (bisquare):
 *   h = 6 * median(|R[t]|)
 *   rho[t] = B(|R[t]| / h)  where B(u) = (1 - u^2)^2 for u < 1, else 0
 *   These weights multiply the kernel weights in all LOESS fits in the
 *   following inner loop iteration.
 *
 * LOESS used here is a private implementation within this translation unit;
 * it does not depend on loki_filter. It supports external (robustness) weights
 * in addition to the standard distance-based kernel weights.
 *
 * Bandwidth auto-selection (when sBandwidth or tBandwidth = 0):
 *   sBandwidth = 1.5 / period   (at least degree+1 neighbours per subseries)
 *   tBandwidth = 1.5 * period / n  (smooth over ~1.5 periods)
 *
 * Reference: Cleveland, R.B., Cleveland, W.S., McRae, J.E., & Terpenning, I.
 * (1990). STL: A Seasonal-Trend Decomposition Procedure Based on LOESS.
 * Journal of Official Statistics, 6(1), 3-73.
 */
class StlDecomposer {
public:

    using Config = StlDecompositionConfig;

    /**
     * @brief Constructs with configuration.
     * @param cfg STL configuration.
     */
    explicit StlDecomposer(Config cfg = Config{});

    /**
     * @brief Decomposes the series into trend, seasonal, and residual via STL.
     *
     * @param ts     Input time series. Must be free of NaN. Must have
     *               at least 2 * period observations.
     * @param period Period in samples (>= 2).
     * @return DecompositionResult with all three components.
     * @throws DataException   if ts contains NaN or is too short.
     * @throws ConfigException if period < 2.
     */
    [[nodiscard]]
    DecompositionResult decompose(const TimeSeries& ts, int period) const;

private:

    Config m_cfg;

    // -------------------------------------------------------------------------
    //  Private LOESS implementation (supports external robustness weights)
    // -------------------------------------------------------------------------

    /**
     * @brief Locally weighted polynomial regression (LOESS/LOWESS).
     *
     * For each output point i, selects k = ceil(bandwidth * n) nearest
     * neighbours by index distance, computes tricube kernel weights, multiplies
     * by external robustness weights (if provided), and fits a polynomial of
     * the given degree via weighted least squares. The smoothed value at i is
     * the constant term (fitted value at x=0 in local coordinates).
     *
     * @param y          Input values (length n). Must be NaN-free.
     * @param bandwidth  Fraction of n used as neighbourhood. In (0, 1].
     * @param degree     Local polynomial degree (1 or 2).
     * @param robWeights External robustness weights, length n, or empty (= all 1).
     *                   Combined multiplicatively with the tricube kernel weights.
     * @return Smoothed vector of length n.
     * @throws ConfigException if bandwidth or degree is invalid.
     */
    [[nodiscard]]
    std::vector<double> loess(const std::vector<double>& y,
                              double bandwidth,
                              int degree,
                              const std::vector<double>& robWeights) const;

    /**
     * @brief Computes bisquare robustness weights from current residuals.
     *
     * h = 6 * median(|R[t]|)
     * rho[t] = (1 - (|R[t]|/h)^2)^2  for |R[t]| < h, else 0.
     * If h < epsilon (all residuals near zero), returns all-ones weights.
     *
     * @param residual Current residual vector R[t] = Y[t] - T[t] - S[t].
     * @return Robustness weight vector of same length.
     */
    [[nodiscard]]
    static std::vector<double> bisquareWeights(const std::vector<double>& residual);

    /**
     * @brief 3-point simple moving average (helper for low-pass filter step).
     *
     * Edge positions are filled by bfill/ffill (nearest neighbour).
     *
     * @param y Input vector.
     * @return Smoothed vector of same length.
     */
    [[nodiscard]]
    static std::vector<double> ma3(const std::vector<double>& y);

    /**
     * @brief Centered moving average of given width (helper for low-pass step).
     *
     * NaN edges are filled by bfill/ffill. If width is even, it is rounded up.
     *
     * @param y     Input vector.
     * @param width Window width.
     * @return Smoothed vector of same length (no NaN).
     */
    [[nodiscard]]
    static std::vector<double> maFilled(const std::vector<double>& y, int width);

    /**
     * @brief Inner STL loop: one pass of subseries smoothing + trend smoothing.
     *
     * @param y          Original series values (length n).
     * @param trend      Current trend estimate T[t] (length n).
     * @param period     Period in samples.
     * @param sBw        Seasonal LOESS bandwidth (resolved).
     * @param tBw        Trend LOESS bandwidth (resolved).
     * @param robWeights Robustness weights (length n, or empty for all-ones).
     * @param[out] seasonal Updated seasonal component S[t] (length n).
     * @param[out] trendOut Updated trend component T[t] (length n).
     */
    void innerLoop(const std::vector<double>& y,
                   const std::vector<double>& trend,
                   int period,
                   double sBw,
                   double tBw,
                   const std::vector<double>& robWeights,
                   std::vector<double>& seasonal,
                   std::vector<double>& trendOut) const;
};

} // namespace loki
