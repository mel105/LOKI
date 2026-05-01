#include "loki/multivariate/manova.hpp"

#include "loki/core/exceptions.hpp"
#include "loki/core/logger.hpp"

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

using namespace loki::multivariate;

// -----------------------------------------------------------------------------
//  Construction
// -----------------------------------------------------------------------------

Manova::Manova(const MultivariateManovaConfig& cfg)
    : m_cfg(cfg)
{}

// -----------------------------------------------------------------------------
//  compute
// -----------------------------------------------------------------------------

ManovaResult Manova::compute(const MultivariateSeries& data,
                              const std::vector<int>&   labels) const
{
    if (data.empty()) {
        throw DataException("Manova::compute(): data is empty.");
    }

    const int n = static_cast<int>(data.nObs());
    const int p = static_cast<int>(data.nChannels());

    if (static_cast<int>(labels.size()) != n) {
        throw DataException(
            "Manova: labels.size() != n.");
    }

    // Identify unique groups.
    std::vector<int> uniqueGroups = labels;
    std::sort(uniqueGroups.begin(), uniqueGroups.end());
    uniqueGroups.erase(std::unique(uniqueGroups.begin(), uniqueGroups.end()),
                       uniqueGroups.end());
    const int g = static_cast<int>(uniqueGroups.size());

    if (g < 2) {
        throw DataException("Manova: at least 2 groups required.");
    }
    if (n < p + g) {
        throw DataException(
            "Manova: insufficient observations. n=" + std::to_string(n)
            + " must be >= p+g=" + std::to_string(p + g) + ".");
    }

    std::map<int,int> groupIdx;
    for (int c = 0; c < g; ++c) {
        groupIdx[uniqueGroups[static_cast<std::size_t>(c)]] = c;
    }

    // -- Step 1: group statistics --------------------------------------------
    std::vector<int>            groupCount(static_cast<std::size_t>(g), 0);
    std::vector<Eigen::VectorXd> groupMean(static_cast<std::size_t>(g),
                                            Eigen::VectorXd::Zero(p));

    for (int i = 0; i < n; ++i) {
        const int c = groupIdx.at(labels[static_cast<std::size_t>(i)]);
        groupMean[static_cast<std::size_t>(c)] +=
            data.data().row(i).transpose();
        ++groupCount[static_cast<std::size_t>(c)];
    }
    for (int c = 0; c < g; ++c) {
        groupMean[static_cast<std::size_t>(c)] /=
            static_cast<double>(groupCount[static_cast<std::size_t>(c)]);
    }

    const Eigen::VectorXd grandMean = data.data().colwise().mean();

    // -- Step 2: SSCP matrices -----------------------------------------------
    Eigen::MatrixXd H = Eigen::MatrixXd::Zero(p, p); // between-group
    Eigen::MatrixXd E = Eigen::MatrixXd::Zero(p, p); // within-group

    for (int c = 0; c < g; ++c) {
        const Eigen::VectorXd d =
            groupMean[static_cast<std::size_t>(c)] - grandMean;
        H += static_cast<double>(groupCount[static_cast<std::size_t>(c)])
           * d * d.transpose();
    }

    for (int i = 0; i < n; ++i) {
        const int c = groupIdx.at(labels[static_cast<std::size_t>(i)]);
        const Eigen::VectorXd d =
            data.data().row(i).transpose()
            - groupMean[static_cast<std::size_t>(c)];
        E += d * d.transpose();
    }

    // Regularise E.
    E += Eigen::MatrixXd::Identity(p, p) * 1.0e-10 * E.trace() / p;

    // -- Step 3: eigenvalues of E^{-1} H -------------------------------------
    Eigen::GeneralizedSelfAdjointEigenSolver<Eigen::MatrixXd> geig(H, E);
    const Eigen::VectorXd& lambdaAll = geig.eigenvalues();

    // Keep positive eigenvalues only, sorted descending.
    const int s = std::min(p, g - 1);
    Eigen::VectorXd lambda(s);
    for (int i = 0; i < s; ++i) {
        const int idx = static_cast<int>(lambdaAll.size()) - 1 - i;
        lambda(i) = std::max(0.0, lambdaAll(idx));
    }

    // -- Step 4: test statistics ---------------------------------------------
    // Wilks Lambda.
    double wilks = 1.0;
    for (int i = 0; i < s; ++i) {
        wilks *= 1.0 / (1.0 + lambda(i));
    }

    // Pillai trace.
    double pillai = 0.0;
    for (int i = 0; i < s; ++i) {
        pillai += lambda(i) / (1.0 + lambda(i));
    }

    // Hotelling-Lawley trace.
    double hotelling = lambda.sum();

    // Roy maximum root.
    double roy = lambda(0) / (1.0 + lambda(0));

    // -- Step 5: Wilks F approximation (Rao 1951) ----------------------------
    const double pd = static_cast<double>(p);
    const double gd = static_cast<double>(g);
    const double nd = static_cast<double>(n);

    // df1, df2 for Wilks F.
    const double df1w = pd * (gd - 1.0);
    const double t    = std::sqrt((pd*pd * (gd-1.0)*(gd-1.0) - 4.0)
                                 / (pd*pd + (gd-1.0)*(gd-1.0) - 5.0));
    const double df2w = (nd - 1.0 - (pd + gd) / 2.0) * t - (df1w - 2.0) / 2.0;

    const double wilksF = (df2w > 0.0 && wilks > 0.0)
        ? ((1.0 - std::pow(wilks, 1.0/t)) / std::pow(wilks, 1.0/t))
          * (df2w / df1w)
        : 0.0;
    const double wilksPval = _fPvalue(wilksF, df1w, df2w);

    // Pillai F approximation.
    const double sm   = static_cast<double>(s);
    const double mPil = (std::abs(pd - gd + 1.0) - 1.0) / 2.0;
    const double nPil = (nd - gd - pd - 1.0) / 2.0;
    const double df1p = sm * (2.0 * mPil + sm + 1.0);
    const double df2p = sm * (2.0 * nPil + sm + 1.0);
    const double pillaiF = (df2p > 0.0 && pillai < sm)
        ? (pillai / sm) / ((sm - pillai) / sm) * (df2p / df1p)
        : 0.0;
    const double pillaiPval = _fPvalue(pillaiF, df1p, df2p);

    LOKI_INFO("Manova: g=" + std::to_string(g)
              + " p=" + std::to_string(p)
              + " Wilks=" + std::to_string(wilks)
              + " F=" + std::to_string(wilksF)
              + " p=" + std::to_string(wilksPval) + ".");

    // -- Step 6: assemble result ---------------------------------------------
    ManovaResult result;
    result.wilksLambda   = wilks;
    result.wilksF        = wilksF;
    result.wilksDf1      = df1w;
    result.wilksDf2      = df2w;
    result.wilksPvalue   = wilksPval;
    result.pillaiTrace   = pillai;
    result.pillaiF       = pillaiF;
    result.pillaiDf1     = df1p;
    result.pillaiDf2     = df2p;
    result.pillaiPvalue  = pillaiPval;
    result.hotellingTrace= hotelling;
    result.royRoot       = roy;
    result.eigenvalues   = lambda;
    result.groupMeans    = groupMean;
    result.nGroups       = g;
    result.nObs          = n;
    result.nChannels     = p;
    result.significant   = (wilksPval < m_cfg.significanceLevel);

    return result;
}

