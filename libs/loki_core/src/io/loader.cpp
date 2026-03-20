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

// -----------------------------------------------------------------------------
//  Constructor
// -----------------------------------------------------------------------------

Loader::Loader(const InputConfig& config)
    : m_config(config)
{}

// -----------------------------------------------------------------------------
//  load
// -----------------------------------------------------------------------------

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

    // Determine how many fields form the time token.
    // timeColumns empty -> use field 0 only (numTimeFields = 1).
    // timeColumns = [0, 1] -> join fields 0 and 1 with a space (numTimeFields = 2).
    const std::size_t numTimeFields = m_config.timeColumns.empty()
        ? 1u
        : static_cast<std::size_t>(m_config.timeColumns.size());

    // GPS_WEEK_SOW always occupies exactly 2 fields regardless of timeColumns.
    const bool isGpsWeekSow = (m_config.timeFormat == TimeFormat::GPS_WEEK_SOW);
    const std::size_t timeFieldCount = isGpsWeekSow ? 2u : numTimeFields;

    LoadResult result;
    result.filePath = filePath;

    std::vector<std::string> detectedNames;
    std::size_t numValueCols = 0;
    bool        layoutFixed  = false;
    std::vector<std::size_t> selectedFieldIndices;

    std::string line;
    while (std::getline(ifs, line)) {

        ++result.linesRead;

        if (line.empty() || line.find_first_not_of(" \t\r\n") == std::string::npos) {
            ++result.linesSkipped;
            continue;
        }

        if (line.front() == m_config.commentChar) {
            ++result.linesSkipped;
            if (detectedNames.empty()) {
                _parseColumnHeader(line, m_config.commentChar, detectedNames);
            }
            continue;
        }

        const auto fields = _splitLine(line);

        // Minimum fields: time block + at least one value.
        const std::size_t minFields = timeFieldCount + 1u;
        if (fields.size() < minFields) {
            LOKI_WARNING("Loader: line " + std::to_string(result.linesRead)
                         + " has too few fields (" + std::to_string(fields.size())
                         + ", need " + std::to_string(minFields) + ") -- skipping.");
            ++result.linesSkipped;
            continue;
        }

        // Fix layout on first data line.
        if (!layoutFixed) {
            const std::size_t firstValueField = timeFieldCount;

            const std::size_t availableValueFields = fields.size() - firstValueField;

            if (m_config.columns.empty()) {
                numValueCols = availableValueFields;
                for (std::size_t i = 0; i < numValueCols; ++i) {
                    selectedFieldIndices.push_back(firstValueField + i);
                }
            } else {
                for (const int col : m_config.columns) {
                    // col is 1-based from the start of the line (field 0 = column 1).
                    const std::size_t fieldIdx = static_cast<std::size_t>(col) - 1u;
                    if (fieldIdx < firstValueField || fieldIdx >= fields.size()) {
                        LOKI_WARNING("Loader: requested column " + std::to_string(col)
                                     + " is out of range or points to a time field -- skipping.");
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

            result.series.resize(numValueCols);
            layoutFixed = true;

            LOKI_INFO("Loader: time occupies " + std::to_string(timeFieldCount)
                      + " field(s), " + std::to_string(numValueCols)
                      + " value column(s) selected.");
        }

        // Parse time.
        TimeStamp ts;
        try {
            ts = _parseTime(fields);
        } catch (const LOKIException& e) {
            LOKI_WARNING("Loader: line " + std::to_string(result.linesRead)
                         + ": time parse error (" + e.what() + ") -- skipping.");
            ++result.linesSkipped;
            continue;
        }

        // Parse values.
        bool lineOk = true;
        std::vector<double> values(numValueCols);

        for (std::size_t i = 0; i < numValueCols; ++i) {
            const std::size_t fieldIdx = selectedFieldIndices[i];

            if (fieldIdx >= fields.size()) {
                LOKI_WARNING("Loader: line " + std::to_string(result.linesRead)
                             + ": missing field " + std::to_string(fieldIdx)
                             + " -- skipping line.");
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
                             + " -- skipping line.");
                lineOk = false;
                break;
            }
            values[i] = val;
        }

        if (!lineOk) {
            ++result.linesSkipped;
            continue;
        }

        for (std::size_t i = 0; i < numValueCols; ++i) {
            result.series[i].append(ts, values[i]);
        }
    }

    if (result.series.empty() || result.series[0].empty()) {
        throw ParseException(
            "Loader: no valid data found in '" + filePath.string() + "'.");
    }

    // Assign column names.
    result.columnNames.resize(numValueCols);
    for (std::size_t i = 0; i < numValueCols; ++i) {
        const std::size_t fieldIdx = selectedFieldIndices[i];
        if (fieldIdx < detectedNames.size()) {
            result.columnNames[i] = detectedNames[fieldIdx];
        } else {
            result.columnNames[i] = "col_" + std::to_string(fieldIdx + 1);
        }
    }

    for (std::size_t i = 0; i < numValueCols; ++i) {
        SeriesMetadata meta;
        meta.stationId = filePath.stem().string();

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

// -----------------------------------------------------------------------------
//  _parseColumnHeader
// -----------------------------------------------------------------------------

bool Loader::_parseColumnHeader(const std::string&        line,
                                char                      commentChar,
                                std::vector<std::string>& names)
{
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

    const std::string rest = line.substr(pos + marker.size());

    std::istringstream iss(rest);
    std::string token;
    while (std::getline(iss, token, ',')) {
        const auto start = token.find_first_not_of(" \t");
        const auto end   = token.find_last_not_of(" \t\r\n");
        if (start != std::string::npos) {
            names.push_back(token.substr(start, end - start + 1));
        }
    }

    return !names.empty();
}

// -----------------------------------------------------------------------------
//  _parseTime
// -----------------------------------------------------------------------------

TimeStamp Loader::_parseTime(const std::vector<std::string>& fields) const
{
    switch (m_config.timeFormat) {

        case TimeFormat::GPS_TOTAL_SECONDS: {
            double gps{};
            const auto [ptr, ec] = std::from_chars(
                fields[0].data(), fields[0].data() + fields[0].size(), gps);
            if (ec != std::errc{}) {
                throw ParseException("Cannot parse GPS seconds: '" + fields[0] + "'.");
            }
            return TimeStamp::fromGpsTotalSeconds(gps);
        }

        case TimeFormat::GPS_WEEK_SOW: {
            if (fields.size() < 2) {
                throw ParseException("GPS_WEEK_SOW requires at least 2 fields.");
            }
            int    week{};
            double sow{};
            const auto [p1, e1] = std::from_chars(
                fields[0].data(), fields[0].data() + fields[0].size(), week);
            const auto [p2, e2] = std::from_chars(
                fields[1].data(), fields[1].data() + fields[1].size(), sow);
            if (e1 != std::errc{} || e2 != std::errc{}) {
                throw ParseException(
                    "Cannot parse GPS week/SOW: '" + fields[0] + "' '" + fields[1] + "'.");
            }
            return TimeStamp::fromGpsWeekSow(week, sow);
        }

        case TimeFormat::MJD: {
            double mjd{};
            const auto [ptr, ec] = std::from_chars(
                fields[0].data(), fields[0].data() + fields[0].size(), mjd);
            if (ec != std::errc{}) {
                throw ParseException("Cannot parse MJD: '" + fields[0] + "'.");
            }
            return TimeStamp::fromMjd(mjd);
        }

        case TimeFormat::UNIX: {
            double unix{};
            const auto [ptr, ec] = std::from_chars(
                fields[0].data(), fields[0].data() + fields[0].size(), unix);
            if (ec != std::errc{}) {
                throw ParseException("Cannot parse Unix time: '" + fields[0] + "'.");
            }
            return TimeStamp::fromUnix(unix);
        }

        case TimeFormat::UTC: {
            // Build the time token by joining the configured time fields.
            // If timeColumns is empty, use field 0 only.
            // If timeColumns = [0, 1], join fields[0] + " " + fields[1].
            std::string token;
            if (m_config.timeColumns.empty()) {
                token = fields[0];
            } else {
                for (std::size_t i = 0; i < m_config.timeColumns.size(); ++i) {
                    const std::size_t fi =
                        static_cast<std::size_t>(m_config.timeColumns[i]);
                    if (fi >= fields.size()) {
                        throw ParseException(
                            "UTC time_columns[" + std::to_string(i) +
                            "] = " + std::to_string(fi) +
                            " is out of range (line has " +
                            std::to_string(fields.size()) + " fields).");
                    }
                    if (i > 0) token += ' ';
                    token += fields[fi];
                }
            }
            return TimeStamp(token);
        }

        case TimeFormat::INDEX: {
            double idx{};
            const auto [ptr, ec] = std::from_chars(
                fields[0].data(), fields[0].data() + fields[0].size(), idx);
            if (ec != std::errc{}) {
                throw ParseException("Cannot parse index: '" + fields[0] + "'.");
            }
            return TimeStamp::fromUnix(idx);
        }
    }

    throw ParseException("Loader: unknown TimeFormat.");
}

// -----------------------------------------------------------------------------
//  _splitLine
// -----------------------------------------------------------------------------

std::vector<std::string> Loader::_splitLine(const std::string& line) const
{
    std::vector<std::string> fields;
    std::istringstream iss(line);
    std::string token;
    while (std::getline(iss, token, m_config.delimiter)) {
        if (!token.empty() && token.back() == '\r') {
            token.pop_back();
        }
        fields.push_back(std::move(token));
    }
    return fields;
}

} // namespace loki
