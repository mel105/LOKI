#include <loki/io/geodesyLoader.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace loki::io;

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

std::vector<double> GeodesyLoader::_tokenise(const std::string& line, char delim)
{
    std::vector<double> vals;
    std::stringstream   ss(line);
    std::string         token;
    while (std::getline(ss, token, delim)) {
        if (token.empty()) continue;
        try {
            vals.push_back(std::stod(token));
        } catch (...) {
            throw loki::ParseException(
                "GeodesyLoader: cannot parse token '" + token + "'");
        }
    }
    return vals;
}

Eigen::MatrixXd GeodesyLoader::_upperTriangleToMatrix(
    const std::vector<double>& vals, int N, std::size_t offset)
{
    int expected = N * (N + 1) / 2;
    if (static_cast<int>(vals.size()) - static_cast<int>(offset) < expected)
        throw loki::ParseException(
            "GeodesyLoader: not enough columns for " + std::to_string(N)
            + "x" + std::to_string(N) + " covariance");

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

// ---------------------------------------------------------------------------
// _resolvePath
// ---------------------------------------------------------------------------

std::filesystem::path GeodesyLoader::_resolvePath() const
{
    std::filesystem::path p(m_cfg.geodesy.input.file);
    if (p.is_absolute()) return p;
    return m_cfg.workspace / "INPUT" / p;
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

GeodesyLoader::GeodesyLoader(const AppConfig& cfg)
    : m_cfg(cfg)
{
    int ss = m_cfg.geodesy.input.stateSize;
    if (ss != 3 && ss != 6)
        throw loki::ConfigException(
            "GeodesyLoader: stateSize must be 3 or 6, got "
            + std::to_string(ss));
}

// ---------------------------------------------------------------------------
// load()
// ---------------------------------------------------------------------------

GeodesyLoadResult GeodesyLoader::load() const
{
    std::filesystem::path resolvedPath = _resolvePath();
    std::ifstream         file(resolvedPath);
    if (!file.is_open())
        throw loki::FileNotFoundException(
            "GeodesyLoader: cannot open '" + resolvedPath.string() + "'");

    int  N        = m_cfg.geodesy.input.stateSize;
    const std::string& delimStr = m_cfg.geodesy.input.delimiter;
    char delim    = delimStr.empty() ? ';' : delimStr[0];
    int  covElems = N * (N + 1) / 2;   // 6 for N=3, 21 for N=6
    int  fullCols = N + covElems;

    GeodesyLoadResult res;
    res.filePath    = resolvedPath;
    res.coordSystem = m_cfg.geodesy.input.coordSystem;
    res.stateSize   = N;

    bool hasCov   = false;
    bool covKnown = false;

    std::string line;
    int         lineNo = 0;

    while (std::getline(file, line)) {
        ++lineNo;

        // Strip Windows \r
        if (!line.empty() && line.back() == '\r') line.pop_back();

        // Skip comments and blank lines
        if (line.empty() || line[0] == '#') { ++res.linesSkipped; continue; }

        std::vector<double> vals = _tokenise(line, delim);
        if (vals.empty()) { ++res.linesSkipped; continue; }

        // Detect covariance columns from first data line
        if (!covKnown) {
            if (static_cast<int>(vals.size()) >= fullCols) {
                hasCov = true;
                LOKI_INFO("GeodesyLoader: covariance columns detected (N="
                          + std::to_string(N) + ")");
            } else if (static_cast<int>(vals.size()) >= N) {
                hasCov = false;
            } else {
                throw loki::ParseException(
                    "GeodesyLoader: line " + std::to_string(lineNo)
                    + " has too few columns (" + std::to_string(vals.size())
                    + ", expected >= " + std::to_string(N) + ")");
            }
            covKnown = true;
        }

        // Enforce consistent column count
        if (hasCov && static_cast<int>(vals.size()) < fullCols)
            throw loki::ParseException(
                "GeodesyLoader: line " + std::to_string(lineNo)
                + " expected " + std::to_string(fullCols) + " columns, got "
                + std::to_string(vals.size()));

        // Parse state vector
        Eigen::VectorXd pos(N);
        for (int i = 0; i < N; ++i)
            pos(i) = vals[static_cast<std::size_t>(i)];
        res.positions.push_back(pos);

        // Parse covariance (upper triangle)
        if (hasCov)
            res.covariances.push_back(
                _upperTriangleToMatrix(vals, N,
                                       static_cast<std::size_t>(N)));

        ++res.linesRead;
    }

    if (res.positions.empty())
        throw loki::DataException(
            "GeodesyLoader: no data rows found in '"
            + resolvedPath.string() + "'");

    LOKI_INFO("GeodesyLoader: loaded " + std::to_string(res.positions.size())
              + " points from '" + resolvedPath.string() + "'");
    return res;
}