// -----------------------------------------------------------------------------
//  _fPvalue  (identical logic to var.cpp -- self-contained)
// -----------------------------------------------------------------------------

double Manova::_fPvalue(double fStat, double df1, double df2)
{
    if (fStat <= 0.0 || df1 <= 0.0 || df2 <= 0.0) return 1.0;

    const double x = df2 / (df2 + df1 * fStat);
    const double a = df2 * 0.5;
    const double b = df1 * 0.5;
    const double logBeta =
        std::lgamma(a) + std::lgamma(b) - std::lgamma(a + b);

    if (x <= 0.0) return 1.0;
    if (x >= 1.0) return 0.0;

    double aa = a, bb = b, xx = x;
    bool swapped = false;
    if (xx > (aa + 1.0) / (aa + bb + 2.0)) {
        std::swap(aa, bb);
        xx = 1.0 - xx;
        swapped = true;
    }

    const double front =
        std::exp(aa * std::log(xx) + bb * std::log(1.0 - xx) - logBeta) / aa;

    constexpr int    MAXITER = 200;
    constexpr double EPS     = 1.0e-12;
    constexpr double FPMIN   = 1.0e-300;

    double f = FPMIN, C = f, D = 0.0;
    for (int m2 = 0; m2 <= MAXITER * 2; ++m2) {
        double num;
        const int m = m2 / 2;
        if (m2 == 0) {
            num = 1.0;
        } else if (m2 % 2 == 0) {
            num = static_cast<double>(m) * (bb - static_cast<double>(m)) * xx
                / ((aa + static_cast<double>(2*m) - 1.0)
                   * (aa + static_cast<double>(2*m)));
        } else {
            num = -(aa + static_cast<double>(m))
                 * (aa + bb + static_cast<double>(m)) * xx
                 / ((aa + static_cast<double>(2*m))
                    * (aa + static_cast<double>(2*m) + 1.0));
        }
        D = 1.0 + num * D; if (std::abs(D) < FPMIN) D = FPMIN;
        C = 1.0 + num / C; if (std::abs(C) < FPMIN) C = FPMIN;
        D = 1.0 / D;
        const double delta = C * D;
        f *= delta;
        if (std::abs(delta - 1.0) < EPS) break;
    }

    const double Ix   = front * f;
    const double pval = swapped ? (1.0 - Ix) : Ix;
    return std::max(0.0, std::min(1.0, pval));
}
