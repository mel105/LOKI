#include <loki/regression/nonlinearRegressor.hpp>
#include <loki/regression/regressorUtils.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/math/lm.hpp>
#include <loki/stats/distributions.hpp>

#include <Eigen/Dense>

#include <cmath>
#include <limits>
#include <numbers>
#include <sstream>
#include <string>

using namespace loki;
using namespace loki::regression;

// ---------------------------------------------------------------------------
//  Built-in model functions
// ---------------------------------------------------------------------------

static loki::ModelFn makeExponential()
{
    // f(x, [a, b]) = a * exp(b * x)
    return [](double x, const Eigen::VectorXd& p) -> double {
        return p(0) * std::exp(p(1) * x);
    };
}

static loki::ModelFn makeLogistic()
{
    // f(x, [L, k, x0]) = L / (1 + exp(-k * (x - x0)))
    return [](double x, const Eigen::VectorXd& p) -> double {
        return p(0) / (1.0 + std::exp(-p(1) * (x - p(2))));
    };
}

static loki::ModelFn makeGaussian()
{
    // f(x, [A, mu, sigma]) = A * exp(-((x - mu)^2) / (2 * sigma^2))
    return [](double x, const Eigen::VectorXd& p) -> double {
        const double z = (x - p(1)) / p(2);
        return p(0) * std::exp(-0.5 * z * z);
    };
}

// ---------------------------------------------------------------------------
//  NonlinearRegressor::makeBuiltinModel
// ---------------------------------------------------------------------------

loki::ModelFn NonlinearRegressor::makeBuiltinModel(NonlinearModelEnum model)
{
    switch (model) {
        case NonlinearModelEnum::EXPONENTIAL: return makeExponential();
        case NonlinearModelEnum::LOGISTIC:    return makeLogistic();
        case NonlinearModelEnum::GAUSSIAN:    return makeGaussian();
    }
    // Unreachable -- satisfies -Wreturn-type
    throw AlgorithmException("NonlinearRegressor::makeBuiltinModel: unknown model enum.");
}

// ---------------------------------------------------------------------------
//  NonlinearRegressor::defaultP0
// ---------------------------------------------------------------------------

Eigen::VectorXd NonlinearRegressor::defaultP0(NonlinearModelEnum model)
{
    switch (model) {
        case NonlinearModelEnum::EXPONENTIAL: {
            // a=1, b=0 (start near identity, let LM find direction)
            Eigen::VectorXd p(2);
            p << 1.0, 0.0;
            return p;
        }
        case NonlinearModelEnum::LOGISTIC: {
            // L=1, k=0.01, x0=0  (shallow S-curve centred at origin)
            Eigen::VectorXd p(3);
            p << 1.0, 0.01, 0.0;
            return p;
        }
        case NonlinearModelEnum::GAUSSIAN: {
            // A=1, mu=0, sigma=1
            Eigen::VectorXd p(3);
            p << 1.0, 0.0, 1.0;
            return p;
        }
    }
    throw AlgorithmException("NonlinearRegressor::defaultP0: unknown model enum.");
}

// ---------------------------------------------------------------------------
//  NonlinearRegressor::paramNamesStr
// ---------------------------------------------------------------------------

std::string NonlinearRegressor::paramNamesStr(NonlinearModelEnum model)
{
    switch (model) {
        case NonlinearModelEnum::EXPONENTIAL: return "a, b";
        case NonlinearModelEnum::LOGISTIC:    return "L, k, x0";
        case NonlinearModelEnum::GAUSSIAN:    return "A, mu, sigma";
    }
    return "params";
}

// ---------------------------------------------------------------------------
//  Constructors
// ---------------------------------------------------------------------------

static int expectedParamCount(NonlinearModelEnum model)
{
    switch (model) {
        case NonlinearModelEnum::EXPONENTIAL: return 2;
        case NonlinearModelEnum::LOGISTIC:    return 3;
        case NonlinearModelEnum::GAUSSIAN:    return 3;
    }
    return 0;
}

