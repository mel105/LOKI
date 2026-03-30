#pragma once

#include <loki/regression/regressionResult.hpp>

#include <Eigen/Dense>

#include <vector>

namespace loki::regression::detail {

/**
 * @brief Computes R^2, adjusted R^2, AIC and BIC and writes them into result.
 *
 * Uses MLE sigma^2 = ssRes/n for AIC/BIC (standard convention for model
 * comparison). The unbiased sigma0 = sqrt(ssRes/dof) is already in result.
 *
 * @param result   RegressionResult to populate (dof must already be set).
 * @param l        Observation vector (m x 1).
 * @param nParams  Number of model parameters.
 */
void computeGoodnessOfFit(RegressionResult&      result,
                           const Eigen::VectorXd& l,
                           int                    nParams);

/**
 * @brief Computes confidence and prediction intervals for a set of new points.
 *
 * @param result     Fitted RegressionResult (sigma0, cofactorX, dof must be set).
 * @param aNew       Design matrix for new points (k x nParams).
 * @param xNew       x values (mjd - tRef) corresponding to rows of aNew.
 *                   Used to populate PredictionPoint::x correctly for all
 *                   model types (linear, harmonic, polynomial, etc.).
 * @param confLevel  Confidence level in (0, 1), e.g. 0.95.
 * @return           Vector of PredictionPoint, one per row of aNew.
 * @throws AlgorithmException if dof <= 0 or xNew.size() != aNew.rows().
 */
std::vector<PredictionPoint> computeIntervals(
    const RegressionResult&    result,
    const Eigen::MatrixXd&     aNew,
    const std::vector<double>& xNew,
    double                     confLevel);

} // namespace loki::regression::detail