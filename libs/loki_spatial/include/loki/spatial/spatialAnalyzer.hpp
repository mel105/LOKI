#pragma once

#include <loki/core/config.hpp>
#include <loki/math/spatialTypes.hpp>
#include <loki/spatial/spatialResult.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace loki::spatial {

/**
 * @brief Orchestrator for the spatial interpolation pipeline.
 *
 * Executes the full workflow for one variable (one set of (x, y, value) points):
 *   1. Validate scatter data and compute grid extent.
 *   2. Dispatch to the configured interpolation method:
 *        - "kriging"          : variogram + Kriging system + grid prediction.
 *        - "idw"              : Inverse Distance Weighting.
 *        - "rbf"              : Radial Basis Function interpolation.
 *        - "natural_neighbor" : Sibson interpolation via Delaunay triangulation.
 *        - "bspline_surface"  : Tensor product B-spline surface.
 *        - "nurbs"            : PLACEHOLDER -- throws AlgorithmException.
 *   3. LOO cross-validation (if enabled).
 *   4. Write protocol and CSV output.
 *   5. Return SpatialResult for the caller to pass to PlotSpatial.
 *
 * Usage:
 * @code
 *   loki::spatial::SpatialAnalyzer analyzer(cfg);
 *   loki::spatial::PlotSpatial     plotter(cfg);
 *   const auto result = analyzer.run(points, "dataset", "temperature");
 *   plotter.plot(result, "dataset");
 * @endcode
 */
class SpatialAnalyzer {
public:

    explicit SpatialAnalyzer(const AppConfig& cfg);

    /**
     * @brief Run the spatial interpolation pipeline for one variable.
     *
     * Writes protocol and CSV files as a side-effect.
     * Returns the full SpatialResult for downstream use (plotting).
     *
     * @param points      Input scatter observations (x, y, value).
     * @param datasetName File stem for output naming.
     * @param varName     Variable label for output naming and protocol.
     * @return            SpatialResult containing grid, CV, and metadata.
     * @throws DataException      if fewer than 3 valid points.
     * @throws AlgorithmException on numerical failure.
     * @throws ConfigException    if unsupported method is requested.
     */
    SpatialResult run(const std::vector<loki::math::SpatialPoint>& points,
                      const std::string&                            datasetName,
                      const std::string&                            varName);

private:

    const AppConfig& m_cfg;

    loki::math::GridExtent _buildGridExtent(
        const std::vector<loki::math::SpatialPoint>& points) const;

    static double _estimateResolution(
        const std::vector<loki::math::SpatialPoint>& points);

    SpatialResult _runKriging(
        const std::vector<loki::math::SpatialPoint>& points,
        const loki::math::GridExtent&                extent,
        const std::string&                            varName) const;

    SpatialResult _runIDW(
        const std::vector<loki::math::SpatialPoint>& points,
        const loki::math::GridExtent&                extent,
        const std::string&                            varName) const;

    SpatialResult _runRBF(
        const std::vector<loki::math::SpatialPoint>& points,
        const loki::math::GridExtent&                extent,
        const std::string&                            varName) const;

    SpatialResult _runNaturalNeighbor(
        const std::vector<loki::math::SpatialPoint>& points,
        const loki::math::GridExtent&                extent,
        const std::string&                            varName) const;

    SpatialResult _runBSplineSurface(
        const std::vector<loki::math::SpatialPoint>& points,
        const loki::math::GridExtent&                extent,
        const std::string&                            varName) const;

    void _writeProtocol(const SpatialResult& result,
                        const std::string&   datasetName) const;

    void _writeCsv(const SpatialResult& result,
                   const std::string&   datasetName) const;

    static void _computeStats(const std::vector<loki::math::SpatialPoint>& points,
                               double& mean, double& var);
};

} // namespace loki::spatial