NonlinearRegressor::NonlinearRegressor(const RegressionConfig& cfg)
    : m_cfg(cfg)
    , m_modelFn(makeBuiltinModel(cfg.nonlinear.model))
{
    const auto& nlCfg = cfg.nonlinear;
    const int   nExpected = expectedParamCount(nlCfg.model);

    if (!nlCfg.initialParams.empty()) {
        if (static_cast<int>(nlCfg.initialParams.size()) != nExpected) {
            throw ConfigException(
                "NonlinearRegressor: initial_params has " +
                std::to_string(nlCfg.initialParams.size()) +
                " values but model requires " +
                std::to_string(nExpected) + ".");
        }
        m_p0 = Eigen::Map<const Eigen::VectorXd>(
            nlCfg.initialParams.data(),
            static_cast<Eigen::Index>(nlCfg.initialParams.size()));
    } else {
        LOKI_WARNING(
            "NonlinearRegressor: initial_params not set -- using built-in defaults. "
            "LM convergence is sensitive to starting values; provide explicit params.");
        m_p0 = defaultP0(nlCfg.model);
    }
}

NonlinearRegressor::NonlinearRegressor(const RegressionConfig& cfg,
                                       loki::ModelFn           modelFn,
                                       Eigen::VectorXd         p0)
    : m_cfg(cfg)
    , m_modelFn(std::move(modelFn))
    , m_p0(std::move(p0))
{
    if (m_p0.size() == 0) {
        throw ConfigException(
            "NonlinearRegressor: p0 must be non-empty for CUSTOM model.");
    }
}

// ---------------------------------------------------------------------------
//  NonlinearRegressor::name
// ---------------------------------------------------------------------------

std::string NonlinearRegressor::name() const
{
    const auto& nlCfg = m_cfg.nonlinear;
    std::string modelStr;
    switch (nlCfg.model) {
        case NonlinearModelEnum::EXPONENTIAL: modelStr = "Exponential"; break;
        case NonlinearModelEnum::LOGISTIC:    modelStr = "Logistic";    break;
        case NonlinearModelEnum::GAUSSIAN:    modelStr = "Gaussian";    break;
    }
    return "NonlinearRegressor[" + modelStr + "]";
}

// ---------------------------------------------------------------------------
//  NonlinearRegressor::fit
// ---------------------------------------------------------------------------

RegressionResult NonlinearRegressor::fit(const TimeSeries& ts)
{
    // Collect valid observations
    std::vector<double> xVec, yVec;
    double tRef = std::numeric_limits<double>::quiet_NaN();

    for (std::size_t i = 0; i < ts.size(); ++i) {
        if (!loki::isValid(ts[i])) continue;
        const double mjd = ts[i].time.mjd();
        if (std::isnan(tRef)) tRef = mjd;
        xVec.push_back(mjd - tRef);
        yVec.push_back(ts[i].value);
    }

    const int n = static_cast<int>(xVec.size());
    const int p = static_cast<int>(m_p0.size());

    if (n < p + 1) {
        throw DataException(
            "NonlinearRegressor::fit: need at least " + std::to_string(p + 1) +
            " valid observations, got " + std::to_string(n) + ".");
    }

    const Eigen::VectorXd xEig = Eigen::Map<const Eigen::VectorXd>(xVec.data(), n);
    const Eigen::VectorXd yEig = Eigen::Map<const Eigen::VectorXd>(yVec.data(), n);

    // Build LM config from NonlinearConfig
    LmConfig lmCfg;
    lmCfg.maxIterations = m_cfg.nonlinear.maxIterations;
    lmCfg.gradTol       = m_cfg.nonlinear.gradTol;
    lmCfg.stepTol       = m_cfg.nonlinear.stepTol;
    lmCfg.lambdaInit    = m_cfg.nonlinear.lambdaInit;
    lmCfg.lambdaFactor  = m_cfg.nonlinear.lambdaFactor;

    LmResult lm = LmSolver::solve(m_modelFn, xEig, yEig, m_p0, lmCfg);

    // Map LmResult -> RegressionResult
    RegressionResult result;
    result.tRef          = tRef;
    result.coefficients  = lm.params;
    result.residuals     = lm.residuals;
    result.sigma0        = lm.sigma0;
    result.cofactorX     = lm.covariance;
    result.designMatrix  = lm.jacobian;   // Jacobian at solution ~ linearised design matrix
    result.converged     = lm.converged;
    result.dof           = n - p;

    // Build fitted TimeSeries
    {
        std::size_t validIdx = 0;
        for (std::size_t i = 0; i < ts.size(); ++i) {
            if (!loki::isValid(ts[i])) continue;
            const double fv = m_modelFn(xVec[validIdx], lm.params);
            result.fitted.append(ts[i].time, fv);
            ++validIdx;
        }
    }

    // R-squared
    {
        double yMean = yEig.mean();
        double ssTot = (yEig.array() - yMean).square().sum();
        double ssRes = lm.residuals.squaredNorm();
        result.rSquared    = (ssTot > 0.0) ? (1.0 - ssRes / ssTot) : 0.0;
        result.rSquaredAdj = (ssTot > 0.0 && result.dof > 0)
            ? (1.0 - (ssRes / static_cast<double>(result.dof)) /
                     (ssTot / static_cast<double>(n - 1)))
            : 0.0;
    }

    // Information criteria
    {
        const double logLik = -0.5 * static_cast<double>(n) *
            (1.0 + std::log(2.0 * std::numbers::pi) +
             std::log(lm.residuals.squaredNorm() / static_cast<double>(n)));
        result.aic = -2.0 * logLik + 2.0 * static_cast<double>(p);
        result.bic = -2.0 * logLik + static_cast<double>(p) * std::log(static_cast<double>(n));
    }

    // Model name with parameter values
    {
        std::ostringstream oss;
        oss << name() << "  params=[";
        for (int j = 0; j < p; ++j) {
            if (j > 0) oss << ", ";
            oss << paramNamesStr(m_cfg.nonlinear.model).c_str();
            // Just show values -- param names are in the header comment
            oss.str("");
            oss.clear();
            break;
        }
        // Rebuild cleanly
        oss << name() << "  [" << paramNamesStr(m_cfg.nonlinear.model) << "] = [";
        for (int j = 0; j < p; ++j) {
            if (j > 0) oss << ", ";
            oss << lm.params(j);
        }
        oss << "]";
        if (!lm.converged) oss << "  [NOT CONVERGED after " << lm.iterations << " iter]";
        result.modelName = oss.str();
    }

    m_lastResult = result;
    m_fitted     = true;
    return result;
}

