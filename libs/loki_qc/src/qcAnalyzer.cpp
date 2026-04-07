#include <loki/qc/qcAnalyzer.hpp>

#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/outlier/iqrDetector.hpp>
#include <loki/outlier/madDetector.hpp>
#include <loki/outlier/zScoreDetector.hpp>
#include <loki/qc/plotQc.hpp>
#include <loki/qc/qcFlags.hpp>
#include <loki/stats/descriptive.hpp>
#include <loki/stats/filter.hpp>
#include <loki/stats/hypothesis.hpp>
#include <loki/timeseries/gapFiller.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <unordered_set>

using namespace loki;
using namespace loki::qc;

// =============================================================================
//  Construction
// =============================================================================

QcAnalyzer::QcAnalyzer(const AppConfig& cfg)
    : m_cfg(cfg)
{}

// =============================================================================
//  Public: run
// =============================================================================

QcResult QcAnalyzer::run(const TimeSeries& series, const std::string& datasetName)
{
    if (series.size() < 4) {
        throw DataException(
            "QcAnalyzer: series '" + series.metadata().componentName
            + "' has fewer than 4 observations -- cannot run QC.");
    }

    QcResult result;
    result.componentName = series.metadata().componentName;
    result.datasetName   = datasetName;

    // Sampling first -- medianStepSeconds needed by all other sections.
    if (m_cfg.qc.samplingEnabled) {
        _analyzeSampling(series, result);
    }

    if (m_cfg.qc.temporalEnabled) {
        _analyzeCoverage(series, result);
    }

    if (m_cfg.qc.statsEnabled) {
        _analyzeStats(series, result);
    }

    if (m_cfg.qc.outlierEnabled) {
        _analyzeOutliers(series, result);
    }

    if (m_cfg.qc.seasonalEnabled) {
        _analyzeSeasonal(series, result);
    }

    _buildFlags(series, result);
    _buildRecommendations(result);
    _writeProtocol(result);
    _writeFlagsCsv(series, result);

    PlotQc plotter(m_cfg);
    plotter.plotAll(series, result);

    LOKI_INFO("QcAnalyzer: finished '" + result.componentName
              + "'  n=" + std::to_string(result.nEpochs)
              + "  gaps=" + std::to_string(result.nGaps)
              + "  outliers=" + std::to_string(result.nOutliersTotal));

    return result;
}

// =============================================================================
//  Section 4: Sampling rate
// =============================================================================

void QcAnalyzer::_analyzeSampling(const TimeSeries& series, QcResult& result) const
{
    const std::size_t n = series.size();
    if (n < 2) {
        result.medianStepSeconds = 0.0;
        return;
    }

    std::vector<double> steps;
    steps.reserve(n - 1);
    for (std::size_t i = 1; i < n; ++i) {
        steps.push_back((series[i].time.mjd() - series[i - 1].time.mjd()) * 86400.0);
    }

    std::vector<double> sorted = steps;
    std::sort(sorted.begin(), sorted.end());

    const double medStep = sorted[sorted.size() / 2];
    result.medianStepSeconds = medStep;
    result.minStepSeconds    = sorted.front();
    result.maxStepSeconds    = sorted.back();

    const double threshold = medStep * 1.1;
    std::size_t nNonUniform = 0;
    for (double s : steps) {
        if (s > threshold) ++nNonUniform;
    }
    result.nNonUniform        = nNonUniform;
    result.uniformityFraction = static_cast<double>(nNonUniform)
                                / static_cast<double>(steps.size());
}

// =============================================================================
//  Section 1: Temporal coverage
// =============================================================================

