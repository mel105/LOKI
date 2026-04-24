#pragma once

#include <loki/math/spatialTypes.hpp>

#include <Eigen/Dense>

#include <vector>

namespace loki::math {

// =============================================================================
//  Natural Neighbor interpolation (Sibson 1981)
// =============================================================================

/**
 * @brief Delaunay triangulation built via the Bowyer-Watson algorithm.
 *
 * Supports incremental point insertion. The triangulation is used internally
 * by NaturalNeighborInterpolator to compute Sibson coordinates.
 *
 * The bounding super-triangle is added automatically at construction and
 * its vertices are excluded from all query results.
 *
 * Time complexity:
 *   Construction: O(n log n) expected (O(n^2) worst case for degenerate inputs).
 *   Query:        O(n) -- circumcircle search is linear in the current v1.
 *                 FUTURE: O(log n) via spatial indexing.
 *
 * Numerical stability:
 *   Circumcircle test uses exact arithmetic via the standard 3x3 determinant.
 *   For nearly co-circular points the test may be unreliable at machine
 *   precision. A small epsilon jitter (1e-9 * mean inter-point distance)
 *   is applied to input coordinates to avoid exact degeneracies.
 */
struct Triangulation {
    // Internal triangle representation (indices into vertex list)
    struct Triangle {
        int  v[3];       ///< Vertex indices (CCW order).
        int  adj[3];     ///< Adjacent triangle indices (-1 = boundary).
        // adj[k] is opposite to vertex v[k]
    };

    std::vector<std::array<double, 2>> vertices;  ///< All vertices incl. super-triangle.
    std::vector<Triangle>              triangles;  ///< All non-deleted triangles.
    int                                nSuper = 0; ///< Number of super-triangle vertices (3).

    /**
     * @brief True if triangle index refers to the super-triangle boundary.
     */
    bool isSuperTriangle(int triIdx) const noexcept;

    /**
     * @brief Circumcircle test: returns true if point (px, py) is strictly
     *        inside the circumcircle of triangle triIdx.
     */
    bool inCircumcircle(int triIdx, double px, double py) const noexcept;
};

/**
 * @brief Build a Delaunay triangulation of the input scatter points.
 *
 * Uses the Bowyer-Watson incremental insertion algorithm. A super-triangle
 * encompassing all input points is initialised first, then each point
 * is inserted one by one.
 *
 * @param points  Input scatter observations (x, y, value).
 * @return        Fully constructed Triangulation.
 * @throws DataException if fewer than 3 points are provided.
 */
Triangulation buildDelaunay(const std::vector<SpatialPoint>& points);

// =============================================================================
//  Natural Neighbor interpolation
// =============================================================================

/**
 * @brief Natural Neighbor (Sibson) interpolant for 2-D scatter data.
 *
 * The Sibson weights for a query point P are the fractional areas of the
 * Voronoi cells of each input point that are "stolen" by P when it is
 * temporarily inserted into the Delaunay triangulation.
 *
 * For a query coincident with an input point the weight is 1 for that point
 * and 0 for all others (exact interpolation property).
 *
 * For a query outside the convex hull of the input points, extrapolation
 * is not defined. The implementation returns the value of the nearest
 * input point with a warning logged.
 *
 * Properties:
 *   - C^1 everywhere except at input points (C^0 there).
 *   - Local: only nearby input points contribute to each query.
 *   - No shape parameter required.
 *   - Reproduces linear functions exactly.
 *
 * Reference: Sibson (1981), "A Brief Description of Natural Neighbour
 * Interpolation", in Interpolating Multivariate Data, Wiley, pp. 21-36.
 */
class NaturalNeighborInterpolator {
public:

    /**
     * @brief Construct the interpolator from scatter observations.
     *
     * Builds the Delaunay triangulation immediately at construction.
     *
     * @param points  Input scatter observations. Must have >= 3 non-coincident.
     * @throws DataException if fewer than 3 points or all collinear.
     */
    explicit NaturalNeighborInterpolator(const std::vector<SpatialPoint>& points);

    /**
     * @brief Estimate the value at a single query point (qx, qy).
     *
     * @param qx  Query X coordinate.
     * @param qy  Query Y coordinate.
     * @return    Interpolated value.
     */
    double interpolate(double qx, double qy) const;

    /**
     * @brief Estimate values on a regular grid.
     *
     * @param extent  Grid geometry (must have nRows, nCols, xMin, yMin, resX, resY).
     * @return        Matrix of interpolated values (nRows x nCols).
     */
    Eigen::MatrixXd interpolateGrid(const GridExtent& extent) const;

    /**
     * @brief Leave-one-out cross-validation.
     *
     * @return SpatialCrossValidationResult (stdErrors are zero -- no UQ).
     * @throws DataException if fewer than 4 points.
     */
    SpatialCrossValidationResult crossValidate() const;

private:

    std::vector<SpatialPoint> m_points;   ///< Input scatter data (cached).
    Triangulation             m_tri;      ///< Delaunay triangulation.

    /**
     * @brief Compute Sibson coordinates for query point (qx, qy).
     *
     * Returns a sparse vector of (pointIndex, weight) pairs.
     * Weights sum to 1. Returns empty vector if (qx, qy) is outside
     * the convex hull.
     *
     * Algorithm:
     *   1. Insert the query point Q temporarily into the triangulation.
     *   2. Find all triangles whose circumcircle contains Q (Bowyer-Watson
     *      cavity = natural neighbours).
     *   3. For each natural neighbour i, weight_i = area(VoronoiCell(i) INTERSECT
     *      VoronoiCell_new(Q)) / area(VoronoiCell_new(Q)).
     *
     * Area computation uses the fact that the Voronoi vertices are the
     * circumcentres of the Delaunay triangles.
     */
    std::vector<std::pair<int, double>> _sibsonCoords(double qx, double qy) const;
};

} // namespace loki::math
