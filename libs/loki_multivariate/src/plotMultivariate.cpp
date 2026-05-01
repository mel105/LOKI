#include "loki/multivariate/plotMultivariate.hpp"

#include "loki/io/gnuplot.hpp"
#include "loki/core/logger.hpp"
#include "loki/core/exceptions.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

using namespace loki;
using namespace loki::multivariate;

// ----------------------------------------------------------------------------
//  Helpers
// ----------------------------------------------------------------------------

static std::string fwdSlash(const std::filesystem::path& p)
{
    std::string s = p.string();
    for (auto& c : s) { if (c == '\\') c = '/'; }
    return s;
}

// Formats a double with fixed precision for gnuplot inline data.
static std::string fmt(double v, int prec = 6)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(prec) << v;
    return oss.str();
}

// ----------------------------------------------------------------------------
//  Construction
// ----------------------------------------------------------------------------

PlotMultivariate::PlotMultivariate(const AppConfig& cfg)
    : m_cfg(cfg)
{}

// ----------------------------------------------------------------------------
//  plotAll
// ----------------------------------------------------------------------------

void PlotMultivariate::plotAll(const MultivariateSeries& data,
                                const MultivariateResult& result,
                                const std::string&        stem) const
{
    const auto& p = m_cfg.plots;

    // -- CCF ------------------------------------------------------------------
    if (p.mvCorrelationMatrix) {
        try { plotCorrelationMatrix(data, stem); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotMultivariate: correlation matrix failed: "
                         + std::string(ex.what()));
        }
    }
    if (p.mvCcfHeatmap && !result.ccf.empty()) {
        try { plotCcfHeatmap(data, result.ccf, stem); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotMultivariate: CCF heatmap failed: "
                         + std::string(ex.what()));
        }
    }

    // -- PCA ------------------------------------------------------------------
    if (result.pca) {
        if (p.mvPcaScree) {
            try { plotPcaScree(*result.pca, stem); }
            catch (const LOKIException& ex) {
                LOKI_WARNING("PlotMultivariate: PCA scree failed: "
                             + std::string(ex.what()));
            }
        }
        if (p.mvPcaBiplot) {
            try { plotPcaBiplot(*result.pca, data, stem); }
            catch (const LOKIException& ex) {
                LOKI_WARNING("PlotMultivariate: PCA biplot failed: "
                             + std::string(ex.what()));
            }
        }
        if (p.mvPcaScores) {
            try { plotPcaScores(*result.pca, data, stem); }
            catch (const LOKIException& ex) {
                LOKI_WARNING("PlotMultivariate: PCA scores failed: "
                             + std::string(ex.what()));
            }
        }
    }

    // -- MSSA -----------------------------------------------------------------
    if (result.mssa) {
        if (p.mvMssaEigenvalues) {
            try { plotMssaEigenvalues(*result.mssa, stem); }
            catch (const LOKIException& ex) {
                LOKI_WARNING("PlotMultivariate: MSSA eigenvalues failed: "
                             + std::string(ex.what()));
            }
        }
        if (p.mvMssaComponents) {
            try { plotMssaComponents(*result.mssa, data, stem); }
            catch (const LOKIException& ex) {
                LOKI_WARNING("PlotMultivariate: MSSA components failed: "
                             + std::string(ex.what()));
            }
        }
    }

    // -- VAR ------------------------------------------------------------------
    if (result.var_) {
        if (p.mvVarCoefficients) {
            try { plotVarCoefficients(*result.var_, data, stem); }
            catch (const LOKIException& ex) {
                LOKI_WARNING("PlotMultivariate: VAR coefficients failed: "
                             + std::string(ex.what()));
            }
        }
        if (p.mvVarResiduals) {
            try { plotVarResiduals(*result.var_, data, stem); }
            catch (const LOKIException& ex) {
                LOKI_WARNING("PlotMultivariate: VAR residuals failed: "
                             + std::string(ex.what()));
            }
        }
        if (p.mvGrangerHeatmap && !result.var_->granger.empty()) {
            try { plotGrangerHeatmap(*result.var_, data, stem); }
            catch (const LOKIException& ex) {
                LOKI_WARNING("PlotMultivariate: Granger heatmap failed: "
                             + std::string(ex.what()));
            }
        }
    }

    // -- Factor ---------------------------------------------------------------
    if (result.factor) {
        if (p.mvFactorHeatmap) {
            try { plotFactorLoadings(*result.factor, data, stem); }
            catch (const LOKIException& ex) {
                LOKI_WARNING("PlotMultivariate: factor loadings failed: "
                             + std::string(ex.what()));
            }
        }
        if (p.mvFactorScores) {
            try { plotFactorScores(*result.factor, data, stem); }
            catch (const LOKIException& ex) {
                LOKI_WARNING("PlotMultivariate: factor scores failed: "
                             + std::string(ex.what()));
            }
        }
    }

    // -- CCA ------------------------------------------------------------------
    if (result.cca) {
        if (p.mvCcaCorrelations) {
            try { plotCcaCorrelations(*result.cca, stem); }
            catch (const LOKIException& ex) {
                LOKI_WARNING("PlotMultivariate: CCA correlations failed: "
                             + std::string(ex.what()));
            }
        }
        if (p.mvCcaScatterPairs) {
            try { plotCcaScatterPairs(*result.cca, stem); }
            catch (const LOKIException& ex) {
                LOKI_WARNING("PlotMultivariate: CCA scatter pairs failed: "
                             + std::string(ex.what()));
            }
        }
    }

    // -- LDA ------------------------------------------------------------------
    if (result.lda) {
        if (p.mvLdaProjection) {
            try { plotLdaProjection(*result.lda, stem); }
            catch (const LOKIException& ex) {
                LOKI_WARNING("PlotMultivariate: LDA projection failed: "
                             + std::string(ex.what()));
            }
        }
        if (p.mvLdaConfusion) {
            try { plotLdaConfusion(*result.lda, stem); }
            catch (const LOKIException& ex) {
                LOKI_WARNING("PlotMultivariate: LDA confusion failed: "
                             + std::string(ex.what()));
            }
        }
    }

    // -- Mahalanobis ----------------------------------------------------------
    if (result.mahalanobis) {
        if (p.mvMahalanobisDist) {
            try { plotMahalanobisDist(*result.mahalanobis, stem); }
            catch (const LOKIException& ex) {
                LOKI_WARNING("PlotMultivariate: Mahalanobis dist failed: "
                             + std::string(ex.what()));
            }
        }
        if (p.mvMahalanobisQq) {
            try { plotMahalanobisQq(*result.mahalanobis, stem); }
            catch (const LOKIException& ex) {
                LOKI_WARNING("PlotMultivariate: Mahalanobis QQ failed: "
                             + std::string(ex.what()));
            }
        }
    }

    // -- MANOVA ---------------------------------------------------------------
    if (result.manova) {
        if (p.mvManovaEigenvalues) {
            try { plotManovaEigenvalues(*result.manova, stem); }
            catch (const LOKIException& ex) {
                LOKI_WARNING("PlotMultivariate: MANOVA eigenvalues failed: "
                             + std::string(ex.what()));
            }
        }
    }
}