void QcAnalyzer::_analyzeCoverage(const TimeSeries& series, QcResult& result) const
{
    const std::size_t n = series.size();

    result.nEpochs    = n;
    result.startMjd   = series[0].time.mjd();
    result.endMjd     = series[n - 1].time.mjd();
    result.spanDays   = result.endMjd - result.startMjd;
    result.startUtc   = series[0].time.utcString();
    result.endUtc     = series[n - 1].time.utcString();
    result.startGps   = series[0].time.gpsTotalSeconds();
    result.endGps     = series[n - 1].time.gpsTotalSeconds();

    // Expected epoch count based on median step.
    if (result.medianStepSeconds > 0.0) {
        const double medStepDays = result.medianStepSeconds / 86400.0;
        result.nExpected = static_cast<std::size_t>(
            std::round(result.spanDays / medStepDays)) + 1;
        result.completenessFraction = static_cast<double>(n)
                                      / static_cast<double>(result.nExpected);
    } else {
        result.nExpected            = n;
        result.completenessFraction = 1.0;
    }

    // Gap detection via GapFiller.
    GapFiller::Config gfCfg{};
    gfCfg.strategy            = GapFiller::Strategy::NONE;
    gfCfg.gapThresholdFactor  = 1.5;
    GapFiller gf(gfCfg);

    try {
        result.gaps = gf.detectGaps(series);
    } catch (const LOKIException& ex) {
        LOKI_WARNING("QcAnalyzer: gap detection failed: " + std::string(ex.what()));
        result.gaps.clear();
    }

    result.nGaps = result.gaps.size();

    if (result.nGaps > 0) {
        // Longest gap.
        const auto& longest = *std::max_element(
            result.gaps.begin(), result.gaps.end(),
            [](const GapInfo& a, const GapInfo& b) {
                return a.count < b.count;
            });
        result.longestGapDays = static_cast<double>(longest.count)
                                * (result.medianStepSeconds / 86400.0);
        result.longestGapStartMjd = longest.startMjd.value_or(0.0);

        // Median gap length.
        std::vector<double> gapLens;
        gapLens.reserve(result.nGaps);
        for (const auto& g : result.gaps) {
            gapLens.push_back(static_cast<double>(g.count)
                              * (result.medianStepSeconds / 86400.0));
        }
        std::sort(gapLens.begin(), gapLens.end());
        result.medianGapDays = gapLens[gapLens.size() / 2];
    } else {
        result.longestGapDays     = 0.0;
        result.longestGapStartMjd = 0.0;
        result.medianGapDays      = 0.0;
    }
}

// =============================================================================
//  Section 2: Descriptive statistics
// =============================================================================

void QcAnalyzer::_analyzeStats(const TimeSeries& series, QcResult& result) const
{
    std::vector<double> vals;
    vals.reserve(series.size());
    for (std::size_t i = 0; i < series.size(); ++i) {
        vals.push_back(series[i].value);
    }

    const stats::SummaryStats ss = stats::summarize(
        vals, NanPolicy::SKIP, m_cfg.qc.hurstEnabled);

    result.nValid   = ss.n;
    result.nNan     = ss.nMissing;
    result.mean     = ss.mean;
    result.median   = ss.median;
    result.stddev   = ss.stddev;
    result.iqrValue = ss.iqr;
    result.skewness = ss.skewness;
    result.kurtosis = ss.kurtosis;
    result.minVal   = ss.min;
    result.maxVal   = ss.max;
    result.hurstExp = ss.hurstExp;

    // Percentiles from sorted valid values.
    std::vector<double> valid = _validValues(series);
    std::sort(valid.begin(), valid.end());
    result.p05 = _percentile(valid, 5.0);
    result.p25 = _percentile(valid, 25.0);
    result.p75 = _percentile(valid, 75.0);
    result.p95 = _percentile(valid, 95.0);

    // Jarque-Bera normality test.
    if (valid.size() >= 8) {
        result.jbTest = stats::jarqueBera(valid, m_cfg.qc.significanceLevel,
                                          NanPolicy::SKIP);
    }
}

// =============================================================================
//  Section 3: Outlier detection
// =============================================================================

