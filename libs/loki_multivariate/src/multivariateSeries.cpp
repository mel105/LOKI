#include "loki/multivariate/multivariateSeries.hpp"

#include <stdexcept>

using namespace loki::multivariate;

// -----------------------------------------------------------------------------
//  Construction
// -----------------------------------------------------------------------------

MultivariateSeries::MultivariateSeries(Eigen::MatrixXd          data,
                                       std::vector<TimeStamp>   timestamps,
                                       std::vector<std::string> channelNames)
    : m_data        (std::move(data))
    , m_timestamps  (std::move(timestamps))
    , m_channelNames(std::move(channelNames))
{
    if (m_data.size() == 0) {
        throw std::invalid_argument(
            "MultivariateSeries: data matrix must not be empty.");
    }
    if (static_cast<Eigen::Index>(m_timestamps.size()) != m_data.rows()) {
        throw std::invalid_argument(
            "MultivariateSeries: timestamps.size() must equal data.rows().");
    }
    if (static_cast<Eigen::Index>(m_channelNames.size()) != m_data.cols()) {
        throw std::invalid_argument(
            "MultivariateSeries: channelNames.size() must equal data.cols().");
    }
}

// -----------------------------------------------------------------------------
//  Accessors
// -----------------------------------------------------------------------------

Eigen::VectorXd MultivariateSeries::channel(Eigen::Index j) const
{
    if (j < 0 || j >= m_data.cols()) {
        throw std::out_of_range(
            "MultivariateSeries::channel(): index " + std::to_string(j)
            + " out of range [0, " + std::to_string(m_data.cols()) + ").");
    }
    return m_data.col(j);
}

const std::string& MultivariateSeries::channelName(Eigen::Index j) const
{
    if (j < 0 || j >= static_cast<Eigen::Index>(m_channelNames.size())) {
        throw std::out_of_range(
            "MultivariateSeries::channelName(): index " + std::to_string(j)
            + " out of range.");
    }
    return m_channelNames[static_cast<std::size_t>(j)];
}

const TimeStamp& MultivariateSeries::timestamp(Eigen::Index i) const
{
    if (i < 0 || i >= static_cast<Eigen::Index>(m_timestamps.size())) {
        throw std::out_of_range(
            "MultivariateSeries::timestamp(): index " + std::to_string(i)
            + " out of range.");
    }
    return m_timestamps[static_cast<std::size_t>(i)];
}

std::vector<double> MultivariateSeries::mjdAxis() const
{
    std::vector<double> mjd;
    mjd.reserve(static_cast<std::size_t>(m_data.rows()));
    for (const auto& ts : m_timestamps) {
        mjd.push_back(ts.mjd());
    }
    return mjd;
}
