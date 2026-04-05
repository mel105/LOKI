#pragma once

#include <string>
#include <vector>

namespace loki {

// -----------------------------------------------------------------------------
//  DecompositionMethod
// -----------------------------------------------------------------------------

/// Identifies which algorithm produced a DecompositionResult.
enum class DecompositionMethod {
    CLASSICAL, ///< Moving-average trend + per-slot seasonal median/mean.
    STL        ///< Iterative LOESS trend + LOESS-smoothed seasonal (Cleveland 1990).
};

// -----------------------------------------------------------------------------
//  DecompositionResult
// -----------------------------------------------------------------------------

/**
 * @brief Holds the three additive components of a decomposed time series.
 *
 * The decomposition identity holds for every valid index i:
 *   original[i] == trend[i] + seasonal[i] + residual[i]
 *
 * All three vectors have the same length as the input series passed to
 * ClassicalDecomposer::decompose() or StlDecomposer::decompose(). No NaN
 * values are present in any component -- the decomposer guarantees this
 * by requiring a fully gap-filled input series.
 */
struct DecompositionResult {
    std::vector<double> trend;    ///< Estimated trend component T[t].
    std::vector<double> seasonal; ///< Estimated seasonal component S[t].
    std::vector<double> residual; ///< Remainder R[t] = Y[t] - T[t] - S[t].

    DecompositionMethod method;   ///< Algorithm that produced this result.
    int                 period;   ///< Period used for decomposition (in samples).
};

} // namespace loki
