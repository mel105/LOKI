#include "loki/multivariate/var.hpp"

#include "loki/core/exceptions.hpp"
#include "loki/core/logger.hpp"

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

using namespace loki::multivariate;

// -----------------------------------------------------------------------------
//  Construction
// -----------------------------------------------------------------------------

Var::Var(const MultivariateVarConfig& cfg)
    : m_cfg(cfg)
{}

// -----------------------------------------------------------------------------
//  compute
// -----------------------------------------------------------------------------

VarResult Var::compute(const MultivariateSeries& data) const
{
    if (data.empty()) {
        throw DataException("Var::compute(): data series is empty.");
    }
    if (data.nChannels() < 2) {
        throw DataException(
            "Var::compute(): at least 2 channels required, got "
            + std::to_string(data.nChannels()) + ".");
    }

    const Eigen::MatrixXd& Y = data.data();
    const int n = static_cast<int>(Y.rows());
    const int q = static_cast<int>(Y.cols());

    if (n <= m_cfg.maxOrder * q + 1) {
        throw AlgorithmException(
            "Var::compute(): insufficient data. n=" + std::to_string(n)
            + " must exceed maxOrder*q+1="
            + std::to_string(m_cfg.maxOrder * q + 1) + ".");
    }

    // -- Step 1: select lag order ---------------------------------------------
    const int order = _selectOrder(Y);
    LOKI_INFO("Var: selected order=" + std::to_string(order)
              + " criterion=" + m_cfg.orderCriterion + ".");

    // -- Step 2: fit VAR(order) and get coefficient matrices ------------------
    const Eigen::MatrixXd Z = _buildRegressors(Y, order); // (n-order) x (order*q)
    const Eigen::MatrixXd Ydep = Y.bottomRows(n - order); // (n-order) x q

    // OLS per equation: B = (Z^T Z)^{-1} Z^T Ydep  (shape: order*q x q)
    // Use ColPivHouseholderQR for numerical stability.
    const Eigen::MatrixXd B =
        Z.colPivHouseholderQr().solve(Ydep); // (order*q x q)

    const Eigen::MatrixXd residuals = Ydep - Z * B; // (n-order) x q
    const int T = n - order;

    // Residual covariance (unbiased).
    const Eigen::MatrixXd sigma =
        (residuals.transpose() * residuals) / static_cast<double>(T - order * q - 1);

    // IC value at selected order.
    const double icVal = _criterion(sigma, T, q, order);

    // Extract coefficient matrices A_1..A_p.
    // B has shape (order*q x q). Columns of B^T are the equations.
    // B^T row l gives all coefficients for equation l.
    // Coefficients for lag s (1-based) occupy rows [(s-1)*q .. s*q-1] of B.
    std::vector<Eigen::MatrixXd> coefficients(static_cast<std::size_t>(order));
    for (int s = 0; s < order; ++s) {
        // A_{s+1} is a (q x q) matrix.
        // Row l of A_{s+1} = B.block(s*q, l, q, 1)^T  -- coefficients of
        // all channels at lag s+1 in the equation for channel l.
        // Equivalently: A_{s+1} = B.block(s*q, 0, q, q).transpose()
        // because B columns = target equations, rows = regressors.
        coefficients[static_cast<std::size_t>(s)] =
            B.block(s * q, 0, q, q).transpose(); // (q x q)
    }

    LOKI_INFO("Var: residual covariance trace="
              + std::to_string(sigma.trace()) + ".");

    // -- Step 3: Granger causality --------------------------------------------
    std::vector<GrangerResult> granger;

    if (m_cfg.granger) {
        LOKI_INFO("Var: running pairwise Granger F-tests ("
                  + std::to_string(q * (q - 1)) + " pairs).");
        for (int from = 0; from < q; ++from) {
            for (int to = 0; to < q; ++to) {
                if (from == to) continue;
                granger.push_back(_grangerTest(Y, data, from, to, order));
            }
        }
    }

    // -- Step 4: assemble result ----------------------------------------------
    VarResult result;
    result.order        = order;
    result.criterion    = m_cfg.orderCriterion;
    result.criterionVal = icVal;
    result.coefficients = std::move(coefficients);
    result.residuals    = residuals;
    result.sigma        = sigma;
    result.granger      = std::move(granger);
    result.nObs         = T;
    result.nChannels    = q;

    return result;
}

// -----------------------------------------------------------------------------
//  _buildRegressors
// -----------------------------------------------------------------------------

