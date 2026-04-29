#include <loki/geodesy/geodesyLoader.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace loki::geodesy;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

std::vector<double> splitLine(const std::string& line, char delim)
{
    std::vector<double> vals;
    std::stringstream   ss(line);
    std::string         token;
    while (std::getline(ss, token, delim)) {
        if (token.empty()) continue;
        try {
            vals.push_back(std::stod(token));
        } catch (...) {
            throw loki::ParseException("GeodesyLoader: cannot parse token '" + token + "'");
        }
    }
    return vals;
}

// Reconstruct symmetric NxN matrix from upper triangle (row-major order).
Eigen::MatrixXd upperTriangleToMatrix(const std::vector<double>& vals, int N,
                                       std::size_t offset)
{
    int expected = N * (N + 1) / 2;
    if (static_cast<int>(vals.size() - offset) < expected)
        throw loki::ParseException("GeodesyLoader: not enough columns for covariance");

    Eigen::MatrixXd C = Eigen::MatrixXd::Zero(N, N);
    std::size_t idx = offset;
    for (int i = 0; i < N; ++i)
        for (int j = i; j < N; ++j) {
            C(i, j) = vals[idx];
            C(j, i) = vals[idx];
            ++idx;
        }
    return C;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// inputCoordSystemFromString
// ---------------------------------------------------------------------------

InputCoordSystem loki::geodesy::inputCoordSystemFromString(const std::string& s)
{
    if (s == "ecef")                    return InputCoordSystem::ECEF;
    if (s == "geod" || s == "geodetic") return InputCoordSystem::GEOD;
    if (s == "sphere")                  return InputCoordSystem::SPHERE;
    if (s == "enu")                     return InputCoordSystem::ENU;
    throw loki::ConfigException("GeodesyLoader: unknown coord system '" + s + "'");
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

GeodesyLoader::GeodesyLoader(const std::string& filePath,
                              InputCoordSystem    coordSystem,
                              char                delimiter,
                              int                 stateSize)
    : m_filePath(filePath)
    , m_coordSystem(coordSystem)
    , m_delimiter(delimiter)
    , m_stateSize(stateSize)
{
    if (m_stateSize != 3 && m_stateSize != 6)
        throw loki::ConfigException("GeodesyLoader: stateSize must be 3 or 6");
}

// ---------------------------------------------------------------------------
// load()
// ---------------------------------------------------------------------------

GeodesyDataset GeodesyLoader::load() const
{
    std::ifstream file(m_filePath);
    if (!file.is_open())
        throw loki::FileNotFoundException("GeodesyLoader: cannot open '" + m_filePath + "'");

    GeodesyDataset ds;
    ds.coordSystem = m_coordSystem;
    ds.sourcePath  = m_filePath;
    ds.stateSize   = m_stateSize;

    // Number of upper-triangle elements for covariance
    int N         = m_stateSize;
    int covElems  = N * (N + 1) / 2;    // 6 for N=3, 21 for N=6
    int minCols   = N;
    int fullCols  = N + covElems;

    bool hasCov = false;   // determined from first data line

    std::string line;
    int lineNo = 0;
    while (std::getline(file, line)) {
        ++lineNo;

        // Strip trailing \r (Windows line endings)
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') continue;

        std::vector<double> vals = splitLine(line, m_delimiter);

        if (vals.empty()) continue;

        // Determine presence of covariance from first data line
        if (ds.positions.empty()) {
            if (static_cast<int>(vals.size()) >= fullCols) {
                hasCov = true;
                LOKI_INFO("GeodesyLoader: covariance columns detected");
            } else if (static_cast<int>(vals.size()) >= minCols) {
                hasCov = false;
            } else {
                throw loki::ParseException(
                    "GeodesyLoader: line " + std::to_string(lineNo)
                    + " has too few columns (" + std::to_string(vals.size())
                    + ", expected >= " + std::to_string(minCols) + ")");
            }
        }

        // Parse state vector
        Eigen::VectorXd pos(N);
        for (int i = 0; i < N; ++i)
            pos(i) = vals[static_cast<std::size_t>(i)];
        ds.positions.push_back(pos);

        // Parse covariance if present
        if (hasCov) {
            if (static_cast<int>(vals.size()) < fullCols)
                throw loki::ParseException(
                    "GeodesyLoader: line " + std::to_string(lineNo)
                    + " expected " + std::to_string(fullCols) + " columns");
            ds.covariances.push_back(
                upperTriangleToMatrix(vals, N, static_cast<std::size_t>(N)));
        }
    }

    if (ds.positions.empty())
        throw loki::DataException("GeodesyLoader: no data rows found in '" + m_filePath + "'");

    LOKI_INFO("GeodesyLoader: loaded " + std::to_string(ds.positions.size())
              + " points from '" + m_filePath + "'");
    return ds;
}
