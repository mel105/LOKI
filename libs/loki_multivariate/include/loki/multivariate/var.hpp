#pragma once

#include "loki/multivariate/multivariateResult.hpp"
#include "loki/multivariate/multivariateSeries.hpp"
#include "loki/core/config.hpp"

namespace loki::multivariate {

/**
 * @brief Vector Autoregression (VAR) model with Granger causality testing.
 *
 * Fits a VAR(p) model to the multivariate series Y (n x q):
 *
 *   Y_t = A_1 * Y_{t-1} + ... + A_p * Y_{t-p} + epsilon_t
 *
 * where A_l are (q x q) coefficient matrices and epsilon_t is white noise
 * with covariance Sigma (q x q).
 *
 * Each equation is estimated by OLS independently (equation-by-equation OLS
 * is equivalent to GLS when all equations share the same regressors, which
 * is the case in a standard VAR). The design matrix for each equation is:
 *
 *   Z = [ Y_{t-1} | Y_{t-2} | ... | Y_{t-p} ]   (n-p) x (p*q)
 *
 * Lag order selection:
 *   Searches p in [1, maxOrder] and selects the order minimising the chosen
 *   information criterion evaluated on the residual covariance matrix:
 *
 *   AIC  = log|Sigma| + 2 * p * q^2 / T
 *   BIC  = log|Sigma| + log(T) * p * q^2 / T
 *   HQC  = log|Sigma| + 2 * log(log(T)) * p * q^2 / T
 *
 *   where T = n - p (effective sample size).
 *
 * Granger causality:
 *   For each ordered pair (from, to), tests H0: coefficients of channel
 *   'from' in the equation for channel 'to' are jointly zero.
 *   Uses an F-test on the restricted vs. unrestricted OLS residuals:
 *
 *   F = ((RSS_r - RSS_u) / p) / (RSS_u / (T - p*q - 1))
 *
 *   where RSS_r is from the model excluding lags of 'from' and RSS_u is
 *   the full model. p-value from F(p, T - p*q - 1) distribution.
 *
 * Reuses loki::math::buildLagMatrix() for constructing the regressor matrix.
 */
class Var {
public:

    /**
     * @brief Constructs a VAR engine with the given configuration.
     * @param cfg VAR configuration from AppConfig.
     */
    explicit Var(const MultivariateVarConfig& cfg);

    /**
     * @brief Fits the VAR model to the given multivariate series.
     *
     * @param data Synchronised multivariate series (n x q). Must be NaN-free.
     * @return     Fully populated VarResult.
     * @throws DataException      if data is empty or has fewer than 2 channels.
     * @throws AlgorithmException if n <= maxOrder * q (insufficient data).
     */
    [[nodiscard]] VarResult compute(const MultivariateSeries& data) const;

private:

    MultivariateVarConfig m_cfg;

    // -------------------------------------------------------------------------
    //  Private helpers
    // -------------------------------------------------------------------------

    /**
     * @brief Builds the lagged regressor matrix Z for VAR(p).
     *
     * Z has shape (n-p) x (p*q).
     * Row t of Z is [ Y_{t+p-1}^T, Y_{t+p-2}^T, ..., Y_t^T ] (stacked lags).
     *
     * @param Y     Data matrix (n x q), rows = time.
     * @param order Lag order p.
     * @return      Regressor matrix ((n-p) x (p*q)).
     */
    [[nodiscard]]
    static Eigen::MatrixXd _buildRegressors(const Eigen::MatrixXd& Y, int order);

    /**
     * @brief Fits VAR(order) by equation-by-equation OLS.
     *
     * @param Y     Data matrix (n x q).
     * @param order Lag order.
     * @return      Residual matrix ((n-order) x q).
     */
    [[nodiscard]]
    Eigen::MatrixXd _fitResiduals(const Eigen::MatrixXd& Y, int order) const;

    /**
     * @brief Selects lag order by minimising the information criterion.
     *
     * @param Y Data matrix (n x q).
     * @return  Selected order in [1, maxOrder].
     */
    [[nodiscard]]
    int _selectOrder(const Eigen::MatrixXd& Y) const;

    /**
     * @brief Evaluates the information criterion for a given residual covariance.
     *
     * @param sigma Residual covariance (q x q).
     * @param T     Effective sample size (n - order).
     * @param q     Number of channels.
     * @param order Current lag order.
     * @return      IC value.
     */
    [[nodiscard]]
    double _criterion(const Eigen::MatrixXd& sigma, int T, int q, int order) const;

    /**
     * @brief Performs the Granger F-test for one (from, to) pair.
     *
     * @param Y     Data matrix (n x q).
     * @param from  0-based index of the candidate causal channel.
     * @param to    0-based index of the target channel.
     * @param order Selected VAR lag order.
     * @return      Populated GrangerResult.
     */
    [[nodiscard]]
    GrangerResult _grangerTest(const Eigen::MatrixXd& Y,
                               const MultivariateSeries& data,
                               int from, int to, int order) const;

    /**
     * @brief Computes the F distribution p-value P(F(d1,d2) >= fStat).
     *
     * Uses a numerical incomplete beta function approximation.
     *
     * @param fStat  Observed F statistic.
     * @param d1     Numerator degrees of freedom.
     * @param d2     Denominator degrees of freedom.
     * @return       p-value in [0, 1].
     */
    [[nodiscard]]
    static double _fPvalue(double fStat, int d1, int d2);
};

} // namespace loki::multivariate
