#pragma once

#include <loki/regression/regressionResult.hpp>

#include <Eigen/Dense>

#include <vector>

namespace loki::regression {

/**
 * @brief ANOVA table for a regression model.
 *
 * Partitions total sum of squares SST = SSR + SSE, and provides the
 * F-statistic and associated p-value for the overall model significance test.
 */
struct AnovaTable {
    double ssr{0.0};        ///< Regression sum of squares (explained variation).
    double sse{0.0};        ///< Error sum of squares (residual variation).
    double sst{0.0};        ///< Total sum of squares (SSR + SSE).
    double fStatistic{0.0}; ///< F = (SSR / dfRegression) / (SSE / dfError).
    double pValue{1.0};     ///< P-value from F(dfRegression, dfError) distribution.
    int    dfRegression{0}; ///< Degrees of freedom for regression: nParams - 1.
    int    dfError{0};      ///< Degrees of freedom for error: nObs - nParams.
    int    dfTotal{0};      ///< Total degrees of freedom: nObs - 1.
};

/**
 * @brief Influence measures for individual observations.
 *
 * Computed from the hat matrix diagonal (leverages) and regression residuals.
 * All vectors have length equal to the number of observations used in fit().
 */
struct InfluenceMeasures {
    Eigen::VectorXd leverages;              ///< h_ii: diagonal of hat matrix H.
    Eigen::VectorXd standardizedResiduals;  ///< e_i / (sigma0 * sqrt(1 - h_ii)).
    Eigen::VectorXd cooksDistance;          ///< D_i: overall influence of observation i.
    double          leverageThreshold{0.0}; ///< 2p/n: flag h_ii above this.
    double          cooksThreshold{0.0};    ///< 4/n: flag D_i above this.
};

/**
 * @brief Variance inflation factors for multicollinearity detection.
 *
 * VIF_j = 1 / (1 - R^2_j), where R^2_j is the R^2 from regressing column j
 * of the design matrix on all other columns. Computed for each predictor
 * column except the intercept (column 0).
 *
 * Thresholds: VIF > 5 indicates moderate multicollinearity,
 *             VIF > 10 indicates severe multicollinearity.
 */
struct VifResult {
    Eigen::VectorXd  vifValues;         ///< VIF per predictor, length nParams - 1 (no intercept).
    double           threshold{10.0};   ///< Flag threshold (default 10).
    std::vector<int> flaggedIndices;    ///< Indices into vifValues where VIF > threshold.
};

/**
 * @brief Result of the Breusch-Pagan test for heteroscedasticity.
 *
 * Tests H0: residual variance is constant (homoscedastic).
 * Test statistic: n * R^2 from auxiliary OLS of squared residuals on fitted values.
 * Follows chi^2(1) under H0.
 */
struct BreuschPaganResult {
    double testStatistic{0.0}; ///< LM statistic: n * R^2_aux.
    double pValue{1.0};        ///< P-value from chi^2(1) distribution.
    bool   rejected{false};    ///< True if pValue < significanceLevel.
};

/**
 * @brief Computes ANOVA table and regression diagnostics for a RegressionResult.
 *
 * Consumes a RegressionResult produced by any Regressor::fit() call.
 * The result must contain a non-empty designMatrix field (set by all regressors).
 *
 * ANOVA:
 *   SST = sum (y_i - y_bar)^2
 *   SSR = SST - SSE
 *   SSE = sigma0^2 * dof
 *   F   = (SSR / dfR) / (SSE / dfE)
 *
 * Influence (Cook's distance):
 *   D_i = (e_i^2 * h_ii) / (p * sigma0^2 * (1 - h_ii)^2)
 *
 * Standardized residuals:
 *   r_i = e_i / (sigma0 * sqrt(1 - h_ii))
 *
 * VIF:
 *   VIF_j = 1 / (1 - R^2_j), R^2_j from auxiliary OLS of X_j on remaining columns.
 *
 * Breusch-Pagan:
 *   Auxiliary OLS of e_i^2 on fitted values y-hat_i (with intercept).
 *   LM = n * R^2_aux ~ chi^2(1) under H0.
 */
class RegressionDiagnostics {
public:
    /**
     * @brief Constructs diagnostics with the given significance level.
     * @param significanceLevel Alpha for F-test and Breusch-Pagan (default 0.05).
     */
    explicit RegressionDiagnostics(double significanceLevel = 0.05);

    /**
     * @brief Computes the ANOVA table from a regression result.
     * @param result Populated RegressionResult from any Regressor::fit().
     * @return AnovaTable with SSR, SSE, SST, F-statistic, p-value, and df values.
     * @throws DataException if result.residuals is empty.
     * @throws AlgorithmException if degrees of freedom are non-positive.
     */
    AnovaTable computeAnova(const RegressionResult& result) const;

    /**
     * @brief Computes leverage, standardized residuals, and Cook's distance.
     * @param result Populated RegressionResult including designMatrix field.
     * @return InfluenceMeasures with all vectors of length nObs.
     * @throws DataException if result.designMatrix is empty.
     */
    InfluenceMeasures computeInfluence(const RegressionResult& result) const;

    /**
     * @brief Computes variance inflation factors for each predictor column.
     *
     * Skips the intercept column (column 0). For each remaining column j,
     * fits an auxiliary OLS of X_j on all other non-intercept columns
     * and computes VIF_j = 1 / (1 - R^2_j).
     *
     * @param result Populated RegressionResult including designMatrix field.
     * @return VifResult with one VIF per predictor (nParams - 1 values).
     * @throws DataException if designMatrix has fewer than 2 columns.
     */
    VifResult computeVif(const RegressionResult& result) const;

    /**
     * @brief Performs the Breusch-Pagan test for heteroscedasticity.
     *
     * Auxiliary OLS: e_i^2 = a + b * y-hat_i + u_i.
     * LM statistic = n * R^2_aux, tested against chi^2(1).
     *
     * @param result Populated RegressionResult with residuals and fitted values.
     * @return BreuschPaganResult with test statistic, p-value, and rejection flag.
     * @throws DataException if residuals or fitted series is empty.
     */
    BreuschPaganResult computeBreuschPagan(const RegressionResult& result) const;

private:
    double m_significanceLevel;

    /// Internal OLS helper: solves min ||Xb - y||^2, returns R^2. No intercept added.
    static double auxiliaryR2(const Eigen::MatrixXd& X, const Eigen::VectorXd& y);
};

} // namespace loki::regression
