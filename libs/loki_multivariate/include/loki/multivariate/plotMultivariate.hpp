#pragma once

#include "loki/multivariate/multivariateResult.hpp"
#include "loki/multivariate/multivariateSeries.hpp"
#include "loki/core/config.hpp"
#include "loki/io/gnuplot.hpp"

#include <filesystem>
#include <string>

namespace loki::multivariate {

/**
 * @brief Generates all plots for the loki_multivariate pipeline.
 *
 * Each plot method is guarded by the corresponding PlotConfig flag.
 * All plots follow the LOKI naming convention:
 *   multivariate_<stem>_<channel>_<plottype>.<format>
 *
 * Gnuplot notes:
 *   - Terminal: pngcairo noenhanced font 'Sans,12'
 *   - Inline data via plot '-' (no tmp files except heatmaps).
 *   - No -persist flag.
 *   - All paths via fwdSlash() helper for Windows compatibility.
 *   - Datablocks ($name << EOD) used for multiplot panels.
 */
class PlotMultivariate {
public:

    explicit PlotMultivariate(const AppConfig& cfg);

    /**
     * @brief Runs all enabled plots.
     * @param data   Assembled multivariate series.
     * @param result Aggregated analysis result.
     * @param stem   Dataset stem for output file naming.
     */
    void plotAll(const MultivariateSeries& data,
                 const MultivariateResult& result,
                 const std::string&        stem) const;

private:

    const AppConfig& m_cfg;

    // -------------------------------------------------------------------------
    //  Individual plot methods
    // -------------------------------------------------------------------------

    /// Pearson correlation matrix heatmap (p x p).
    void plotCorrelationMatrix(const MultivariateSeries& data,
                               const std::string& stem) const;

    /// CCF peak heatmap: max abs cross-correlation per pair.
    void plotCcfHeatmap(const MultivariateSeries& data,
                        const std::vector<CcfPairResult>& ccf,
                        const std::string& stem) const;

    /// PCA scree plot: eigenvalues + cumulative variance.
    void plotPcaScree(const PcaResult& pca,
                      const std::string& stem) const;

    /// PCA biplot: scores PC1/PC2 + loading arrows.
    void plotPcaBiplot(const PcaResult& pca,
                       const MultivariateSeries& data,
                       const std::string& stem) const;

    /// PCA scores vs time (one panel per retained component).
    void plotPcaScores(const PcaResult& pca,
                       const MultivariateSeries& data,
                       const std::string& stem) const;

    /// MSSA eigenvalue spectrum.
    void plotMssaEigenvalues(const MssaResult& mssa,
                             const std::string& stem) const;

    /// MSSA reconstructed components vs time.
    void plotMssaComponents(const MssaResult& mssa,
                            const MultivariateSeries& data,
                            const std::string& stem) const;

    /// VAR coefficient matrix heatmap per lag (one plot per lag).
    void plotVarCoefficients(const VarResult& var,
                             const MultivariateSeries& data,
                             const std::string& stem) const;

    /// VAR residuals vs time (one panel per channel).
    void plotVarResiduals(const VarResult& var,
                          const MultivariateSeries& data,
                          const std::string& stem) const;

    /// Granger causality F-stat heatmap (from x to).
    void plotGrangerHeatmap(const VarResult& var,
                            const MultivariateSeries& data,
                            const std::string& stem) const;

    /// Factor loading heatmap (p x k).
    void plotFactorLoadings(const FactorAnalysisResult& fa,
                            const MultivariateSeries& data,
                            const std::string& stem) const;

    /// Factor scores vs time.
    void plotFactorScores(const FactorAnalysisResult& fa,
                          const MultivariateSeries& data,
                          const std::string& stem) const;

    /// Canonical correlations bar chart.
    void plotCcaCorrelations(const CcaResult& cca,
                             const std::string& stem) const;

    /// CCA canonical variate scatter (first pair).
    void plotCcaScatterPairs(const CcaResult& cca,
                             const std::string& stem) const;

    /// LDA 2-D projection scatter coloured by class.
    void plotLdaProjection(const LdaResult& lda,
                           const std::string& stem) const;

    /// LDA confusion matrix heatmap.
    void plotLdaConfusion(const LdaResult& lda,
                          const std::string& stem) const;

    /// Mahalanobis D^2 vs index with chi^2 threshold line.
    void plotMahalanobisDist(const MahalanobisResult& mah,
                             const std::string& stem) const;

    /// Chi^2 QQ plot of D^2 values.
    void plotMahalanobisQq(const MahalanobisResult& mah,
                           const std::string& stem) const;

    /// MANOVA eigenvalues of E^{-1}H.
    void plotManovaEigenvalues(const ManovaResult& manova,
                               const std::string& stem) const;

    // -------------------------------------------------------------------------
    //  Helpers
    // -------------------------------------------------------------------------

    /// Builds output path: imgDir / "multivariate_<stem>_<tag>.<fmt>".
    [[nodiscard]]
    std::filesystem::path _outPath(const std::string& stem,
                                   const std::string& tag) const;

    /// Converts backslashes to forward slashes for gnuplot on Windows.
    [[nodiscard]]
    static std::string _fwdSlash(const std::filesystem::path& p);

    /// Writes gnuplot terminal + output preamble.
    void _setPngOutput(Gnuplot& gp,
                       const std::filesystem::path& outFile,
                       int widthPx = 900,
                       int heightPx = 600) const;
};

} // namespace loki::multivariate
