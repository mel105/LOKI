#include <loki/io/spatialLoader.hpp>

#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

using namespace loki;

namespace loki::io {

// =============================================================================
//  Accepted comment prefixes
// =============================================================================

static const std::vector<std::string> VALID_COMMENT_PREFIXES = {
    "#", "%", "!", "//", ";", "*"
};

// =============================================================================
//  Construction
// =============================================================================

SpatialLoader::SpatialLoader(const AppConfig& cfg)
    : m_cfg(cfg)
{}

// =============================================================================
//  _validateCommentPrefix
// =============================================================================

void SpatialLoader::_validateCommentPrefix(const std::string& prefix)
{
    for (const auto& v : VALID_COMMENT_PREFIXES) {
        if (prefix == v) return;
    }
    std::string allowed;
    for (const auto& v : VALID_COMMENT_PREFIXES) {
        if (!allowed.empty()) allowed += ", ";
        allowed += '"' + v + '"';
    }
    throw ConfigException(
        "SpatialLoader: commentPrefix '" + prefix + "' is not recognised. "
        "Allowed values: " + allowed + ".");
}

// =============================================================================
//  _isComment
// =============================================================================

bool SpatialLoader::_isComment(const std::string& line, const std::string& prefix)
{
    // Find first non-whitespace position.
    const std::size_t pos = line.find_first_not_of(" \t\r\n");
    if (pos == std::string::npos) return false;  // blank line -- not a comment
    return line.compare(pos, prefix.size(), prefix) == 0;
}

// =============================================================================
//  _extractNameFromComment
//
//  Pattern: <prefix><whitespace><NAME><whitespace>...
//  NAME must match [A-Za-z][A-Za-z0-9_]* (starts with letter).
//  Returns "" if the pattern does not match.
// =============================================================================

std::string SpatialLoader::_extractNameFromComment(const std::string& line,
                                                    const std::string& prefix)
{
    const std::size_t pos = line.find_first_not_of(" \t\r\n");
    if (pos == std::string::npos) return "";

    // Skip past prefix.
    const std::size_t afterPrefix = pos + prefix.size();
    if (afterPrefix >= line.size()) return "";

    // Skip whitespace after prefix.
    std::size_t nameStart = line.find_first_not_of(" \t", afterPrefix);
    if (nameStart == std::string::npos) return "";

    // Read until next whitespace.
    std::size_t nameEnd = line.find_first_of(" \t\r\n", nameStart);
    if (nameEnd == std::string::npos) nameEnd = line.size();

    const std::string candidate = line.substr(nameStart, nameEnd - nameStart);

    // Must start with a letter.
    if (candidate.empty() || !std::isalpha(static_cast<unsigned char>(candidate[0]))) {
        return "";
    }
    // Must contain only [A-Za-z0-9_].
    for (char c : candidate) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return "";
    }
    return candidate;
}

// =============================================================================
//  _tokenise
// =============================================================================

std::vector<std::string> SpatialLoader::_tokenise(const std::string& line, char delim)
{
    std::vector<std::string> tokens;
    if (delim == ' ') {
        // Split on any whitespace sequence.
        std::istringstream ss(line);
        std::string tok;
        while (ss >> tok) tokens.push_back(tok);
    } else {
        std::istringstream ss(line);
        std::string tok;
        while (std::getline(ss, tok, delim)) {
            // Trim surrounding whitespace.
            const std::size_t s = tok.find_first_not_of(" \t\r\n");
            const std::size_t e = tok.find_last_not_of (" \t\r\n");
            if (s != std::string::npos) tokens.push_back(tok.substr(s, e - s + 1));
        }
    }
    return tokens;
}

// =============================================================================
//  _isNoData
// =============================================================================

bool SpatialLoader::_isNoData(double v, double noDataValue, double tolerance)
{
    return std::abs(v - noDataValue) <= tolerance;
}

// =============================================================================
//  load
// =============================================================================