// ----------------------------------------------------------------------------
//  Helpers
// ----------------------------------------------------------------------------

std::filesystem::path PlotMultivariate::_outPath(const std::string& stem,
                                                   const std::string& tag) const
{
    const std::string fmt = m_cfg.plots.outputFormat.empty()
        ? "png" : m_cfg.plots.outputFormat;
    return m_cfg.imgDir / ("multivariate_" + stem + "_" + tag + "." + fmt);
}

std::string PlotMultivariate::_fwdSlash(const std::filesystem::path& p)
{
    return fwdSlash(p);
}

void PlotMultivariate::_setPngOutput(Gnuplot& gp,
                                      const std::filesystem::path& outFile,
                                      int widthPx, int heightPx) const
{
    gp("set terminal pngcairo noenhanced font 'Sans,12' size "
       + std::to_string(widthPx) + "," + std::to_string(heightPx));
    gp("set output '" + _fwdSlash(outFile) + "'");
}

// ----------------------------------------------------------------------------
//  plotCorrelationMatrix
// ----------------------------------------------------------------------------

void PlotMultivariate::plotCorrelationMatrix(const MultivariateSeries& data,
                                              const std::string& stem) const
{
    const int p = static_cast<int>(data.nChannels());
    const int n = static_cast<int>(data.nObs());

    // Compute Pearson correlation matrix.
    Eigen::MatrixXd C(p, p);
    const Eigen::VectorXd mean = data.data().colwise().mean();
    Eigen::MatrixXd Xc = data.data().rowwise() - mean.transpose();

    for (int i = 0; i < p; ++i) {
        for (int j = 0; j < p; ++j) {
            const double si = std::sqrt(Xc.col(i).squaredNorm() / (n - 1));
            const double sj = std::sqrt(Xc.col(j).squaredNorm() / (n - 1));
            if (si > 0.0 && sj > 0.0) {
                C(i, j) = Xc.col(i).dot(Xc.col(j))
                         / (static_cast<double>(n - 1) * si * sj);
            } else {
                C(i, j) = 0.0;
            }
        }
    }

    const auto outFile = _outPath(stem, "correlation_matrix");
    Gnuplot gp;
    _setPngOutput(gp, outFile, 800, 700);

    gp("set title 'Pearson Correlation Matrix' noenhanced");
    gp("set xrange [-0.5:" + std::to_string(p - 1) + ".5]");
    gp("set yrange [-0.5:" + std::to_string(p - 1) + ".5]");
    gp("set cbrange [-1:1]");
    gp("set palette defined (-1 'blue', 0 'white', 1 'red')");
    gp("set view map");
    gp("set size square");
    gp("unset key");

    // X/Y tic labels = channel names.
    std::string xtics = "set xtics (";
    std::string ytics = "set ytics (";
    for (int j = 0; j < p; ++j) {
        const std::string name = "'" + data.channelName(j) + "' " + std::to_string(j);
        xtics += name + (j < p - 1 ? ", " : ")");
        ytics += name + (j < p - 1 ? ", " : ")");
    }
    gp(xtics);
    gp(ytics);
    gp("set xtics rotate by -45");

    // Send as nonuniform matrix.
    std::string dataStr = "$mat << EOD\n";
    for (int i = p - 1; i >= 0; --i) {
        for (int j = 0; j < p; ++j) {
            dataStr += fmt(C(i, j), 4) + (j < p - 1 ? " " : "\n");
        }
    }
    dataStr += "EOD";
    gp(dataStr);

    gp("plot $mat matrix rowheaders columnheaders using 1:2:3 with image");

    LOKI_INFO("PlotMultivariate: wrote " + outFile.filename().string());
}

