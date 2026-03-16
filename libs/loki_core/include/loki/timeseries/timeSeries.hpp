#pragma once

#include "loki/timeseries/timeStamp.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace loki {

// ─────────────────────────────────────────────────────────────────────────────
//  SeriesMetadata
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Descriptive metadata attached to a TimeSeries.
 */
struct SeriesMetadata {
    std::string stationId;      ///< Station or sensor identifier (e.g. "GRAZ").
    std::string componentName;  ///< Physical component (e.g. "dN", "temperature").
    std::string unit;           ///< Physical unit string (e.g. "mm", "°C", "hPa").
    std::string description;    ///< Free-form description.
};

// ─────────────────────────────────────────────────────────────────────────────
//  Observation
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief A single sample in a time series: a timestamp, a value, and a quality flag.
 *
 * A value of NaN indicates a missing or invalid observation. Use the free
 * function isValid() to test this without comparing against NaN directly.
 *
 * Flag semantics are defined by loki_qc. The default value 0 means "OK / unchecked".
 *
 * Future extension note: an auxiliary channel map (sigma, dE, dN, …) may be
 * added here without breaking the existing interface.
 */
struct Observation {
    TimeStamp time;
    double    value{0.0};
    uint8_t   flag{0};
};

/**
 * @brief Returns true if the observation holds a valid (non-NaN) value.
 * @param obs Observation to test.
 */
[[nodiscard]] inline bool isValid(const Observation& obs) noexcept
{
    // NaN is the only value that compares unequal to itself.
    return obs.value == obs.value;
}

// ─────────────────────────────────────────────────────────────────────────────
//  TimeSeries
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief An ordered sequence of (TimeStamp, value, flag) observations.
 *
 * Observations are stored in a contiguous std::vector for cache-efficient
 * iteration. The series tracks a sorted flag lazily: appending in chronological
 * order keeps the flag set at no extra cost; out-of-order appends clear it.
 *
 * Methods that rely on binary search (indexOf, atTime, slice by TimeStamp)
 * require the series to be sorted. If it is not, they throw AlgorithmException.
 * Call sortByTime() explicitly before using these methods.
 *
 * Usage example:
 * @code
 *   TimeSeries ts({ "GRAZ", "dN", "mm", "North component" });
 *   ts.reserve(1000);
 *   ts.append(TimeStamp(2020, 1, 1), 0.5);
 *   ts.append(TimeStamp(2020, 1, 2), 0.7);
 *
 *   auto idx = ts.indexOf(TimeStamp(2020, 1, 1));
 *   if (idx) std::cout << ts[*idx].value;
 * @endcode
 */
class TimeSeries {
public:

    // ── Construction ──────────────────────────────────────────────────────────

    /// @brief Constructs an empty TimeSeries with default metadata.
    TimeSeries() = default;

    /**
     * @brief Constructs an empty TimeSeries with the given metadata.
     * @param metadata Descriptive information about this series.
     */
    explicit TimeSeries(SeriesMetadata metadata);

    // ── Population ────────────────────────────────────────────────────────────

    /**
     * @brief Appends a new observation to the end of the series.
     *
     * If the new timestamp is earlier than the last stored timestamp,
     * the internal sorted flag is cleared. The observation is always appended
     * regardless of order.
     *
     * @param time  Timestamp of the observation.
     * @param value Measured value (NaN represents a missing observation).
     * @param flag  Quality flag; default 0 = OK / unchecked.
     */
    void append(const TimeStamp& time, double value, uint8_t flag = 0);

    /**
     * @brief Reserves capacity for at least n observations.
     *
     * Call before bulk-appending to avoid repeated reallocations.
     * @param n Expected number of observations.
     */
    void reserve(std::size_t n);

    // ── Element access ────────────────────────────────────────────────────────

    /**
     * @brief Returns the observation at position index (no bounds check).
     * @param index Zero-based position.
     * @return Const reference to the Observation.
     */
    const Observation& operator[](std::size_t index) const noexcept;

    /**
     * @brief Returns the observation at position index with bounds checking.
     * @param index Zero-based position.
     * @return Const reference to the Observation.
     * @throws DataException if index >= size().
     */
    const Observation& at(std::size_t index) const;

