#pragma once

#include <loki/geodesy/coordTransform.hpp>
#include <loki/geodesy/geodesicLine.hpp>
#include <loki/geodesy/geodesyResult.hpp>
#include <loki/io/geodesyLoader.hpp>
#include <loki/math/ellipsoid.hpp>

#include <Eigen/Dense>
#include <string>
#include <vector>

namespace loki::geodesy {

// ---------------------------------------------------------------------------
// Task and method enums
// ---------------------------------------------------------------------------

/// @brief Task type.
enum class GeodesyTask {
    TRANSFORM,
    DISTANCE,
    MONTE_CARLO
};

GeodesyTask    geodesyTaskFromString   (const std::string& s);

/// @brief Distance method.
enum class DistanceMethod { VINCENTY, HAVERSINE };
DistanceMethod distanceMethodFromString(const std::string& s);

// ---------------------------------------------------------------------------
// GeodesyConfig
// ---------------------------------------------------------------------------

/**
 * @brief Runtime configuration for GeodesyAnalyzer.
 *
 * All coordinate system and reference body fields are stored as strings
 * to avoid GCC 13 aggregate-init issues with enum members that have
 * default values.  Conversion to the appropriate enums happens inside
 * GeodesyAnalyzer at construction time.
 */
struct GeodesyConfig {
    GeodesyTask  task        { GeodesyTask::TRANSFORM };
    std::string  inputSystem { "ecef" };   ///< "ecef"|"geod"|"sphere"|"enu"
    std::string  outputSystem{ "geod" };   ///< "ecef"|"geod"|"sphere"|"enu"
    loki::math::EllipsoidModel ellipsoidModel{ loki::math::EllipsoidModel::WGS84 };
    std::string  ellipsoidName{ "WGS84" };
    std::string  refBody     { "ellipsoid" }; ///< "ellipsoid"|"sphere"
    int          stateSize   { 3 };
    DistanceMethod distMethod{ DistanceMethod::VINCENTY };
    int          mcSamples   { 1000 };
    double       mcTolerance { 0.05 };
    GeodPoint    enuOrigin   { 0.0, 0.0, 0.0 };
    std::string  outputDir   { "OUTPUT/" };
};

// ---------------------------------------------------------------------------
// GeodesyAnalyzer
// ---------------------------------------------------------------------------

/**
 * @brief Orchestrates coordinate transformation, geodesic computation and
 *        Monte Carlo covariance validation.
 */
class GeodesyAnalyzer {
public:

    /**
     * @brief Construct with configuration.
     *
     * Converts string fields in GeodesyConfig to enums once at construction.
     */
    explicit GeodesyAnalyzer(const GeodesyConfig& cfg);

    /**
     * @brief Run the full pipeline on pre-loaded data.
     * @param loadResult  Data loaded by GeodesyLoader.
     * @return GeodesyResult.
     */
    GeodesyResult run(const loki::io::GeodesyLoadResult& loadResult) const;

private:

    GeodesyConfig         m_cfg;
    loki::math::Ellipsoid m_ell;

    // Resolved enums (converted from strings in constructor)
    InputCoordSystem m_inputSystem;
    InputCoordSystem m_outputSystem;
    RefBody          m_refBody;

    TransformResult transformPoint(const Eigen::VectorXd& pos,
                                   const Eigen::MatrixXd& cov) const;

    MonteCarloResult monteCarlo(const Eigen::VectorXd& pos,
                                const Eigen::MatrixXd& cov) const;

    std::vector<GeodLineResult> computeDistances(
        const loki::io::GeodesyLoadResult& ds) const;

    void writeProtocol(const GeodesyResult& result) const;

    static std::string sysName(InputCoordSystem s) noexcept;
};

} // namespace loki::geodesy