// ----------------------------------------------------------------------------
//  plotCcfHeatmap
// ----------------------------------------------------------------------------

void PlotMultivariate::plotCcfHeatmap(const MultivariateSeries& data,
                                       const std::vector<CcfPairResult>& ccf,
                                       const std::string& stem) const
{
    const int p = static_cast<int>(data.nChannels());
    const auto outFile = _outPath(stem, "ccf_heatmap");

    // Build p x p matrix of peak correlations (only lower triangle filled).
    Eigen::MatrixXd peakMat = Eigen::MatrixXd::Zero(p, p);
    for (const auto& pr : ccf) {
        peakMat(pr.channelA, pr.channelB) = pr.peakCorr;
        peakMat(pr.channelB, pr.channelA) = pr.peakCorr;
    }

    Gnuplot gp;
    _setPngOutput(gp, outFile, 800, 700);
    gp("set title 'CCF Peak Correlations' noenhanced");
    gp("set cbrange [-1:1]");
    gp("set palette defined (-1 'blue', 0 'white', 1 'red')");
    gp("set view map");
    gp("set size square");
    gp("unset key");
    gp("set xrange [-0.5:" + std::to_string(p - 1) + ".5]");
    gp("set yrange [-0.5:" + std::to_string(p - 1) + ".5]");

    std::string xtics = "set xtics (";
    std::string ytics = "set ytics (";
    for (int j = 0; j < p; ++j) {
        const std::string name = "'" + data.channelName(j) + "' " + std::to_string(j);
        xtics += name + (j < p - 1 ? ", " : ")");
        ytics += name + (j < p - 1 ? ", " : ")");
    }
    gp(xtics); gp(ytics);
    gp("set xtics rotate by -45");

    std::string dataStr = "$ccf << EOD\n";
    for (int i = p - 1; i >= 0; --i) {
        for (int j = 0; j < p; ++j) {
            dataStr += fmt(peakMat(i, j), 4) + (j < p - 1 ? " " : "\n");
        }
    }
    dataStr += "EOD";
    gp(dataStr);
    gp("plot $ccf matrix rowheaders columnheaders using 1:2:3 with image");

    LOKI_INFO("PlotMultivariate: wrote " + outFile.filename().string());
}

// ----------------------------------------------------------------------------
//  plotPcaScree
// ----------------------------------------------------------------------------

void PlotMultivariate::plotPcaScree(const PcaResult& pca,
                                     const std::string& stem) const
{
    const auto outFile = _outPath(stem, "pca_scree");
    const int k = static_cast<int>(pca.explainedVarRatio.size());

    Gnuplot gp;
    _setPngOutput(gp, outFile, 900, 500);
    gp("set title 'PCA Scree Plot' noenhanced");
    gp("set xlabel 'Component'");
    gp("set ylabel 'Explained Variance Ratio'");
    gp("set y2label 'Cumulative Variance'");
    gp("set ytics nomirror");
    gp("set y2tics");
    gp("set y2range [0:1.05]");
    gp("set yrange [0:*]");
    gp("set key top right");
    gp("set grid");
    gp("set style data histogram");
    gp("set style fill solid 0.7");

    // Datablock.
    std::string db = "$scree << EOD\n";
    db += "# comp ratio cumul\n";
    for (int i = 0; i < k; ++i) {
        db += std::to_string(i + 1) + " "
            + fmt(pca.explainedVarRatio(i)) + " "
            + fmt(pca.cumulativeVar(i)) + "\n";
    }
    db += "EOD";
    gp(db);

    gp("plot $scree using 2:xtic(1) axes x1y1 title 'Variance ratio' lc rgb '#4472C4', "
       "     $scree using 3 axes x1y2 with linespoints title 'Cumulative' lc rgb '#ED7D31' pt 7 ps 1");

    LOKI_INFO("PlotMultivariate: wrote " + outFile.filename().string());
}

// ----------------------------------------------------------------------------
//  plotPcaBiplot
// ----------------------------------------------------------------------------

void PlotMultivariate::plotPcaBiplot(const PcaResult& pca,
                                      const MultivariateSeries& data,
                                      const std::string& stem) const
{
    if (pca.nComponents < 2) return;

    const auto outFile = _outPath(stem, "pca_biplot");
    const int n = static_cast<int>(pca.scores.rows());
    const int p = static_cast<int>(pca.loadings.rows());

    // Scale factor for loading arrows (scale to score range).
    const double scaleX = pca.scores.col(0).cwiseAbs().maxCoeff();
    const double scaleY = pca.scores.col(1).cwiseAbs().maxCoeff();
    const double scale  = std::max(scaleX, scaleY) * 0.8;

    Gnuplot gp;
    _setPngOutput(gp, outFile, 900, 800);
    gp("set title 'PCA Biplot (PC1 vs PC2)' noenhanced");
    gp("set xlabel 'PC1 (" + std::to_string(static_cast<int>(
        pca.explainedVarRatio(0) * 100.0)) + "%)' noenhanced");
    gp("set ylabel 'PC2 (" + std::to_string(static_cast<int>(
        pca.explainedVarRatio(1) * 100.0)) + "%)' noenhanced");
    gp("set key top left");
    gp("set grid");

    // Scores datablock.
    std::string sc = "$scores << EOD\n";
    for (int i = 0; i < n; ++i) {
        sc += fmt(pca.scores(i, 0)) + " " + fmt(pca.scores(i, 1)) + "\n";
    }
    sc += "EOD";
    gp(sc);

    // Loadings datablock (arrows from origin).
    std::string ld = "$loadings << EOD\n";
    for (int j = 0; j < p; ++j) {
        ld += "0 0 "
            + fmt(pca.loadings(j, 0) * scale) + " "
            + fmt(pca.loadings(j, 1) * scale) + " "
            + "'" + data.channelName(j) + "'\n";
    }
    ld += "EOD";
    gp(ld);

    gp("plot $scores using 1:2 with points pt 7 ps 0.6 lc rgb '#4472C4' title 'Scores', "
       "     $loadings using 1:2:3:4 with vectors head filled lc rgb '#ED7D31' title 'Loadings', "
       "     $loadings using ($3*1.08):($4*1.08):5 with labels font 'Sans,10' tc rgb '#ED7D31' notitle");

    LOKI_INFO("PlotMultivariate: wrote " + outFile.filename().string());
}