// ---------------------------------------------------------------------------
//  NonlinearRegressor::predict
// ---------------------------------------------------------------------------

std::vector<PredictionPoint> NonlinearRegressor::predict(
    const std::vector<double>& xNew) const
{
    if (!m_fitted) {
        throw AlgorithmException(
            "NonlinearRegressor::predict: must call fit() before predict().");
    }

    const int    nNew = static_cast<int>(xNew.size());
    const int    dof  = m_lastResult.dof;
    const double s2   = m_lastResult.sigma0 * m_lastResult.sigma0;
    const int    p    = static_cast<int>(m_lastResult.coefficients.size());
    const double conf = m_cfg.nonlinear.confidenceLevel;

    // t critical value: upper tail alpha/2 of t(dof)
    const double alpha  = 1.0 - conf;
    const double tCrit  = (dof > 0)
        ? loki::stats::tQuantile(1.0 - alpha / 2.0, static_cast<double>(dof))
        : loki::stats::tQuantile(1.0 - alpha / 2.0, 1.0);

    std::vector<PredictionPoint> pts;
    pts.reserve(static_cast<std::size_t>(nNew));

    for (int k = 0; k < nNew; ++k) {
        const double xk   = xNew[static_cast<std::size_t>(k)];
        const double yHat = m_modelFn(xk, m_lastResult.coefficients);

        // Gradient of f w.r.t. params at xk via central differences
        const double h = 1.0e-5;
        Eigen::RowVectorXd grad(p);
        for (int j = 0; j < p; ++j) {
            Eigen::VectorXd pFwd = m_lastResult.coefficients;
            Eigen::VectorXd pBwd = m_lastResult.coefficients;
            const double hj = h * (1.0 + std::abs(m_lastResult.coefficients(j)));
            pFwd(j) += hj;
            pBwd(j) -= hj;
            grad(j) = (m_modelFn(xk, pFwd) - m_modelFn(xk, pBwd)) / (2.0 * hj);
        }

        const double varMean = (grad * m_lastResult.cofactorX * grad.transpose())(0, 0);
        const double varPred = varMean + s2;

        const double halfConf = tCrit * std::sqrt(std::max(0.0, varMean));
        const double halfPred = tCrit * std::sqrt(std::max(0.0, varPred));

        PredictionPoint pt;
        pt.x         = xk;
        pt.predicted = yHat;
        pt.confLow   = yHat - halfConf;
        pt.confHigh  = yHat + halfConf;
        pt.predLow   = yHat - halfPred;
        pt.predHigh  = yHat + halfPred;
        pts.push_back(pt);
    }

    return pts;
}
