#pragma once

#include <Eigen/Dense>

#include <optional>
#include <string>
#include <vector>

namespace loki::multivariate {

// -----------------------------------------------------------------------------
//  CCF
// -----------------------------------------------------------------------------

/**
 * @brief Cross-correlation function result for a single channel pair.
 *
 * ccf[i] corresponds to lag = -maxLag + i, so ccf[maxLag] is the zero-lag
 * (Pearson) correlation. Peak lag and peak correlation are extracted
 * automatically from the full CCF vector.
 */
struct CcfPairResult {
    int                 channelA;    ///< 0-based index of first channel.
    int                 channelB;    ///< 0-based index of second channel.
    std::string         nameA;       ///< Name of channel A.
    std::string         nameB;       ///< Name of channel B.
    std::vector<double> ccf;         ///< CCF values at lags [-maxLag .. +maxLag].
    int                 maxLag;      ///< Maximum lag used.
    int                 peakLag;     ///< Lag at which |CCF| is maximised.
    double              peakCorr;    ///< CCF value at peakLag.
    double              threshold;   ///< +/- significance threshold (1.96 / sqrt(n)).
    bool                significant; ///< True if |peakCorr| > threshold.
};

// -----------------------------------------------------------------------------
//  PCA
// -----------------------------------------------------------------------------

/**
 * @brief Result of Principal Component Analysis.
 *
 * loadings: (p x k) matrix -- each column is a principal axis (eigenvector).
 * scores:   (n x k) matrix -- projection of centred data onto principal axes.
 *
 * Relationship: scores = (X - mean) * loadings
 * where X is the (n x p) data matrix and mean is the column mean vector.
 */
struct PcaResult {
    Eigen::MatrixXd loadings;           ///< Principal axes (p x k).
    Eigen::MatrixXd scores;             ///< Projected data  (n x k).
    Eigen::VectorXd explainedVar;       ///< Variance per component (k).
    Eigen::VectorXd explainedVarRatio;  ///< Fraction of total variance (k).
    Eigen::VectorXd cumulativeVar;      ///< Cumulative variance fraction (k).
    Eigen::VectorXd mean;               ///< Column means used for centring (p).
    int             nComponents;        ///< Number of retained components.
    int             nObs;              ///< Number of observations n.
    int             nChannels;         ///< Number of input channels p.
};

// -----------------------------------------------------------------------------
//  MSSA
// -----------------------------------------------------------------------------

/**
 * @brief Result of Multivariate SSA.
 *
 * reconstruction: (n x nComponents) matrix -- each column is one reconstructed
 * component summed over all input channels. Channel-wise decomposition can be
 * obtained by running MSSA per channel (not supported here -- use loki_ssa).
 */
struct MssaResult {
    Eigen::VectorXd eigenvalues;         ///< Singular values (k) -- NOT squared.
    Eigen::VectorXd explainedVarRatio;   ///< Fraction of total variance (k).
    Eigen::VectorXd cumulativeVar;       ///< Cumulative variance fraction (k).
    Eigen::MatrixXd reconstruction;      ///< Reconstructed components (n x nComponents).
    int             window;              ///< Embedding window L used.
    int             nComponents;         ///< Number of retained components.
    int             nObs;               ///< Length of input series n.
    int             nChannels;          ///< Number of input channels p.
};

// -----------------------------------------------------------------------------
//  VAR + Granger
// -----------------------------------------------------------------------------

/**
 * @brief Pairwise Granger causality test result.
 *
 * Tests H0: channel 'from' does NOT Granger-cause channel 'to'.
 * Rejection of H0 (significant == true) means past values of 'from'
 * provide statistically significant information about 'to' beyond
 * what past values of 'to' alone provide.
 */
struct GrangerResult {
    int         fromChannel;   ///< 0-based index of causing channel.
    int         toChannel;     ///< 0-based index of caused channel.
    std::string fromName;      ///< Name of causing channel.
    std::string toName;        ///< Name of caused channel.
    double      fStat;         ///< F-statistic of the Granger test.
    double      pValue;        ///< p-value (F distribution).
    bool        significant;   ///< True if pValue < significanceLevel.
};

/**
 * @brief Result of VAR(p) model fitting.
 *
 * coefficients[l] is the (nChannels x nChannels) coefficient matrix A_{l+1}
 * for lag l+1, l = 0..order-1.
 * The VAR model: Y_t = A_1 * Y_{t-1} + ... + A_p * Y_{t-p} + epsilon_t
 */
struct VarResult {
    int                          order;        ///< Selected VAR lag order p.
    std::string                  criterion;    ///< Information criterion used.
    double                       criterionVal; ///< IC value at selected order.
    std::vector<Eigen::MatrixXd> coefficients; ///< A_1..A_p, each (nCh x nCh).
    Eigen::MatrixXd              residuals;    ///< OLS residuals (n-order x nCh).
    Eigen::MatrixXd              sigma;        ///< Residual covariance (nCh x nCh).
    std::vector<GrangerResult>   granger;      ///< All pairwise Granger results.
    int                          nObs;        ///< Observations used (n - order).
    int                          nChannels;   ///< Number of channels p.
};

// -----------------------------------------------------------------------------
//  Factor Analysis
// -----------------------------------------------------------------------------

/**
 * @brief Result of Factor Analysis.
 *
 * loadings:  (p x k) -- factor loading matrix (after optional Varimax rotation).
 * scores:    (n x k) -- estimated factor scores (Thompson regression method).
 * communalities: (p) -- h^2_j = sum of squared loadings for variable j.
 * uniqueness:    (p) -- 1 - communality = specific variance.
 * rotationMatrix: (k x k) -- Varimax rotation matrix (identity if no rotation).
 */
struct FactorAnalysisResult {
    Eigen::MatrixXd loadings;        ///< Factor loadings (p x k).
    Eigen::MatrixXd scores;          ///< Factor scores   (n x k).
    Eigen::VectorXd communalities;   ///< h^2 per variable (p).
    Eigen::VectorXd uniqueness;      ///< 1 - h^2 per variable (p).
    Eigen::MatrixXd rotationMatrix;  ///< Varimax rotation matrix (k x k).
    int             nFactors;        ///< Number of extracted factors k.
    int             nObs;            ///< Number of observations n.
    int             nChannels;       ///< Number of input channels p.
    std::string     rotationMethod;  ///< "varimax" or "none".
};

// -----------------------------------------------------------------------------
//  CCA
// -----------------------------------------------------------------------------

/**
 * @brief Result of Canonical Correlation Analysis.
 *
 * canonicalCorrelations: (k) -- sorted descending, k = min(|X|, |Y|) or
 *                                cfg.nComponents if specified.
 * weightsX: (p x k) -- canonical weights for variable set X.
 * weightsY: (q x k) -- canonical weights for variable set Y.
 * scoresX:  (n x k) -- canonical variates for X.
 * scoresY:  (n x k) -- canonical variates for Y.
 */
struct CcaResult {
    Eigen::VectorXd canonicalCorrelations; ///< rho_1 >= rho_2 >= ... (k).
    Eigen::MatrixXd weightsX;              ///< Canonical weights for X (p x k).
    Eigen::MatrixXd weightsY;              ///< Canonical weights for Y (q x k).
    Eigen::MatrixXd scoresX;              ///< X canonical variates (n x k).
    Eigen::MatrixXd scoresY;              ///< Y canonical variates (n x k).
    int             nPairs;               ///< Number of canonical pairs retained.
    int             nObs;                 ///< Number of observations.
    std::vector<int> groupXIdx;           ///< 0-based channel indices for X.
    std::vector<int> groupYIdx;           ///< 0-based channel indices for Y.
};

// -----------------------------------------------------------------------------
//  LDA / QDA
// -----------------------------------------------------------------------------

/**
 * @brief Result of Linear / Quadratic Discriminant Analysis.
 */
struct LdaResult {
    Eigen::MatrixXd              discriminantAxes; ///< (p x nDims) -- discriminant vectors.
    Eigen::VectorXd              eigenvalues;      ///< Discriminant eigenvalues (nDims).
    Eigen::MatrixXd              scores;           ///< Projected data (n x nDims).
    std::vector<Eigen::VectorXd> classMeans;       ///< Mean per class in original space.
    std::vector<int>             predictedLabels;  ///< Predicted class (0-based).
    std::vector<int>             trueLabels;       ///< Input class labels.
    double                       accuracy;         ///< Fraction correctly classified.
    int                          nClasses;         ///< Number of classes g.
    int                          nDims;            ///< Number of discriminant axes.
    int                          nObs;             ///< Number of observations.
    int                          nChannels;        ///< Number of channels p.
    bool                         isQda;            ///< True if QDA was used.
};

// -----------------------------------------------------------------------------
//  Mahalanobis outlier detection
// -----------------------------------------------------------------------------

/**
 * @brief Result of multivariate Mahalanobis outlier detection.
 */
struct MahalanobisResult {
    Eigen::VectorXd distances;         ///< Squared Mahalanobis distances D^2 (n).
    std::vector<bool> isOutlier;       ///< True if D^2_i > chi2_critical.
    std::vector<int>  outlierIndices;  ///< 0-based indices of detected outliers.
    double            chi2Critical;    ///< Chi^2(p, alpha) threshold used.
    Eigen::VectorXd   mean;            ///< Location estimate (p).
    Eigen::MatrixXd   covariance;      ///< Scatter estimate (p x p).
    int               nOutliers;       ///< Number of flagged outliers.
    int               nObs;            ///< Total observations.
    int               nChannels;       ///< Number of channels p.
    bool              robust;          ///< True if MCD estimator was used.
    double            significanceLevel;
};

// -----------------------------------------------------------------------------
//  MANOVA
// -----------------------------------------------------------------------------

/**
 * @brief Result of one-way MANOVA.
 *
 * Four test statistics are reported: Wilks Lambda, Pillai Trace,
 * Hotelling-Lawley Trace, Roy Maximum Root.
 * F approximations and p-values are provided for Wilks and Pillai.
 */
struct ManovaResult {
    // Wilks Lambda
    double wilksLambda;   ///< Wilks Lambda statistic.
    double wilksF;        ///< Rao F approximation.
    double wilksDf1;      ///< Numerator df.
    double wilksDf2;      ///< Denominator df.
    double wilksPvalue;   ///< p-value.

