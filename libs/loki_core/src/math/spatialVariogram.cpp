#include <loki/math/spatialVariogram.hpp>

#include <loki/core/exceptions.hpp>
#include <loki/math/krigingVariogram.hpp>

#include <algorithm>
#include <cmath>

using namespace loki;

namespace loki::math {

// -----------------------------------------------------------------------------
//  computeSpatialVariogram
// -----------------------------------------------------------------------------

std::vector<VariogramPoint> computeSpatialVariogram(
    const std::vector<SpatialPoint>& points,
    int                              nLagBins,
    double                           maxLag)
{
    if (points.size() < 3) {
        throw DataException(
            "computeSpatialVariogram: need at least 3 points, got "
            + std::to_string(points.size()) + ".");
    }
    if (nLagBins < 3) {
        throw ConfigException(
            "computeSpatialVariogram: nLagBins must be >= 3, got "
            + std::to_string(nLagBins) + ".");
    }

    const std::size_t n = points.size();

    // Determine maxLag automatically as half of the maximum pairwise distance.
    if (maxLag <= 0.0) {
        double maxDist = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = i + 1; j < n; ++j) {
                const double d = euclideanDistance(points[i], points[j]);
                if (d > maxDist) maxDist = d;
            }
        }
        maxLag = maxDist * 0.5;
    }

    if (maxLag <= 0.0) {
        throw DataException(
            "computeSpatialVariogram: all points are coincident -- cannot build variogram.");
    }

    const double binWidth = maxLag / static_cast<double>(nLagBins);
    const std::size_t nb = static_cast<std::size_t>(nLagBins);

    std::vector<double> sumSq(nb, 0.0);
    std::vector<int>    cnt  (nb, 0);

    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = i + 1; j < n; ++j) {
            const double h = euclideanDistance(points[i], points[j]);
            if (h > maxLag) continue;

            std::size_t k = static_cast<std::size_t>(h / binWidth);
            if (k >= nb) k = nb - 1;  // guard against floating-point edge

            const double diff = points[i].value - points[j].value;
            sumSq[k] += diff * diff;
            ++cnt[k];
        }
    }

    std::vector<VariogramPoint> result;
    result.reserve(nb);

    for (std::size_t k = 0; k < nb; ++k) {
        if (cnt[k] < 2) continue;
        VariogramPoint vp;
        vp.lag   = (static_cast<double>(k) + 0.5) * binWidth;
        vp.gamma = 0.5 * sumSq[k] / static_cast<double>(cnt[k]);
        vp.count = cnt[k];
        result.push_back(vp);
    }

    return result;
}

// -----------------------------------------------------------------------------
//  fitSpatialVariogram
// -----------------------------------------------------------------------------

VariogramFitResult fitSpatialVariogram(
    const std::vector<VariogramPoint>& empirical,
    const KrigingVariogramConfig&      cfg)
{
    return fitVariogram(empirical, cfg);
}

} // namespace loki::math
