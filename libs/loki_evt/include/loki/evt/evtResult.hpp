#pragma once

#include <string>
#include <vector>

namespace loki::evt {

// -----------------------------------------------------------------------------
//  GPD fit result
// -----------------------------------------------------------------------------

/**
 * @brief Result of a Generalised Pareto Distribution fit.
 */
struct GpdFitResult {
    double xi        {0.0};   ///< Shape parameter.
    double sigma     {1.0};   ///< Scale parameter (must be > 0).
    double logLik    {0.0};   ///< Log-likelihood at fitted parameters.
    int    nExceedances{0};   ///< Number of exceedances used.
    double threshold {0.0};   ///< Threshold u used.
    double lambda    {1.0};   ///< Exceedances per time_unit (estimated from dt).
    bool   converged {false}; ///< True if MLE converged; false if PWM fallback used.
};

// -----------------------------------------------------------------------------
//  GEV fit result
// -----------------------------------------------------------------------------

/**
 * @brief Result of a Generalised Extreme Value distribution fit.
 */
struct GevFitResult {
    double xi          {0.0};   ///< Shape parameter.
    double sigma       {1.0};   ///< Scale parameter (must be > 0).
    double mu          {0.0};   ///< Location parameter.
    double logLik      {0.0};   ///< Log-likelihood at fitted parameters.
    int    nBlockMaxima{0};     ///< Number of block maxima used.
    bool   converged   {false}; ///< True if MLE converged.
};

// -----------------------------------------------------------------------------
//  Return level with confidence interval
// -----------------------------------------------------------------------------

/**
 * @brief A single return level estimate with CI for one return period.
 */
struct ReturnLevelCI {
    double period;    ///< Return period in time_unit units.
    double estimate;  ///< Point estimate of the return level.
    double lower;     ///< Lower CI bound.
    double upper;     ///< Upper CI bound.
};

// -----------------------------------------------------------------------------
//  Goodness-of-fit results
// -----------------------------------------------------------------------------

/**
 * @brief Anderson-Darling and Kolmogorov-Smirnov GoF test results.
 */
struct GoFResult {
    double adStatistic {0.0};
    double adPvalue    {0.0};
    double ksStatistic {0.0};
    double ksPvalue    {0.0};
};

// -----------------------------------------------------------------------------
//  Full EVT result
// -----------------------------------------------------------------------------

/**
 * @brief Complete output of one EVT analysis run for a single series.
 */
struct EvtResult {
    std::string method; ///< "pot" | "block_maxima"

    // Fitted model (exactly one is populated depending on method).
    GpdFitResult gpd;   ///< Populated when method == "pot".
    GevFitResult gev;   ///< Populated when method == "block_maxima".

    // Return levels (one entry per configured return period).
    std::vector<ReturnLevelCI> returnLevels;

    // Goodness of fit.
    GoFResult gof;

    // Raw data extracted from the series.
    std::vector<double> exceedances;   ///< Values above threshold (POT).
    std::vector<double> blockMaxima;   ///< Per-block maxima (block maxima method).

    // Threshold selector diagnostics (for mean excess and stability plots).
    std::vector<double> thresholdCandidates; ///< Candidate threshold values.
    std::vector<double> meanExcessValues;    ///< Mean excess at each candidate.
    std::vector<double> sigmaStability;      ///< Fitted sigma at each candidate.
    std::vector<double> xiStability;         ///< Fitted xi at each candidate.
};

} // namespace loki::evt
