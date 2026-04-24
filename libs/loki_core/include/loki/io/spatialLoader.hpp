#pragma once

#include <loki/core/config.hpp>
#include <loki/math/spatialTypes.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace loki::io {

/**
 * @brief Result of loading a spatial scatter dataset from a flat text file.
 *
 * One SpatialLoadResult is produced per input file. It contains one entry
 * in variables / varNames per loaded value column.
 */
struct SpatialLoadResult {
    std::filesystem::path                              filePath;
    std::vector<std::string>                           varNames;   ///< One per value column.
    std::vector<std::vector<loki::math::SpatialPoint>> variables;  ///< Parallel to varNames.
    int                                                linesRead   = 0;
    int                                                linesSkipped = 0;
};

/**
 * @brief Loader for 2-D spatial scatter data files.
 *
 * Supported file format:
 *   - One observation per line: x y v1 v2 ...
 *   - Whitespace or configurable single-character delimiter.
 *   - Comment lines: lines whose first non-whitespace token starts with
 *     commentPrefix ("#", "%", "!", "//", ";", "*").
 *   - No-data values (e.g. -999.9999) are filtered per column: the
 *     observation is skipped for that variable but retained for others.
 *   - Column names extracted from comment lines of the form:
 *       # NAME unit description
 *     First word after commentPrefix + whitespace becomes the column name,
 *     provided it contains only [A-Za-z0-9_]. Lines that do not match
 *     this pattern (e.g. "# 8" for point count) are ignored for naming.
 *
 * Coordinate interpretation:
 *   Coordinates are treated as Cartesian. For LLA (lat/lon) data in small
 *   regions, the Mercator approximation holds and no transformation is
 *   needed. For global datasets or accuracy-critical geodetic work, project
 *   coordinates to a Cartesian system before loading (loki_geodesy).
 *
 * Usage:
 * @code
 *   SpatialLoader loader(cfg);
 *   SpatialLoadResult result = loader.load();
 * @endcode
 */
class SpatialLoader {
public:

    /**
     * @brief Construct loader from application configuration.
     *
     * @param cfg  AppConfig with populated cfg.spatial.input sub-config.
     */
    explicit SpatialLoader(const AppConfig& cfg);

    /**
     * @brief Load the configured spatial data file.
     *
     * @return  SpatialLoadResult with one entry per value column.
     * @throws FileNotFoundException if the file does not exist.
     * @throws ParseException        on malformed lines (after tolerance).
     * @throws ConfigException       on invalid commentPrefix or column indices.
     */
    SpatialLoadResult load() const;

private:

    const AppConfig& m_cfg;

    /**
     * @brief Validate that the configured commentPrefix is recognised.
     *
     * Accepted: "#", "%", "!", "//", ";", "*"
     * @throws ConfigException if prefix is not in the allowed set.
     */
    static void _validateCommentPrefix(const std::string& prefix);

    /**
     * @brief Check whether a line is a comment.
     */
    static bool _isComment(const std::string& line, const std::string& prefix);

    /**
     * @brief Try to extract a column name from a comment line.
     *
     * Returns empty string if the comment does not match the
     * "# NAME ..." pattern (e.g. "# 8" is not a name).
     */
    static std::string _extractNameFromComment(const std::string& line,
                                               const std::string& prefix);

    /**
     * @brief Split a line into tokens using the configured delimiter.
     *
     * If delimiter is ' ', splits on any whitespace sequence (istringstream).
     * Otherwise splits on the single delimiter character.
     */
    static std::vector<std::string> _tokenise(const std::string& line, char delim);

    /**
     * @brief Check whether |v - noDataValue| <= noDataTolerance.
     */
    static bool _isNoData(double v, double noDataValue, double tolerance);
};

} // namespace loki::io
