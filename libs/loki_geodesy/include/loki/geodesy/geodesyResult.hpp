#pragma once

#include <loki/geodesy/coordTransform.hpp>
#include <loki/geodesy/geodesicLine.hpp>
#include <loki/math/ellipsoid.hpp>

#include <Eigen/Dense>
#include <string>
#include <vector>

namespace loki::geodesy {

/// @brief Result of a coordinate transformation for one point.
struct TransformResult {
    Eigen::VectorXd inputPos;
    Eigen::MatrixXd inputCov;
    Eigen::VectorXd outputPos;
    Eigen::MatrixXd outputCov;      ///< Analytical (Jacobian-propagated) covariance
    Eigen::MatrixXd empiricalCov;   ///< Empirical covariance from MC
    Eigen::MatrixXd mcSamples;      ///< MC sample matrix (N x nSamples)
    Eigen::VectorXd empiricalMean;  ///< Empirical mean of MC samples
    std::string inputSystem;
    std::string outputSystem;
    std::string ellipsoidName;
};

/// @brief Summary MC validation result (first point only, for protocol).
struct MonteCarloResult {
    Eigen::MatrixXd analyticalCov;
    Eigen::MatrixXd empiricalCov;
    Eigen::VectorXd empiricalMean;
    Eigen::MatrixXd samples;
    int             nSamples{ 0 };
    double          maxRelDiff{ 0.0 };
    bool            passed{ false };
};

struct GeodLineResult {
    GeodesicResult geodesic;
    std::string    method;
    std::string    ellipsoidName;
};

struct GeodesyResult {
    std::vector<TransformResult> transforms;
    std::vector<GeodLineResult>  lines;
    MonteCarloResult             monteCarlo;
    bool                         hasMonteCarlo{ false };
    bool                         hasLines{ false };
};

} // namespace loki::geodesy