Eigen::MatrixXd Var::_buildRegressors(const Eigen::MatrixXd& Y, int order)
{
    const int n = static_cast<int>(Y.rows());
    const int q = static_cast<int>(Y.cols());
    const int T = n - order;

    // Z row t = [ Y[t+order-1]^T, Y[t+order-2]^T, ..., Y[t]^T ]
    // i.e. the most recent lag is first.
    Eigen::MatrixXd Z(T, order * q);

    for (int t = 0; t < T; ++t) {
        for (int s = 0; s < order; ++s) {
            // Lag s+1: row index in Y is t + order - 1 - s.
            Z.block(t, s * q, 1, q) = Y.row(t + order - 1 - s);
        }
    }

    return Z;
}

// -----------------------------------------------------------------------------
//  _fitResiduals
// -----------------------------------------------------------------------------

Eigen::MatrixXd Var::_fitResiduals(const Eigen::MatrixXd& Y, int order) const
{
    const int n = static_cast<int>(Y.rows());
    const Eigen::MatrixXd Z    = _buildRegressors(Y, order);
    const Eigen::MatrixXd Ydep = Y.bottomRows(n - order);
    const Eigen::MatrixXd B    = Z.colPivHouseholderQr().solve(Ydep);
    return Ydep - Z * B;
}

// -----------------------------------------------------------------------------
//  _selectOrder
// -----------------------------------------------------------------------------

int Var::_selectOrder(const Eigen::MatrixXd& Y) const
{
    const int n = static_cast<int>(Y.rows());
    const int q = static_cast<int>(Y.cols());

    double bestIC = std::numeric_limits<double>::max();
    int    bestP  = 1;

    for (int p = 1; p <= m_cfg.maxOrder; ++p) {
        const int T = n - p;
        if (T <= p * q + 1) break; // insufficient data for this order

        const Eigen::MatrixXd resid = _fitResiduals(Y, p);
        const Eigen::MatrixXd sig   =
            (resid.transpose() * resid) / static_cast<double>(T);

        const double ic = _criterion(sig, T, q, p);
        if (ic < bestIC) {
            bestIC = ic;
            bestP  = p;
        }
    }

    return bestP;
}

// -----------------------------------------------------------------------------
//  _criterion
// -----------------------------------------------------------------------------

double Var::_criterion(const Eigen::MatrixXd& sigma,
                       int T, int q, int order) const
{
    // log|Sigma| via LU determinant.
    const double logDet = std::log(std::abs(sigma.determinant()));

    const double penalty = static_cast<double>(order * q * q);
    const double Td      = static_cast<double>(T);

    if (m_cfg.orderCriterion == "bic") {
        return logDet + std::log(Td) * penalty / Td;
    }
    if (m_cfg.orderCriterion == "hqc") {
        return logDet + 2.0 * std::log(std::log(Td)) * penalty / Td;
    }
    // Default: "aic"
    return logDet + 2.0 * penalty / Td;
}

// -----------------------------------------------------------------------------
//  _grangerTest
// -----------------------------------------------------------------------------

GrangerResult Var::_grangerTest(const Eigen::MatrixXd& Y,
                                const MultivariateSeries& data,
                                int from, int to, int order) const
{
    const int n = static_cast<int>(Y.rows());
    const int q = static_cast<int>(Y.cols());
    const int T = n - order;

    // Unrestricted model: full VAR regressor Z (T x order*q).
    const Eigen::MatrixXd Z    = _buildRegressors(Y, order);
    const Eigen::VectorXd ytTo = Y.col(to).tail(T); // dependent: channel 'to'

    // OLS unrestricted.
    const Eigen::VectorXd Bu = Z.colPivHouseholderQr().solve(ytTo);
    const Eigen::VectorXd ru = ytTo - Z * Bu;
    const double          RSSu = ru.squaredNorm();

    // Restricted model: remove columns corresponding to 'from' at all lags.
    // Columns of Z for channel 'from' at lag s+1 are column s*q + from.
    const int nCols = order * q;
    std::vector<int> keepCols;
    keepCols.reserve(static_cast<std::size_t>(nCols - order));
    for (int col = 0; col < nCols; ++col) {
        // Column col belongs to channel (col % q) at lag (col / q + 1).
        if ((col % q) != from) {
            keepCols.push_back(col);
        }
    }

    const int nKeep = static_cast<int>(keepCols.size());
    Eigen::MatrixXd Zr(T, nKeep);
    for (int k = 0; k < nKeep; ++k) {
        Zr.col(k) = Z.col(keepCols[static_cast<std::size_t>(k)]);
    }

    const Eigen::VectorXd Br = Zr.colPivHouseholderQr().solve(ytTo);
    const Eigen::VectorXd rr = ytTo - Zr * Br;
    const double          RSSr = rr.squaredNorm();

    // F statistic: F = ((RSSr - RSSu) / order) / (RSSu / (T - order*q - 1))
    const int    df1   = order;
    const int    df2   = T - order * q - 1;
    const double fStat = (df2 > 0 && RSSu > 0.0)
        ? ((RSSr - RSSu) / static_cast<double>(df1))
          / (RSSu / static_cast<double>(df2))
        : 0.0;

    const double pVal = _fPvalue(fStat, df1, df2);

    GrangerResult gr;
    gr.fromChannel = from;
    gr.toChannel   = to;
    gr.fromName    = data.channelName(from);
    gr.toName      = data.channelName(to);
    gr.fStat       = fStat;
    gr.pValue      = pVal;
    gr.significant = (pVal < m_cfg.grangerSignificanceLevel);

    return gr;
}