// ----------------------------------------------------------------------------
//  plotPcaScores
// ----------------------------------------------------------------------------

void PlotMultivariate::plotPcaScores(const PcaResult& pca,
                                      const MultivariateSeries& data,
                                      const std::string& stem) const
{
    const int k = pca.nComponents;
    const int n = static_cast<int>(pca.scores.rows());
    const std::vector<double> mjd = data.mjdAxis();

    const auto outFile = _outPath(stem, "pca_scores");
    Gnuplot gp;
    _setPngOutput(gp, outFile, 1000, 200 * k);

    gp("set multiplot layout " + std::to_string(k) + ",1 title 'PCA Scores' noenhanced");

    for (int i = 0; i < k; ++i) {
        std::string db = "$s" + std::to_string(i) + " << EOD\n";
        for (int t = 0; t < n; ++t) {
            db += fmt(mjd[static_cast<std::size_t>(t)]) + " "
                + fmt(pca.scores(t, i)) + "\n";
        }
        db += "EOD";
        gp(db);

        gp("set ylabel 'PC" + std::to_string(i + 1) + "' noenhanced");
        if (i == k - 1) gp("set xlabel 'MJD'");
        gp("set grid");
        gp("plot $s" + std::to_string(i)
           + " using 1:2 with lines lc rgb '#4472C4' notitle");
    }
    gp("unset multiplot");

    LOKI_INFO("PlotMultivariate: wrote " + outFile.filename().string());
}

// ----------------------------------------------------------------------------
//  plotMssaEigenvalues
// ----------------------------------------------------------------------------

void PlotMultivariate::plotMssaEigenvalues(const MssaResult& mssa,
                                            const std::string& stem) const
{
    const auto outFile = _outPath(stem, "mssa_eigenvalues");
    const int k = static_cast<int>(mssa.eigenvalues.size());

    Gnuplot gp;
    _setPngOutput(gp, outFile, 900, 500);
    gp("set title 'MSSA Eigenvalue Spectrum' noenhanced");
    gp("set xlabel 'Component'");
    gp("set ylabel 'Singular Value'");
    gp("set y2label 'Cumulative Variance'");
    gp("set ytics nomirror");
    gp("set y2tics");
    gp("set y2range [0:1.05]");
    gp("set grid");
    gp("set style data histogram");
    gp("set style fill solid 0.7");

    std::string db = "$ev << EOD\n";
    for (int i = 0; i < k; ++i) {
        db += std::to_string(i + 1) + " "
            + fmt(mssa.eigenvalues(i)) + " "
            + fmt(mssa.cumulativeVar(i)) + "\n";
    }
    db += "EOD";
    gp(db);

    gp("plot $ev using 2:xtic(1) axes x1y1 title 'Singular value' lc rgb '#4472C4', "
       "     $ev using 3 axes x1y2 with linespoints title 'Cumulative' lc rgb '#ED7D31' pt 7 ps 1");

    LOKI_INFO("PlotMultivariate: wrote " + outFile.filename().string());
}

// ----------------------------------------------------------------------------
//  plotMssaComponents
// ----------------------------------------------------------------------------

void PlotMultivariate::plotMssaComponents(const MssaResult& mssa,
                                           const MultivariateSeries& data,
                                           const std::string& stem) const
{
    const int k = mssa.nComponents;
    const int n = mssa.nObs;
    const std::vector<double> mjd = data.mjdAxis();

    const auto outFile = _outPath(stem, "mssa_components");
    Gnuplot gp;
    _setPngOutput(gp, outFile, 1000, 200 * k);

    gp("set multiplot layout " + std::to_string(k) + ",1 "
       "title 'MSSA Reconstructed Components' noenhanced");

    for (int i = 0; i < k; ++i) {
        std::string db = "$rc" + std::to_string(i) + " << EOD\n";
        for (int t = 0; t < n; ++t) {
            db += fmt(mjd[static_cast<std::size_t>(t)]) + " "
                + fmt(mssa.reconstruction(t, i)) + "\n";
        }
        db += "EOD";
        gp(db);

        gp("set ylabel 'RC" + std::to_string(i + 1) + "' noenhanced");
        if (i == k - 1) gp("set xlabel 'MJD'");
        gp("set grid");
        gp("plot $rc" + std::to_string(i)
           + " using 1:2 with lines lc rgb '#4472C4' notitle");
    }
    gp("unset multiplot");

    LOKI_INFO("PlotMultivariate: wrote " + outFile.filename().string());
}

