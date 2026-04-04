#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace loki::ssa {

/**
 * @brief A named group of eigentriples and their combined reconstruction.
 *
 * Each group aggregates one or more elementary SSA components (identified
 * by their 0-based eigentriple index) into a single reconstructed signal.
 * The reconstruction is the sum of the individual diagonal-averaged
 * elementary series belonging to this group.
 */
struct SsaGroup {
    std::string         name;             ///< Group label: "trend", "annual", "noise", etc.
    std::vector<int>    indices;          ///< 0-based eigentriple indices assigned to this group.
    std::vector<double> reconstruction;  ///< Diagonal-averaged reconstruction, length n.
    double              varianceFraction{0.0}; ///< Sum of eigenvalue fractions for this group.
};

/**
 * @brief Complete result of a single-channel SSA decomposition.
 *
 * Produced by SsaAnalyzer::analyze(). Contains the full SVD output,
 * elementary reconstructed components, w-correlation matrix, and the
 * final grouped reconstructions.
 *
 * Dimension conventions:
 *   n = original series length
 *   L = window length (number of columns of trajectory matrix)
 *   K = n - L + 1    (number of rows of trajectory matrix)
 *   r = min(K, L)    (number of non-zero singular values)
 */
struct SsaResult {

    // -------------------------------------------------------------------------
    //  Input info
    // -------------------------------------------------------------------------

    std::size_t n{0};  ///< Original series length.
    int         L{0};  ///< Window length used.
    int         K{0};  ///< K = n - L + 1.

    // -------------------------------------------------------------------------
    //  SVD output  (length r = min(K, L) for each vector)
    // -------------------------------------------------------------------------

    std::vector<double> eigenvalues;        ///< Squared singular values s_i^2, descending.
    std::vector<double> varianceFractions;  ///< Per-component: eigenvalues[i] / sum(eigenvalues).

    // -------------------------------------------------------------------------
    //  Elementary reconstructed series
    //  components[i] is the diagonal-averaged reconstruction of eigentriple i,
    //  length n. Produced before grouping.
    // -------------------------------------------------------------------------

    std::vector<std::vector<double>> components;

    // -------------------------------------------------------------------------
    //  W-correlation matrix
    //  wCorrMatrix is r x r. Empty if SsaConfig::computeWCorr == false.
    // -------------------------------------------------------------------------

    std::vector<std::vector<double>> wCorrMatrix;

    // -------------------------------------------------------------------------
    //  Grouping and reconstruction
    // -------------------------------------------------------------------------

    std::vector<SsaGroup> groups;          ///< Named groups with reconstructed signals.
    std::string groupingMethod;            ///< "manual" | "wcorr" | "kmeans" | "variance"

    // -------------------------------------------------------------------------
    //  Convenience aliases (filled from groups by name lookup after grouping)
    //  Empty if no group with the corresponding name exists.
    // -------------------------------------------------------------------------

    std::vector<double> trend;  ///< Reconstruction of the group named "trend".
    std::vector<double> noise;  ///< Reconstruction of the group named "noise".
};

} // namespace loki::ssa
