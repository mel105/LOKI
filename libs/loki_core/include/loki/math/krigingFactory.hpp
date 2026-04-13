#pragma once

#include <loki/core/config.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/math/krigingBase.hpp>
#include <loki/math/ordinaryKriging.hpp>
#include <loki/math/simpleKriging.hpp>
#include <loki/math/universalKriging.hpp>

#include <memory>

namespace loki::math {

/**
 * @brief Create the appropriate Kriging estimator from configuration.
 *
 * Dispatches on cfg.mode and cfg.method:
 *   mode = "temporal"   : fully implemented.
 *   mode = "spatial"    : PLACEHOLDER -- throws AlgorithmException.
 *   mode = "space_time" : PLACEHOLDER -- throws AlgorithmException.
 *
 *   method = "simple"    : SimpleKriging   (known mean)
 *   method = "ordinary"  : OrdinaryKriging (default)
 *   method = "universal" : UniversalKriging (polynomial drift)
 *
 * @throws ConfigException    if cfg.method is not recognised.
 * @throws AlgorithmException if cfg.mode is "spatial" or "space_time".
 */
inline std::unique_ptr<KrigingBase> createKriging(const KrigingConfig& cfg)
{
    if (cfg.mode == "spatial" || cfg.mode == "space_time") {
        throw AlgorithmException(
            "createKriging: mode '" + cfg.mode + "' is not yet implemented. "
            "Only temporal Kriging is supported in this release. "
            "Spatial and space-time modes are planned for a future release.");
    }
    if (cfg.mode != "temporal") {
        throw ConfigException(
            "createKriging: unrecognised mode '" + cfg.mode + "'. "
            "Valid: temporal, spatial (placeholder), space_time (placeholder).");
    }

    if (cfg.method == "simple") {
        return std::make_unique<SimpleKriging>(cfg.knownMean, cfg.confidenceLevel);
    }
    if (cfg.method == "ordinary") {
        return std::make_unique<OrdinaryKriging>(cfg.confidenceLevel);
    }
    if (cfg.method == "universal") {
        if (cfg.trendDegree < 1 || cfg.trendDegree > 5) {
            throw ConfigException(
                "createKriging: trendDegree must be in [1, 5], got "
                + std::to_string(cfg.trendDegree) + ".");
        }
        return std::make_unique<UniversalKriging>(cfg.trendDegree,
                                                  cfg.confidenceLevel);
    }
    throw ConfigException(
        "createKriging: unrecognised method '" + cfg.method
        + "'. Valid: simple, ordinary, universal.");
}

} // namespace loki::math
