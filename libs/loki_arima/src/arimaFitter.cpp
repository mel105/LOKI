#include <loki/arima/arimaFitter.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/math/lagMatrix.hpp>
#include <loki/math/lsq.hpp>
#include <loki/stats/descriptive.hpp>

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <set>
#include <string>

using namespace loki;
using namespace loki::arima;

// ---------------------------------------------------------------------------
//  ArimaFitter -- construction
// ---------------------------------------------------------------------------

ArimaFitter::ArimaFitter(Config cfg)
    : m_cfg(std::move(cfg))
{}

// ---------------------------------------------------------------------------
//  arLags / maLags -- multiplicative SARIMA lag index generation
// ---------------------------------------------------------------------------

std::vector<int> ArimaFitter::arLags(int p, int P, int s)
{
    std::set<int> lags;

    if (s > 0 && P > 0) {
        // Multiplicative expansion: {i + j*s : i in 0..p, j in 0..P} \ {0}
        for (int i = 0; i <= p; ++i) {
            for (int j = 0; j <= P; ++j) {
                const int lag = i + j * s;
                if (lag > 0) { lags.insert(lag); }
            }
        }
    } else {
        // Non-seasonal: plain {1, ..., p}
        for (int i = 1; i <= p; ++i) { lags.insert(i); }
    }

    return std::vector<int>(lags.begin(), lags.end());  // sorted ascending
}

std::vector<int> ArimaFitter::maLags(int q, int Q, int s)
{
    std::set<int> lags;

    if (s > 0 && Q > 0) {
        for (int i = 0; i <= q; ++i) {
            for (int j = 0; j <= Q; ++j) {
                const int lag = i + j * s;
                if (lag > 0) { lags.insert(lag); }
            }
        }
    } else {
        for (int i = 1; i <= q; ++i) { lags.insert(i); }
    }

    return std::vector<int>(lags.begin(), lags.end());
}

// ---------------------------------------------------------------------------
//  applyDifferencing
// ---------------------------------------------------------------------------

std::vector<double> ArimaFitter::applyDifferencing(const std::vector<double>& y,
                                                     int d, int D, int s)
{
    std::vector<double> out = y;

    // Seasonal differencing first (standard Box-Jenkins convention)
    if (s > 0 && D > 0) {
        for (int pass = 0; pass < D; ++pass) {
            out = loki::stats::laggedDiff(out, s, loki::NanPolicy::THROW);
        }
    }

    // Then ordinary differencing
    for (int pass = 0; pass < d; ++pass) {
        out = loki::stats::diff(out, loki::NanPolicy::THROW);
    }

    return out;
}

// ---------------------------------------------------------------------------
//  computeInfoCriteria
// ---------------------------------------------------------------------------

void ArimaFitter::computeInfoCriteria(ArimaResult& result, int k)
{
    const double n      = static_cast<double>(result.n);
    const double sigma2 = result.sigma2;

    // CSS log-likelihood approximation (Gaussian assumption)
    result.logLik = -0.5 * n * (std::log(2.0 * std::numbers::pi * sigma2) + 1.0);
    result.aic    = -2.0 * result.logLik + 2.0 * static_cast<double>(k);
    result.bic    = -2.0 * result.logLik + static_cast<double>(k) * std::log(n);
}

// ---------------------------------------------------------------------------
//  fitCss  -- Hannan-Rissanen two-step OLS
// ---------------------------------------------------------------------------

