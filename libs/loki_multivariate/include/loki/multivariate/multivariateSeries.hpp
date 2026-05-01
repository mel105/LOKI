#pragma once

#include "loki/timeseries/timeStamp.hpp"

#include <Eigen/Dense>

#include <stdexcept>
#include <string>
#include <vector>

namespace loki::multivariate {

/**
 * @brief Synchronised multivariate data matrix with timestamps and channel names.
 *
 * Stores N observations of P channels in a column-major Eigen matrix.
 * Row i corresponds to timestamp[i]; column j corresponds to channelName[j].
 *
 * All timestamps are guaranteed to be strictly increasing (sorted).
 * All values have been gap-filled and optionally standardised by
 * MultivariateAssembler before this object is constructed.
 *
 * Layout:
 *   data(i, j)  -- observation at time i for channel j
 *   rows() == nObs()
 *   cols() == nChannels()
 */
class MultivariateSeries {
public:

    // -------------------------------------------------------------------------
    //  Construction
    // -------------------------------------------------------------------------

    /**
     * @brief Default constructor -- produces an empty series.
     */
    MultivariateSeries() = default;

    /**
     * @brief Constructs a MultivariateSeries from pre-assembled components.
     *
     * @param data         Matrix of shape (nObs x nChannels). Must not be empty.
     * @param timestamps   Vector of nObs sorted timestamps.
     * @param channelNames Vector of nChannels channel labels.
     * @throws std::invalid_argument if dimensions are inconsistent or data is empty.
     */
    MultivariateSeries(Eigen::MatrixXd            data,
                       std::vector<TimeStamp>     timestamps,
                       std::vector<std::string>   channelNames);

    // -------------------------------------------------------------------------
    //  Accessors
    // -------------------------------------------------------------------------

    /// @brief Returns the full data matrix (nObs x nChannels), read-only.
    [[nodiscard]] const Eigen::MatrixXd& data() const noexcept { return m_data; }

    /// @brief Returns the full data matrix (nObs x nChannels), mutable.
    [[nodiscard]] Eigen::MatrixXd& data() noexcept { return m_data; }

    /// @brief Returns the vector of timestamps, one per row.
    [[nodiscard]] const std::vector<TimeStamp>& timestamps() const noexcept
    { return m_timestamps; }

    /// @brief Returns the vector of channel names, one per column.
    [[nodiscard]] const std::vector<std::string>& channelNames() const noexcept
    { return m_channelNames; }

    /// @brief Number of observations (rows).
    [[nodiscard]] Eigen::Index nObs() const noexcept { return m_data.rows(); }

    /// @brief Number of channels (columns).
    [[nodiscard]] Eigen::Index nChannels() const noexcept { return m_data.cols(); }

    /// @brief Returns true if the series contains no data.
    [[nodiscard]] bool empty() const noexcept { return m_data.size() == 0; }

    /**
     * @brief Returns the column vector for channel j (0-based index).
     * @throws std::out_of_range if j is out of bounds.
     */
    [[nodiscard]] Eigen::VectorXd channel(Eigen::Index j) const;

    /**
     * @brief Returns the name of channel j (0-based index).
     * @throws std::out_of_range if j is out of bounds.
     */
    [[nodiscard]] const std::string& channelName(Eigen::Index j) const;

    /**
     * @brief Returns the timestamp for observation i (0-based index).
     * @throws std::out_of_range if i is out of bounds.
     */
    [[nodiscard]] const TimeStamp& timestamp(Eigen::Index i) const;

    // -------------------------------------------------------------------------
    //  Convenience
    // -------------------------------------------------------------------------

    /**
     * @brief Returns the time axis as a vector of MJD values.
     *
     * Convenient for plotting and numerical lag computations.
     */
    [[nodiscard]] std::vector<double> mjdAxis() const;

private:

    Eigen::MatrixXd          m_data;         ///< (nObs x nChannels) data matrix.
    std::vector<TimeStamp>   m_timestamps;   ///< One TimeStamp per row.
    std::vector<std::string> m_channelNames; ///< One label per column.
};

} // namespace loki::multivariate
