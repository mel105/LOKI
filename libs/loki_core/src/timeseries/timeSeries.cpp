#include "loki/timeseries/timeSeries.hpp"

#include "loki/core/exceptions.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

using namespace loki;

namespace loki {

// ─────────────────────────────────────────────────────────────────────────────
//  Construction
// ─────────────────────────────────────────────────────────────────────────────

TimeSeries::TimeSeries(SeriesMetadata metadata)
    : m_metadata(std::move(metadata))
{}

// ─────────────────────────────────────────────────────────────────────────────
//  Population
// ─────────────────────────────────────────────────────────────────────────────

void TimeSeries::append(const TimeStamp& time, double value, uint8_t flag)
{
    // Clear the sorted flag if the new point is earlier than the last one.
    if (m_isSorted && !m_data.empty() && time < m_data.back().time) {
        m_isSorted = false;
    }
    m_data.push_back({time, value, flag});
}

void TimeSeries::reserve(std::size_t n)
{
    m_data.reserve(n);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Element access
// ─────────────────────────────────────────────────────────────────────────────

const Observation& TimeSeries::operator[](std::size_t index) const noexcept
{
    return m_data[index];
}

const Observation& TimeSeries::at(std::size_t index) const
{
    if (index >= m_data.size()) {
        throw DataException(
            "TimeSeries::at: index " + std::to_string(index) +
            " out of range (size = " + std::to_string(m_data.size()) + ").");
    }
    return m_data[index];
}

std::size_t TimeSeries::size() const noexcept
{
    return m_data.size();
}

bool TimeSeries::empty() const noexcept
{
    return m_data.empty();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Time-based access
// ─────────────────────────────────────────────────────────────────────────────

std::optional<std::size_t> TimeSeries::indexOf(
    const TimeStamp& t, double toleranceMjd) const
{
    _requireSorted("indexOf");

    if (m_data.empty()) {
        return std::nullopt;
    }

    // Binary search for the first element with time >= t.
    const auto it = std::lower_bound(
        m_data.cbegin(), m_data.cend(), t,
        [](const Observation& obs, const TimeStamp& target) {
            return obs.time < target;
        });

    // Check the found element and its left neighbour — pick the closer one.
    std::size_t bestIdx    = m_data.size(); // sentinel: "not found"
    double      bestDelta  = toleranceMjd + 1.0; // worse than any valid match

    auto checkCandidate = [&](std::size_t idx) {
        const double delta = std::abs(m_data[idx].time.mjd() - t.mjd());
        if (delta <= toleranceMjd && delta < bestDelta) {
            bestDelta = delta;
            bestIdx   = idx;
        }
    };

    if (it != m_data.cend()) {
        checkCandidate(static_cast<std::size_t>(it - m_data.cbegin()));
    }
    if (it != m_data.cbegin()) {
        checkCandidate(static_cast<std::size_t>(it - m_data.cbegin()) - 1u);
    }

    if (bestIdx == m_data.size()) {
        return std::nullopt;
    }
    return bestIdx;
}

const Observation& TimeSeries::atTime(
    const TimeStamp& t, double toleranceMjd) const
{
    const auto idx = indexOf(t, toleranceMjd);
    if (!idx) {
        std::ostringstream oss;
        oss << "TimeSeries::atTime: no observation found near "
            << t.utcString()
            << " within tolerance " << toleranceMjd << " MJD.";
        throw DataException(oss.str());
    }
    return m_data[*idx];
}

// ─────────────────────────────────────────────────────────────────────────────
//  Slicing
// ─────────────────────────────────────────────────────────────────────────────

TimeSeries TimeSeries::slice(const TimeStamp& from, const TimeStamp& to) const
{
    _requireSorted("slice");

    if (to < from) {
        throw DataException("TimeSeries::slice: 'from' timestamp is after 'to'.");
    }

    // Find the first element >= from.
    const auto first = std::lower_bound(
        m_data.cbegin(), m_data.cend(), from,
        [](const Observation& obs, const TimeStamp& t) {
            return obs.time < t;
        });

    // Find the first element > to (upper_bound).
    const auto last = std::upper_bound(
        first, m_data.cend(), to,
        [](const TimeStamp& t, const Observation& obs) {
            return t < obs.time;
        });

    TimeSeries result(m_metadata);
    result.m_data.assign(first, last);
    result.m_isSorted = true; // slice of a sorted series is sorted
    return result;
}

TimeSeries TimeSeries::slice(std::size_t from, std::size_t to) const
{
    if (from > to) {
        throw DataException(
            "TimeSeries::slice: 'from' index (" + std::to_string(from) +
            ") is greater than 'to' index (" + std::to_string(to) + ").");
    }
    if (to > m_data.size()) {
        throw DataException(
            "TimeSeries::slice: 'to' index (" + std::to_string(to) +
            ") exceeds series size (" + std::to_string(m_data.size()) + ").");
    }

    TimeSeries result(m_metadata);
    result.m_data.assign(m_data.cbegin() + static_cast<std::ptrdiff_t>(from),
                         m_data.cbegin() + static_cast<std::ptrdiff_t>(to));
    // Preserve sorted status only if the parent was sorted.
    result.m_isSorted = m_isSorted;
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Time extent
// ─────────────────────────────────────────────────────────────────────────────

const TimeStamp& TimeSeries::startTime() const
{
    if (m_data.empty()) {
        throw DataException("TimeSeries::startTime: series is empty.");
    }
    return m_data.front().time;
}

const TimeStamp& TimeSeries::endTime() const
{
    if (m_data.empty()) {
        throw DataException("TimeSeries::endTime: series is empty.");
    }
    return m_data.back().time;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Sorting
// ─────────────────────────────────────────────────────────────────────────────

bool TimeSeries::isSorted() const noexcept
{
    return m_isSorted;
}

void TimeSeries::sortByTime()
{
    // Stable sort preserves relative order of observations with equal timestamps
    // (e.g. duplicate epochs from different receivers).
    std::stable_sort(m_data.begin(), m_data.end(),
        [](const Observation& a, const Observation& b) {
            return a.time < b.time;
        });
    m_isSorted = true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Metadata
// ─────────────────────────────────────────────────────────────────────────────

const SeriesMetadata& TimeSeries::metadata() const noexcept
{
    return m_metadata;
}

void TimeSeries::setMetadata(SeriesMetadata metadata)
{
    m_metadata = std::move(metadata);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

void TimeSeries::_requireSorted(const char* callerName) const
{
    if (!m_isSorted) {
        throw AlgorithmException(
            std::string("TimeSeries::") + callerName +
            ": series is not sorted by time. Call sortByTime() first.");
    }
}

} // namespace loki
