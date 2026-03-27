#pragma once

#include <loki/regression/regressionResult.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <string>
#include <vector>

namespace loki::regression {

/**
 * @brief Abstract base class for all regression models.
 *
 * Concrete regressors (LinearRegressor, PolynomialRegressor, etc.) inherit
 * from this class and implement fit(), predict(), and name().
 *
 * Convention: the x-axis used internally is always x = mjd - tRef, where
 * tRef is the MJD of the first valid observation in the input TimeSeries.
 * This is transparent to the caller -- fit() accepts a raw TimeSeries and
 * stores tRef in the returned RegressionResult.
 */
class Regressor {
public:

    virtual ~Regressor() = default;

    /**
     * @brief Fits the model to the given time series.
     *
     * NaN observations are silently skipped. The series must have at least
     * nParams + 1 valid observations.
     *
     * @param ts Input time series.
     * @return   Fully populated RegressionResult.
     * @throws DataException      if ts has too few valid observations.
     * @throws AlgorithmException on numerical failure (singular matrix, etc.).
     */
    virtual RegressionResult fit(const TimeSeries& ts) = 0;

    /**
     * @brief Computes predictions and intervals at the given x locations.
     *
     * Must be called after fit(). x values must use the same reference as
     * the fit: x = mjd - tRef. Uses the t-distribution with dof degrees of
     * freedom and the confidence level from RegressionConfig.
     *
     * @param xNew x values in days relative to tRef (= mjd - tRef).
     * @return     Vector of PredictionPoint, one per element of xNew.
     * @throws AlgorithmException if called before fit().
     */
    virtual std::vector<PredictionPoint> predict(const std::vector<double>& xNew) const = 0;

    /**
     * @brief Returns a human-readable model name (e.g. "LinearRegressor").
     */
    virtual std::string name() const = 0;
};

} // namespace loki::regression