// -----------------------------------------------------------------------------
//  _fPvalue  -- P(F(d1,d2) >= fStat) via regularised incomplete beta
// -----------------------------------------------------------------------------

double Var::_fPvalue(double fStat, int d1, int d2)
{
    if (fStat <= 0.0) return 1.0;
    if (d1 <= 0 || d2 <= 0) return 1.0;

    // x = d2 / (d2 + d1 * F)  transforms F(d1,d2) to Beta(d2/2, d1/2).
    const double x = static_cast<double>(d2)
                   / (static_cast<double>(d2) + static_cast<double>(d1) * fStat);

    // Regularised incomplete beta I_x(a, b) via continued fraction (Lentz).
    // P(F >= fStat) = I_x(d2/2, d1/2).
    const double a = static_cast<double>(d2) * 0.5;
    const double b = static_cast<double>(d1) * 0.5;

    // Log beta function via lgamma.
    const double logBeta = std::lgamma(a) + std::lgamma(b) - std::lgamma(a + b);

    // Continued fraction via modified Lentz algorithm (Numerical Recipes).
    // I_x(a,b) = x^a * (1-x)^b / (a * B(a,b)) * CF
    if (x < 0.0 || x > 1.0) return 1.0;
    if (x == 0.0) return 1.0;
    if (x == 1.0) return 0.0;

    // Use symmetry: I_x(a,b) = 1 - I_{1-x}(b,a) when x > (a+1)/(a+b+2).
    double aa = a, bb = b, xx = x;
    bool   swapped = false;
    if (xx > (aa + 1.0) / (aa + bb + 2.0)) {
        std::swap(aa, bb);
        xx      = 1.0 - xx;
        swapped = true;
    }

    // Front factor.
    const double front = std::exp(aa * std::log(xx) + bb * std::log(1.0 - xx) - logBeta) / aa;

    // Modified Lentz continued fraction.
    constexpr int    MAXITER = 200;
    constexpr double EPS     = 1.0e-12;
    constexpr double FPMIN   = 1.0e-300;

    double f = FPMIN, C = f, D = 0.0;
    for (int m2 = 0; m2 <= MAXITER * 2; ++m2) {
        double numerator;
        const int m = m2 / 2;
        if (m2 == 0) {
            numerator = 1.0;
        } else if (m2 % 2 == 0) {
            numerator = static_cast<double>(m) * (bb - static_cast<double>(m)) * xx
                      / ((aa + static_cast<double>(2 * m) - 1.0)
                         * (aa + static_cast<double>(2 * m)));
        } else {
            numerator = -(aa + static_cast<double>(m))
                       * (aa + bb + static_cast<double>(m)) * xx
                       / ((aa + static_cast<double>(2 * m))
                          * (aa + static_cast<double>(2 * m) + 1.0));
        }
        D = 1.0 + numerator * D;
        if (std::abs(D) < FPMIN) D = FPMIN;
        C = 1.0 + numerator / C;
        if (std::abs(C) < FPMIN) C = FPMIN;
        D = 1.0 / D;
        const double delta = C * D;
        f *= delta;
        if (std::abs(delta - 1.0) < EPS) break;
    }

    const double Ix = front * f;
    const double pval = swapped ? (1.0 - Ix) : Ix;

    return std::max(0.0, std::min(1.0, pval));
}