void QcAnalyzer::_analyzeOutliers(const TimeSeries& series, QcResult& result) const
{
    // Extract values; keep NaN positions tracked for index mapping.
    const std::size_t n = series.size();
    std::vector<double> vals(n);
    for (std::size_t i = 0; i < n; ++i) {
        vals[i] = series[i].value;
    }

    // Apply MA filter to estimate trend; compute residuals where MA is valid.
    const int halfWin = m_cfg.qc.maWindowSize / 2;
    std::vector<double> maValues;
    try {
        maValues = stats::movingAverage(vals, m_cfg.qc.maWindowSize);
    } catch (const LOKIException& ex) {
        LOKI_WARNING("QcAnalyzer: MA filter failed, using raw values for outlier detection: "
                     + std::string(ex.what()));
        maValues.assign(n, std::numeric_limits<double>::quiet_NaN());
    }

    // Build residual vector and index map for valid (non-NaN) residual positions.
    std::vector<double> residuals;
    std::vector<std::size_t> validIdx;
    residuals.reserve(n);
    validIdx.reserve(n);

    for (std::size_t i = 0; i < n; ++i) {
        if (!std::isnan(vals[i]) && !std::isnan(maValues[i])) {
            residuals.push_back(vals[i] - maValues[i]);
            validIdx.push_back(i);
        }
    }

    if (residuals.size() < 4) {
        LOKI_WARNING("QcAnalyzer: too few valid residuals for outlier detection (n="
                     + std::to_string(residuals.size()) + "), skipping.");
        return;
    }

    // Helper lambda: remap OutlierResult indices from residuals back to series.
    auto remapIndices = [&](outlier::OutlierResult& res) {
        for (auto& pt : res.points) {
            pt.index = validIdx[pt.index];
        }
    };

    // IQR detector.
    if (m_cfg.qc.outlier.iqrEnabled) {
        try {
            outlier::IqrDetector det(m_cfg.qc.outlier.iqrMultiplier);
            result.iqrResult = det.detect(residuals);
            remapIndices(result.iqrResult);
        } catch (const LOKIException& ex) {
            LOKI_WARNING("QcAnalyzer: IQR detection failed: " + std::string(ex.what()));
        }
    }

    // MAD detector.
    if (m_cfg.qc.outlier.madEnabled) {
        try {
            outlier::MadDetector det(m_cfg.qc.outlier.madMultiplier);
            result.madResult = det.detect(residuals);
            remapIndices(result.madResult);
        } catch (const LOKIException& ex) {
            LOKI_WARNING("QcAnalyzer: MAD detection failed: " + std::string(ex.what()));
        }
    }

    // Z-score detector.
    if (m_cfg.qc.outlier.zscoreEnabled) {
        try {
            outlier::ZScoreDetector det(m_cfg.qc.outlier.zscoreThreshold);
            result.zscResult = det.detect(residuals);
            remapIndices(result.zscResult);
        } catch (const LOKIException& ex) {
            LOKI_WARNING("QcAnalyzer: Z-score detection failed: " + std::string(ex.what()));
        }
    }

    // Count union of outlier indices.
    std::unordered_set<std::size_t> outlierSet;
    for (const auto& pt : result.iqrResult.points) outlierSet.insert(pt.index);
    for (const auto& pt : result.madResult.points) outlierSet.insert(pt.index);
    for (const auto& pt : result.zscResult.points) outlierSet.insert(pt.index);
    result.nOutliersTotal = outlierSet.size();

    // Suppress unused variable warning for halfWin when MA succeeded.
    (void)halfWin;
}

// =============================================================================
//  Section 5: Seasonal consistency
// =============================================================================

void QcAnalyzer::_analyzeSeasonal(const TimeSeries& series, QcResult& result) const
{
    // Auto-disable for sub-hourly data.
    if (_isSubHourly(result.medianStepSeconds)) {
        result.seasonalSectionRan = false;
        LOKI_INFO("QcAnalyzer: seasonal section disabled (sub-hourly data, step="
                  + std::to_string(result.medianStepSeconds) + "s).");
        return;
    }

    result.seasonalSectionRan = true;

    // Check span feasibility.
    const double spanYears = result.spanDays / 365.25;
    if (spanYears < m_cfg.qc.minSpanYears) {
        result.medianYearFeasible    = false;
        result.yearsWithPoorCoverage = 0;
        return;
    }

    // Count epochs per (year, month) bucket.
    // Count total months per year, then count years with poor coverage.
    std::map<int, std::map<int, int>> yearMonthCount; // [year][month] = count
    for (std::size_t i = 0; i < series.size(); ++i) {
        if (!std::isnan(series[i].value)) {
            const int y = series[i].time.year();
            const int m = series[i].time.month();
            yearMonthCount[y][m]++;
        }
    }

    // Estimate expected epochs per month from medianStep.
    const double medStepDays     = result.medianStepSeconds / 86400.0;
    const double expectedPerMonth = 30.44 / medStepDays;

    int poorYears = 0;
    for (const auto& [year, months] : yearMonthCount) {
        int poorMonths = 0;
        for (int mo = 1; mo <= 12; ++mo) {
            const auto it = months.find(mo);
            const int cnt = (it != months.end()) ? it->second : 0;
            const double frac = static_cast<double>(cnt) / expectedPerMonth;
            if (frac < m_cfg.qc.seasonal.minMonthCoverage) {
                ++poorMonths;
            }
        }
        // A year is "poor" if more than half its months have insufficient data.
        if (poorMonths > 6) ++poorYears;
    }

    result.yearsWithPoorCoverage = poorYears;
    result.medianYearFeasible    = (poorYears == 0)
                                   && (spanYears >= m_cfg.qc.minSpanYears)
                                   && (m_cfg.qc.seasonal.minYearsPerSlot >= 1);
}

