#include <loki/math/bsplineFit.hpp>

#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <numeric>

using namespace loki;

namespace loki::math {

// =============================================================================
//  Internal helpers
// =============================================================================

namespace {

/**
 * @brief Solve N * c = z (overdetermined or square) via ColPivHouseholderQR.
 *
 * Returns the control point vector c.
 * Throws AlgorithmException if the system is rank-deficient.
 */
Eigen::VectorXd solveNormalEq(const Eigen::MatrixXd& N,
                               const Eigen::VectorXd& z)
{
    // For overdetermined: N^T N c = N^T z  (normal equations, solved via QR).
    // For square: direct solve via QR.
    Eigen::ColPivHouseholderQR<Eigen::MatrixXd> qr(N);
    if (qr.rank() < N.cols()) {
        throw AlgorithmException(
            "bsplineFit: basis matrix is rank-deficient (rank="
            + std::to_string(qr.rank()) + ", cols=" + std::to_string(N.cols())
            + "). Reduce nCtrl or change knot placement.");
    }
    return qr.solve(z);
}

/** @brief Root mean squared error between two equal-length vectors. */
double computeRmse(const std::vector<double>& pred,
                   const std::vector<double>& obs)
{
    if (pred.size() != obs.size() || pred.empty()) return 0.0;
    double sse = 0.0;
    for (std::size_t i = 0; i < pred.size(); ++i) {
        const double e = pred[i] - obs[i];
        sse += e * e;
    }
    return std::sqrt(sse / static_cast<double>(pred.size()));
}

/** @brief R^2 coefficient of determination. */
double computeR2(const std::vector<double>& pred,
                 const std::vector<double>& obs)
{
    if (obs.empty()) return 0.0;
    const double mean = std::accumulate(obs.begin(), obs.end(), 0.0)
                      / static_cast<double>(obs.size());
    double ssTot = 0.0, ssRes = 0.0;
    for (std::size_t i = 0; i < obs.size(); ++i) {
        const double d = obs[i] - mean;
        ssTot += d * d;
        ssRes += (obs[i] - pred[i]) * (obs[i] - pred[i]);
    }
    return (ssTot < 1.0e-15) ? 1.0 : 1.0 - ssRes / ssTot;
}

/** @brief Standard deviation of residuals. */
double computeResidualStd(const std::vector<double>& pred,
                          const std::vector<double>& obs)
{
    if (obs.size() < 2) return 0.0;
    double mean = 0.0;
    for (std::size_t i = 0; i < obs.size(); ++i) mean += obs[i] - pred[i];
    mean /= static_cast<double>(obs.size());
    double var = 0.0;
    for (std::size_t i = 0; i < obs.size(); ++i) {
        const double r = (obs[i] - pred[i]) - mean;
        var += r * r;
    }
    return std::sqrt(var / static_cast<double>(obs.size() - 1));
}

} // anonymous namespace

// =============================================================================
//  fitBSpline
// =============================================================================

BSplineFitResult fitBSpline(const std::vector<double>& t,
                            const std::vector<double>& z,
                            int degree,
                            int nCtrl,
                            const std::string& knotPlacement)
{
    // -- Input validation -----------------------------------------------------
    if (t.size() != z.size() || t.empty()) {
        throw DataException(
            "bsplineFit::fitBSpline: t and z must be non-empty and same size.");
    }
    const int nObs = static_cast<int>(t.size());
    if (degree < 1 || degree > 5) {
        throw ConfigException(
            "bsplineFit::fitBSpline: degree must be in [1, 5], got "
            + std::to_string(degree) + ".");
    }
    if (nCtrl < degree + 1) {
        throw ConfigException(
            "bsplineFit::fitBSpline: nCtrl must be >= degree+1 (>="
            + std::to_string(degree + 1) + "), got " + std::to_string(nCtrl) + ".");
    }
    if (nCtrl > nObs) {
        throw ConfigException(
            "bsplineFit::fitBSpline: nCtrl (" + std::to_string(nCtrl)
            + ") cannot exceed nObs (" + std::to_string(nObs) + ").");
    }
    if (knotPlacement != "uniform" && knotPlacement != "chord_length") {
        throw ConfigException(
            "bsplineFit::fitBSpline: knotPlacement must be 'uniform' or "
            "'chord_length', got '" + knotPlacement + "'.");
    }

    // -- Normalise parameter values to [0, 1] ---------------------------------
    const std::vector<double> tNorm = normaliseParams(t);

    // -- Build knot vector ----------------------------------------------------
    std::vector<double> knots;
    if (knotPlacement == "chord_length") {
        knots = buildChordLengthKnots(tNorm, nCtrl, degree);
    } else {
        knots = buildUniformKnots(nCtrl, degree);
    }

    // -- Build basis matrix N (nObs x nCtrl) ----------------------------------
    const Eigen::MatrixXd N = bsplineBasisMatrix(tNorm, degree, knots);

    // -- Solve for control points ---------------------------------------------
    const Eigen::Map<const Eigen::VectorXd> zVec(z.data(), nObs);
    const Eigen::VectorXd cVec = solveNormalEq(N, zVec);

    // -- Evaluate fitted curve on training data --------------------------------
    std::vector<double> cp(static_cast<std::size_t>(nCtrl));
    for (int i = 0; i < nCtrl; ++i) cp[static_cast<std::size_t>(i)] = cVec(i);

    const std::vector<double> fitted = evalBSpline(tNorm, cp, degree, knots);

    // -- Assemble result -------------------------------------------------------
    BSplineFitResult res;
    res.controlPoints = cp;
    res.knots         = knots;
    res.tNorm         = tNorm;
    res.tMin          = t.front();
    res.tMax          = t.back();
    res.degree        = degree;
    res.nCtrl         = nCtrl;
    res.nObs          = nObs;
    res.rmse          = computeRmse(fitted, z);
    res.rSquared      = computeR2(fitted, z);
    res.residualStd   = computeResidualStd(fitted, z);
    res.knotPlacement = knotPlacement;
    res.isExact       = (nCtrl == nObs);

    return res;
}

// =============================================================================
//  crossValidateBSpline
// =============================================================================

std::vector<CvPoint> crossValidateBSpline(const std::vector<double>& t,
                                          const std::vector<double>& z,
                                          int degree,
                                          int nCtrlMin,
                                          int nCtrlMax,
                                          const std::string& knotPlacement,
                                          int folds)
{
    const int nObs = static_cast<int>(t.size());

    // -- Validate inputs -------------------------------------------------------
    if (nObs < 4) {
        throw DataException(
            "bsplineFit::crossValidateBSpline: need at least 4 observations, got "
            + std::to_string(nObs) + ".");
    }
    if (folds < 2 || folds > nObs) {
        throw ConfigException(
            "bsplineFit::crossValidateBSpline: folds must be in [2, nObs], got "
            + std::to_string(folds) + ".");
    }
    const int minPossible = degree + 1;
    if (nCtrlMin < minPossible) {
        LOKI_WARNING("bsplineFit::crossValidateBSpline: nCtrlMin=" 
                     + std::to_string(nCtrlMin) 
                     + " < degree+1=" + std::to_string(minPossible)
                     + " -- clamping to " + std::to_string(minPossible) + ".");
        nCtrlMin = minPossible;
    }
    if (nCtrlMax <= 0) {
        // Auto cap: min(n/5, 200) but at least nCtrlMin + 1.
        nCtrlMax = std::max(nCtrlMin + 1,
                            std::min(nObs / 5, 200));
    }
    nCtrlMax = std::min(nCtrlMax, nObs / folds); // must leave enough train data
    if (nCtrlMax < nCtrlMin) {
        LOKI_WARNING("bsplineFit::crossValidateBSpline: nCtrlMax < nCtrlMin after "
                     "adjustments -- returning single-point CV curve.");
        nCtrlMax = nCtrlMin;
    }

    // -- Build fold indices ----------------------------------------------------
    // Each observation gets a fold id in [0, folds-1].
    std::vector<int> foldId(static_cast<std::size_t>(nObs));
    for (int i = 0; i < nObs; ++i) {
        foldId[static_cast<std::size_t>(i)] = i % folds;
    }

    // -- Sweep nCtrl -----------------------------------------------------------
    std::vector<CvPoint> curve;
    curve.reserve(static_cast<std::size_t>(nCtrlMax - nCtrlMin + 1));

    for (int nCtrl = nCtrlMin; nCtrl <= nCtrlMax; ++nCtrl) {
        double sseTot = 0.0;
        int    nTot   = 0;
        bool   failed = false;

        for (int fold = 0; fold < folds; ++fold) {
            // -- Build train/test split ----------------------------------------
            std::vector<double> tTrain, zTrain, tTest, zTest;
            tTrain.reserve(static_cast<std::size_t>(nObs));
            zTrain.reserve(static_cast<std::size_t>(nObs));

            for (int i = 0; i < nObs; ++i) {
                const std::size_t si = static_cast<std::size_t>(i);
                if (foldId[si] == fold) {
                    tTest.push_back(t[si]);
                    zTest.push_back(z[si]);
                } else {
                    tTrain.push_back(t[si]);
                    zTrain.push_back(z[si]);
                }
            }

            // Skip if train set is too small for this nCtrl.
            if (static_cast<int>(tTrain.size()) < nCtrl + 1) {
                failed = true;
                break;
            }

            // -- Fit on training fold ------------------------------------------
            BSplineFitResult trainFit;
            try {
                trainFit = fitBSpline(tTrain, zTrain, degree, nCtrl, knotPlacement);
            } catch (const LOKIException&) {
                failed = true;
                break;
            }

            // -- Predict on test fold ------------------------------------------
            // Normalise test t using training range (same scale as fit).
            const double tRange = trainFit.tMax - trainFit.tMin;
            if (tRange <= 0.0) { failed = true; break; }

            std::vector<double> tTestNorm(tTest.size());
            for (std::size_t i = 0; i < tTest.size(); ++i) {
                double tn = (tTest[i] - trainFit.tMin) / tRange;
                // Clamp to [0,1]: test points near boundary may slightly exceed.
                tn = std::max(0.0, std::min(1.0, tn));
                tTestNorm[i] = tn;
            }

            const std::vector<double> pred =
                evalBSpline(tTestNorm, trainFit.controlPoints,
                            trainFit.degree, trainFit.knots);

            for (std::size_t i = 0; i < zTest.size(); ++i) {
                const double e = pred[i] - zTest[i];
                sseTot += e * e;
                ++nTot;
            }
        }

        if (!failed && nTot > 0) {
            CvPoint pt;
            pt.nCtrl = nCtrl;
            pt.rmse  = std::sqrt(sseTot / static_cast<double>(nTot));
            curve.push_back(pt);
        }
    }

    return curve;
}

// =============================================================================
//  selectOptimalNCtrl  --  one-standard-error elbow rule
// =============================================================================

int selectOptimalNCtrl(const std::vector<CvPoint>& cv)
{
    if (cv.empty()) {
        throw DataException(
            "bsplineFit::selectOptimalNCtrl: CV curve is empty.");
    }

    // Find global minimum RMSE.
    const auto minIt = std::min_element(
        cv.begin(), cv.end(),
        [](const CvPoint& a, const CvPoint& b){ return a.rmse < b.rmse; });

    const double minRmse = minIt->rmse;

    // Compute std dev of RMSE values across the curve.
    double sum = 0.0;
    for (const auto& p : cv) sum += p.rmse;
    const double meanRmse = sum / static_cast<double>(cv.size());
    double var = 0.0;
    for (const auto& p : cv) {
        const double d = p.rmse - meanRmse;
        var += d * d;
    }
    const double stdRmse = (cv.size() > 1)
        ? std::sqrt(var / static_cast<double>(cv.size() - 1))
        : 0.0;

    // One-SE threshold: select smallest nCtrl whose RMSE <= minRmse + stdRmse.
    const double threshold = minRmse + stdRmse;
    for (const auto& p : cv) {
        if (p.rmse <= threshold) return p.nCtrl;
    }

    // Fallback: return the nCtrl at the minimum RMSE.
    return minIt->nCtrl;
}

} // namespace loki::math
