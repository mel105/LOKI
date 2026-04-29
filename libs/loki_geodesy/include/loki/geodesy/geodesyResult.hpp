#pragma once

#include <loki/geodesy/coordTransform.hpp>
#include <loki/geodesy/geodesicLine.hpp>
#include <loki/math/ellipsoid.hpp>

#include <Eigen/Dense>
#include <string>
#include <vector>

namespace loki::geodesy {

// ---------------------------------------------------------------------------
// Single-point transformation result
// ---------------------------------------------------------------------------

/// @brief Result of a coordinate transformation for one point.
struct TransformResult {
    // Input
    Eigen::VectorXd inputPos;    ///< Input state vector (size 3 or 6)
    Eigen::MatrixXd inputCov;    ///< Input covariance matrix

    // Output
    Eigen::VectorXd outputPos;   ///< Output state vector (size 3 or 6)
    Eigen::MatrixXd outputCov;   ///< Output covariance matrix (analytical propagation)
    Eigen::MatrixXd empiricalCov;///< Empirical covariance from Monte Carlo (if run)

    // Metadata
    std::string inputSystem;     ///< e.g. "ECEF", "GEOD", "ENU", "SPHERE"
    std::string outputSystem;    ///< e.g. "ENU", "GEOD", etc.
    std::string ellipsoidName;   ///< e.g. "WGS84"
};

// ---------------------------------------------------------------------------
// Monte Carlo validation result
// ---------------------------------------------------------------------------

/// @brief Result of Monte Carlo covariance validation.
struct MonteCarloResult {
    Eigen::MatrixXd analyticalCov;   ///< Analytical (Jacobian-propagated) covariance
    Eigen::MatrixXd empiricalCov;    ///< Empirical covariance from N samples
    Eigen::VectorXd empiricalMean;   ///< Empirical mean of transformed samples
    int             nSamples;        ///< Number of Monte Carlo samples used
    double          maxRelDiff;      ///< Max relative difference on diagonal: |a-e|/|a|
    bool            passed;          ///< True if maxRelDiff < tolerance
};

// ---------------------------------------------------------------------------
// Geodesic computation result
// ---------------------------------------------------------------------------

/// @brief Result of a geodesic distance/direction computation.
struct GeodLineResult {
    GeodesicResult geodesic;   ///< Distance [m] and azimuths [deg]
    std::string    method;     ///< "vincenty" or "haversine"
    std::string    ellipsoidName;
};

// ---------------------------------------------------------------------------
// Full pipeline result
// ---------------------------------------------------------------------------

/// @brief Aggregated result of one loki_geodesy run.
struct GeodesyResult {
    std::vector<TransformResult> transforms;  ///< Transformation results per point
    std::vector<GeodLineResult>  lines;        ///< Geodesic results (if requested)
    MonteCarloResult             monteCarlo;   ///< MC validation (if enabled)
    bool                         hasMonteCarlo{ false };
    bool                         hasLines{ false };
};

} // namespace loki::geodesy
