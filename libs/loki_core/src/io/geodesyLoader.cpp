#include <loki/io/geodesyLoader.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <charconv>
#include <fstream>
#include <sstream>
#include <string>

using namespace loki::io;

namespace loki::io {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

GeodesyLoader::GeodesyLoader(const AppConfig& cfg)
    : m_cfg(cfg)
{
    int ss = m_cfg.geodesy.input.stateSize;
    if (ss != 3 && ss != 6)
        throw loki::ConfigException(
            "GeodesyLoader: state_size must be 3 or 6, got "
            + std::to_string(ss));
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
// _tokenise
// ---------------------------------------------------------------------------

std::vector<double> GeodesyLoader::_tokenise(const std::string& line, char delim)
{
    std::vector<double> vals;
    std::istringstream  ss(line);
    std::string         tok;

    // whitespace delimiter: split on any whitespace run
    if (delim == ' ') {
        double v{};
        while (ss >> v) vals.push_back(v);
        return vals;
    }

    while (std::getline(ss, tok, delim)) {
        if (tok.empty()) continue;
        // strip \r
        if (!tok.empty() && tok.back() == '\r') tok.pop_back();
        double v{};
        auto [ptr, ec] = std::from_chars(tok.data(), tok.data() + tok.size(), v);
        if (ec != std::errc{})
            throw loki::ParseException(
                "GeodesyLoader: cannot parse token '" + tok + "'");
        vals.push_back(v);
    }
    return vals;
}

// ---------------------------------------------------------------------------
// _upperTriangleToMatrix
// ---------------------------------------------------------------------------

Eigen::MatrixXd GeodesyLoader::_upperTriangleToMatrix(
    const std::vector<double>& vals, int N, std::size_t offset)
{
    int expected = N * (N + 1) / 2;
    if (static_cast<int>(vals.size() - offset) < expected)
        throw loki::ParseException(
            "GeodesyLoader: not enough columns for covariance matrix");

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
// load()
// ---------------------------------------------------------------------------

GeodesyLoadResult GeodesyLoader::load() const
{
    namespace fs = std::filesystem;

    const fs::path filePath = _resolvePath();
    if (!fs::exists(filePath))
        throw loki::FileNotFoundException(
            "GeodesyLoader: file not found: '" + filePath.string() + "'");

    std::ifstream file(filePath);
    if (!file.is_open())
        throw loki::IoException(
            "GeodesyLoader: cannot open '" + filePath.string() + "'");

    LOKI_INFO("GeodesyLoader: opening '" + filePath.string() + "'");

    GeodesyLoadResult ds;
    ds.filePath    = filePath;
    ds.coordSystem = m_cfg.geodesy.input.coordSystem;
    ds.stateSize   = m_cfg.geodesy.input.stateSize;

    const int    N        = ds.stateSize;
    const int    covElems = N * (N + 1) / 2;
    const int    minCols  = N;
    const int    fullCols = N + covElems;
    const char   delim    = m_cfg.geodesy.input.delimiter.empty()
                            ? ';'
                            : m_cfg.geodesy.input.delimiter[0];

    bool hasCov    = false;
    bool layoutSet = false;

    std::string line;
    while (std::getline(file, line)) {
        ++ds.linesRead;

        // Strip \r
        if (!line.empty() && line.back() == '\r') line.pop_back();

        // Skip comments and blanks
        if (line.empty() || line[0] == '#') { ++ds.linesSkipped; continue; }
        if (line.find_first_not_of(" \t") == std::string::npos)
            { ++ds.linesSkipped; continue; }

        std::vector<double> vals;
        try {
            vals = _tokenise(line, delim);
        } catch (const loki::ParseException& ex) {
            LOKI_WARNING(std::string("GeodesyLoader: line ")
                         + std::to_string(ds.linesRead) + ": " + ex.what()
                         + " -- skipping");
            ++ds.linesSkipped;
            continue;
        }

        if (static_cast<int>(vals.size()) < minCols) {
            LOKI_WARNING("GeodesyLoader: line " + std::to_string(ds.linesRead)
                         + " has too few columns ("
                         + std::to_string(vals.size()) + ", need "
                         + std::to_string(minCols) + ") -- skipping");
            ++ds.linesSkipped;
            continue;
        }

        // Auto-detect covariance presence from first data line
        if (!layoutSet) {
            hasCov    = (static_cast<int>(vals.size()) >= fullCols);
            layoutSet = true;
            LOKI_INFO("GeodesyLoader: stateSize=" + std::to_string(N)
                      + " covariance=" + (hasCov ? "yes" : "no"));
        }

        // State vector
        Eigen::VectorXd pos(N);
        for (int i = 0; i < N; ++i)
            pos(i) = vals[static_cast<std::size_t>(i)];
        ds.positions.push_back(pos);

        // Covariance
        if (hasCov) {
            if (static_cast<int>(vals.size()) < fullCols) {
                LOKI_WARNING("GeodesyLoader: line " + std::to_string(ds.linesRead)
                             + " expected " + std::to_string(fullCols)
                             + " columns, got " + std::to_string(vals.size())
                             + " -- using zero covariance");
                ds.covariances.push_back(Eigen::MatrixXd::Zero(N, N));
            } else {
                ds.covariances.push_back(
                    _upperTriangleToMatrix(vals, N,
                                           static_cast<std::size_t>(N)));
            }
        }
    }

    if (ds.positions.empty())
        throw loki::DataException(
            "GeodesyLoader: no valid data found in '" + filePath.string() + "'");

    LOKI_INFO("GeodesyLoader: loaded " + std::to_string(ds.positions.size())
              + " points  (read=" + std::to_string(ds.linesRead)
              + " skipped=" + std::to_string(ds.linesSkipped) + ")");
    return ds;
}

} // namespace loki::io