// ----------------------------------------------------------------------------
//  plotVarCoefficients
// ----------------------------------------------------------------------------

void PlotMultivariate::plotVarCoefficients(const VarResult& var,
                                            const MultivariateSeries& data,
                                            const std::string& stem) const
{
    const int q = var.nChannels;

    for (int lag = 0; lag < var.order; ++lag) {
        const auto outFile = _outPath(stem, "var_coeff_lag" + std::to_string(lag + 1));
        Gnuplot gp;
        _setPngOutput(gp, outFile, 700, 600);

        const Eigen::MatrixXd& A = var.coefficients[static_cast<std::size_t>(lag)];
        const double absMax = A.cwiseAbs().maxCoeff();
        const double cmax   = (absMax > 0.0) ? absMax : 1.0;

        gp("set title 'VAR Coefficients A_{" + std::to_string(lag + 1)
           + "}' noenhanced");
        gp("set cbrange [" + fmt(-cmax) + ":" + fmt(cmax) + "]");
        gp("set palette defined (-1 'blue', 0 'white', 1 'red')");
        gp("set view map");
        gp("set size square");
        gp("unset key");
        gp("set xrange [-0.5:" + std::to_string(q - 1) + ".5]");
        gp("set yrange [-0.5:" + std::to_string(q - 1) + ".5]");
        gp("set xlabel 'From channel (predictor)' noenhanced");
        gp("set ylabel 'To channel (response)' noenhanced");

        std::string xtics = "set xtics (";
        std::string ytics = "set ytics (";
        for (int j = 0; j < q; ++j) {
            const std::string name = "'" + data.channelName(j) + "' "
                                   + std::to_string(j);
            xtics += name + (j < q - 1 ? ", " : ")");
            ytics += name + (j < q - 1 ? ", " : ")");
        }
        gp(xtics); gp(ytics);
        gp("set xtics rotate by -45");

        std::string db = "$coeff << EOD\n";
        for (int i = q - 1; i >= 0; --i) {
            for (int j = 0; j < q; ++j) {
                db += fmt(A(i, j), 4) + (j < q - 1 ? " " : "\n");
            }
        }
        db += "EOD";
        gp(db);
        gp("plot $coeff matrix rowheaders columnheaders using 1:2:3 with image");

        LOKI_INFO("PlotMultivariate: wrote " + outFile.filename().string());
    }
}

// ----------------------------------------------------------------------------
//  plotVarResiduals
// ----------------------------------------------------------------------------

void PlotMultivariate::plotVarResiduals(const VarResult& var,
                                         const MultivariateSeries& data,
                                         const std::string& stem) const
{
    const int q = var.nChannels;
    const int T = var.nObs;
    // Residuals cover the last T observations of the MJD axis.
    const std::vector<double> mjdAll = data.mjdAxis();
    const int offset = static_cast<int>(mjdAll.size()) - T;

    const auto outFile = _outPath(stem, "var_residuals");
    Gnuplot gp;
    _setPngOutput(gp, outFile, 1000, 180 * q);
    gp("set multiplot layout " + std::to_string(q) + ",1 "
       "title 'VAR Residuals' noenhanced");

    for (int j = 0; j < q; ++j) {
        std::string db = "$r" + std::to_string(j) + " << EOD\n";
        for (int t = 0; t < T; ++t) {
            const int mjdIdx = offset + t;
            db += fmt(mjdAll[static_cast<std::size_t>(mjdIdx)]) + " "
                + fmt(var.residuals(t, j)) + "\n";
        }
        db += "EOD";
        gp(db);

        gp("set ylabel '" + data.channelName(j) + "' noenhanced");
        if (j == q - 1) gp("set xlabel 'MJD'");
        gp("set grid");
        gp("plot $r" + std::to_string(j)
           + " using 1:2 with lines lc rgb '#4472C4' notitle");
    }
    gp("unset multiplot");

    LOKI_INFO("PlotMultivariate: wrote " + outFile.filename().string());
}

// ----------------------------------------------------------------------------
//  plotGrangerHeatmap
// ----------------------------------------------------------------------------

void PlotMultivariate::plotGrangerHeatmap(const VarResult& var,
                                           const MultivariateSeries& data,
                                           const std::string& stem) const
{
    const int q = var.nChannels;
    const auto outFile = _outPath(stem, "granger_heatmap");

    // Matrix: row = 'to', col = 'from'. Value = -log10(pValue), capped at 5.
    Eigen::MatrixXd mat = Eigen::MatrixXd::Zero(q, q);
    for (const auto& gr : var.granger) {
        const double val = (gr.pValue > 0.0)
            ? std::min(5.0, -std::log10(gr.pValue))
            : 5.0;
        mat(gr.toChannel, gr.fromChannel) = val;
    }

    Gnuplot gp;
    _setPngOutput(gp, outFile, 800, 700);
    gp("set title 'Granger Causality (-log10 p-value)' noenhanced");
    gp("set cbrange [0:5]");
    gp("set palette defined (0 'white', 1.3 'yellow', 2 'orange', 5 'red')");
    gp("set view map");
    gp("set size square");
    gp("unset key");
    gp("set xrange [-0.5:" + std::to_string(q - 1) + ".5]");
    gp("set yrange [-0.5:" + std::to_string(q - 1) + ".5]");
    gp("set xlabel 'From (cause)' noenhanced");
    gp("set ylabel 'To (effect)' noenhanced");

    std::string xtics = "set xtics (";
    std::string ytics = "set ytics (";
    for (int j = 0; j < q; ++j) {
        const std::string name = "'" + data.channelName(j) + "' " + std::to_string(j);
        xtics += name + (j < q - 1 ? ", " : ")");
        ytics += name + (j < q - 1 ? ", " : ")");
    }
    gp(xtics); gp(ytics);
    gp("set xtics rotate by -45");

    std::string db = "$gr << EOD\n";
    for (int i = q - 1; i >= 0; --i) {
        for (int j = 0; j < q; ++j) {
            db += fmt(mat(i, j), 3) + (j < q - 1 ? " " : "\n");
        }
    }
    db += "EOD";
    gp(db);
    gp("plot $gr matrix rowheaders columnheaders using 1:2:3 with image");

    LOKI_INFO("PlotMultivariate: wrote " + outFile.filename().string());
}

