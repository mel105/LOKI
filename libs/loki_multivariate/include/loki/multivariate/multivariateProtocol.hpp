#pragma once

#include "loki/multivariate/multivariateResult.hpp"
#include "loki/multivariate/multivariateSeries.hpp"
#include "loki/core/config.hpp"

#include <filesystem>
#include <string>

namespace loki::multivariate {

/**
 * @brief Writes a plain-text protocol for loki_multivariate results.
 *
 * Output: OUTPUT/PROTOCOLS/multivariate_<stem>_protocol.txt
 *
 * Sections written (only when method was enabled and result is present):
 *   1. Assembly summary (channels, n, sync strategy)
 *   2. CCF matrix (peak correlation and lag per pair)
 *   3. PCA (explained variance per component, cumulative, top loadings)
 *   4. MSSA (singular values, cumulative variance)
 *   5. VAR (selected order, criterion value, Granger pairs with p-values)
 *   6. Factor Analysis (communalities, uniqueness per variable, rotation)
 *   7. CCA (canonical correlations per pair)
 *   8. LDA (eigenvalues per axis, classification accuracy, confusion table)
 *   9. Mahalanobis (threshold, number of outliers, outlier indices)
 *  10. MANOVA (Wilks, Pillai, Hotelling, Roy with F and p-values)
 */
class MultivariateProtocol {
public:

    explicit MultivariateProtocol(const AppConfig& cfg);

    /**
     * @brief Writes the protocol file.
     * @param data   Assembled multivariate series.
     * @param result Aggregated analysis result.
     * @param stem   Dataset stem for output file naming.
     */
    void write(const MultivariateSeries& data,
               const MultivariateResult& result,
               const std::string&        stem) const;

private:

    const AppConfig& m_cfg;

    [[nodiscard]]
    std::filesystem::path _outPath(const std::string& stem) const;

    static std::string _line(char c = '-', int n = 60);
    static std::string _fmt(double v, int prec = 6);
    static std::string _fmtPct(double v);
    static std::string _fmtPval(double v);
};

} // namespace loki::multivariate
