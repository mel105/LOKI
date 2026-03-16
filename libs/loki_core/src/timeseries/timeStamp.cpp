#include "loki/timeseries/timeStamp.hpp"

#include "loki/core/exceptions.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <format>
#include <iomanip>
#include <sstream>
#include <stdexcept>

using namespace loki;

// ─────────────────────────────────────────────────────────────────────────────
//  Leap-second table
//
//  Each entry holds the MJD (UTC) at which a new leap second took effect,
//  and the cumulative GPS−UTC offset in seconds from that date onward.
//  GPS time = UTC + offset.
//
//  Source: IERS Bulletin C / USNO
//  Last entry: 2017-01-01 (offset = 18 s). Update this table if the IERS
//  announces a new leap second.
// ─────────────────────────────────────────────────────────────────────────────

namespace {

struct LeapEntry {
    double mjd;    ///< MJD (UTC) from which this offset applies.
    int    offset; ///< Cumulative GPS−UTC offset in seconds.
};

// Entries are listed in ascending MJD order.
constexpr std::array<LeapEntry, 28> LEAP_SECOND_TABLE{{
    {41317.0,  1},  // 1972-01-01
    {41499.0,  2},  // 1972-07-01
    {41683.0,  3},  // 1973-01-01
    {42048.0,  4},  // 1974-01-01
    {42413.0,  5},  // 1975-01-01
    {42778.0,  6},  // 1976-01-01
    {43144.0,  7},  // 1977-01-01
    {43509.0,  8},  // 1978-01-01
    {43874.0,  9},  // 1979-01-01
    {44239.0, 10},  // 1980-01-01
    {44786.0, 11},  // 1981-07-01
    {45151.0, 12},  // 1982-07-01
    {45516.0, 13},  // 1983-07-01
    {46247.0, 14},  // 1985-07-01
    {47161.0, 15},  // 1988-01-01
    {47892.0, 16},  // 1990-01-01
    {48257.0, 17},  // 1991-01-01
    {48804.0, 18},  // 1992-07-01
    {49169.0, 19},  // 1993-07-01
    {49534.0, 20},  // 1994-07-01
    {50083.0, 21},  // 1996-01-01
    {50630.0, 22},  // 1997-07-01
    {51179.0, 23},  // 1999-01-01
    {53736.0, 24},  // 2006-01-01
    {54832.0, 25},  // 2009-01-01
    {56109.0, 26},  // 2012-07-01
    {57204.0, 27},  // 2015-07-01
    {57754.0, 28},  // 2017-01-01  ← last known entry
}};

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
//  Constructors
// ─────────────────────────────────────────────────────────────────────────────

TimeStamp::TimeStamp(int year, int month, int day,
                     int hour, int minute, double second)
{
    // Validate ranges before any computation.
    if (year  < 1900 || year  > 2100) throw DataException("TimeStamp: year out of range [1900, 2100].");
    if (month < 1    || month > 12  ) throw DataException("TimeStamp: month out of range [1, 12].");
    if (day   < 1    || day   > 31  ) throw DataException("TimeStamp: day out of range [1, 31].");
    if (hour  < 0    || hour  > 23  ) throw DataException("TimeStamp: hour out of range [0, 23].");
    if (minute < 0   || minute > 59 ) throw DataException("TimeStamp: minute out of range [0, 59].");
    if (second < 0.0 || second >= 61.0) throw DataException("TimeStamp: second out of range [0, 61).");

    _calendarToMjd(year, month, day, hour, minute, second);
}

TimeStamp::TimeStamp(std::string_view utcString)
{
    _parseUtcString(utcString);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Factory methods
// ─────────────────────────────────────────────────────────────────────────────

TimeStamp TimeStamp::fromMjd(double mjd)
{
    if (mjd < MJD_MIN || mjd > MJD_MAX) {
        throw DataException("TimeStamp::fromMjd: MJD out of valid range [15079, 88127].");
    }
    TimeStamp ts;
    ts.m_mjd = mjd;
    return ts;
}

TimeStamp TimeStamp::fromUnix(double unixSeconds)
{
    // Unix epoch is 1970-01-01 = MJD 40587.
    const double mjd = MJD_UNIX_EPOCH + unixSeconds / SECONDS_PER_DAY;
    return fromMjd(mjd);
}

TimeStamp TimeStamp::fromGpsTotalSeconds(double gpsTotalSeconds)
{
    // Convert GPS seconds to MJD in GPS time scale, then subtract leap seconds
    // to obtain UTC MJD.
    const double mjdGps = MJD_GPS_EPOCH + gpsTotalSeconds / SECONDS_PER_DAY;

    // We need the leap-second offset at this UTC instant. Since GPS = UTC + offset,
    // we first estimate UTC without correction, look up the offset, then adjust.
    // One iteration is sufficient because leap seconds change by at most 1 s.
    const int leapSec = _leapSecondsAt(mjdGps);
    const double mjdUtc = mjdGps - static_cast<double>(leapSec) / SECONDS_PER_DAY;

    return fromMjd(mjdUtc);
}

TimeStamp TimeStamp::fromGpsWeekSow(int gpsWeek, double secondsOfWeek)
{
    if (secondsOfWeek < 0.0 || secondsOfWeek >= SECONDS_PER_GPS_WEEK) {
        throw DataException("TimeStamp::fromGpsWeekSow: secondsOfWeek out of range [0, 604800).");
    }
    const double gpsTotalSec = static_cast<double>(gpsWeek) * SECONDS_PER_GPS_WEEK
                               + secondsOfWeek;
    return fromGpsTotalSeconds(gpsTotalSec);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Calendar getters
// ─────────────────────────────────────────────────────────────────────────────

int TimeStamp::year() const
{
    int Y, M, D, h, m; double s;
    _mjdToCalendar(Y, M, D, h, m, s);
    return Y;
}

int TimeStamp::month() const
{
    int Y, M, D, h, m; double s;
    _mjdToCalendar(Y, M, D, h, m, s);
    return M;
}

int TimeStamp::day() const
{
    int Y, M, D, h, m; double s;
    _mjdToCalendar(Y, M, D, h, m, s);
    return D;
}

int TimeStamp::hour() const
{
    int Y, M, D, h, m; double s;
    _mjdToCalendar(Y, M, D, h, m, s);
    return h;
}

int TimeStamp::minute() const
{
    int Y, M, D, h, m; double s;
    _mjdToCalendar(Y, M, D, h, m, s);
    return m;
}

double TimeStamp::second() const
{
    int Y, M, D, h, m; double s;
    _mjdToCalendar(Y, M, D, h, m, s);
    return s;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Format getters
// ─────────────────────────────────────────────────────────────────────────────

double TimeStamp::mjd() const
{
    return m_mjd;
}

double TimeStamp::unixTime() const
{
    return (m_mjd - MJD_UNIX_EPOCH) * SECONDS_PER_DAY;
}

double TimeStamp::gpsTotalSeconds() const
{
    // Convert UTC MJD to GPS time by adding the leap-second offset.
    const int leapSec = _leapSecondsAt(m_mjd);
    const double mjdGps = m_mjd + static_cast<double>(leapSec) / SECONDS_PER_DAY;
    return (mjdGps - MJD_GPS_EPOCH) * SECONDS_PER_DAY;
}

int TimeStamp::gpsWeek() const
{
    return static_cast<int>(gpsTotalSeconds() / SECONDS_PER_GPS_WEEK);
}

double TimeStamp::gpsSecondsOfWeek() const
{
    const double total = gpsTotalSeconds();
    return total - static_cast<double>(gpsWeek()) * SECONDS_PER_GPS_WEEK;
}

std::string TimeStamp::utcString() const
{
    int Y, M, D, h, m; double s;
    _mjdToCalendar(Y, M, D, h, m, s);

    // Format seconds: integer part zero-padded to 2 digits, always 3 decimal places.
    const int    secInt  = static_cast<int>(s);
    const int    secMs   = static_cast<int>(std::round((s - secInt) * 1000.0));
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << Y << '-'
        << std::setw(2) << M << '-'
        << std::setw(2) << D << ' '
        << std::setw(2) << h << ':'
        << std::setw(2) << m << ':'
        << std::setw(2) << secInt << '.'
        << std::setw(3) << secMs;
    return oss.str();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Internal: leap seconds
// ─────────────────────────────────────────────────────────────────────────────

int TimeStamp::_leapSecondsAt(double mjd)
{
    // Walk the table from the end; return the offset for the last entry whose
    // MJD is <= the requested MJD. Before the first GPS leap second (1972-01-01)
    // the offset is 0 (GPS epoch 1980 starts at +0 relative to UTC of that time;
    // the table starts from 1972 for completeness).
    int offset = 0;
    for (const auto& entry : LEAP_SECOND_TABLE) {
        if (mjd >= entry.mjd) {
            offset = entry.offset;
        } else {
            break;
        }
    }
    return offset;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Internal: calendar ↔ MJD
// ─────────────────────────────────────────────────────────────────────────────

void TimeStamp::_calendarToMjd(int Y, int M, int D, int h, int m, double s)
{
    // Standard Julian Date formula (Gregorian calendar).
    double dY = static_cast<double>(Y);
    double dM = static_cast<double>(M);
    double dD = static_cast<double>(D);

    if (dM <= 2.0) { dY -= 1.0; dM += 12.0; }

    const int A = static_cast<int>(dY / 100.0);
    const int B = 2 - A + A / 4;

    // JD of the calendar date at 0h UTC
    const double jd = std::floor(365.25 * (dY + 4716.0))
                    + std::floor(30.6001 * (dM + 1.0))
                    + dD
                    + static_cast<double>(B)
                    - 1524.5;

    // MJD = JD - 2400000.5
    const double dayFraction = (static_cast<double>(h) * 3600.0
                               + static_cast<double>(m) * 60.0
                               + s)
                               / SECONDS_PER_DAY;

    m_mjd = (jd - 2400000.5) + dayFraction;
}

void TimeStamp::_mjdToCalendar(int& Y, int& M, int& D, int& h, int& m, double& s) const
{
    // Separate integer day and fractional time-of-day.
    const double mjdDay  = std::floor(m_mjd);
    const double dayFrac = m_mjd - mjdDay;

    // Convert MJD day to Julian Date integer part (at noon).
    const long J = static_cast<long>(mjdDay) + 2400001L;

    // Gregorian calendar conversion (algorithm: Meeus, "Astronomical Algorithms").
    const long a = J + 32044L;
    const long b = (4L * a + 3L) / 146097L;
    const long c = a - (146097L * b) / 4L;
    const long d = (4L * c + 3L) / 1461L;
    const long e = c - (1461L * d) / 4L;
    const long mm = (5L * e + 2L) / 153L;

    D = static_cast<int>(e - (153L * mm + 2L) / 5L + 1L);
    M = static_cast<int>(mm + 3L - 12L * (mm / 10L));
    Y = static_cast<int>(100L * b + d - 4800L + mm / 10L);

    // Convert fractional day to h, m, s.
    const double totalSeconds = dayFrac * SECONDS_PER_DAY;
    h = static_cast<int>(totalSeconds / 3600.0);
    m = static_cast<int>((totalSeconds - h * 3600.0) / 60.0);
    s = totalSeconds - h * 3600.0 - m * 60.0;

    // Guard against floating-point rounding pushing seconds to 60.
    if (s >= 60.0) { s -= 60.0; m += 1; }
    if (m >= 60)   { m -= 60;   h += 1; }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Internal: UTC string parser
// ─────────────────────────────────────────────────────────────────────────────

void TimeStamp::_parseUtcString(std::string_view sv)
{
    int    Y = 0, Mo = 0, D = 0, h = 0, mi = 0;
    double s = 0.0;

    // sscanf handles both "YYYY-MM-DD hh:mm:ss" and "YYYY-M-D h:m:s[.sss]".
    const int parsed = std::sscanf(sv.data(),
                                   "%d-%d-%d %d:%d:%lf",
                                   &Y, &Mo, &D, &h, &mi, &s);

    // Accept 3 fields (date only) or 6 fields (full datetime).
    if (parsed != 6 && parsed != 3) {
        throw ParseException(
            std::string("TimeStamp: cannot parse UTC string: \"")
            + std::string(sv) + "\".");
    }

    // Reuse the validating constructor path.
    *this = TimeStamp(Y, Mo, D, h, mi, s);
}
