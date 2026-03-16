#include "loki/io/loader.hpp"

#include "loki/core/exceptions.hpp"
#include "loki/core/logger.hpp"
#include "loki/timeseries/timeStamp.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <sstream>

using namespace loki;

namespace loki {

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor
// ─────────────────────────────────────────────────────────────────────────────

Loader::Loader(const InputConfig& config)
    : m_config(config)
{}

// ─────────────────────────────────────────────────────────────────────────────
//  load
// ─────────────────────────────────────────────────────────────────────────────

LoadResult Loader::load(const std::filesystem::path& filePath) const
{
    if (!std::filesystem::exists(filePath)) {
        throw FileNotFoundException(
            "Loader: file not found: '" + filePath.string() + "'.");
    }

    std::ifstream ifs(filePath);
    if (!ifs.is_open()) {
        throw IoException(
            "Loader: cannot open file: '" + filePath.string() + "'.");
    }

    LOKI_INFO("Loader: opening '" + filePath.string() + "'.");

    LoadResult result;
    result.filePath = filePath;

    // Column names detected from the header comment line.
    std::vector<std::string> detectedNames;

    // How many value columns we decided to load (determined on first data line).
    std::size_t numValueCols = 0;
    bool        layoutFixed  = false;

    // Indices into the raw data fields for selected value columns (0-based,
    // where field 0 is the time column). Populated on the first data line.
    std::vector<std::size_t> selectedFieldIndices;

    std::string line;
    while (std::getline(ifs, line)) {

        ++result.linesRead;

        // ── Skip blank lines ──────────────────────────────────────────────────
        if (line.empty() || line.find_first_not_of(" \t\r\n") == std::string::npos) {
            ++result.linesSkipped;
            continue;
        }

        // ── Handle comment lines ──────────────────────────────────────────────
        if (line.front() == m_config.commentChar) {
            ++result.linesSkipped;
            // Try to extract column names even if we already have some —
            // config names override, so we only store detected ones here.
            if (detectedNames.empty()) {
                _parseColumnHeader(line, m_config.commentChar, detectedNames);
            }
            continue;
        }

        // ── Data line ─────────────────────────────────────────────────────────
        const auto fields = _splitLine(line);

        // GPS_WEEK_SOW occupies two fields for time — minimum 3 fields total.
        const std::size_t minFields =
            (m_config.timeFormat == TimeFormat::GPS_WEEK_SOW) ? 3u : 2u;

        if (fields.size() < minFields) {
            LOKI_WARNING("Loader: line " + std::to_string(result.linesRead)
                         + " has too few fields (" + std::to_string(fields.size())
                         + ") — skipping.");
            ++result.linesSkipped;
            continue;
        }

        // ── Fix layout on first data line ─────────────────────────────────────
        if (!layoutFixed) {

            // Time occupies field 0 (and field 1 for GPS_WEEK_SOW).
            const std::size_t firstValueField =
                (m_config.timeFormat == TimeFormat::GPS_WEEK_SOW) ? 2u : 1u;

            const std::size_t availableValueFields = fields.size() - firstValueField;

            if (m_config.columns.empty()) {
                // Load all value columns.
                numValueCols = availableValueFields;
                for (std::size_t i = 0; i < numValueCols; ++i) {
                    selectedFieldIndices.push_back(firstValueField + i);
                }
            } else {
                // Load only the requested columns (1-based config indices).
                for (const int col : m_config.columns) {
                    // col is 1-based; field 1 is the first value field.
                    const std::size_t fieldIdx = static_cast<std::size_t>(col) - 1u;
                    if (fieldIdx < firstValueField || fieldIdx >= fields.size()) {
                        LOKI_WARNING("Loader: requested column " + std::to_string(col)
                                     + " is out of range — skipping.");
                        continue;
                    }
                    selectedFieldIndices.push_back(fieldIdx);
                }
                numValueCols = selectedFieldIndices.size();
            }

            if (numValueCols == 0) {
                throw ParseException(
                    "Loader: no valid value columns to load from '"
                    + filePath.string() + "'.");
            }

            // Initialise one TimeSeries per value column.
            result.series.resize(numValueCols);
            layoutFixed = true;

            LOKI_INFO("Loader: " + std::to_string(numValueCols)
                      + " value column(s) selected.");
        }

        // ── Parse time ────────────────────────────────────────────────────────
        TimeStamp ts;
        try {
            const std::string nextToken =
                (m_config.timeFormat == TimeFormat::GPS_WEEK_SOW && fields.size() > 1)
                ? fields[1] : "";
            ts = _parseTime(fields[0], nextToken);
        } catch (const LOKIException& e) {
            LOKI_WARNING("Loader: line " + std::to_string(result.linesRead)
                         + std::string(": time parse error (") + e.what() + ") — skipping.");
            ++result.linesSkipped;
            continue;
        }

        // ── Parse values ──────────────────────────────────────────────────────
        bool lineOk = true;
        std::vector<double> values(numValueCols);

        for (std::size_t i = 0; i < numValueCols; ++i) {
            const std::size_t fieldIdx = selectedFieldIndices[i];

            if (fieldIdx >= fields.size()) {
                LOKI_WARNING("Loader: line " + std::to_string(result.linesRead)
                             + ": missing field " + std::to_string(fieldIdx)
                             + " — skipping line.");
                lineOk = false;
                break;
            }

            const std::string& tok = fields[fieldIdx];
            double val{};
            const auto [ptr, ec] = std::from_chars(
                tok.data(), tok.data() + tok.size(), val);

            if (ec != std::errc{}) {
                LOKI_WARNING("Loader: line " + std::to_string(result.linesRead)
                             + ": cannot parse value '" + tok
                             + "' in field " + std::to_string(fieldIdx)
                             + " — skipping line.");
                lineOk = false;
                break;
            }
            values[i] = val;
        }

        if (!lineOk) {
            ++result.linesSkipped;
            continue;
        }

        // ── Append to series ──────────────────────────────────────────────────
        for (std::size_t i = 0; i < numValueCols; ++i) {
            result.series[i].append(ts, values[i]);
        }
    }

    if (result.series.empty() || result.series[0].empty()) {
        throw ParseException(
            "Loader: no valid data found in '" + filePath.string() + "'.");
    }

    // ── Assign column names ───────────────────────────────────────────────────
    // Config names have priority; fall back to detected, then auto-generated.
    result.columnNames.resize(numValueCols);
    for (std::size_t i = 0; i < numValueCols; ++i) {
        const std::size_t fieldIdx = selectedFieldIndices[i];
        // detectedNames includes the time column at index 0, so value column i
        // corresponds to detectedNames[fieldIdx] if it exists.
        if (fieldIdx < detectedNames.size()) {
            result.columnNames[i] = detectedNames[fieldIdx];
        } else {
            result.columnNames[i] = "col_" + std::to_string(fieldIdx + 1);
        }
    }

    // Assign metadata to each TimeSeries from detected column names.
    for (std::size_t i = 0; i < numValueCols; ++i) {
        SeriesMetadata meta;
        meta.stationId = filePath.stem().string();

        // Split "WIG_SPEED[m/s]" -> componentName="WIG_SPEED", unit="m/s"
        const auto& raw     = result.columnNames[i];
        const auto  bracket = raw.find('[');
        if (bracket != std::string::npos && raw.back() == ']') {
            meta.componentName = raw.substr(0, bracket);
            meta.unit          = raw.substr(bracket + 1, raw.size() - bracket - 2);
        } else {
            meta.componentName = raw;
            meta.unit          = "-";
        }

        result.series[i].setMetadata(meta);
    }

    LOKI_INFO("Loader: read " + std::to_string(result.linesRead)
              + " lines, skipped " + std::to_string(result.linesSkipped)
              + ", loaded " + std::to_string(result.series[0].size()) + " records.");

    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
//  _parseColumnHeader
// ─────────────────────────────────────────────────────────────────────────────

bool Loader::_parseColumnHeader(const std::string&        line,
                                char                      commentChar,
                                std::vector<std::string>& names)
{
    // Look for pattern: "<commentChar> Columns:" (case-insensitive)
    std::string lower = line;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    const std::string marker = std::string(1, static_cast<char>(
                                   std::tolower(static_cast<unsigned char>(commentChar))))
                               + " columns:";

    const auto pos = lower.find(marker);
    if (pos == std::string::npos) {
        return false;
    }

    // Extract everything after "columns:"
    const std::string rest = line.substr(pos + marker.size());

    std::istringstream iss(rest);
    std::string token;
    while (std::getline(iss, token, ',')) {
        // Trim leading/trailing whitespace.
        const auto start = token.find_first_not_of(" \t");
        const auto end   = token.find_last_not_of(" \t\r\n");
        if (start != std::string::npos) {
            names.push_back(token.substr(start, end - start + 1));
        }
    }

    return !names.empty();
}

// ─────────────────────────────────────────────────────────────────────────────
//  _parseTime
// ─────────────────────────────────────────────────────────────────────────────

TimeStamp Loader::_parseTime(const std::string& token,
                              const std::string& nextToken) const
{
    switch (m_config.timeFormat) {

        case TimeFormat::GPS_TOTAL_SECONDS: {
            double gps{};
            const auto [ptr, ec] = std::from_chars(
                token.data(), token.data() + token.size(), gps);
            if (ec != std::errc{}) {
                throw ParseException("Cannot parse GPS seconds: '" + token + "'.");
            }
            return TimeStamp::fromGpsTotalSeconds(gps);
        }

        case TimeFormat::GPS_WEEK_SOW: {
            int    week{};
            double sow{};
            const auto [p1, e1] = std::from_chars(
                token.data(), token.data() + token.size(), week);
            const auto [p2, e2] = std::from_chars(
                nextToken.data(), nextToken.data() + nextToken.size(), sow);
            if (e1 != std::errc{} || e2 != std::errc{}) {
                throw ParseException(
                    "Cannot parse GPS week/SOW: '" + token + "' '" + nextToken + "'.");
            }
            return TimeStamp::fromGpsWeekSow(week, sow);
        }

        case TimeFormat::MJD: {
            double mjd{};
            const auto [ptr, ec] = std::from_chars(
                token.data(), token.data() + token.size(), mjd);
            if (ec != std::errc{}) {
                throw ParseException("Cannot parse MJD: '" + token + "'.");
            }
            return TimeStamp::fromMjd(mjd);
        }

        case TimeFormat::UNIX: {
            double unix{};
            const auto [ptr, ec] = std::from_chars(
                token.data(), token.data() + token.size(), unix);
            if (ec != std::errc{}) {
                throw ParseException("Cannot parse Unix time: '" + token + "'.");
            }
            return TimeStamp::fromUnix(unix);
        }

        case TimeFormat::UTC: {
            return TimeStamp(token);
        }

        case TimeFormat::INDEX: {
            // Index has no absolute time — use Unix epoch + index seconds as
            // a monotonic placeholder. The index value is preserved as the
            // fractional MJD offset so relative ordering is maintained.
            double idx{};
            const auto [ptr, ec] = std::from_chars(
                token.data(), token.data() + token.size(), idx);
            if (ec != std::errc{}) {
                throw ParseException("Cannot parse index: '" + token + "'.");
            }
            return TimeStamp::fromUnix(idx);
        }
    }

    // Unreachable — all enum cases handled above.
    throw ParseException("Loader: unknown TimeFormat.");
}

// ─────────────────────────────────────────────────────────────────────────────
//  _splitLine
// ─────────────────────────────────────────────────────────────────────────────

std::vector<std::string> Loader::_splitLine(const std::string& line) const
{
    std::vector<std::string> fields;
    std::istringstream iss(line);
    std::string token;
    while (std::getline(iss, token, m_config.delimiter)) {
        // Trim trailing carriage return (Windows line endings).
        if (!token.empty() && token.back() == '\r') {
            token.pop_back();
        }
        fields.push_back(std::move(token));
    }
    return fields;
}

} // namespace loki
