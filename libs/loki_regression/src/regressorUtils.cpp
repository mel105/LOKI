#include <loki/regression/regressorUtils.hpp>
#include <loki/stats/distributions.hpp>
#include <loki/core/exceptions.hpp>

#include <cmath>
#include <numbers>

using namespace loki;

namespace loki::regression::detail {

void computeGoodnessOfFit(RegressionResult&      result,
                           const Eigen::VectorXd& l,
                           int                    nParams)
{
    const int    n  = static_cast<int>(l.size());
    const double nd = static_cast<double>(n);

    const double yMean = l.mean();
    const double ssTot = (l.array() - yMean).square().sum();
    const double ssRes = result.residuals.squaredNorm();

    result.rSquared    = (ssTot > 0.0) ? (1.0 - ssRes / ssTot) : 1.0;
    result.rSquaredAdj = 1.0 - (1.0 - result.rSquared) *
                         (nd - 1.0) / static_cast<double>(result.dof);

    const double sigma2Mle = ssRes / nd;
    const double logLik    = -0.5 * nd *
                             (std::log(2.0 * std::numbers::pi * sigma2Mle) + 1.0);
    const double k         = static_cast<double>(nParams);

    result.aic = 2.0 * k - 2.0 * logLik;
    result.bic = k * std::log(nd) - 2.0 * logLik;
}

std::vector<PredictionPoint> computeIntervals(
    const RegressionResult&    result,
    const Eigen::MatrixXd&     aNew,
    const std::vector<double>& xNew,
    double                     confLevel)
{
    if (result.dof <= 0) {
        throw AlgorithmException(
            "computeIntervals: dof must be > 0, got "
            + std::to_string(result.dof) + ".");
    }

    if (static_cast<int>(xNew.size()) != static_cast<int>(aNew.rows())) {
        throw AlgorithmException(
            "computeIntervals: xNew.size()=" + std::to_string(xNew.size()) +
            " must equal aNew.rows()=" + std::to_string(aNew.rows()) + ".");
    }

    const double alpha = 1.0 - confLevel;
    const double tCrit = loki::stats::tQuantile(
        1.0 - alpha / 2.0, static_cast<double>(result.dof));
    const double s = result.sigma0;

    const bool hasCofactor = (result.cofactorX.rows() > 0 &&
                               result.cofactorX.cols() > 0);

    const int k = static_cast<int>(aNew.rows());
    std::vector<PredictionPoint> out;
    out.reserve(static_cast<std::size_t>(k));

    for (int i = 0; i < k; ++i) {
        const Eigen::VectorXd aRow = aNew.row(i);
        const double yHat = aRow.dot(result.coefficients);

        double varConf = 0.0;
        if (hasCofactor) {
            varConf = s * s *
                (aRow.transpose() * result.cofactorX * aRow).value();
        }
        const double varPred = varConf + s * s;
        const double seConf  = std::sqrt(std::max(0.0, varConf));
        const double sePred  = std::sqrt(std::max(0.0, varPred));

        PredictionPoint pt;
        pt.x         = xNew[static_cast<std::size_t>(i)];
        pt.predicted = yHat;
        pt.confLow   = yHat - tCrit * seConf;
        pt.confHigh  = yHat + tCrit * seConf;
        pt.predLow   = yHat - tCrit * sePred;
        pt.predHigh  = yHat + tCrit * sePred;
        out.push_back(pt);
    }

    return out;
}

} // namespace loki::regression::detail