// =============================================================================
//  Flagging
// =============================================================================

void QcAnalyzer::_buildFlags(const TimeSeries& series, QcResult& result) const
{
    const std::size_t n = series.size();
    result.flags.assign(n, FLAG_VALID);

    // Mark NaN epochs as gaps.
    for (std::size_t i = 0; i < n; ++i) {
        if (std::isnan(series[i].value)) {
            result.flags[i] |= FLAG_GAP;
        }
    }

    // Mark gaps detected in time axis (absent epochs are stored in result.gaps
    // as NaN observations already inserted by the loader, so NaN marking above
    // covers them; no extra loop needed here).

    // IQR outliers.
    for (const auto& pt : result.iqrResult.points) {
        if (pt.index < n) {
            result.flags[pt.index] |= FLAG_OUTLIER_IQR;
        }
    }

    // MAD outliers.
    for (const auto& pt : result.madResult.points) {
        if (pt.index < n) {
            result.flags[pt.index] |= FLAG_OUTLIER_MAD;
        }
    }

    // Z-score outliers.
    for (const auto& pt : result.zscResult.points) {
        if (pt.index < n) {
            result.flags[pt.index] |= FLAG_OUTLIER_ZSC;
        }
    }
}

// =============================================================================
//  Recommendations
// =============================================================================

void QcAnalyzer::_buildRecommendations(QcResult& result) const
{
    std::ostringstream oss;
    oss << "RECOMMENDATIONS\n"
        << "===============\n\n";

    // Gap filling strategy.
    oss << "Gap filling:\n";
    if (result.medianYearFeasible) {
        oss << "  MEDIAN_YEAR -- series is long enough and seasonal coverage is good.\n";
    } else if (result.spanDays > 30.0) {
        oss << "  LINEAR -- series is too short or coverage too poor for MEDIAN_YEAR.\n";
    } else {
        oss << "  FORWARD_FILL -- short series, use simple forward fill.\n";
    }

    // Spectral method recommendation.
    oss << "\nSpectral analysis:\n";
    if (result.uniformityFraction > m_cfg.qc.uniformityThreshold) {
        oss << "  LOMB_SCARGLE -- "
            << std::fixed << std::setprecision(1)
            << result.uniformityFraction * 100.0
            << "% of steps are non-uniform (threshold "
            << m_cfg.qc.uniformityThreshold * 100.0
            << "%). Use Lomb-Scargle periodogram.\n";
    } else {
        oss << "  FFT -- sampling is sufficiently uniform ("
            << std::fixed << std::setprecision(1)
            << result.uniformityFraction * 100.0
            << "% non-uniform steps).\n";
    }

    // Outlier cleaning.
    oss << "\nOutlier cleaning:\n";
    if (result.nOutliersTotal > 0) {
        const double pct = 100.0 * static_cast<double>(result.nOutliersTotal)
                           / static_cast<double>(result.nValid > 0 ? result.nValid : 1);
        oss << "  RECOMMENDED -- " << result.nOutliersTotal
            << " outlier epochs detected (" << std::fixed << std::setprecision(2)
            << pct << "%). Run loki_outlier before further analysis.\n";
    } else {
        oss << "  NOT REQUIRED -- no outliers detected by enabled methods.\n";
    }

    // Homogeneity testing.
    oss << "\nHomogeneity testing:\n";
    if (result.spanDays >= 365.25 * 3.0 && result.completenessFraction >= 0.7) {
        oss << "  CONSIDER -- series is long and reasonably complete. "
            << "Run loki_homogeneity to check for change points.\n";
    } else {
        oss << "  SKIP -- series is too short or incomplete for reliable change point detection.\n";
    }

    // MedianYearSeries.
    oss << "\nMedianYearSeries feasibility:\n";
    if (result.seasonalSectionRan) {
        if (result.medianYearFeasible) {
            oss << "  FEASIBLE -- sufficient span and per-month coverage.\n";
        } else {
            oss << "  NOT FEASIBLE -- span < " << m_cfg.qc.minSpanYears
                << " years or " << result.yearsWithPoorCoverage
                << " year(s) have poor monthly coverage.\n";
        }
    } else {
        oss << "  NOT EVALUATED -- seasonal section disabled (sub-hourly data).\n";
    }

    result.recommendations = oss.str();
}

