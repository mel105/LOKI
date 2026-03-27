#include <loki/stats/metrics.hpp>
#include <loki/core/exceptions.hpp>

#include <cmath>
#include <limits>

using namespace loki;

namespace {

// Filter NaN pairs and return clean (observed, predicted) vectors.
std::pair<std::vector<double>, std::vector<double>> preparePairs(
    const std::vector<double>& observed,
    const std::vector<double>& predicted,
    NanPolicy                  policy,
    const std::string&         caller)
{
    if (observed.size() != predicted.size()) {
        throw DataException(
            caller + ": observed and predicted vectors must have the same size ("
            + std::to_string(observed.size()) + " vs "
            + std::to_string(predicted.size()) + ").");
    }

    std::vector<double> obs, pred;
    obs.reserve(observed.size());
    pred.reserve(predicted.size());

    for (std::size_t i = 0; i < observed.size(); ++i) {
        const bool nanObs  = std::isnan(observed[i]);
        const bool nanPred = std::isnan(predicted[i]);

        if (nanObs || nanPred) {
            if (policy == NanPolicy::THROW) {
                throw MissingValueException(
                    caller + ": NaN encountered at index " +
                    std::to_string(i) + " with NanPolicy::THROW.");
            }
            if (policy == NanPolicy::PROPAGATE) {
                // Signal propagation by returning empty vectors.
                return {{}, {}};
            }
            // NanPolicy::SKIP -- omit this pair.
            continue;
        }
        obs.push_back(observed[i]);
        pred.push_back(predicted[i]);
    }

    if (obs.size() < 2) {
        throw DataException(
            caller + ": need at least 2 valid pairs, got "
            + std::to_string(obs.size()) + ".");
    }

    return {obs, pred};
}

} // anonymous namespace

namespace loki::stats {

Metrics computeMetrics(
    const std::vector<double>& observed,
    const std::vector<double>& predicted,
    NanPolicy                  policy)
{
    const auto [obs, pred] = preparePairs(observed, predicted, policy, "computeMetrics");

    if (obs.empty()) {
        // NanPolicy::PROPAGATE
        Metrics m;
        m.rmse = m.mae = m.bias = m.mape =
            std::numeric_limits<double>::quiet_NaN();
        return m;
    }

    const double nd = static_cast<double>(obs.size());
    double sumSq   = 0.0;
    double sumAbs  = 0.0;
    double sumErr  = 0.0;
    double sumMape = 0.0;
    bool   mapeOk  = true;

    for (std::size_t i = 0; i < obs.size(); ++i) {
        const double err = pred[i] - obs[i];
        sumSq  += err * err;
        sumAbs += std::fabs(err);
        sumErr += err;

        if (mapeOk) {
            if (std::fabs(obs[i]) < std::numeric_limits<double>::epsilon()) {
                mapeOk = false;
            } else {
                sumMape += std::fabs(err / obs[i]);
            }
        }
    }

    Metrics m;
    m.n    = static_cast<int>(obs.size());
    m.rmse = std::sqrt(sumSq  / nd);
    m.mae  = sumAbs / nd;
    m.bias = sumErr / nd;
    m.mape = mapeOk ? (sumMape / nd * 100.0)
                    : std::numeric_limits<double>::quiet_NaN();
    return m;
}

double rmse(
    const std::vector<double>& observed,
    const std::vector<double>& predicted,
    NanPolicy                  policy)
{
    const auto [obs, pred] = preparePairs(observed, predicted, policy, "rmse");
    if (obs.empty()) return std::numeric_limits<double>::quiet_NaN();

    double sumSq = 0.0;
    for (std::size_t i = 0; i < obs.size(); ++i) {
        const double e = pred[i] - obs[i];
        sumSq += e * e;
    }
    return std::sqrt(sumSq / static_cast<double>(obs.size()));
}

double mae(
    const std::vector<double>& observed,
    const std::vector<double>& predicted,
    NanPolicy                  policy)
{
    const auto [obs, pred] = preparePairs(observed, predicted, policy, "mae");
    if (obs.empty()) return std::numeric_limits<double>::quiet_NaN();

    double sumAbs = 0.0;
    for (std::size_t i = 0; i < obs.size(); ++i)
        sumAbs += std::fabs(pred[i] - obs[i]);
    return sumAbs / static_cast<double>(obs.size());
}

double bias(
    const std::vector<double>& observed,
    const std::vector<double>& predicted,
    NanPolicy                  policy)
{
    const auto [obs, pred] = preparePairs(observed, predicted, policy, "bias");
    if (obs.empty()) return std::numeric_limits<double>::quiet_NaN();

    double sumErr = 0.0;
    for (std::size_t i = 0; i < obs.size(); ++i)
        sumErr += pred[i] - obs[i];
    return sumErr / static_cast<double>(obs.size());
}

} // namespace loki::stats
