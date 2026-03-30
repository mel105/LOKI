#include <loki/regression/calibrationRegressor.hpp>
#include <loki/regression/regressorUtils.hpp>
#include <loki/math/svd.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/stats/distributions.hpp>

#include <cmath>
#include <limits>

using namespace loki;
using namespace loki::regression;

// -----------------------------------------------------------------------------
//  Construction
// -----------------------------------------------------------------------------

CalibrationRegressor::CalibrationRegressor(const RegressionConfig& cfg)
    : m_cfg(cfg)
{}

// -----------------------------------------------------------------------------
//  fit()
// -----------------------------------------------------------------------------

RegressionResult CalibrationRegressor::fit(const TimeSeries& ts)
{
    static constexpr int MIN_OBS  = 3;
    static constexpr int N_PARAMS = 2; // intercept + slope

    // Collect valid observations.
    std::vector<double>      xVec;
    std::vector<double>      yVec;
    std::vector<::TimeStamp> times;

    xVec.reserve(ts.size());
    yVec.reserve(ts.size());
    times.reserve(ts.size());

    double tRef = std::numeric_limits<double>::quiet_NaN();

    for (std::size_t i = 0; i < ts.size(); ++i) {
        if (!isValid(ts[i])) continue;
        if (std::isnan(tRef)) tRef = ts[i].time.mjd();
        xVec.push_back(ts[i].time.mjd() - tRef);
        yVec.push_back(ts[i].value);
        times.push_back(ts[i].time);
    }

    const int n = static_cast<int>(xVec.size());

    if (n < MIN_OBS) {
        throw DataException(
            "CalibrationRegressor::fit(): need at least " +
            std::to_string(MIN_OBS) + " valid observations, got " +
            std::to_string(n) + ".");
    }

    // Map to Eigen.
    const Eigen::VectorXd x = Eigen::Map<const Eigen::VectorXd>(xVec.data(), n);
    const Eigen::VectorXd y = Eigen::Map<const Eigen::VectorXd>(yVec.data(), n);

    // Centre x and y -- TLS is translation-invariant but centering avoids
    // numerical issues and decouples slope from intercept estimation.
    const double xBar = x.mean();
    const double yBar = y.mean();
    const Eigen::VectorXd xc = x.array() - xBar;
    const Eigen::VectorXd yc = y.array() - yBar;

    // Form Z = [xc, yc] (n x 2) and compute SVD.
    Eigen::MatrixXd Z(n, 2);
    Z.col(0) = xc;
    Z.col(1) = yc;

    // const SvdDecomposition svd(Z);
    Eigen::JacobiSVD<Eigen::MatrixXd> svd(Z, Eigen::ComputeThinU | Eigen::ComputeThinV);

    // The last right singular vector v = V[:, 1] defines the orthogonal complement
    // of the best-fit line. The line direction is v_perp = V[:, 0].
    // Line equation: v[0] * xc + v[1] * yc = 0
    // => slope = -v[0] / v[1]
    //const Eigen::MatrixXd& Vmat = svd.V();
    const Eigen::MatrixXd& Vmat = svd.matrixV();
    
    const double v0 = Vmat(0, 1); // last right singular vector, x component
    const double v1 = Vmat(1, 1); // last right singular vector, y component

    if (std::fabs(v1) < std::numeric_limits<double>::epsilon() * 1e6) {
        throw AlgorithmException(
            "CalibrationRegressor::fit(): TLS line is undefined -- "
            "all x values may be identical (vertical scatter only).");
    }

    const double slope     = -v0 / v1;
    const double intercept = yBar - slope * xBar;

    // Orthogonal residuals: distance from each point to line y = a0 + a1*x.
    // For line a1*x - y + a0 = 0, orthogonal distance = (a1*xi - yi + a0) / sqrt(a1^2 + 1).
    const double normFactor = std::sqrt(slope * slope + 1.0);
    Eigen::VectorXd orthoRes(n);
    for (int i = 0; i < n; ++i)
        orthoRes[i] = (slope * x[i] - y[i] + intercept) / normFactor;

    // sigma0 = RMS of orthogonal residuals.
    const int    dof    = n - N_PARAMS;
    const double sigma0 = std::sqrt(orthoRes.squaredNorm() / static_cast<double>(dof));

    // Design matrix (n x 2): [1, x] -- same layout as LinearRegressor.
    // Stored for RegressionDiagnostics::computeInfluence().
    Eigen::MatrixXd A(n, 2);
    A.col(0).setOnes();
    A.col(1) = x;

    // Fitted values (vertical projection onto line, for plotting).
    const Eigen::VectorXd fittedVals = A.col(0) * intercept + A.col(1) * slope;

    // Build result.
    RegressionResult result;
    result.modelName              = name();
    result.tRef                   = tRef;
    result.coefficients           = Eigen::Vector2d{intercept, slope};
    result.residuals              = orthoRes;
    result.cofactorX              = Eigen::MatrixXd(); // not available for TLS
    result.sigma0                 = sigma0;
    result.dof                    = dof;
    result.converged              = true;
    result.designMatrix           = A;

    detail::computeGoodnessOfFit(result, y, N_PARAMS);

    result.fitted = TimeSeries(ts.metadata());
    for (int i = 0; i < n; ++i)
        result.fitted.append(times[static_cast<std::size_t>(i)], fittedVals[i]);

    m_lastResult = result;
    m_fitted     = true;

    return result;
}

// -----------------------------------------------------------------------------
//  predict()
// -----------------------------------------------------------------------------

std::vector<PredictionPoint>
CalibrationRegressor::predict(const std::vector<double>& xNew) const
{
    if (!m_fitted) {
        throw AlgorithmException(
            "CalibrationRegressor::predict(): must call fit() before predict().");
    }
 
    const int k = static_cast<int>(xNew.size());
    Eigen::MatrixXd aNew(k, 2);
    for (int i = 0; i < k; ++i) {
        aNew(i, 0) = 1.0;
        aNew(i, 1) = xNew[static_cast<std::size_t>(i)];
    }
 
    return detail::computeIntervals(m_lastResult, aNew, xNew, m_cfg.confidenceLevel);
}

// -----------------------------------------------------------------------------
//  name()
// -----------------------------------------------------------------------------

std::string CalibrationRegressor::name() const
{
    return "CalibrationRegressor (TLS)";
}