ArimaResult ArimaFitter::fitCss(const std::vector<double>& y) const
{
    const int p = m_cfg.order.p;
    const int q = m_cfg.order.q;
    const int P = m_cfg.seasonal.P;
    const int Q = m_cfg.seasonal.Q;
    const int s = m_cfg.seasonal.s;

    const std::vector<int> arIdx = arLags(p, P, s);
    const std::vector<int> maIdx = maLags(q, Q, s);

    const int nArLags = static_cast<int>(arIdx.size());
    const int nMaLags = static_cast<int>(maIdx.size());

    const std::size_t n = y.size();

    // Maximum lag needed overall (to know how many initial observations to skip)
    int maxLag = 0;
    for (int lag : arIdx) { maxLag = std::max(maxLag, lag); }
    for (int lag : maIdx) { maxLag = std::max(maxLag, lag); }

    // -------------------------------------------------------------------------
    //  Step 1: High-order AR to obtain innovation proxies epsilon_t
    // -------------------------------------------------------------------------

    // AR order for proxy fit: min(max(p + q + P + Q + 4, 10), n / 4)
    const int arProxyOrder = std::min(
        static_cast<int>(n / 4),
        std::max(p + q + P + Q + 4, 10));

    if (static_cast<int>(n) <= arProxyOrder + maxLag + 1) {
        throw SeriesTooShortException(
            "ArimaFitter::fitCss: series length " + std::to_string(n)
            + " is too short for the requested ARIMA order and proxy AR order "
            + std::to_string(arProxyOrder) + ".");
    }

    // Build proxy AR design matrix and solve
    const Eigen::MatrixXd Gproxy = loki::math::buildLagMatrix(
        y, arProxyOrder);  // (n - arProxyOrder) x arProxyOrder

    const std::size_t nProxy = n - static_cast<std::size_t>(arProxyOrder);
    Eigen::VectorXd lProxy(static_cast<Eigen::Index>(nProxy));
    for (std::size_t i = 0; i < nProxy; ++i) {
        lProxy(static_cast<Eigen::Index>(i)) =
            y[i + static_cast<std::size_t>(arProxyOrder)];
    }

    // Add intercept column to proxy design matrix
    Eigen::MatrixXd AProxy(static_cast<Eigen::Index>(nProxy),
                            arProxyOrder + 1);
    AProxy.col(0) = Eigen::VectorXd::Ones(static_cast<Eigen::Index>(nProxy));
    AProxy.rightCols(arProxyOrder) = Gproxy;

    const LsqResult proxyFit = LsqSolver::solve(AProxy, lProxy);

    // Compute proxy innovations: epsilon[t] = y[t] - fitted[t]
    // epsilon is defined for t >= arProxyOrder (same length as lProxy)
    const Eigen::VectorXd proxyFitted = AProxy * proxyFit.coefficients;
    const Eigen::VectorXd proxyResid  = lProxy - proxyFitted;

    // Store epsilon in a full-length vector (index aligned with y)
    // Positions 0 .. arProxyOrder-1 are unavailable; set to 0 (conditional on past = 0)
    std::vector<double> epsilon(n, 0.0);
    for (std::size_t i = 0; i < nProxy; ++i) {
        epsilon[i + static_cast<std::size_t>(arProxyOrder)] =
            proxyResid(static_cast<Eigen::Index>(i));
    }

    // -------------------------------------------------------------------------
    //  Step 2: Joint OLS on AR lags, MA lags, and intercept
    // -------------------------------------------------------------------------

    // Number of rows usable: observations where all required lags are available
    // First usable index: maxLag (0-based), so rows = n - maxLag
    if (static_cast<int>(n) <= maxLag) {
        throw SeriesTooShortException(
            "ArimaFitter::fitCss: series too short after lag truncation.");
    }

    const std::size_t nFit  = n - static_cast<std::size_t>(maxLag);
    const int         nCols = 1 + nArLags + nMaLags;  // intercept + AR + MA

    Eigen::MatrixXd A(static_cast<Eigen::Index>(nFit),
                      static_cast<Eigen::Index>(nCols));
    Eigen::VectorXd l(static_cast<Eigen::Index>(nFit));

    for (std::size_t row = 0; row < nFit; ++row) {
        const std::size_t t = row + static_cast<std::size_t>(maxLag);  // current obs index

        l(static_cast<Eigen::Index>(row)) = y[t];

        // Intercept
        A(static_cast<Eigen::Index>(row), 0) = 1.0;

        // AR regressors: y[t - lag]
        for (int col = 0; col < nArLags; ++col) {
            const std::size_t lagIdx = static_cast<std::size_t>(arIdx[col]);
            A(static_cast<Eigen::Index>(row),
              static_cast<Eigen::Index>(1 + col)) = y[t - lagIdx];
        }

        // MA regressors: epsilon[t - lag]
        for (int col = 0; col < nMaLags; ++col) {
            const std::size_t lagIdx = static_cast<std::size_t>(maIdx[col]);
            const std::size_t src    = (t >= lagIdx) ? (t - lagIdx) : 0;
            A(static_cast<Eigen::Index>(row),
              static_cast<Eigen::Index>(1 + nArLags + col)) = epsilon[src];
        }
    }

    const LsqResult fit = LsqSolver::solve(A, l);

    // -------------------------------------------------------------------------
    //  Populate ArimaResult
    // -------------------------------------------------------------------------

    ArimaResult result;
    result.order    = m_cfg.order;
    result.seasonal = m_cfg.seasonal;
    result.arLags   = arIdx;
    result.maLags   = maIdx;
    result.method   = "css";
    result.n        = nFit;

    // Coefficients: layout is [intercept, AR..., MA...]
    result.intercept = fit.coefficients(0);
    result.arCoeffs.resize(static_cast<std::size_t>(nArLags));
    for (int i = 0; i < nArLags; ++i) {
        result.arCoeffs[static_cast<std::size_t>(i)] =
            fit.coefficients(static_cast<Eigen::Index>(1 + i));
    }
    result.maCoeffs.resize(static_cast<std::size_t>(nMaLags));
    for (int i = 0; i < nMaLags; ++i) {
        result.maCoeffs[static_cast<std::size_t>(i)] =
            fit.coefficients(static_cast<Eigen::Index>(1 + nArLags + i));
    }

    // Residuals and fitted on the fitting window
    const Eigen::VectorXd fittedEig = A * fit.coefficients;
    const Eigen::VectorXd residEig  = l - fittedEig;

    result.fitted.resize(nFit);
    result.residuals.resize(nFit);
    for (std::size_t i = 0; i < nFit; ++i) {
        result.fitted  [i] = fittedEig(static_cast<Eigen::Index>(i));
        result.residuals[i] = residEig (static_cast<Eigen::Index>(i));
    }

    // sigma2 from residual sum of squares
    const int k = nCols;  // intercept + AR + MA
    const int dof = static_cast<int>(nFit) - k;
    result.sigma2 = (dof > 0)
        ? residEig.squaredNorm() / static_cast<double>(dof)
        : residEig.squaredNorm() / static_cast<double>(nFit);

    computeInfoCriteria(result, k);

    return result;
}

// ---------------------------------------------------------------------------
//  fit  -- public entry point
// ---------------------------------------------------------------------------

ArimaResult ArimaFitter::fit(const std::vector<double>& y) const
{
    if (y.empty()) {
        throw SeriesTooShortException(
            "ArimaFitter::fit: input series is empty.");
    }

    // Apply differencing
    const int d = m_cfg.order.d;
    const int D = m_cfg.seasonal.D;
    const int s = m_cfg.seasonal.s;

    std::vector<double> ydiff = applyDifferencing(y, d, D, s);

    if (ydiff.size() < 10) {
        throw SeriesTooShortException(
            "ArimaFitter::fit: series too short after differencing (n="
            + std::to_string(ydiff.size()) + ").");
    }

    if (m_cfg.method != "css") {
        LOKI_WARNING("ArimaFitter: method '" + m_cfg.method
                     + "' not yet implemented; falling back to 'css'.");
    }

    return fitCss(ydiff);
}
