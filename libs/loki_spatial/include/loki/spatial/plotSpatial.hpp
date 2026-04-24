// ==========================================================================
// plotSpatial.hpp
// ==========================================================================
#pragma once

#include <loki/core/config.hpp>
#include <loki/spatial/spatialResult.hpp>

#include <string>

namespace loki::spatial {

/**
 * @brief Plot generator for the spatial interpolation pipeline.
 *
 * All plots follow the naming convention:
 *   spatial_[dataset]_[variable]_[plottype].[format]
 *
 * Available plots (controlled via PlotConfig flags):
 *   spatial_heatmap      -- interpolated grid as pm3d colour map.
 *   spatial_scatter      -- input scatter points coloured by value.
 *   spatial_variogram    -- empirical bins + fitted model (Kriging only).
 *   spatial_crossval     -- LOO errors scatter map.
 *   spatial_variance     -- prediction variance grid (Kriging only).
 */
class PlotSpatial {
public:

    explicit PlotSpatial(const AppConfig& cfg);

    void plot(const SpatialResult& result,
              const std::string&   datasetName) const;

private:

    const AppConfig& m_cfg;

    void _plotHeatmap   (const SpatialResult& result, const std::string& dataset) const;
    void _plotScatter   (const SpatialResult& result, const std::string& dataset) const;
    void _plotVariogram (const SpatialResult& result, const std::string& dataset) const;
    void _plotCrossVal  (const SpatialResult& result, const std::string& dataset) const;
    void _plotVariance  (const SpatialResult& result, const std::string& dataset) const;

    static std::string fwdSlash(const std::string& p);

    std::string _plotPath(const std::string& dataset,
                          const std::string& variable,
                          const std::string& plottype) const;
};

} // namespace loki::spatial
