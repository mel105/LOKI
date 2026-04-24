#include <loki/math/naturalNeighbor.hpp>

#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <unordered_set>

using namespace loki;

namespace loki::math {

// =============================================================================
//  Internal geometry helpers
// =============================================================================

namespace {

// Circumcentre of a triangle given its three vertices.
// Returns false if the triangle is degenerate.
bool circumcentre(double ax, double ay,
                  double bx, double by,
                  double cx, double cy,
                  double& outX, double& outY)
{
    const double D = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
    if (std::abs(D) < 1.0e-14) return false;

    const double a2 = ax * ax + ay * ay;
    const double b2 = bx * bx + by * by;
    const double c2 = cx * cx + cy * cy;

    outX = (a2 * (by - cy) + b2 * (cy - ay) + c2 * (ay - by)) / D;
    outY = (a2 * (cx - bx) + b2 * (ax - cx) + c2 * (bx - ax)) / D;
    return true;
}

// Signed area of triangle (CCW positive).
double signedArea(double ax, double ay,
                  double bx, double by,
                  double cx, double cy)
{
    return 0.5 * ((bx - ax) * (cy - ay) - (cx - ax) * (by - ay));
}

// Area of convex polygon given its vertices in CCW order.
double polygonArea(const std::vector<std::array<double, 2>>& verts)
{
    const int n = static_cast<int>(verts.size());
    if (n < 3) return 0.0;
    double area = 0.0;
    for (int i = 0; i < n; ++i) {
        const int j = (i + 1) % n;
        area += verts[static_cast<std::size_t>(i)][0]
              * verts[static_cast<std::size_t>(j)][1];
        area -= verts[static_cast<std::size_t>(j)][0]
              * verts[static_cast<std::size_t>(i)][1];
    }
    return std::abs(area) * 0.5;
}

// Sort points of a convex polygon into CCW order relative to centroid.
void sortCCW(std::vector<std::array<double, 2>>& verts)
{
    if (verts.size() < 3) return;
    double cx = 0.0, cy = 0.0;
    for (const auto& v : verts) { cx += v[0]; cy += v[1]; }
    cx /= static_cast<double>(verts.size());
    cy /= static_cast<double>(verts.size());

    std::sort(verts.begin(), verts.end(),
        [cx, cy](const std::array<double,2>& a, const std::array<double,2>& b) {
            return std::atan2(a[1] - cy, a[0] - cx)
                 < std::atan2(b[1] - cy, b[0] - cx);
        });
}

} // anonymous namespace

// =============================================================================
//  Triangulation member functions
// =============================================================================

bool Triangulation::isSuperTriangle(int triIdx) const noexcept
{
    const auto& t = triangles[static_cast<std::size_t>(triIdx)];
    // Super-triangle vertices have indices 0, 1, 2 (added first).
    return (t.v[0] < nSuper || t.v[1] < nSuper || t.v[2] < nSuper);
}

bool Triangulation::inCircumcircle(int triIdx, double px, double py) const noexcept
{
    const auto& t   = triangles[static_cast<std::size_t>(triIdx)];
    const auto& va  = vertices[static_cast<std::size_t>(t.v[0])];
    const auto& vb  = vertices[static_cast<std::size_t>(t.v[1])];
    const auto& vc  = vertices[static_cast<std::size_t>(t.v[2])];

    // Exact incircle test via 3x3 determinant (CCW orientation assumed).
    const double ax = va[0] - px;
    const double ay = va[1] - py;
    const double bx = vb[0] - px;
    const double by = vb[1] - py;
    const double cx = vc[0] - px;
    const double cy = vc[1] - py;

    const double det = ax * (by * (cx*cx + cy*cy) - cy * (bx*bx + by*by))
                     - ay * (bx * (cx*cx + cy*cy) - cx * (bx*bx + by*by))
                     + (ax*ax + ay*ay) * (bx * cy - by * cx);
    return det > 0.0;
}

// =============================================================================
//  buildDelaunay  (Bowyer-Watson)
// =============================================================================

Triangulation buildDelaunay(const std::vector<SpatialPoint>& points)
{
    const int n = static_cast<int>(points.size());
    if (n < 3) {
        throw DataException(
            "buildDelaunay: need at least 3 points, got " + std::to_string(n) + ".");
    }

    Triangulation tri;
    tri.nSuper = 3;

    // Find bounding box for the super-triangle.
    double xMin = points[0].x, xMax = points[0].x;
    double yMin = points[0].y, yMax = points[0].y;
    for (const auto& p : points) {
        xMin = std::min(xMin, p.x); xMax = std::max(xMax, p.x);
        yMin = std::min(yMin, p.y); yMax = std::max(yMax, p.y);
    }

    const double dx  = xMax - xMin;
    const double dy  = yMax - yMin;
    const double dMax = std::max(dx, dy);
    const double cx  = (xMin + xMax) * 0.5;
    const double cy  = (yMin + yMax) * 0.5;

    // Super-triangle far enough to contain all input points.
    const double M = 20.0;
    tri.vertices.push_back({ cx - M * dMax, cy - M * dMax });
    tri.vertices.push_back({ cx,            cy + M * dMax });
    tri.vertices.push_back({ cx + M * dMax, cy - M * dMax });

    // Initial super-triangle.
    Triangulation::Triangle superTri;
    superTri.v[0] = 0; superTri.v[1] = 1; superTri.v[2] = 2;
    superTri.adj[0] = -1; superTri.adj[1] = -1; superTri.adj[2] = -1;
    tri.triangles.push_back(superTri);

    // Deleted flags (marks triangles removed during insertion).
    std::vector<bool> deleted(1, false);

    // Insert each input point one by one.
    for (int pi = 0; pi < n; ++pi) {
        const double px = points[static_cast<std::size_t>(pi)].x;
        const double py = points[static_cast<std::size_t>(pi)].y;

        // Apply tiny jitter to avoid exact degeneracies (cocircular points).
        const double jitter = 1.0e-10 * dMax;
        const double qx = px + jitter * (pi % 3 - 1);
        const double qy = py + jitter * (pi % 2 == 0 ? 1 : -1);

        // Add vertex.
        tri.vertices.push_back({ qx, qy });
        const int vIdx = static_cast<int>(tri.vertices.size()) - 1;

        // Find all triangles whose circumcircle contains (qx, qy) -- the cavity.
        std::vector<int> cavity;
        const int nTri = static_cast<int>(tri.triangles.size());
        for (int ti = 0; ti < nTri; ++ti) {
            if (!deleted[static_cast<std::size_t>(ti)] &&
                tri.inCircumcircle(ti, qx, qy)) {
                cavity.push_back(ti);
            }
        }

        // Extract boundary edges of the cavity (edges NOT shared by two cavity triangles).
        std::vector<std::array<int, 2>> boundary;
        for (int ti : cavity) {
            const auto& t = tri.triangles[static_cast<std::size_t>(ti)];
            for (int e = 0; e < 3; ++e) {
                const int va = t.v[e];
                const int vb = t.v[(e + 1) % 3];
                // Check if the edge (va, vb) is shared with another cavity triangle.
                bool shared = false;
                for (int tj : cavity) {
                    if (ti == tj) continue;
                    const auto& s = tri.triangles[static_cast<std::size_t>(tj)];
                    for (int f = 0; f < 3; ++f) {
                        if ((s.v[f] == va && s.v[(f+1)%3] == vb) ||
                            (s.v[f] == vb && s.v[(f+1)%3] == va)) {
                            shared = true;
                            break;
                        }
                    }
                    if (shared) break;
                }
                if (!shared) boundary.push_back({ va, vb });
            }
        }

        // Mark cavity triangles as deleted.
        for (int ti : cavity) {
            deleted[static_cast<std::size_t>(ti)] = true;
        }

        // Create new triangles connecting each boundary edge to the new vertex.
        for (const auto& edge : boundary) {
            Triangulation::Triangle newTri;
            newTri.v[0] = edge[0];
            newTri.v[1] = edge[1];
            newTri.v[2] = vIdx;
            // Adjacency is reconstructed in a post-processing pass below.
            newTri.adj[0] = -1; newTri.adj[1] = -1; newTri.adj[2] = -1;

            // Ensure CCW orientation.
            const auto& va2 = tri.vertices[static_cast<std::size_t>(newTri.v[0])];
            const auto& vb2 = tri.vertices[static_cast<std::size_t>(newTri.v[1])];
            const auto& vc2 = tri.vertices[static_cast<std::size_t>(vIdx)];
            if (signedArea(va2[0], va2[1], vb2[0], vb2[1], vc2[0], vc2[1]) < 0.0) {
                std::swap(newTri.v[0], newTri.v[1]);
            }

            tri.triangles.push_back(newTri);
            deleted.push_back(false);
        }
    }

    // Compact: remove deleted triangles.
    std::vector<Triangulation::Triangle> alive;
    for (std::size_t i = 0; i < tri.triangles.size(); ++i) {
        if (!deleted[i]) alive.push_back(tri.triangles[i]);
    }
    tri.triangles = std::move(alive);

    return tri;
}

// =============================================================================
//  NaturalNeighborInterpolator
// =============================================================================

NaturalNeighborInterpolator::NaturalNeighborInterpolator(
    const std::vector<SpatialPoint>& points)
    : m_points(points)
    , m_tri(buildDelaunay(points))
{}

// -----------------------------------------------------------------------------
//  _sibsonCoords
//
//  Computes natural neighbour (Sibson) weights for query point (qx, qy).
//
//  Algorithm:
//    1. Temporarily insert Q into the Delaunay triangulation.
//    2. The natural neighbours are the input points that appear in triangles
//       of the cavity (triangles whose circumcircle contains Q).
//    3. For each natural neighbour i, the Sibson coordinate is:
//         w_i = area(VoronoiCell_old(i) INTERSECT VoronoiCell_new(Q))
//               / totalArea(VoronoiCell_new(Q))
//    In practice: area of the sub-cell = area of the polygon formed by
//    the circumcentres of triangles in the cavity that touch input point i,
//    plus the midpoints of the edges connecting point i to Q.
//
//  For the boundary case (Q outside convex hull) we return empty weights.
// -----------------------------------------------------------------------------

std::vector<std::pair<int, double>>
NaturalNeighborInterpolator::_sibsonCoords(double qx, double qy) const
{
    // Find cavity: triangles whose circumcircle contains Q.
    const int nTri = static_cast<int>(m_tri.triangles.size());
    std::vector<int> cavity;
    for (int ti = 0; ti < nTri; ++ti) {
        if (!m_tri.isSuperTriangle(ti) &&
            m_tri.inCircumcircle(ti, qx, qy)) {
            cavity.push_back(ti);
        }
    }

    if (cavity.empty()) return {};

    // Collect natural neighbours = input point indices appearing in cavity.
    // Input points are stored at vertices[3..3+n-1] (0,1,2 = super-triangle).
    const int superOffset = m_tri.nSuper;
    std::unordered_set<int> neighbourSet;
    for (int ti : cavity) {
        const auto& t = m_tri.triangles[static_cast<std::size_t>(ti)];
        for (int k = 0; k < 3; ++k) {
            if (t.v[k] >= superOffset) {
                neighbourSet.insert(t.v[k] - superOffset);
            }
        }
    }

    if (neighbourSet.empty()) return {};

    // For each natural neighbour i, collect the circumcentres of cavity
    // triangles that share vertex i.  These circumcentres form the Voronoi
    // cell polygon stolen by Q.  The area of that polygon gives w_i.
    std::vector<std::pair<int, double>> coords;
    double totalArea = 0.0;

    for (int ni : neighbourSet) {
        const int vIdx = ni + superOffset;  // vertex index in Triangulation

        // Gather circumcentres of cavity triangles touching vIdx.
        std::vector<std::array<double, 2>> cellVerts;

        for (int ti : cavity) {
            const auto& t = m_tri.triangles[static_cast<std::size_t>(ti)];
            bool hasV = (t.v[0] == vIdx || t.v[1] == vIdx || t.v[2] == vIdx);
            if (!hasV) continue;

            const auto& a = m_tri.vertices[static_cast<std::size_t>(t.v[0])];
            const auto& b = m_tri.vertices[static_cast<std::size_t>(t.v[1])];
            const auto& c = m_tri.vertices[static_cast<std::size_t>(t.v[2])];
            double ccx, ccy;
            if (circumcentre(a[0], a[1], b[0], b[1], c[0], c[1], ccx, ccy)) {
                cellVerts.push_back({ ccx, ccy });
            }
        }
        // Also add Q itself as a vertex of the stolen cell.
        cellVerts.push_back({ qx, qy });

        sortCCW(cellVerts);
        const double area = polygonArea(cellVerts);
        coords.emplace_back(ni, area);
        totalArea += area;
    }

    if (totalArea < 1.0e-15) return {};

    // Normalise.
    for (auto& [idx, w] : coords) {
        w /= totalArea;
    }

    return coords;
}

// -----------------------------------------------------------------------------

double NaturalNeighborInterpolator::interpolate(double qx, double qy) const
{
    const auto coords = _sibsonCoords(qx, qy);

    if (coords.empty()) {
        // Query is outside the convex hull -- fall back to nearest input point.
        LOKI_WARNING(
            "NaturalNeighborInterpolator: query (" + std::to_string(qx) + ", "
            + std::to_string(qy) + ") is outside the convex hull. "
            "Returning nearest-point value.");

        double bestDist = std::numeric_limits<double>::max();
        double bestVal  = 0.0;
        for (const auto& p : m_points) {
            const double d = std::sqrt((p.x - qx)*(p.x - qx) + (p.y - qy)*(p.y - qy));
            if (d < bestDist) { bestDist = d; bestVal = p.value; }
        }
        return bestVal;
    }

    double result = 0.0;
    for (const auto& [idx, w] : coords) {
        result += w * m_points[static_cast<std::size_t>(idx)].value;
    }
    return result;
}

Eigen::MatrixXd NaturalNeighborInterpolator::interpolateGrid(
    const GridExtent& extent) const
{
    Eigen::MatrixXd grid(extent.nRows, extent.nCols);
    for (int row = 0; row < extent.nRows; ++row) {
        const double qy = extent.yMin + static_cast<double>(row) * extent.resY;
        for (int col = 0; col < extent.nCols; ++col) {
            const double qx = extent.xMin + static_cast<double>(col) * extent.resX;
            grid(row, col)  = interpolate(qx, qy);
        }
    }
    return grid;
}

SpatialCrossValidationResult NaturalNeighborInterpolator::crossValidate() const
{
    const int n = static_cast<int>(m_points.size());
    if (n < 4) {
        throw DataException(
            "NaturalNeighborInterpolator::crossValidate: need >= 4 points, got "
            + std::to_string(n) + ".");
    }

    SpatialCrossValidationResult cv;
    cv.errors.resize(static_cast<std::size_t>(n));
    cv.stdErrors.resize(static_cast<std::size_t>(n), 0.0);
    cv.x.resize(static_cast<std::size_t>(n));
    cv.y.resize(static_cast<std::size_t>(n));

    for (int i = 0; i < n; ++i) {
        std::vector<SpatialPoint> trainPts;
        trainPts.reserve(static_cast<std::size_t>(n - 1));
        for (int j = 0; j < n; ++j) {
            if (j != i) trainPts.push_back(m_points[static_cast<std::size_t>(j)]);
        }
        NaturalNeighborInterpolator looPred(trainPts);
        const double pred = looPred.interpolate(
            m_points[static_cast<std::size_t>(i)].x,
            m_points[static_cast<std::size_t>(i)].y);

        cv.errors[static_cast<std::size_t>(i)] =
            m_points[static_cast<std::size_t>(i)].value - pred;
        cv.x[static_cast<std::size_t>(i)] = m_points[static_cast<std::size_t>(i)].x;
        cv.y[static_cast<std::size_t>(i)] = m_points[static_cast<std::size_t>(i)].y;
    }

    double sumSq = 0.0, sumAbs = 0.0;
    for (std::size_t i = 0; i < static_cast<std::size_t>(n); ++i) {
        sumSq  += cv.errors[i] * cv.errors[i];
        sumAbs += std::abs(cv.errors[i]);
    }
    cv.rmse    = std::sqrt(sumSq / static_cast<double>(n));
    cv.mae     = sumAbs / static_cast<double>(n);
    cv.meanSE  = 0.0;
    cv.meanSSE = 0.0;

    return cv;
}

} // namespace loki::math
