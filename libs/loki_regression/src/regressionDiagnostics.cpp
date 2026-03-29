#include <loki/regression/regressionDiagnostics.hpp>
#include <loki/math/hatMatrix.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/stats/distributions.hpp>

#include <cmath>

using namespace loki;
using namespace loki::regression;

// -----------------------------------------------------------------------------
//  Construction
// -----------------------------------------------------------------------------

RegressionDiagnostics::RegressionDiagnostics(double significanceLevel)
    : m_significanceLevel(significanceLevel)
{}

// -----------------------------------------------------------------------------
//  computeAnova()
// -----------------------------------------------------------------------------

AnovaTable RegressionDiagnostics::computeAnova(const RegressionResult& result) const
{
    const Eigen::VectorXd& e = result.residuals;
    const int n = static_cast<int>(e.size());

    if (n == 0) {
        throw DataException(
            "RegressionDiagnostics::computeAnova(): residuals vector is empty.");
    }

    const int nParam       = static_cast<int>(result.coefficients.size());
    const int dfError      = result.dof;
    const int dfRegression = nParam - 1;
    const int dfTotal      = dfError + dfRegression;

    if (dfError <= 0) {
        throw AlgorithmException(
            "RegressionDiagnostics::computeAnova(): non-positive error degrees of "
            "freedom (" + std::to_string(dfError) + ").");
    }
    if (dfRegression <= 0) {
        throw AlgorithmException(
            "RegressionDiagnostics::computeAnova(): model has only an intercept "
            "-- F-test not defined.");
    }

    // SSE from sigma0 and dof -- consistent with the fit.
    const double sse = result.sigma0 * result.sigma0 * static_cast<double>(dfError);

    // Recover observed values y_i = fitted_i + e_i, then compute y_bar and SST.
    double ySum = 0.0;
    for (std::size_t i = 0; i < result.fitted.size(); ++i)
        ySum += result.fitted[i].value + e[static_cast<int>(i)];
    const double yBar = ySum / static_cast<double>(n);

    double sst = 0.0;
    for (std::size_t i = 0; i < static_cast<std::size_t>(n); ++i) {
        const double yi = result.fitted[i].value + e[static_cast<int>(i)];
        sst += (yi - yBar) * (yi - yBar);
    }

    const double ssr = sst - sse;
    const double msr = ssr / static_cast<double>(dfRegression);
    const double mse = sse / static_cast<double>(dfError);
    const double fStat = (mse > 0.0) ? (msr / mse) : 0.0;

    const double pVal = (fStat > 0.0)
        ? (1.0 - stats::fCdf(fStat,
                             static_cast<double>(dfRegression),
                             static_cast<double>(dfError)))
        : 1.0;

    AnovaTable table;
    table.ssr          = ssr;
    table.sse          = sse;
    table.sst          = sst;
    table.fStatistic   = fStat;
    table.pValue       = pVal;
    table.dfRegression = dfRegression;
    table.dfError      = dfError;
    table.dfTotal      = dfTotal;
    return table;
}

// -----------------------------------------------------------------------------
//  computeInfluence()
// -----------------------------------------------------------------------------

InfluenceMeasures RegressionDiagnostics::computeInfluence(
    const RegressionResult& result) const
{
    if (result.designMatrix.rows() == 0) {
        throw DataException(
            "RegressionDiagnostics::computeInfluence(): designMatrix is empty. "
            "Ensure all regressors store X in RegressionResult after fit().");
    }

    const HatMatrix         hm(result.designMatrix);
    const Eigen::VectorXd&  h      = hm.leverages();
    const Eigen::VectorXd&  e      = result.residuals;
    const int               n      = hm.n();
    const int               p      = hm.rankP();
    const double            sigma0 = result.sigma0;

    Eigen::VectorXd stdRes(n);
    Eigen::VectorXd cooks(n);

    for (int i = 0; i < n; ++i) {
        const double hii   = h[i];
        const double denom = 1.0 - hii;

        if (denom < 1e-10 || sigma0 < 1e-15) {
            // Perfect leverage point -- skip to avoid division by zero.
            stdRes[i] = 0.0;
            cooks[i]  = 0.0;
        } else {
            stdRes[i] = e[i] / (sigma0 * std::sqrt(denom));
            // D_i = (e_i^2 * h_ii) / (p * sigma0^2 * (1 - h_ii)^2)
            cooks[i] = (e[i] * e[i] * hii)
                     / (static_cast<double>(p) * sigma0 * sigma0 * denom * denom);
        }
    }

    InfluenceMeasures im;
    im.leverages             = h;
    im.standardizedResiduals = stdRes;
    im.cooksDistance         = cooks;
    im.leverageThreshold     = 2.0 * static_cast<double>(p) / static_cast<double>(n);
    im.cooksThreshold        = 4.0 / static_cast<double>(n);
    return im;
}