// =============================================================================
//  Protocol output
// =============================================================================

void QcAnalyzer::_writeProtocol(const QcResult& result) const
{
    const std::string stem = "qc_" + result.datasetName
                             + "_" + result.componentName + "_protocol.txt";
    const auto path = m_cfg.protocolsDir / stem;

    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        throw IoException("QcAnalyzer: cannot open protocol file: " + path.string());
    }

    auto line = [&](const std::string& s) { ofs << s << "\n"; };
    auto sep  = [&]() { ofs << std::string(72, '-') << "\n"; };

    line("LOKI Quality Control Report");
    line("Dataset   : " + result.datasetName);
    line("Component : " + result.componentName);
    sep();

    // -- Section 1: Temporal coverage ----------------------------------------
    line("SECTION 1: TEMPORAL COVERAGE");
    sep();
    ofs << std::fixed << std::setprecision(6);
    line("  Start MJD  : " + std::to_string(result.startMjd));
    line("  End MJD    : " + std::to_string(result.endMjd));
    line("  Start UTC  : " + result.startUtc);
    line("  End UTC    : " + result.endUtc);
    ofs << std::fixed << std::setprecision(3);
    line("  Start GPS  : " + std::to_string(result.startGps) + " s");
    line("  End GPS    : " + std::to_string(result.endGps) + " s");
    line("  Span       : " + std::to_string(result.spanDays) + " days  ("
         + std::to_string(result.spanDays / 365.25) + " years)");
    line("  Epochs     : " + std::to_string(result.nEpochs)
         + " actual / " + std::to_string(result.nExpected) + " expected");
    ofs << std::fixed << std::setprecision(2);
    line("  Completeness: " + std::to_string(result.completenessFraction * 100.0) + " %");
    line("  Gaps       : " + std::to_string(result.nGaps));
    if (result.nGaps > 0) {
        ofs << std::fixed << std::setprecision(4);
        line("  Longest gap: " + std::to_string(result.longestGapDays) + " days");
        line("  Median gap : " + std::to_string(result.medianGapDays) + " days");
    }
    ofs << std::fixed << std::setprecision(2);
    line("  Non-uniform steps: " + std::to_string(result.uniformityFraction * 100.0) + " %");
    sep();

    // -- Section 4: Sampling rate --------------------------------------------
    line("SECTION 4: SAMPLING RATE");
    sep();
    ofs << std::fixed << std::setprecision(3);
    line("  Median step: " + std::to_string(result.medianStepSeconds) + " s");
    line("  Min step   : " + std::to_string(result.minStepSeconds) + " s");
    line("  Max step   : " + std::to_string(result.maxStepSeconds) + " s");
    line("  Non-uniform: " + std::to_string(result.nNonUniform) + " steps");
    sep();

    // -- Section 2: Descriptive statistics -----------------------------------
    line("SECTION 2: DESCRIPTIVE STATISTICS");
    sep();
    line("  N valid    : " + std::to_string(result.nValid));
    line("  N NaN      : " + std::to_string(result.nNan));
    ofs << std::fixed << std::setprecision(6);
    line("  Mean       : " + std::to_string(result.mean));
    line("  Median     : " + std::to_string(result.median));
    line("  Std dev    : " + std::to_string(result.stddev));
    line("  IQR        : " + std::to_string(result.iqrValue));
    line("  Skewness   : " + std::to_string(result.skewness));
    line("  Kurtosis   : " + std::to_string(result.kurtosis));
    line("  Min        : " + std::to_string(result.minVal));
    line("  Max        : " + std::to_string(result.maxVal));
    line("  P05        : " + std::to_string(result.p05));
    line("  P25        : " + std::to_string(result.p25));
    line("  P75        : " + std::to_string(result.p75));
    line("  P95        : " + std::to_string(result.p95));
    if (!std::isnan(result.hurstExp)) {
        line("  Hurst exp  : " + std::to_string(result.hurstExp));
    } else {
        line("  Hurst exp  : (disabled or n < 20)");
    }
    line("  JB test    : stat=" + std::to_string(result.jbTest.statistic)
         + "  p=" + std::to_string(result.jbTest.pValue)
         + "  normal=" + (result.jbTest.rejected ? "NO" : "YES"));
    sep();

    // -- Section 3: Outlier detection ----------------------------------------
    line("SECTION 3: OUTLIER DETECTION");
    sep();
    auto writeOutlierSection = [&](const std::string& name,
                                   bool enabled,
                                   const outlier::OutlierResult& res) {
        if (!enabled) {
            line("  " + name + ": disabled");
            return;
        }
        ofs << std::fixed << std::setprecision(2);
        const double pct = result.nValid > 0
            ? 100.0 * static_cast<double>(res.nOutliers)
              / static_cast<double>(result.nValid)
            : 0.0;
        line("  " + name + ": " + std::to_string(res.nOutliers)
             + " outliers (" + std::to_string(pct) + " %)");
    };
    writeOutlierSection("IQR",     m_cfg.qc.outlier.iqrEnabled,    result.iqrResult);
    writeOutlierSection("MAD",     m_cfg.qc.outlier.madEnabled,    result.madResult);
    writeOutlierSection("Z-score", m_cfg.qc.outlier.zscoreEnabled, result.zscResult);
    line("  Union      : " + std::to_string(result.nOutliersTotal) + " unique outlier epochs");
    sep();

    // -- Section 5: Seasonal consistency -------------------------------------
    line("SECTION 5: SEASONAL CONSISTENCY");
    sep();
    if (!result.seasonalSectionRan) {
        line("  Auto-disabled: sub-hourly data (step="
             + std::to_string(result.medianStepSeconds) + " s).");
    } else {
        line("  MEDIAN_YEAR feasible : " + std::string(result.medianYearFeasible ? "YES" : "NO"));
        line("  Years with poor coverage: " + std::to_string(result.yearsWithPoorCoverage));
    }
    sep();

    // -- Recommendations -----------------------------------------------------
    ofs << "\n" << result.recommendations;

    ofs.close();
    LOKI_INFO("QcAnalyzer: protocol written to " + path.string());
}

