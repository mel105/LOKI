#pragma once

#include <string>
#include <vector>

namespace loki::simulate {

// -----------------------------------------------------------------------------
//  ParameterCI
// -----------------------------------------------------------------------------

/**
 * @brief Bootstrap confidence interval for one estimated model parameter.
 */
struct ParameterCI {
    std::string name;      ///< Parameter name (e.g. "ar[1]", "sigma2", "Q", "R").
    double      estimate;  ///< Statistic on the original data.
    double      lower;     ///< Lower CI bound.
    double      upper;     ///< Upper CI bound.
    double      bias;      ///< Bootstrap bias: mean(boot) - estimate.
    double      se;        ///< Bootstrap standard error.
};

// -----------------------------------------------------------------------------
//  SimulationResult
// -----------------------------------------------------------------------------

/**
 * @brief Output of one SimulateAnalyzer run (one series / one synthetic run).
 *
 * In "synthetic" mode: original is empty, simulations contains all nSim realizations.
 * In "bootstrap" mode: original holds the gap-filled input series,
 *   simulations holds B replicas generated from the fitted model,
 *   and parameterCIs holds bootstrap CIs for each fitted parameter.
 */
struct SimulationResult {
    // ---- identification -----------------------------------------------------
    std::string datasetName;    ///< Input file stem (or "synthetic").
    std::string componentName;  ///< Series componentName from metadata.
    std::string mode;           ///< "synthetic" | "bootstrap"
    std::string model;          ///< "arima" | "kalman" | "ar"

    // ---- dimensions ---------------------------------------------------------
    int n;             ///< Length of each simulated series.
    int nSimulations;  ///< Number of simulations generated.

    // ---- data ---------------------------------------------------------------
    std::vector<double> original;  ///< Original (gap-filled) values -- bootstrap mode only.

    /// All simulated series: outer index = simulation index, inner = time step.
    std::vector<std::vector<double>> simulations;

    // ---- envelope (percentiles across simulations at each time step) --------
    std::vector<double> env05;  ///< 5th percentile at each step.
    std::vector<double> env25;  ///< 25th percentile at each step.
    std::vector<double> env50;  ///< Median at each step.
    std::vector<double> env75;  ///< 75th percentile at each step.
    std::vector<double> env95;  ///< 95th percentile at each step.

    // ---- bootstrap parameter CIs (bootstrap mode only) ----------------------
    std::vector<ParameterCI> parameterCIs;

    // ---- cross-simulation summary statistics --------------------------------
    double simMeanMean;  ///< Mean of per-simulation means.
    double simMeanStd;   ///< Std dev of per-simulation means.
    double simStdMean;   ///< Mean of per-simulation std devs.
    double simStdStd;    ///< Std dev of per-simulation std devs.
};

} // namespace loki::simulate
