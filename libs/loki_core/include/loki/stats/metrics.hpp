#pragma once

#include <loki/core/nanPolicy.hpp>

#include <vector>

namespace loki::stats {

/**
 * @brief Collection of error metrics comparing predicted and observed values.
 *
 * All metrics are computed from two parallel vectors: observed and predicted.
 * NaN handling is controlled by NanPolicy -- pairs where either value is NaN
 * are skipped (SKIP), cause an exception (THROW), or propagate NaN (PROPAGATE).
 */
struct Metrics {
    double rmse{0.0};   ///< Root Mean Squared Error: sqrt(mean((pred - obs)^2)).
    double mae{0.0};    ///< Mean Absolute Error: mean(|pred - obs|).
    double bias{0.0};   ///< Mean signed error: mean(pred - obs).
    double mape{0.0};   ///< Mean Absolute Percentage Error (%). NaN if any obs == 0.
    int    n{0};        ///< Number of valid pairs used in computation.
};

/**
 * @brief Computes RMSE, MAE, bias and MAPE from observed and predicted vectors.
 *
 * @param observed  Observed (reference) values.
 * @param predicted Predicted (modelled) values. Must be the same size as observed.
 * @param policy    NaN handling policy (default SKIP).
 * @return          Populated Metrics struct.
 * @throws DataException      if vectors have different sizes or fewer than 2
 *                             valid pairs remain after NaN filtering.
 * @throws MissingValueException if policy is THROW and a NaN is encountered.
 */
Metrics computeMetrics(
    const std::vector<double>& observed,
    const std::vector<double>& predicted,
    NanPolicy                  policy = NanPolicy::SKIP);

/**
 * @brief Computes RMSE only (faster than full computeMetrics).
 *
 * @param observed  Observed values.
 * @param predicted Predicted values. Must be the same size as observed.
 * @param policy    NaN handling policy (default SKIP).
 * @return          RMSE value.
 * @throws DataException if vectors differ in size or have fewer than 2 valid pairs.
 */
double rmse(
    const std::vector<double>& observed,
    const std::vector<double>& predicted,
    NanPolicy                  policy = NanPolicy::SKIP);

/**
 * @brief Computes MAE only.
 *
 * @param observed  Observed values.
 * @param predicted Predicted values. Must be the same size as observed.
 * @param policy    NaN handling policy (default SKIP).
 * @return          MAE value.
 * @throws DataException if vectors differ in size or have fewer than 2 valid pairs.
 */
double mae(
    const std::vector<double>& observed,
    const std::vector<double>& predicted,
    NanPolicy                  policy = NanPolicy::SKIP);

/**
 * @brief Computes bias (mean signed error) only.
 *
 * @param observed  Observed values.
 * @param predicted Predicted values. Must be the same size as observed.
 * @param policy    NaN handling policy (default SKIP).
 * @return          Bias value.
 * @throws DataException if vectors differ in size or have fewer than 2 valid pairs.
 */
double bias(
    const std::vector<double>& observed,
    const std::vector<double>& predicted,
    NanPolicy                  policy = NanPolicy::SKIP);

} // namespace loki::stats