// -----------------------------------------------------------------------------
//  computeVif()
// -----------------------------------------------------------------------------

VifResult RegressionDiagnostics::computeVif(const RegressionResult& result) const
{
    const Eigen::MatrixXd& X = result.designMatrix;
    const int p = static_cast<int>(X.cols());

    if (p < 2) {
        throw DataException(
            "RegressionDiagnostics::computeVif(): design matrix must have at least "
            "2 columns (intercept + 1 predictor), got " + std::to_string(p) + ".");
    }

    // Work with predictor columns only (skip column 0 = intercept).
    const int nPred = p - 1;
    Eigen::VectorXd vifVals(nPred);

    for (int j = 0; j < nPred; ++j) {
        // Target: column j+1 of X.
        const Eigen::VectorXd y = X.col(j + 1);

        // Regressors: all other non-intercept columns plus a ones column (intercept).
        // Build matrix with intercept + all predictor columns except j+1.
        Eigen::MatrixXd Xaux(X.rows(), nPred);
        Xaux.col(0).setOnes(); // intercept for auxiliary regression
        int col = 1;
        for (int k = 1; k < p; ++k) {
            if (k == j + 1) continue;
            Xaux.col(col++) = X.col(k);
        }

        const double r2 = auxiliaryR2(Xaux, y);

        // Guard against perfect collinearity (R^2 = 1 -> VIF = inf).
        const double denom = 1.0 - r2;
        vifVals[j] = (denom < 1e-10) ? std::numeric_limits<double>::infinity()
                                      : (1.0 / denom);
    }

    VifResult vif;
    vif.vifValues = vifVals;
    vif.threshold = 10.0;
    for (int j = 0; j < nPred; ++j) {
        if (vifVals[j] > vif.threshold)
            vif.flaggedIndices.push_back(j);
    }
    return vif;
}

// -----------------------------------------------------------------------------
//  computeBreuschPagan()
// -----------------------------------------------------------------------------

BreuschPaganResult RegressionDiagnostics::computeBreuschPagan(
    const RegressionResult& result) const
{
    const Eigen::VectorXd& e = result.residuals;
    const int n = static_cast<int>(e.size());

    if (n == 0) {
        throw DataException(
            "RegressionDiagnostics::computeBreuschPagan(): residuals vector is empty.");
    }
    if (result.fitted.size() == 0) {
        throw DataException(
            "RegressionDiagnostics::computeBreuschPagan(): fitted series is empty.");
    }

    // Squared residuals as dependent variable.
    Eigen::VectorXd eSq(n);
    for (int i = 0; i < n; ++i)
        eSq[i] = e[i] * e[i];

    // Auxiliary regressors: intercept + fitted values.
    Eigen::MatrixXd Xaux(n, 2);
    Xaux.col(0).setOnes();
    for (int i = 0; i < n; ++i)
        Xaux(i, 1) = result.fitted[static_cast<std::size_t>(i)].value;

    const double r2aux  = auxiliaryR2(Xaux, eSq);
    const double lmStat = static_cast<double>(n) * r2aux;

    // LM ~ chi^2(1) under H0 (one auxiliary predictor beyond intercept).
    const double pVal = 1.0 - stats::chi2Cdf(lmStat, 1.0);

    BreuschPaganResult bp;
    bp.testStatistic = lmStat;
    bp.pValue        = pVal;
    bp.rejected      = (pVal < m_significanceLevel);
    return bp;
}

// -----------------------------------------------------------------------------
//  auxiliaryR2()  -- internal helper
// -----------------------------------------------------------------------------

double RegressionDiagnostics::auxiliaryR2(
    const Eigen::MatrixXd& X, const Eigen::VectorXd& y)
{
    // OLS via QR: b = (X^T X)^{-1} X^T y.
    const Eigen::VectorXd b = X.colPivHouseholderQr().solve(y);
    const Eigen::VectorXd yHat = X * b;

    const double yBar = y.mean();
    const double sst  = (y.array() - yBar).square().sum();
    const double sse  = (y - yHat).squaredNorm();

    if (sst < 1e-15) return 0.0;
    return std::max(0.0, 1.0 - sse / sst);
}
