#pragma once

#include <loki/geodesy/coordTransform.hpp>
#include <loki/core/exceptions.hpp>

#include <Eigen/Dense>
#include <string>
#include <vector>

namespace loki::geodesy {

/// @brief Coordinate system of the input data file.
enum class InputCoordSystem {
    ECEF,    ///< X, Y, Z [m]
    GEOD,    ///< lat [deg], lon [deg], h [m]
    SPHERE,  ///< lat [deg], lon [deg], radius [m]
    ENU      ///< E [m], N [m], U [m]
};

/// @brief Parse an InputCoordSystem from a string.
/// Accepted: "ecef", "geod", "geodetic", "sphere", "enu".
/// @throws ConfigException for unknown strings.
InputCoordSystem inputCoordSystemFromString(const std::string& s);

// ---------------------------------------------------------------------------
// Loaded dataset
// ---------------------------------------------------------------------------

/**
 * @brief A set of points loaded from a file, with optional covariance.
 *
 * Each row of `positions` is a state vector (size 3 or 6).
 * Each element of `covariances` is the NxN covariance for the corresponding point.
 * If the file contains no covariance data, `covariances` is empty.
 */
struct GeodesyDataset {
    std::vector<Eigen::VectorXd> positions;    ///< State vectors, one per point
    std::vector<Eigen::MatrixXd> covariances;  ///< Covariances (may be empty)
    InputCoordSystem             coordSystem;
    std::string                  sourcePath;
    int                          stateSize{ 3 }; ///< 3 or 6
};

// ---------------------------------------------------------------------------
// Loader
// ---------------------------------------------------------------------------

/**
 * @brief Loader for geodetic point data.
 *
 * Supported file format: delimiter-separated text (.csv or .txt).
 * Default delimiter: semicolon (;).  Comment lines start with '#'.
 * Empty lines are ignored.
 *
 * Column layout (state size 3, no covariance):
 *   col0  col1  col2
 *   c1    c2    c3
 *
 * Column layout (state size 3, with 3x3 covariance = 9 extra columns):
 *   c1  c2  c3  cov00  cov01  cov02  cov11  cov12  cov22
 *   (upper triangle, symmetric matrix is reconstructed)
 *
 * Column layout (state size 6, no covariance):
 *   c1  c2  c3  v1  v2  v3
 *
 * Column layout (state size 6, with 6x6 covariance = 21 extra columns):
 *   c1  c2  c3  v1  v2  v3  cov00  cov01 ... (upper triangle, 21 elements)
 */
class GeodesyLoader {
public:
    /**
     * @brief Construct with file path and coordinate system.
     * @param filePath    Path to the data file.
     * @param coordSystem Coordinate system of the data columns.
     * @param delimiter   Column delimiter character (default ';').
     * @param stateSize   3 (position only) or 6 (position + velocity).
     */
    explicit GeodesyLoader(const std::string&  filePath,
                           InputCoordSystem     coordSystem,
                           char                 delimiter = ';',
                           int                  stateSize = 3);

    /**
     * @brief Load and parse the file.
     * @return Parsed dataset.
     * @throws IoException if the file cannot be opened.
     * @throws ParseException on malformed data.
     */
    GeodesyDataset load() const;

private:
    std::string      m_filePath;
    InputCoordSystem m_coordSystem;
    char             m_delimiter;
    int              m_stateSize;
};

} // namespace loki::geodesy
