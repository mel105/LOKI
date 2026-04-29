#pragma once

#include <loki/core/config.hpp>

#include <Eigen/Dense>
#include <filesystem>
#include <string>
#include <vector>

namespace loki::io {

/**
 * @brief Result of loading a geodetic point dataset from a flat text file.
 *
 * Parallel vectors: positions[i] and covariances[i] describe point i.
 * covariances is empty when the file contains no covariance columns.
 *
 * coordSystem is stored as a string ("ecef", "geod", "sphere", "enu") so
 * that geodesyLoader has no dependency on loki_geodesy (loki_core must
 * remain dependency-free from higher modules).  The analyzer converts it
 * via inputCoordSystemFromString() on its own side.
 */
struct GeodesyLoadResult {
    std::filesystem::path        filePath;
    std::vector<Eigen::VectorXd> positions;    ///< State vectors (size 3 or 6)
    std::vector<Eigen::MatrixXd> covariances;  ///< NxN covariances (may be empty)
    std::string                  coordSystem;  ///< "ecef" | "geod" | "sphere" | "enu"
    int                          stateSize{ 3 };
    int                          linesRead{ 0 };
    int                          linesSkipped{ 0 };
};

/**
 * @brief Loader for geodetic scatter data files.
 *
 * Supported file format: delimiter-separated text (.csv or .txt).
 * Comment lines start with '#'. Empty lines are ignored.
 *
 * Column layout (stateSize=3, no covariance):
 *   c0  c1  c2
 *
 * Column layout (stateSize=3, with 3x3 covariance -- 6 upper-triangle columns):
 *   c0  c1  c2  cov00  cov01  cov02  cov11  cov12  cov22
 *
 * Column layout (stateSize=6, no covariance):
 *   c0  c1  c2  v0  v1  v2
 *
 * Column layout (stateSize=6, with 6x6 covariance -- 21 upper-triangle columns):
 *   c0  c1  c2  v0  v1  v2  cov00  cov01 ... (21 elements, upper triangle)
 *
 * Covariance presence is auto-detected from the column count of the first
 * data line.
 *
 * Angle units for GEOD/SPHERE coord systems: always degrees.
 * Distance units: always metres.
 */
class GeodesyLoader {
public:

    /**
     * @brief Construct from application configuration.
     *
     * Reads file path, delimiter, coord system and stateSize from
     * cfg.geodesy.input. The file path is resolved against
     * cfg.workspace / "INPUT/" when not absolute.
     *
     * @param cfg  Fully populated AppConfig.
     */
    explicit GeodesyLoader(const AppConfig& cfg);

    /**
     * @brief Load and parse the configured data file.
     *
     * @return  GeodesyLoadResult with positions and optional covariances.
     * @throws FileNotFoundException if the file does not exist.
     * @throws ParseException        on malformed data.
     * @throws ConfigException       on invalid configuration.
     */
    GeodesyLoadResult load() const;

private:

    AppConfig m_cfg;

    // Resolve file path against workspace/INPUT/
    std::filesystem::path _resolvePath() const;

    // Split a line into double tokens using the configured delimiter
    static std::vector<double> _tokenise(const std::string& line, char delim);

    // Reconstruct symmetric NxN matrix from upper-triangle elements (row-major)
    static Eigen::MatrixXd _upperTriangleToMatrix(
        const std::vector<double>& vals, int N, std::size_t offset);
};

} // namespace loki::io
