#include "loki/multivariate/multivariateProtocol.hpp"

#include "loki/core/exceptions.hpp"
#include "loki/core/logger.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

using namespace loki;
using namespace loki::multivariate;

// -----------------------------------------------------------------------------
//  Construction
// -----------------------------------------------------------------------------

MultivariateProtocol::MultivariateProtocol(const AppConfig& cfg)
    : m_cfg(cfg)
{}

// -----------------------------------------------------------------------------
//  Helpers
// -----------------------------------------------------------------------------

std::string MultivariateProtocol::_line(char c, int n)
{
    return std::string(static_cast<std::size_t>(n), c);
}

std::string MultivariateProtocol::_fmt(double v, int prec)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(prec) << v;
    return oss.str();
}

std::string MultivariateProtocol::_fmtPct(double v)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << (v * 100.0) << "%";
    return oss.str();
}

std::string MultivariateProtocol::_fmtPval(double v)
{
    if (v < 1.0e-4) return "< 0.0001";
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4) << v;
    return oss.str();
}

std::filesystem::path MultivariateProtocol::_outPath(const std::string& stem) const
{
    return m_cfg.protocolsDir / ("multivariate_" + stem + "_protocol.txt");
}

// -----------------------------------------------------------------------------
//  write
// -----------------------------------------------------------------------------

