#pragma once

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
 * Uses an explicit constructor to satisfy GCC 13 on Windows/MinGW where
 * aggregate initialisation of structs with enum members and default values
 * fails to compile. All coordinate system fields are strings; enum conversion
 * happens inside GeodesyAnalyzer methods in the .cpp file.
 */
struct GeodesyConfig {
    // Explicit constructor -- required by GCC 13 aggregate-init bug on Windows
    GeodesyConfig()
        : task       { GeodesyTask::TRANSFORM }
        , inputSystem{ "ecef" }
        , outputSystem{ "geod" }
        , ellipsoidModel{ loki::math::EllipsoidModel::WGS84 }
        , ellipsoidName{ "WGS84" }
        , refBody    { "ellipsoid" }
        , stateSize  { 3 }
        , distMethod { DistanceMethod::VINCENTY }
        , mcSamples  { 1000 }
        , mcTolerance{ 0.05 }
        , enuOriginLat{ 0.0 }
        , enuOriginLon{ 0.0 }
        , enuOriginH  { 0.0 }
        , protocolDir{ "" }
        , imgDir     { "" }
        , csvDir     { "" }
    {}

    GeodesyTask                task;
    std::string                inputSystem;   ///< "ecef"|"geod"|"sphere"|"enu"
    std::string                outputSystem;  ///< "ecef"|"geod"|"sphere"|"enu"
    loki::math::EllipsoidModel ellipsoidModel;
    std::string                ellipsoidName;
    std::string                refBody;       ///< "ellipsoid"|"sphere"
    int                        stateSize;
    DistanceMethod             distMethod;
    int                        mcSamples;
    double                     mcTolerance;
    double                     enuOriginLat;  ///< ENU origin latitude [deg]
    double                     enuOriginLon;  ///< ENU origin longitude [deg]
    double                     enuOriginH;    ///< ENU origin height [m]
    std::string                protocolDir;  ///< OUTPUT/PROTOCOLS/
    std::string                imgDir;       ///< OUTPUT/IMG/
    std::string                csvDir;       ///< OUTPUT/CSV/
};

// ---------------------------------------------------------------------------
// GeodesyAnalyzer
// ---------------------------------------------------------------------------

/**
 * @brief Orchestrates coordinate transformation, geodesic computation and
 *        Monte Carlo covariance validation.
 *
 * The loader (GeodesyLoader) is called in main() and the result passed
 * directly to run(). GeodesyAnalyzer does not own a loader.
 */
class GeodesyAnalyzer {
public:

    /**
     * @brief Construct with configuration.
     * @param cfg  Fully populated GeodesyConfig.
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

    // All private helpers defined only in .cpp (which includes coordTransform.hpp)
    TransformResult  _transformPoint(const Eigen::VectorXd& pos,
                                     const Eigen::MatrixXd& cov) const;

    MonteCarloResult _monteCarlo    (const Eigen::VectorXd& pos,
                                     const Eigen::MatrixXd& cov) const;

    std::vector<GeodLineResult> _computeDistances(
        const loki::io::GeodesyLoadResult& ds) const;

    void _writeProtocol(const GeodesyResult& result) const;
};

} // namespace loki::geodesy