// ----------------------------------------------------------------------------
//  plotFactorLoadings
// ----------------------------------------------------------------------------

void PlotMultivariate::plotFactorLoadings(const FactorAnalysisResult& fa,
                                           const MultivariateSeries& data,
                                           const std::string& stem) const
{
    const int p = fa.nChannels;
    const int k = fa.nFactors;
    const auto outFile = _outPath(stem, "factor_loadings");

    const double cmax = fa.loadings.cwiseAbs().maxCoeff();

    Gnuplot gp;
    _setPngOutput(gp, outFile, 700, 600);
    gp("set title 'Factor Loadings (" + fa.rotationMethod + " rotation)' noenhanced");
    gp("set cbrange [" + fmt(-cmax) + ":" + fmt(cmax) + "]");
    gp("set palette defined (-1 'blue', 0 'white', 1 'red')");
    gp("set view map");
    gp("unset key");
    gp("set xrange [-0.5:" + std::to_string(k - 1) + ".5]");
    gp("set yrange [-0.5:" + std::to_string(p - 1) + ".5]");
    gp("set xlabel 'Factor' noenhanced");
    gp("set ylabel 'Variable' noenhanced");

    std::string xtics = "set xtics (";
    for (int j = 0; j < k; ++j) {
        xtics += "'F" + std::to_string(j + 1) + "' " + std::to_string(j)
               + (j < k - 1 ? ", " : ")");
    }
    std::string ytics = "set ytics (";
    for (int j = 0; j < p; ++j) {
        ytics += "'" + data.channelName(j) + "' " + std::to_string(j)
               + (j < p - 1 ? ", " : ")");
    }
    gp(xtics); gp(ytics);

    std::string db = "$fl << EOD\n";
    for (int i = p - 1; i >= 0; --i) {
        for (int j = 0; j < k; ++j) {
            db += fmt(fa.loadings(i, j), 4) + (j < k - 1 ? " " : "\n");
        }
    }
    db += "EOD";
    gp(db);
    gp("plot $fl matrix rowheaders columnheaders using 1:2:3 with image");

    LOKI_INFO("PlotMultivariate: wrote " + outFile.filename().string());
}

// ----------------------------------------------------------------------------
//  plotFactorScores
// ----------------------------------------------------------------------------

void PlotMultivariate::plotFactorScores(const FactorAnalysisResult& fa,
                                         const MultivariateSeries& data,
                                         const std::string& stem) const
{
    const int k = fa.nFactors;
    const int n = fa.nObs;
    const std::vector<double> mjd = data.mjdAxis();

    const auto outFile = _outPath(stem, "factor_scores");
    Gnuplot gp;
    _setPngOutput(gp, outFile, 1000, 200 * k);
    gp("set multiplot layout " + std::to_string(k) + ",1 "
       "title 'Factor Scores' noenhanced");

    for (int i = 0; i < k; ++i) {
        std::string db = "$fs" + std::to_string(i) + " << EOD\n";
        for (int t = 0; t < n; ++t) {
            db += fmt(mjd[static_cast<std::size_t>(t)]) + " "
                + fmt(fa.scores(t, i)) + "\n";
        }
        db += "EOD";
        gp(db);

        gp("set ylabel 'F" + std::to_string(i + 1) + "' noenhanced");
        if (i == k - 1) gp("set xlabel 'MJD'");
        gp("set grid");
        gp("plot $fs" + std::to_string(i)
           + " using 1:2 with lines lc rgb '#4472C4' notitle");
    }
    gp("unset multiplot");

    LOKI_INFO("PlotMultivariate: wrote " + outFile.filename().string());
}

// ----------------------------------------------------------------------------
//  plotCcaCorrelations
// ----------------------------------------------------------------------------

void PlotMultivariate::plotCcaCorrelations(const CcaResult& cca,
                                            const std::string& stem) const
{
    const auto outFile = _outPath(stem, "cca_correlations");
    const int k = cca.nPairs;

    Gnuplot gp;
    _setPngOutput(gp, outFile, 800, 450);
    gp("set title 'Canonical Correlations' noenhanced");
    gp("set xlabel 'Canonical pair'");
    gp("set ylabel 'Correlation'");
    gp("set yrange [0:1.05]");
    gp("set grid");
    gp("set style data histogram");
    gp("set style fill solid 0.7");

    std::string db = "$rho << EOD\n";
    for (int i = 0; i < k; ++i) {
        db += std::to_string(i + 1) + " "
            + fmt(cca.canonicalCorrelations(i)) + "\n";
    }
    db += "EOD";
    gp(db);
    gp("plot $rho using 2:xtic(1) lc rgb '#4472C4' notitle");

    LOKI_INFO("PlotMultivariate: wrote " + outFile.filename().string());
}

