#pragma once

#include <loki/math/krigingTypes.hpp>

#include <string>
#include <vector>

namespace loki::kriging {

/**
 * @brief Top-level result container for the Kriging pipeline.
 *
 * Produced by KrigingAnalyzer::run() and consumed by PlotKriging and
 * the protocol/CSV writers.
 *
 * The component types (VariogramPoint, VariogramFitResult, KrigingPrediction,
 * CrossValidationResult) are defined in loki/math/krigingTypes.hpp and
 * live in namespace loki::math. They are imported here for convenience.
 */
using loki::math::VariogramPoint;
using loki::math::VariogramFitResult;
using loki::math::KrigingPrediction;
using loki::math::CrossValidationResult;

struct KrigingResult {
    std::string                    mode;               ///< "temporal"|"spatial"|"space_time"
    std::string                    method;             ///< "simple"|"ordinary"|"universal"
    int                            nObs;               ///< Number of valid observations used.
    double                         meanValue;          ///< Sample mean of observations.
    double                         sampleVariance;     ///< Sample variance of observations.
    VariogramFitResult             variogram;          ///< Fitted variogram parameters.
    std::vector<VariogramPoint>    empiricalVariogram; ///< Empirical variogram bins.
    std::vector<KrigingPrediction> predictions;        ///< Estimates on obs grid + targets.
    CrossValidationResult          crossValidation;    ///< LOO cross-validation diagnostics.
    std::string                    componentName;      ///< Series component name (for output).
};

} // namespace loki::kriging
