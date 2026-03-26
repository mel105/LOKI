#include <loki/timeseries/medianYearSeries.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <sstream>
#include <vector>

using namespace loki;

namespace loki {

// =============================================================================
//  Constants
// =============================================================================

namespace {

constexpr int    DAYS_PER_PROFILE    = 366;
constexpr double SLOT_TOLERANCE_DAYS = 1.0e-8;

} // anonymous namespace

// =============================================================================
//  Construction
// =============================================================================

MedianYearSeries::MedianYearSeries(const TimeSeries& series, Config cfg)
    : m_cfg(cfg)
{
    if (!series.isSorted()) {
        throw AlgorithmException(
            "MedianYearSeries: series is not sorted. Call sortByTime() first.");
    }
    if (series.size() < 2) {
        throw DataException(
            "MedianYearSeries: series must have at least 2 observations.");
    }

    m_stepDays = estimateStep(series);

    if (m_stepDays < m_cfg.maxResolutionDays - SLOT_TOLERANCE_DAYS) {
        std::ostringstream oss;
        oss << "MedianYearSeries: estimated sampling step ("
            << m_stepDays << " days) is finer than maxResolutionDays ("
            << m_cfg.maxResolutionDays
            << "). Use LINEAR or FORWARD_FILL for sub-hourly data.";
        throw ConfigException(oss.str());
    }

    const int raw     = static_cast<int>(std::round(1.0 / m_stepDays));
    m_slotsPerDay     = std::max(1, std::min(raw, 24));

    const double spanYears =
        (series[series.size() - 1].time.mjd() - series[0].time.mjd()) / 365.25;
    if (static_cast<int>(spanYears) < m_cfg.minYears) {
        std::ostringstream oss;
        oss << "MedianYearSeries: series spans only "
            << spanYears << " years (minYears=" << m_cfg.minYears
            << "). Profile will be computed from available data.";
        LOKI_WARNING(oss.str());
    }

    computeProfile(series);
}

// =============================================================================
//  Query interface
// =============================================================================

double MedianYearSeries::valueAt(const TimeStamp& ts) const noexcept
{
    return m_profile[slotIndex(ts)];
}

std::size_t MedianYearSeries::profileSize() const noexcept
{
    return m_profile.size();
}

double MedianYearSeries::stepDays() const noexcept
{
    return m_stepDays;
}

int MedianYearSeries::slotsPerDay() const noexcept
{
    return m_slotsPerDay;
}

// =============================================================================
//  Private helpers
// =============================================================================

double MedianYearSeries::estimateStep(const TimeSeries& series)
{
    std::vector<double> diffs;
    diffs.reserve(series.size() - 1);
    for (std::size_t i = 1; i < series.size(); ++i) {
        const double d = series[i].time.mjd() - series[i - 1].time.mjd();
        if (d > 0.0) diffs.push_back(d);
    }
    if (diffs.empty()) {
        throw ConfigException(
            "MedianYearSeries: all consecutive timestamps are identical -- "
            "cannot determine sampling resolution.");
    }

    const std::size_t mid = diffs.size() / 2;
    std::nth_element(diffs.begin(),
                     diffs.begin() + static_cast<std::ptrdiff_t>(mid),
                     diffs.end());
    double median = diffs[mid];
    if (diffs.size() % 2 == 0 && mid > 0) {
        const auto it = std::max_element(
            diffs.begin(),
            diffs.begin() + static_cast<std::ptrdiff_t>(mid));
        median = (median + *it) / 2.0;
    }
    return median;
}

// -----------------------------------------------------------------------------

void MedianYearSeries::computeProfile(const TimeSeries& series)
{
    const std::size_t totalSlots =
        static_cast<std::size_t>(DAYS_PER_PROFILE) *
        static_cast<std::size_t>(m_slotsPerDay);

    std::vector<std::vector<double>> buckets(totalSlots);

    std::size_t skippedNaN = 0;
    for (std::size_t i = 0; i < series.size(); ++i) {
        if (!isValid(series[i])) {
            ++skippedNaN;
            continue;
        }
        const std::size_t slot = slotIndex(series[i].time);
        buckets[slot].push_back(series[i].value);
    }

    if (skippedNaN > 0) {
        LOKI_INFO("MedianYearSeries: skipped " + std::to_string(skippedNaN) +
                  " NaN observation(s) during profile computation.");
    }

    m_profile.resize(totalSlots);
    std::size_t thinSlots = 0;

    for (std::size_t s = 0; s < totalSlots; ++s) {
        auto& bucket = buckets[s];
        if (static_cast<int>(bucket.size()) < m_cfg.minYears) {
            m_profile[s] = std::numeric_limits<double>::quiet_NaN();
            ++thinSlots;
            continue;
        }
        const std::size_t mid = bucket.size() / 2;
        std::nth_element(bucket.begin(),
                         bucket.begin() + static_cast<std::ptrdiff_t>(mid),
                         bucket.end());
        double med = bucket[mid];
        if (bucket.size() % 2 == 0 && mid > 0) {
            const auto it = std::max_element(
                bucket.begin(),
                bucket.begin() + static_cast<std::ptrdiff_t>(mid));
            med = (med + *it) / 2.0;
        }
        m_profile[s] = med;
    }

    if (thinSlots > 0) {
        std::ostringstream oss;
        oss << "MedianYearSeries: " << thinSlots << " slot(s) out of "
            << totalSlots << " have fewer than " << m_cfg.minYears
            << " valid value(s) and will return NaN.";
        LOKI_WARNING(oss.str());
    }

    LOKI_INFO("MedianYearSeries: profile computed. slots=" +
              std::to_string(totalSlots) +
              ", slotsPerDay=" + std::to_string(m_slotsPerDay) +
              ", stepDays=" + std::to_string(m_stepDays) + ".");
}

// -----------------------------------------------------------------------------

std::size_t MedianYearSeries::slotIndex(const TimeStamp& ts) const noexcept
{
    const int month = ts.month();
    const int day   = ts.day();

    static constexpr int MONTH_START[13] = {
        0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };

    const int  year     = ts.year();
    const bool isLeap   = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    const int  leapCorr = (isLeap && month >= 3) ? 1 : 0;
    const int  doy      = MONTH_START[month] + day + leapCorr;

    const double fracDay  = ts.mjd() - std::floor(ts.mjd());
    const int    slotOfDay =
        static_cast<int>(fracDay * m_slotsPerDay + 0.5) % m_slotsPerDay;

    const int safeDoy = std::max(1, std::min(doy, DAYS_PER_PROFILE));

    return static_cast<std::size_t>((safeDoy - 1) * m_slotsPerDay + slotOfDay);
}

} // namespace loki