// ----------------------------------------------------------------------------
//  plotCcaScatterPairs  (first canonical pair only)
// ----------------------------------------------------------------------------

void PlotMultivariate::plotCcaScatterPairs(const CcaResult& cca,
                                            const std::string& stem) const
{
    const auto outFile = _outPath(stem, "cca_scatter_pair1");
    const int n = cca.nObs;

    Gnuplot gp;
    _setPngOutput(gp, outFile, 700, 600);
    gp("set title 'CCA Canonical Variates (pair 1)' noenhanced");
    gp("set xlabel 'U1 (X canonical variate)'");
    gp("set ylabel 'V1 (Y canonical variate)'");
    gp("set grid");
    gp("unset key");

    std::string db = "$cv << EOD\n";
    for (int i = 0; i < n; ++i) {
        db += fmt(cca.scoresX(i, 0)) + " " + fmt(cca.scoresY(i, 0)) + "\n";
    }
    db += "EOD";
    gp(db);
    gp("plot $cv using 1:2 with points pt 7 ps 0.6 lc rgb '#4472C4'");

    LOKI_INFO("PlotMultivariate: wrote " + outFile.filename().string());
}

// ----------------------------------------------------------------------------
//  plotLdaProjection
// ----------------------------------------------------------------------------

void PlotMultivariate::plotLdaProjection(const LdaResult& lda,
                                          const std::string& stem) const
{
    if (lda.nDims < 2) return; // need at least 2 axes for scatter

    const auto outFile = _outPath(stem, "lda_projection");
    const int n = lda.nObs;

    Gnuplot gp;
    _setPngOutput(gp, outFile, 800, 700);
    gp("set title 'LDA Projection (LD1 vs LD2)' noenhanced");
    gp("set xlabel 'LD1'");
    gp("set ylabel 'LD2'");
    gp("set grid");
    gp("set key top left");

    // One datablock per class.
    const int nC = lda.nClasses;
    for (int c = 0; c < nC; ++c) {
        std::string db = "$lda" + std::to_string(c) + " << EOD\n";
        for (int i = 0; i < n; ++i) {
            if (lda.trueLabels[static_cast<std::size_t>(i)] == c) {
                db += fmt(lda.scores(i, 0)) + " " + fmt(lda.scores(i, 1)) + "\n";
            }
        }
        db += "EOD";
        gp(db);
    }

    std::string plotCmd = "plot ";
    for (int c = 0; c < nC; ++c) {
        if (c > 0) plotCmd += ", ";
        plotCmd += "$lda" + std::to_string(c)
                + " using 1:2 with points pt " + std::to_string(c + 6)
                + " ps 0.8 title 'Class " + std::to_string(c) + "'";
    }
    gp(plotCmd);

    LOKI_INFO("PlotMultivariate: wrote " + outFile.filename().string());
}

// ----------------------------------------------------------------------------
//  plotLdaConfusion
// ----------------------------------------------------------------------------

void PlotMultivariate::plotLdaConfusion(const LdaResult& lda,
                                         const std::string& stem) const
{
    const int nC = lda.nClasses;
    const int n  = lda.nObs;

    // Build confusion matrix.
    Eigen::MatrixXd conf = Eigen::MatrixXd::Zero(nC, nC);
    for (int i = 0; i < n; ++i) {
        const int trueC = lda.trueLabels[static_cast<std::size_t>(i)];
        const int predC = lda.predictedLabels[static_cast<std::size_t>(i)];
        if (trueC >= 0 && trueC < nC && predC >= 0 && predC < nC) {
            conf(trueC, predC) += 1.0;
        }
    }

    const auto outFile = _outPath(stem, "lda_confusion");
    Gnuplot gp;
    _setPngOutput(gp, outFile, 600, 550);
    gp("set title 'LDA Confusion Matrix (accuracy="
       + std::to_string(static_cast<int>(lda.accuracy * 100.0)) + "%)' noenhanced");
    gp("set cbrange [0:*]");
    gp("set palette defined (0 'white', 1 '#4472C4')");
    gp("set view map");
    gp("set size square");
    gp("unset key");
    gp("set xrange [-0.5:" + std::to_string(nC - 1) + ".5]");
    gp("set yrange [-0.5:" + std::to_string(nC - 1) + ".5]");
    gp("set xlabel 'Predicted class'");
    gp("set ylabel 'True class'");

    std::string db = "$conf << EOD\n";
    for (int i = nC - 1; i >= 0; --i) {
        for (int j = 0; j < nC; ++j) {
            db += fmt(conf(i, j), 0) + (j < nC - 1 ? " " : "\n");
        }
    }
    db += "EOD";
    gp(db);
    gp("plot $conf matrix rowheaders columnheaders using 1:2:3 with image");

    LOKI_INFO("PlotMultivariate: wrote " + outFile.filename().string());
}

// ----------------------------------------------------------------------------
//  plotMahalanobisDist
// ----------------------------------------------------------------------------