void MultivariateProtocol::write(const MultivariateSeries& data,
                                  const MultivariateResult& result,
                                  const std::string&        stem) const
{
    const auto path = _outPath(stem);
    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        throw IoException(
            "MultivariateProtocol: cannot open output file: " + path.string());
    }

    const auto& mcfg = m_cfg.multivariate;
    const int   p    = static_cast<int>(data.nChannels());
    const int   n    = static_cast<int>(data.nObs());

    // -------------------------------------------------------------------------
    //  Header
    // -------------------------------------------------------------------------
    ofs << _line('=') << "\n";
    ofs << "  LOKI Multivariate Analysis Protocol\n";
    ofs << "  Dataset : " << stem << "\n";
    ofs << _line('=') << "\n\n";

    // -------------------------------------------------------------------------
    //  1. Assembly summary
    // -------------------------------------------------------------------------
    ofs << _line('-') << "\n";
    ofs << "  1. ASSEMBLY SUMMARY\n";
    ofs << _line('-') << "\n";
    ofs << "  Observations   : " << n << "\n";
    ofs << "  Channels       : " << p << "\n";
    ofs << "  Sync strategy  : " << mcfg.input.syncStrategy << "\n";
    ofs << "  Sync tolerance : " << _fmt(mcfg.input.syncToleranceSeconds, 1) << " s\n";
    ofs << "  Standardized   : " << (mcfg.preprocessing.standardize ? "yes" : "no") << "\n\n";

    ofs << "  Channel list:\n";
    for (int j = 0; j < p; ++j) {
        ofs << "    [" << j << "] " << data.channelName(j) << "\n";
    }
    ofs << "\n";

    // -------------------------------------------------------------------------
    //  2. CCF
    // -------------------------------------------------------------------------
    if (!result.ccf.empty()) {
        ofs << _line('-') << "\n";
        ofs << "  2. CROSS-CORRELATION FUNCTION (CCF)\n";
        ofs << _line('-') << "\n";
        ofs << "  Max lag: " << mcfg.ccf.maxLag << " samples\n";
        ofs << "  Significance threshold: 1.96/sqrt(n) = "
            << _fmt(1.96 / std::sqrt(static_cast<double>(n)), 4) << "\n\n";

        ofs << "  " << std::left
            << std::setw(20) << "Channel A"
            << std::setw(20) << "Channel B"
            << std::setw(12) << "Peak lag"
            << std::setw(12) << "Peak corr"
            << "Significant\n";
        ofs << "  " << _line('-', 70) << "\n";

        for (const auto& pr : result.ccf) {
            ofs << "  " << std::left
                << std::setw(20) << pr.nameA
                << std::setw(20) << pr.nameB
                << std::setw(12) << pr.peakLag
                << std::setw(12) << _fmt(pr.peakCorr, 4)
                << (pr.significant ? "YES" : "no") << "\n";
        }
        ofs << "\n";
    }

    // -------------------------------------------------------------------------
    //  3. PCA
    // -------------------------------------------------------------------------
    if (result.pca) {
        const auto& pca = *result.pca;
        ofs << _line('-') << "\n";
        ofs << "  3. PRINCIPAL COMPONENT ANALYSIS (PCA)\n";
        ofs << _line('-') << "\n";
        ofs << "  Components retained: " << pca.nComponents << " / "
            << pca.nChannels << "\n\n";

        ofs << "  " << std::left
            << std::setw(6)  << "PC"
            << std::setw(14) << "Variance"
            << std::setw(14) << "Ratio"
            << "Cumulative\n";
        ofs << "  " << _line('-', 50) << "\n";

        for (int k = 0; k < pca.nComponents; ++k) {
            ofs << "  " << std::left
                << std::setw(6)  << (k + 1)
                << std::setw(14) << _fmt(pca.explainedVar(k), 4)
                << std::setw(14) << _fmtPct(pca.explainedVarRatio(k))
                << _fmtPct(pca.cumulativeVar(k)) << "\n";
        }
        ofs << "\n";

        ofs << "  Loadings (eigenvectors, columns = PCs):\n";
        ofs << "  " << std::left << std::setw(22) << "Channel";
        for (int k = 0; k < pca.nComponents; ++k) {
            ofs << std::setw(10) << ("PC" + std::to_string(k + 1));
        }
        ofs << "\n  " << _line('-', 22 + 10 * pca.nComponents) << "\n";

        for (int j = 0; j < pca.nChannels; ++j) {
            ofs << "  " << std::left << std::setw(22) << data.channelName(j);
            for (int k = 0; k < pca.nComponents; ++k) {
                ofs << std::setw(10) << _fmt(pca.loadings(j, k), 4);
            }
            ofs << "\n";
        }
        ofs << "\n";
    }

    // -------------------------------------------------------------------------
    //  4. MSSA
    // -------------------------------------------------------------------------
    if (result.mssa) {
        const auto& mssa = *result.mssa;
        ofs << _line('-') << "\n";
        ofs << "  4. MULTIVARIATE SSA (MSSA)\n";
        ofs << _line('-') << "\n";
        ofs << "  Window (L)   : " << mssa.window << " samples\n";
        ofs << "  Components   : " << mssa.nComponents << "\n\n";

        ofs << "  " << std::left
            << std::setw(6)  << "RC"
            << std::setw(14) << "Singular val"
            << std::setw(14) << "Var ratio"
            << "Cumulative\n";
        ofs << "  " << _line('-', 50) << "\n";

        for (int k = 0; k < mssa.nComponents; ++k) {
            ofs << "  " << std::left
                << std::setw(6)  << (k + 1)
                << std::setw(14) << _fmt(mssa.eigenvalues(k), 4)
                << std::setw(14) << _fmtPct(mssa.explainedVarRatio(k))
                << _fmtPct(mssa.cumulativeVar(k)) << "\n";
        }
        ofs << "\n";
    }

    // -------------------------------------------------------------------------
    //  5. VAR + Granger
    // -------------------------------------------------------------------------
    if (result.var_) {
        const auto& var = *result.var_;
        ofs << _line('-') << "\n";
        ofs << "  5. VECTOR AUTOREGRESSION (VAR)\n";
        ofs << _line('-') << "\n";
        ofs << "  Selected order  : " << var.order << "\n";
        ofs << "  Criterion       : " << var.criterion
            << " = " << _fmt(var.criterionVal, 4) << "\n";
        ofs << "  Observations    : " << var.nObs << "\n";
        ofs << "  Sigma trace     : " << _fmt(var.sigma.trace(), 4) << "\n\n";

        if (!var.granger.empty()) {
            ofs << "  Granger Causality F-tests "
                << "(H0: 'From' does NOT Granger-cause 'To'):\n";
            ofs << "  " << std::left
                << std::setw(24) << "From"
                << std::setw(24) << "To"
                << std::setw(12) << "F-stat"
                << std::setw(12) << "p-value"
                << "Significant\n";
            ofs << "  " << _line('-', 78) << "\n";

            for (const auto& gr : var.granger) {
                ofs << "  " << std::left
                    << std::setw(24) << gr.fromName
                    << std::setw(24) << gr.toName
                    << std::setw(12) << _fmt(gr.fStat, 3)
                    << std::setw(12) << _fmtPval(gr.pValue)
                    << (gr.significant ? "YES **" : "no") << "\n";
            }
            ofs << "\n";
        }
    }

    // -------------------------------------------------------------------------
    //  6. Factor Analysis
    // -------------------------------------------------------------------------
    if (result.factor) {
        const auto& fa = *result.factor;
        ofs << _line('-') << "\n";
        ofs << "  6. FACTOR ANALYSIS\n";
        ofs << _line('-') << "\n";
        ofs << "  Factors    : " << fa.nFactors << "\n";
        ofs << "  Rotation   : " << fa.rotationMethod << "\n\n";

        ofs << "  " << std::left << std::setw(22) << "Channel";
        for (int k = 0; k < fa.nFactors; ++k) {
            ofs << std::setw(10) << ("F" + std::to_string(k + 1));
        }
        ofs << std::setw(14) << "Communality" << "Uniqueness\n";
        ofs << "  " << _line('-', 22 + 10 * fa.nFactors + 28) << "\n";

        for (int j = 0; j < fa.nChannels; ++j) {
            ofs << "  " << std::left << std::setw(22) << data.channelName(j);
            for (int k = 0; k < fa.nFactors; ++k) {
                ofs << std::setw(10) << _fmt(fa.loadings(j, k), 4);
            }
            ofs << std::setw(14) << _fmt(fa.communalities(j), 4)
                << _fmt(fa.uniqueness(j), 4) << "\n";
        }
        ofs << "\n";
    }

    // -------------------------------------------------------------------------
    //  7. CCA
    // -------------------------------------------------------------------------
    if (result.cca) {
        const auto& cca = *result.cca;
        ofs << _line('-') << "\n";
        ofs << "  7. CANONICAL CORRELATION ANALYSIS (CCA)\n";
        ofs << _line('-') << "\n";
        ofs << "  Canonical pairs: " << cca.nPairs << "\n\n";

        ofs << "  " << std::left
            << std::setw(8) << "Pair"
            << "Canonical correlation\n";
        ofs << "  " << _line('-', 30) << "\n";
        for (int k = 0; k < cca.nPairs; ++k) {
            ofs << "  " << std::left
                << std::setw(8) << (k + 1)
                << _fmt(cca.canonicalCorrelations(k), 6) << "\n";
        }
        ofs << "\n";
    }

    // -------------------------------------------------------------------------
    //  8. LDA
    // -------------------------------------------------------------------------
    if (result.lda) {
        const auto& lda = *result.lda;
        ofs << _line('-') << "\n";
        ofs << "  8. LINEAR DISCRIMINANT ANALYSIS (LDA)\n";
        ofs << _line('-') << "\n";
        ofs << "  Method       : " << (lda.isQda ? "QDA" : "LDA") << "\n";
        ofs << "  Classes      : " << lda.nClasses << "\n";
        ofs << "  Disc. axes   : " << lda.nDims << "\n";
        ofs << "  Accuracy     : " << _fmtPct(lda.accuracy) << "\n\n";

        ofs << "  Eigenvalues per discriminant axis:\n";
        for (int k = 0; k < lda.nDims; ++k) {
            ofs << "    LD" << (k + 1) << " = "
                << _fmt(lda.eigenvalues(k), 4) << "\n";
        }
        ofs << "\n";
    }

    // -------------------------------------------------------------------------
    //  9. Mahalanobis
    // -------------------------------------------------------------------------
    if (result.mahalanobis) {
        const auto& mah = *result.mahalanobis;
        ofs << _line('-') << "\n";
        ofs << "  9. MAHALANOBIS OUTLIER DETECTION\n";
        ofs << _line('-') << "\n";
        ofs << "  Estimator      : " << (mah.robust ? "MCD robust" : "sample") << "\n";
        ofs << "  Significance   : " << mah.significanceLevel << "\n";
        ofs << "  Chi2 threshold : " << _fmt(mah.chi2Critical, 4)
            << "  (chi2(" << mah.nChannels << ", "
            << mah.significanceLevel << "))\n";
        ofs << "  Outliers       : " << mah.nOutliers
            << " / " << mah.nObs
            << " (" << _fmtPct(static_cast<double>(mah.nOutliers) / mah.nObs) << ")\n\n";

        if (!mah.outlierIndices.empty()) {
            ofs << "  Outlier indices (0-based):\n  ";
            const int maxShow = std::min(50, static_cast<int>(mah.outlierIndices.size()));
            for (int i = 0; i < maxShow; ++i) {
                ofs << mah.outlierIndices[static_cast<std::size_t>(i)];
                if (i < maxShow - 1) ofs << ", ";
            }
            if (mah.nOutliers > maxShow) {
                ofs << " ... (" << (mah.nOutliers - maxShow) << " more)";
            }
            ofs << "\n\n";
        }
    }

    // -------------------------------------------------------------------------
    //  10. MANOVA
    // -------------------------------------------------------------------------
    if (result.manova) {
        const auto& manova = *result.manova;
        ofs << _line('-') << "\n";
        ofs << "  10. MANOVA\n";
        ofs << _line('-') << "\n";
        ofs << "  Groups     : " << manova.nGroups << "\n";
        ofs << "  Channels   : " << manova.nChannels << "\n";
        ofs << "  Overall significant: "
            << (manova.significant ? "YES" : "no") << "\n\n";

        ofs << "  Test statistics:\n";
        ofs << "  " << std::left
            << std::setw(22) << "Statistic"
            << std::setw(12) << "Value"
            << std::setw(10) << "F"
            << std::setw(8)  << "df1"
            << std::setw(8)  << "df2"
            << "p-value\n";
        ofs << "  " << _line('-', 70) << "\n";

        ofs << "  " << std::left
            << std::setw(22) << "Wilks Lambda"
            << std::setw(12) << _fmt(manova.wilksLambda, 4)
            << std::setw(10) << _fmt(manova.wilksF, 3)
            << std::setw(8)  << _fmt(manova.wilksDf1, 0)
            << std::setw(8)  << _fmt(manova.wilksDf2, 0)
            << _fmtPval(manova.wilksPvalue) << "\n";

        ofs << "  " << std::left
            << std::setw(22) << "Pillai Trace"
            << std::setw(12) << _fmt(manova.pillaiTrace, 4)
            << std::setw(10) << _fmt(manova.pillaiF, 3)
            << std::setw(8)  << _fmt(manova.pillaiDf1, 0)
            << std::setw(8)  << _fmt(manova.pillaiDf2, 0)
            << _fmtPval(manova.pillaiPvalue) << "\n";

        ofs << "  " << std::left
            << std::setw(22) << "Hotelling Trace"
            << std::setw(12) << _fmt(manova.hotellingTrace, 4)
            << "\n";

        ofs << "  " << std::left
            << std::setw(22) << "Roy Max Root"
            << std::setw(12) << _fmt(manova.royRoot, 4)
            << "\n\n";
    }

    // -------------------------------------------------------------------------
    //  Footer
    // -------------------------------------------------------------------------
    ofs << _line('=') << "\n";
    ofs << "  End of protocol\n";
    ofs << _line('=') << "\n";

    ofs.close();
    LOKI_INFO("MultivariateProtocol: wrote " + path.filename().string());
}