    // Pillai Trace
    double pillaiTrace;   ///< Pillai V statistic.
    double pillaiF;       ///< F approximation.
    double pillaiDf1;
    double pillaiDf2;
    double pillaiPvalue;

    // Other statistics (no F approximation -- reported as-is)
    double hotellingTrace; ///< Hotelling-Lawley T^2.
    double royRoot;        ///< Roy maximum root Theta.

    Eigen::VectorXd              eigenvalues; ///< Eigenvalues of E^{-1}H (s).
    std::vector<Eigen::VectorXd> groupMeans;  ///< Mean vector per group.
    int                          nGroups;
    int                          nObs;
    int                          nChannels;
    bool                         significant; ///< Based on Wilks p-value.
};

// -----------------------------------------------------------------------------
//  Top-level result
// -----------------------------------------------------------------------------

/**
 * @brief Aggregated result of the loki_multivariate pipeline.
 *
 * Each method result is optional -- present only when the method was enabled
 * in MultivariateConfig and ran successfully.
 */
struct MultivariateResult {
    std::optional<PcaResult>            pca;
    std::optional<MssaResult>           mssa;
    std::optional<VarResult>            var_;   ///< Trailing underscore avoids C++ keyword clash.
    std::optional<FactorAnalysisResult> factor;
    std::optional<CcaResult>            cca;
    std::optional<LdaResult>            lda;
    std::optional<MahalanobisResult>    mahalanobis;
    std::optional<ManovaResult>         manova;
    std::vector<CcfPairResult>          ccf;    ///< Empty if CCF was disabled.
};

} // namespace loki::multivariate