    /// @brief Returns the number of observations in the series.
    [[nodiscard]] std::size_t size()  const noexcept;

    /// @brief Returns true if the series contains no observations.
    [[nodiscard]] bool        empty() const noexcept;

    // ── Time-based access ─────────────────────────────────────────────────────

    /**
     * @brief Finds the index of the observation closest to time t.
     *
     * Uses binary search — requires the series to be sorted.
     *
     * @param t            Target timestamp.
     * @param toleranceMjd Maximum allowed MJD difference (~1 ms default).
     * @return Index wrapped in std::optional, or std::nullopt if no observation
     *         falls within the tolerance.
     * @throws AlgorithmException if the series is not sorted.
     */
    [[nodiscard]] std::optional<std::size_t> indexOf(
        const TimeStamp& t,
        double toleranceMjd = 1e-8) const;

    /**
     * @brief Returns the observation closest to time t.
     *
     * Uses binary search — requires the series to be sorted.
     *
     * @param t            Target timestamp.
     * @param toleranceMjd Maximum allowed MJD difference (~1 ms default).
     * @return Const reference to the matching Observation.
     * @throws AlgorithmException if the series is not sorted.
     * @throws DataException if no observation falls within the tolerance.
     */
    const Observation& atTime(
        const TimeStamp& t,
        double toleranceMjd = 1e-8) const;

    // ── Slicing ───────────────────────────────────────────────────────────────

    /**
     * @brief Returns a new TimeSeries containing observations in [from, to].
     *
     * Endpoints are inclusive. Uses binary search — requires sorted series.
     * The returned series inherits the metadata of the original.
     *
     * @param from Start of the time interval.
     * @param to   End of the time interval.
     * @return New TimeSeries with copied observations in the range.
     * @throws AlgorithmException if the series is not sorted.
     * @throws DataException if from > to.
     */
    [[nodiscard]] TimeSeries slice(const TimeStamp& from, const TimeStamp& to) const;

    /**
     * @brief Returns a new TimeSeries containing observations at indices [from, to).
     *
     * Half-open range: includes index from, excludes index to.
     * Does not require a sorted series.
     *
     * @param from First index (inclusive).
     * @param to   One-past-last index (exclusive).
     * @return New TimeSeries with copied observations.
     * @throws DataException if from > to or to > size().
     */
    [[nodiscard]] TimeSeries slice(std::size_t from, std::size_t to) const;

    // ── Iteration (read-only) ─────────────────────────────────────────────────

    /// @brief Returns a const iterator to the first observation.
    [[nodiscard]] auto begin() const noexcept { return m_data.cbegin(); }

    /// @brief Returns a const iterator past the last observation.
    [[nodiscard]] auto end()   const noexcept { return m_data.cend();   }

    // ── Time extent ───────────────────────────────────────────────────────────

    /**
     * @brief Returns the timestamp of the first observation.
     * @throws DataException if the series is empty.
     */
    [[nodiscard]] const TimeStamp& startTime() const;

    /**
     * @brief Returns the timestamp of the last observation.
     * @throws DataException if the series is empty.
     */
    [[nodiscard]] const TimeStamp& endTime() const;

    // ── Sorting ───────────────────────────────────────────────────────────────

    /// @brief Returns true if the series is known to be sorted by timestamp.
    [[nodiscard]] bool isSorted() const noexcept;

    /**
     * @brief Sorts all observations by timestamp in ascending order.
     *
     * Stable sort is used to preserve the relative order of observations
     * with equal timestamps. Sets the internal sorted flag upon completion.
     */
    void sortByTime();

    // ── Metadata ──────────────────────────────────────────────────────────────

    /// @brief Returns the series metadata.
    [[nodiscard]] const SeriesMetadata& metadata() const noexcept;

    /// @brief Replaces the series metadata.
    void setMetadata(SeriesMetadata metadata);

private:

    std::vector<Observation> m_data;
    SeriesMetadata           m_metadata;
    bool                     m_isSorted{true};

    // ── Internal helpers ──────────────────────────────────────────────────────

    /**
     * @brief Throws AlgorithmException if the series is not sorted.
     * @param callerName Name of the calling method, included in the error message.
     */
    void _requireSorted(const char* callerName) const;
};

} // namespace loki
