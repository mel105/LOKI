#pragma once

#include <loki/core/config.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/spatial/spatialKrigingBase.hpp>
#include <loki/spatial/spatialKrigingOrdinary.hpp>
#include <loki/spatial/spatialKrigingSimple.hpp>
#include <loki/spatial/spatialKrigingUniversal.hpp>

#include <memory>

namespace loki::spatial {

/**
 * @brief Create a spatial Kriging estimator from SpatialConfig.
 *
 * Dispatches on cfg.kriging.method:
 *   "simple"    -> SpatialSimpleKriging
 *   "ordinary"  -> SpatialOrdinaryKriging (default)
 *   "universal" -> SpatialUniversalKriging
 *
 * @throws ConfigException if method string is not recognised.
 */
inline std::unique_ptr<SpatialKrigingBase> createSpatialKriging(
    const SpatialConfig& cfg)
{
    if (cfg.kriging.method == "simple") {
        return std::make_unique<SpatialSimpleKriging>(
            cfg.kriging.knownMean, cfg.confidenceLevel);
    }
    if (cfg.kriging.method == "ordinary") {
        return std::make_unique<SpatialOrdinaryKriging>(cfg.confidenceLevel);
    }
    if (cfg.kriging.method == "universal") {
        if (cfg.kriging.trendDegree < 1 || cfg.kriging.trendDegree > 2) {
            throw ConfigException(
                "createSpatialKriging: trendDegree must be 1 or 2, got "
                + std::to_string(cfg.kriging.trendDegree) + ".");
        }
        return std::make_unique<SpatialUniversalKriging>(
            cfg.kriging.trendDegree, cfg.confidenceLevel);
    }
    throw ConfigException(
        "createSpatialKriging: unrecognised method '" + cfg.kriging.method
        + "'. Valid: simple, ordinary, universal.");
}

} // namespace loki::spatial
