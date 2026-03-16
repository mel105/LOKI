#pragma once

#include <compare>
#include <string>
#include <string_view>

/**
 * @brief Represents an absolute point in time with millisecond precision.
 *
 * Internally stores time as Modified Julian Date (MJD) in UTC.
 * The fractional part of MJD encodes the time of day:
 *   1 day = 1.0 MJD, 1 ms ≈ 1.157e-8 MJD — well within double precision.
 *
 * Supported input/output formats:
 *   - Calendar date/time  (Y, M, D, h, m, s)
 *   - UTC ISO string      ("YYYY-MM-DD hh:mm:ss.sss", leading zeros optional)
 *   - MJD                 (double)
 *   - Unix time           (seconds since 1970-01-01 00:00:00 UTC)
 *   - GPS total seconds   (seconds since 1980-01-06 00:00:00 UTC)
 *   - GPS week + SOW      (GPS week number + seconds of week)
 *
 * GPS ↔ UTC conversion uses a built-in leap-second table (last entry: 2017-01-01, +18 s).
 */
class TimeStamp {
public:
    /// @brief Default-constructs to MJD 0.0 (1858-11-17 00:00:00 UTC).
    TimeStamp() = default;


    // ── Constructors ──────────────────────────────────────────────────────────

    /**
     * @brief Constructs a TimeStamp from calendar components (UTC).
     * @param year   Four-digit year (1900–2100).
     * @param month  Month (1–12).
     * @param day    Day of month (1–31).
     * @param hour   Hour of day (0–23), default 0.
     * @param minute Minute (0–59), default 0.
     * @param second Second with fractional part (0.0–60.0), default 0.0.
     * @throws DataException if any component is out of range.
     */
    TimeStamp(int year, int month, int day,
              int hour = 0, int minute = 0, double second = 0.0);

    /**
     * @brief Constructs a TimeStamp from a UTC ISO string.
     *
     * Accepted formats:
     *   "YYYY-MM-DD hh:mm:ss"
     *   "YYYY-M-D h:m:s"
     *   "YYYY-MM-DD hh:mm:ss.sss"
     *
     * @param utcString Input string in ISO 8601-like format.
     * @throws ParseException if the string cannot be parsed.
     */
    explicit TimeStamp(std::string_view utcString);

    // ── Factory methods ───────────────────────────────────────────────────────

    /**
     * @brief Creates a TimeStamp from a Modified Julian Date (UTC).
     * @param mjd MJD value (valid range: 15079–88127, years 1900–2100).
     * @throws DataException if mjd is outside the valid range.
     */
    static TimeStamp fromMjd(double mjd);

    /**
     * @brief Creates a TimeStamp from Unix time (seconds since 1970-01-01 UTC).
     * @param unixSeconds Seconds since Unix epoch, may include fractional seconds.
     * @throws DataException if the resulting date is outside the valid range.
     */
    static TimeStamp fromUnix(double unixSeconds);

    /**
     * @brief Creates a TimeStamp from GPS total seconds (seconds since 1980-01-06 UTC).
     *
     * GPS time does not include leap seconds. The conversion applies the
     * built-in leap-second table to produce UTC.
     *
     * @param gpsTotalSeconds Seconds elapsed since GPS epoch.
     * @throws DataException if the resulting date is outside the valid range.
     */
    static TimeStamp fromGpsTotalSeconds(double gpsTotalSeconds);

    /**
     * @brief Creates a TimeStamp from GPS week number and seconds of week.
     * @param gpsWeek        GPS week number (0-based, since 1980-01-06).
     * @param secondsOfWeek  Seconds elapsed within the GPS week (0.0–604800.0).
     * @throws DataException if secondsOfWeek is outside [0, 604800).
     */
    static TimeStamp fromGpsWeekSow(int gpsWeek, double secondsOfWeek);

    // ── Calendar getters ──────────────────────────────────────────────────────

    /// @brief Returns the year component (UTC).
    int year() const;

    /// @brief Returns the month component (1–12, UTC).
    int month() const;

    /// @brief Returns the day-of-month component (1–31, UTC).
    int day() const;

    /// @brief Returns the hour component (0–23, UTC).
    int hour() const;

    /// @brief Returns the minute component (0–59, UTC).
    int minute() const;

    /// @brief Returns the second component including fractional part (0.0–60.0, UTC).
    double second() const;

    // ── Format getters ────────────────────────────────────────────────────────

    /// @brief Returns the MJD representation (UTC).
    double mjd() const;

    /// @brief Returns seconds since Unix epoch (1970-01-01 00:00:00 UTC).
    double unixTime() const;

    /**
     * @brief Returns total GPS seconds since GPS epoch (1980-01-06 00:00:00 UTC).
     *
     * Result is in GPS time scale (no leap seconds).
     */
    double gpsTotalSeconds() const;

    /// @brief Returns the GPS week number.
    int gpsWeek() const;

    /// @brief Returns the seconds elapsed within the current GPS week (0.0–604800.0).
    double gpsSecondsOfWeek() const;

    /**
     * @brief Returns a UTC string in the canonical format "YYYY-MM-DD hh:mm:ss.sss".
     *
     * All fields are zero-padded. Seconds always show three decimal places.
     */
    std::string utcString() const;

    // ── Comparison ────────────────────────────────────────────────────────────

    /**
     * @brief Three-way comparison operator (C++20).
     *
     * Enables all six relational operators (<, <=, >, >=, ==, !=) automatically.
     * Comparison is based solely on the internal MJD value.
     *
     * Example:
     * @code
     *   TimeStamp t1(2020, 1, 1);
     *   TimeStamp t2(2021, 6, 15);
     *   bool earlier = (t1 < t2);   // true
     *   bool same    = (t1 == t1);  // true
     * @endcode
     */
    auto operator<=>(const TimeStamp& other) const = default;

private:

    /// Internal representation: Modified Julian Date in UTC.
    double m_mjd{0.0};

    // ── Internal conversion helpers ───────────────────────────────────────────

    /// @brief Parses a UTC ISO string and sets m_mjd.
    void _parseUtcString(std::string_view s);

    /// @brief Converts calendar components to MJD and sets m_mjd.
    void _calendarToMjd(int Y, int M, int D, int h, int m, double s);

    /// @brief Converts m_mjd to calendar components (UTC).
    void _mjdToCalendar(int& Y, int& M, int& D, int& h, int& m, double& s) const;

    /**
     * @brief Returns the GPS−UTC offset (leap seconds) applicable at the given MJD.
     *
     * Uses the built-in leap-second table. The offset satisfies:
     *   GPS_time = UTC_time + leapSecondsAt(mjd)
     *
     * @param mjd MJD in UTC scale.
     * @return Number of leap seconds (integer, >= 0).
     */
    static int _leapSecondsAt(double mjd);

    // ── Default constructor ───────────────────────────────────────────────────


    // ── Epoch constants ───────────────────────────────────────────────────────

    /// MJD of the Unix epoch: 1970-01-01 00:00:00 UTC.
    static constexpr double MJD_UNIX_EPOCH{40587.0};

    /// MJD of the GPS epoch: 1980-01-06 00:00:00 UTC.
    static constexpr double MJD_GPS_EPOCH{44244.0};

    /// Seconds per day.
    static constexpr double SECONDS_PER_DAY{86400.0};

    /// Seconds per GPS week.
    static constexpr double SECONDS_PER_GPS_WEEK{604800.0};

    /// Valid MJD range: 1900-01-01 to 2100-12-31.
    static constexpr double MJD_MIN{15079.0};
    static constexpr double MJD_MAX{88127.0};
};