void PlotMultivariate::plotMahalanobisDist(const MahalanobisResult& mah,
                                            const std::string& stem) const
{
    const auto outFile = _outPath(stem, "mahalanobis_dist");
    const int n = mah.nObs;

    Gnuplot gp;
    _setPngOutput(gp, outFile, 1000, 450);
    gp("set title 'Mahalanobis Squared Distances D^2' noenhanced");
    gp("set xlabel 'Observation index'");
    gp("set ylabel 'D^2'");
    gp("set grid");
    gp("set key top right");

    std::string db = "$d2 << EOD\n";
    for (int i = 0; i < n; ++i) {
        db += std::to_string(i) + " "
            + fmt(mah.distances(i)) + " "
            + std::to_string(mah.isOutlier[static_cast<std::size_t>(i)] ? 1 : 0) + "\n";
    }
    db += "EOD";
    gp(db);

    gp("set arrow from 0," + fmt(mah.chi2Critical)
       + " to " + std::to_string(n - 1) + "," + fmt(mah.chi2Critical)
       + " nohead lc rgb 'red' lw 2");

    gp("plot $d2 using 1:($3==0 ? $2 : 1/0) with points pt 7 ps 0.6 "
       "lc rgb '#4472C4' title 'Normal', "
       "$d2 using 1:($3==1 ? $2 : 1/0) with points pt 7 ps 0.8 "
       "lc rgb 'red' title 'Outlier'");

    LOKI_INFO("PlotMultivariate: wrote " + outFile.filename().string());
}

// ----------------------------------------------------------------------------
//  plotMahalanobisQq
// ----------------------------------------------------------------------------

void PlotMultivariate::plotMahalanobisQq(const MahalanobisResult& mah,
                                          const std::string& stem) const
{
    const auto outFile = _outPath(stem, "mahalanobis_qq");
    const int n  = mah.nObs;
    const int p  = mah.nChannels;

    // Sort D^2 values.
    std::vector<double> sorted(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        sorted[static_cast<std::size_t>(i)] = mah.distances(i);
    }
    std::sort(sorted.begin(), sorted.end());

    Gnuplot gp;
    _setPngOutput(gp, outFile, 700, 600);
    gp("set title 'Chi^2 QQ Plot of D^2 (p=" + std::to_string(p) + ")' noenhanced");
    gp("set xlabel 'Chi^2 quantile'");
    gp("set ylabel 'Observed D^2'");
    gp("set grid");
    gp("set key top left");

    // Chi^2(p) quantiles via Wilson-Hilferty approximation.
    auto chi2Quantile = [&](double prob) -> double {
        // z quantile of standard normal (Abramowitz & Stegun).
        const double t = std::sqrt(-2.0 * std::log(1.0 - prob));
        const double z = t - (2.515517 + 0.802853*t + 0.010328*t*t)
                           / (1.0 + 1.432788*t + 0.189269*t*t + 0.001308*t*t*t);
        const double pd = static_cast<double>(p);
        const double h  = 1.0 - 2.0 / (9.0 * pd);
        const double s  = std::sqrt(2.0 / (9.0 * pd));
        return std::max(0.0, pd * std::pow(h + z * s, 3.0));
    };

    std::string db = "$qq << EOD\n";
    double maxQ = 0.0, maxD = sorted.back();
    for (int i = 0; i < n; ++i) {
        const double prob = (static_cast<double>(i) + 0.5) / static_cast<double>(n);
        const double q    = chi2Quantile(prob);
        maxQ = std::max(maxQ, q);
        db += fmt(q) + " " + fmt(sorted[static_cast<std::size_t>(i)]) + "\n";
    }
    db += "EOD";
    gp(db);

    const double xyMax = std::max(maxQ, maxD) * 1.05;
    gp("set xrange [0:" + fmt(xyMax) + "]");
    gp("set yrange [0:" + fmt(xyMax) + "]");

    gp("plot $qq using 1:2 with points pt 7 ps 0.6 lc rgb '#4472C4' title 'D^2', "
       "     x lc rgb 'red' lt 1 title 'Reference'");

    LOKI_INFO("PlotMultivariate: wrote " + outFile.filename().string());
}

// ----------------------------------------------------------------------------
//  plotManovaEigenvalues
// ----------------------------------------------------------------------------

void PlotMultivariate::plotManovaEigenvalues(const ManovaResult& manova,
                                              const std::string& stem) const
{
    const auto outFile = _outPath(stem, "manova_eigenvalues");
    const int s = static_cast<int>(manova.eigenvalues.size());

    Gnuplot gp;
    _setPngOutput(gp, outFile, 800, 450);
    gp("set title 'MANOVA Eigenvalues of E^{-1}H' noenhanced");
    gp("set xlabel 'Eigenvalue index'");
    gp("set ylabel 'Eigenvalue'");
    gp("set grid");
    gp("set style data histogram");
    gp("set style fill solid 0.7");

    // Annotation: Wilks Lambda and p-value.
    gp("set label 1 'Wilks={/Symbol L}=" + fmt(manova.wilksLambda, 4)
       + "  p=" + fmt(manova.wilksPvalue, 4)
       + "' at graph 0.05, graph 0.95 noenhanced");

    std::string db = "$ev << EOD\n";
    for (int i = 0; i < s; ++i) {
        db += std::to_string(i + 1) + " " + fmt(manova.eigenvalues(i)) + "\n";
    }
    db += "EOD";
    gp(db);
    gp("plot $ev using 2:xtic(1) lc rgb '#4472C4' notitle");

    LOKI_INFO("PlotMultivariate: wrote " + outFile.filename().string());
}