SpatialLoadResult SpatialLoader::load() const
{
    const auto& icfg = m_cfg.spatial.input;

    _validateCommentPrefix(icfg.commentPrefix);

    const std::filesystem::path filePath =
        m_cfg.workspace / "INPUT" / icfg.file;

    if (!std::filesystem::exists(filePath)) {
        throw FileNotFoundException(
            "SpatialLoader: file not found: '" + filePath.string() + "'.");
    }

    std::ifstream ifs(filePath);
    if (!ifs.is_open()) {
        throw IoException(
            "SpatialLoader: cannot open '" + filePath.string() + "'.");
    }

    LOKI_INFO("SpatialLoader: loading '" + filePath.filename().string() + "'");

    const char delim = icfg.delimiter;

    // -------------------------------------------------------------------------
    //  Pass 1: scan comment lines to build column name table.
    //
    //  Two formats are supported:
    //
    //  A) GSLIB format (auto-detected):
    //       Line 1: dataset title (ignored)
    //       Line 2: integer N = number of columns
    //       Lines 3..N+2: one column name per line (first word used)
    //     Detection: second non-empty comment line is a bare integer.
    //
    //  B) Generic format:
    //       Comment lines matching "# NAME ..." -- first word extracted.
    //       Line is ignored if first word is not a valid identifier
    //       (e.g. "# Zone A Data" -> "Zone" would create a false entry,
    //        so generic mode skips lines where the content after the prefix
    //        contains spaces before the first word -- i.e., we require the
    //        name to be the only meaningful token on the line, or at least
    //        first, for non-GSLIB files).
    //
    //  commentNames[c] = name of column c (0-based) in the data file.
    // -------------------------------------------------------------------------

    std::vector<std::string> commentNames;
    {
        std::string line;
        std::vector<std::string> rawComments;  // all non-empty comment lines

        while (std::getline(ifs, line)) {
            if (line.empty()) continue;
            if (_isComment(line, icfg.commentPrefix)) {
                rawComments.push_back(line);
            } else {
                break;  // first data line
            }
        }

        // Detect GSLIB format: rawComments[1] (2nd line) is a bare integer.
        bool isGslib = false;
        int  gslibNCols = 0;
        if (rawComments.size() >= 2) {
            // Strip prefix from second comment line and check if it is an int.
            const std::string& ln = rawComments[1];
            const std::size_t pos = ln.find_first_not_of(" \t\r\n");
            if (pos != std::string::npos) {
                const std::string afterPrefix =
                    (ln.compare(pos, icfg.commentPrefix.size(), icfg.commentPrefix) == 0)
                    ? ln.substr(pos + icfg.commentPrefix.size())
                    : ln.substr(pos);
                // Trim leading whitespace.
                const std::size_t ns = afterPrefix.find_first_not_of(" \t");
                if (ns != std::string::npos) {
                    const std::string token = afterPrefix.substr(
                        ns, afterPrefix.find_first_of(" \t\r\n", ns) - ns);
                    try {
                        gslibNCols = std::stoi(token);
                        isGslib    = (gslibNCols > 0 &&
                                      static_cast<int>(rawComments.size()) >= 2 + gslibNCols);
                    } catch (...) {}
                }
            }
        }

        if (isGslib) {
            // GSLIB: lines 3..(2+N) are column name lines; first word is the name.
            // commentNames is indexed by column (0-based).
            for (int c = 0; c < gslibNCols; ++c) {
                const int lineIdx = 2 + c;
                if (lineIdx >= static_cast<int>(rawComments.size())) break;
                const std::string name =
                    _extractNameFromComment(rawComments[static_cast<std::size_t>(lineIdx)],
                                           icfg.commentPrefix);
                commentNames.push_back(name.empty() ? "col" + std::to_string(c) : name);
            }
            LOKI_INFO("SpatialLoader: GSLIB format detected, " +
                      std::to_string(gslibNCols) + " columns.");
        } else {
            // Generic: extract names from all comment lines that match "# NAME".
            // Skip lines where the first token after the prefix starts with a digit
            // or where the comment appears to be a multi-word title (contains comma).
            for (const auto& cl : rawComments) {
                const std::string name = _extractNameFromComment(cl, icfg.commentPrefix);
                if (!name.empty()) commentNames.push_back(name);
            }
        }
    }
    ifs.clear();
    ifs.seekg(0);

    // -------------------------------------------------------------------------
    //  Pass 2: parse data lines.
    //  First data line determines nCols.
    //  valueColumns determined from config (empty = all except x and y).
    // -------------------------------------------------------------------------

    int         nCols        = -1;
    int         linesRead    = 0;
    int         linesSkipped = 0;

    // We accumulate raw rows first; column selection happens after nCols is known.
    struct RawRow { double x; double y; std::vector<double> vals; };
    std::vector<RawRow> rawRows;

    std::string line;
    int lineNo = 0;
    while (std::getline(ifs, line)) {
        ++lineNo;
        if (line.empty()) continue;
        if (_isComment(line, icfg.commentPrefix)) continue;

        const auto tokens = _tokenise(line, delim);
        if (tokens.empty()) continue;

        const int nc = static_cast<int>(tokens.size());

        if (nCols < 0) {
            // First data line: determine nCols and validate column indices.
            nCols = nc;
            if (icfg.xColumn < 0 || icfg.xColumn >= nCols) {
                throw ConfigException(
                    "SpatialLoader: x_column=" + std::to_string(icfg.xColumn)
                    + " is out of range for file with " + std::to_string(nCols)
                    + " columns.");
            }
            if (icfg.yColumn < 0 || icfg.yColumn >= nCols) {
                throw ConfigException(
                    "SpatialLoader: y_column=" + std::to_string(icfg.yColumn)
                    + " is out of range for file with " + std::to_string(nCols)
                    + " columns.");
            }
            if (icfg.xColumn == icfg.yColumn) {
                throw ConfigException(
                    "SpatialLoader: x_column and y_column must be different.");
            }
        }

        if (nc != nCols) {
            LOKI_WARNING("SpatialLoader: line " + std::to_string(lineNo)
                         + " has " + std::to_string(nc) + " tokens, expected "
                         + std::to_string(nCols) + " -- skipped.");
            ++linesSkipped;
            continue;
        }

        // Parse all tokens as double.
        std::vector<double> row(static_cast<std::size_t>(nCols));
        bool parseOk = true;
        for (int c = 0; c < nCols; ++c) {
            try {
                row[static_cast<std::size_t>(c)] = std::stod(tokens[static_cast<std::size_t>(c)]);
            } catch (...) {
                LOKI_WARNING("SpatialLoader: line " + std::to_string(lineNo)
                             + " col " + std::to_string(c)
                             + " is not numeric ('" + tokens[static_cast<std::size_t>(c)]
                             + "') -- line skipped.");
                parseOk = false;
                break;
            }
        }
        if (!parseOk) { ++linesSkipped; continue; }

        // Check x, y for no-data (if x or y is no-data, skip entire line).
        if (_isNoData(row[static_cast<std::size_t>(icfg.xColumn)],
                      icfg.noDataValue, icfg.noDataTolerance) ||
            _isNoData(row[static_cast<std::size_t>(icfg.yColumn)],
                      icfg.noDataValue, icfg.noDataTolerance)) {
            ++linesSkipped;
            continue;
        }

        RawRow r;
        r.x   = row[static_cast<std::size_t>(icfg.xColumn)];
        r.y   = row[static_cast<std::size_t>(icfg.yColumn)];
        r.vals = std::move(row);
        rawRows.push_back(std::move(r));
        ++linesRead;
    }

    if (rawRows.empty()) {
        throw DataException(
            "SpatialLoader: no valid data rows found in '" + filePath.string() + "'.");
    }

    // -------------------------------------------------------------------------
    //  Determine value column indices.
    //  If valueColumns is empty -> all columns except xColumn and yColumn.
    // -------------------------------------------------------------------------

    std::vector<int> valCols = icfg.valueColumns;
    if (valCols.empty()) {
        for (int c = 0; c < nCols; ++c) {
            if (c != icfg.xColumn && c != icfg.yColumn) valCols.push_back(c);
        }
    } else {
        // Validate.
        for (int c : valCols) {
            if (c < 0 || c >= nCols) {
                throw ConfigException(
                    "SpatialLoader: value_columns entry " + std::to_string(c)
                    + " is out of range (nCols=" + std::to_string(nCols) + ").");
            }
            if (c == icfg.xColumn || c == icfg.yColumn) {
                throw ConfigException(
                    "SpatialLoader: value_columns entry " + std::to_string(c)
                    + " is the same as x_column or y_column.");
            }
        }
    }

    const int nVars = static_cast<int>(valCols.size());

    // -------------------------------------------------------------------------
    //  Build column name -> index mapping.
    //
    //  commentNames are in file order (including x, y columns if the file
    //  documents all columns).  We assume commentNames[c] corresponds to
    //  column c in the data.  If there are fewer comment names than columns,
    //  fall back to "col{c}" for unnamed columns.
    // -------------------------------------------------------------------------

    auto colName = [&](int colIdx) -> std::string {
        if (colIdx < static_cast<int>(commentNames.size())) {
            return commentNames[static_cast<std::size_t>(colIdx)];
        }
        return "col" + std::to_string(colIdx);
    };

    // -------------------------------------------------------------------------
    //  Build per-variable SpatialPoint vectors.
    //  No-data values are filtered per column independently.
    // -------------------------------------------------------------------------

    SpatialLoadResult result;
    result.filePath     = filePath;
    result.linesRead    = linesRead;
    result.linesSkipped = linesSkipped;
    result.varNames.resize(static_cast<std::size_t>(nVars));
    result.variables.resize(static_cast<std::size_t>(nVars));

    for (int v = 0; v < nVars; ++v) {
        result.varNames[static_cast<std::size_t>(v)] =
            colName(valCols[static_cast<std::size_t>(v)]);
    }

    for (const auto& row : rawRows) {
        for (int v = 0; v < nVars; ++v) {
            const int    c   = valCols[static_cast<std::size_t>(v)];
            const double val = row.vals[static_cast<std::size_t>(c)];
            if (_isNoData(val, icfg.noDataValue, icfg.noDataTolerance)) continue;
            result.variables[static_cast<std::size_t>(v)].push_back(
                { row.x, row.y, val });
        }
    }

    // Log summary.
    LOKI_INFO("SpatialLoader: linesRead=" + std::to_string(linesRead)
              + "  linesSkipped=" + std::to_string(linesSkipped)
              + "  vars=" + std::to_string(nVars));
    for (int v = 0; v < nVars; ++v) {
        LOKI_INFO("  var '" + result.varNames[static_cast<std::size_t>(v)]
                  + "'  n=" + std::to_string(
                      result.variables[static_cast<std::size_t>(v)].size()));
    }

    return result;
}

} // namespace loki::io