// =============================================================================
//  Flags CSV output
// =============================================================================

void QcAnalyzer::_writeFlagsCsv(const TimeSeries& series, const QcResult& result) const
{
    const std::string stem = "qc_" + result.datasetName
                             + "_" + result.componentName + "_flags.csv";
    const auto path = m_cfg.csvDir / stem;

    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        throw IoException("QcAnalyzer: cannot open flags CSV: " + path.string());
    }

    ofs << "mjd ; utc ; value ; flag\n";
    ofs << std::fixed << std::setprecision(8);

    for (std::size_t i = 0; i < series.size(); ++i) {
        const double mjd = series[i].time.mjd();
        const std::string utc = series[i].time.utcString();
        const double val = series[i].value;
        const int flag = static_cast<int>(result.flags[i]);

        ofs << mjd << " ; " << utc << " ; ";
        if (std::isnan(val)) {
            ofs << "NaN";
        } else {
            ofs << std::setprecision(6) << val;
        }
        ofs << " ; " << flag << "\n";
    }

    ofs.close();
    LOKI_INFO("QcAnalyzer: flags CSV written to " + path.string());
}

// =============================================================================
//  Internal helpers
// =============================================================================

bool QcAnalyzer::_isSubHourly(double medianStepSeconds)
{
    // Sub-hourly: step < 3600 s. Protect against uninitialized zero.
    return medianStepSeconds > 0.0 && medianStepSeconds < 3600.0;
}

std::vector<double> QcAnalyzer::_validValues(const TimeSeries& series)
{
    std::vector<double> out;
    out.reserve(series.size());
    for (std::size_t i = 0; i < series.size(); ++i) {
        if (!std::isnan(series[i].value)) {
            out.push_back(series[i].value);
        }
    }
    return out;
}

double QcAnalyzer::_percentile(std::vector<double> sorted, double p)
{
    if (sorted.empty()) return std::numeric_limits<double>::quiet_NaN();
    // sorted must already be sorted by caller.
    const double idx = (p / 100.0) * static_cast<double>(sorted.size() - 1);
    const std::size_t lo = static_cast<std::size_t>(idx);
    const std::size_t hi = lo + 1;
    if (hi >= sorted.size()) return sorted.back();
    const double frac = idx - static_cast<double>(lo);
    return sorted[lo] * (1.0 - frac) + sorted[hi] * frac;
}
