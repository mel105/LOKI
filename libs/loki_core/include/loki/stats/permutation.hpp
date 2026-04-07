#pragma once

#include <loki/stats/bootstrap.hpp>
#include <loki/stats/sampling.hpp>

#include <functional>
#include <string>
#include <vector>

namespace loki::stats {

// ---------------------------------------------------------------------------
//  Supporting types (defined outside functions -- GCC 13 aggregate-init)
// ---------------------------------------------------------------------------

/**
 * @brief Alternative hypothesis direction for permutation tests.
 *
 * TWO_SIDED : H1: statistic != hypothesized value (or a != b).
 * GREATER   : H1: statistic > hypothesized value  (or a > b).
 * LESS      : H1: statistic < hypothesized value  (or a < b).
 */
enum class Alternative {
    TWO_SIDED,
    GREATER,
    LESS
};

/**
 * @brief Configuration for permutation tests.
 */
struct PermutationConfig {
    int         nPermutations;  ///< Number of random permutations. Default 9999.
    double      alpha;          ///< Significance level. Default 0.05.
    Alternative alternative;    ///< Alternative hypothesis direction. Default TWO_SIDED.

    PermutationConfig()
        : nPermutations(9999)
        , alpha(0.05)
        , alternative(Alternative::TWO_SIDED)
    {}

    PermutationConfig(int np, double a, Alternative alt)
        : nPermutations(np)
        , alpha(a)
        , alternative(alt)
    {}
};

/**
 * @brief Result of a permutation test.
 */
struct PermutationResult {
    double      statistic;      ///< Test statistic computed on the original data.
    double      pValue;         ///< Permutation p-value.
    bool        rejected;       ///< True if H0 rejected at cfg.alpha.
    int         nPermutations;  ///< Number of permutations actually used.
    std::string testName;       ///< Name of the test (for logging and reports).
};

// ---------------------------------------------------------------------------
//  StatFn2: two-sample statistic
// ---------------------------------------------------------------------------

/**
 * @brief Type alias for a two-sample scalar statistic.
 *
 * Receives two data vectors and returns a single double.
 * Common examples:
 *   - difference of means: [](const auto& a, const auto& b){ return mean(a)-mean(b); }
 *   - difference of medians
 *   - ratio of variances
 *
 * The function must be pure with respect to its inputs (no hidden state).
 */
using StatFn2 = std::function<double(const std::vector<double>&,
                                     const std::vector<double>&)>;

// ---------------------------------------------------------------------------
//  Permutation test functions
// ---------------------------------------------------------------------------

/**
 * @brief One-sample permutation test against a hypothesized value.
 *
 * Tests H0: the population statistic equals hypothesizedValue.
 *
 * Algorithm:
 *   1. Compute observed = statistic(data).
 *   2. Centre the data: shifted[i] = data[i] - observed + hypothesizedValue,
 *      so that the null hypothesis holds exactly in the shifted sample.
 *   3. For each permutation: randomly flip signs of (shifted[i] - hypothesizedValue)
 *      to simulate sampling under H0, compute statistic on the flipped sample.
 *   4. p-value = proportion of permutation statistics as or more extreme than observed.
 *
 * Suitable for: testing whether the mean, median, or any scalar summary of a
 * single sample equals a known reference value. Non-parametric alternative to
 * the one-sample t-test when normality is doubtful.
 *
 * @param data              Original data vector (NaN-free, n >= 2).
 * @param statistic         Scalar statistic (StatFn from bootstrap.hpp).
 * @param hypothesizedValue Value to test against under H0.
 * @param sampler           Random sampler (caller owns).
 * @param cfg               Test configuration.
 * @return                  PermutationResult with testName "one-sample-permutation".
 * @throws SeriesTooShortException if data.size() < 2.
 * @throws ConfigException         if nPermutations < 99 or alpha not in (0, 0.5).
 */
PermutationResult oneSampleTest(
    const std::vector<double>& data,
    const StatFn&              statistic,
    double                     hypothesizedValue,
    Sampler&                   sampler,
    const PermutationConfig&   cfg = PermutationConfig{});

/**
 * @brief Two-sample permutation test for equality of a statistic across groups.
 *
 * Tests H0: the statistic is the same in both populations.
 *
 * Algorithm:
 *   1. Compute observed = statistic(a, b).
 *   2. Pool a and b into one combined vector.
 *   3. For each permutation: randomly split the pooled vector into two groups
 *      of sizes |a| and |b|, compute statistic on the split.
 *   4. p-value = proportion of permutation statistics as or more extreme than observed.
 *
 * Suitable for: comparing two independent groups without assuming equal variances
 * or normality. Non-parametric alternative to the two-sample t-test or Levene's test.
 *
 * @param a         First group data (NaN-free, n >= 2).
 * @param b         Second group data (NaN-free, n >= 2).
 * @param statistic Two-sample statistic (receives group a, then group b).
 * @param sampler   Random sampler.
 * @param cfg       Test configuration.
 * @return          PermutationResult with testName "two-sample-permutation".
 * @throws SeriesTooShortException if a.size() < 2 or b.size() < 2.
 * @throws ConfigException         if nPermutations < 99 or alpha not in (0, 0.5).
 */
PermutationResult twoSampleTest(
    const std::vector<double>& a,
    const std::vector<double>& b,
    const StatFn2&             statistic,
    Sampler&                   sampler,
    const PermutationConfig&   cfg = PermutationConfig{});

/**
 * @brief Permutation test for zero correlation between two variables.
 *
 * Tests H0: Pearson correlation between x and y equals zero.
 *
 * Algorithm:
 *   1. Compute observed Pearson r on (x, y).
 *   2. For each permutation: randomly shuffle y (leaving x fixed), compute r.
 *   3. p-value = proportion of permutation r values as or more extreme than observed.
 *
 * Suitable for: testing association between two variables without assuming
 * bivariate normality. Non-parametric alternative to the t-test on Pearson r.
 * Also valid when x or y contains ties (unlike Spearman rank approximations).
 *
 * @param x       First variable (NaN-free, same length as y, n >= 4).
 * @param y       Second variable (NaN-free, same length as x, n >= 4).
 * @param sampler Random sampler.
 * @param cfg     Test configuration.
 * @return        PermutationResult with testName "permutation-correlation",
 *                statistic = Pearson r on original data.
 * @throws SeriesTooShortException if x.size() < 4.
 * @throws DataException           if x.size() != y.size().
 * @throws ConfigException         if nPermutations < 99 or alpha not in (0, 0.5).
 */
PermutationResult correlationTest(
    const std::vector<double>& x,
    const std::vector<double>& y,
    Sampler&                   sampler,
    const PermutationConfig&   cfg = PermutationConfig{});

} // namespace loki::stats
