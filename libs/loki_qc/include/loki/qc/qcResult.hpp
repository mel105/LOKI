#pragma once

#include <loki/outlier/outlierResult.hpp>
#include <loki/stats/hypothesis.hpp>
#include <loki/timeseries/gapFiller.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace loki::qc {

/**
 * @brief Aggregated quality control result for a single TimeSeries.
 *
 * Produced by QcAnalyzer::run(). Contains structured results from each
 * independently-enabled QC section: temporal coverage, descriptive statistics,
 * outlier detection, sampling rate analysis, and seasonal consistency.
 *
 * The flags vector maps one-to-one with the epochs of the original series.
 * Each element is a bitfield OR-combination of the FLAG_* constants in qcFlags.hpp.
 *
 * The protocol and recommendations fields contain pre-formatted text written
 * directly to OUTPUT/PROTOCOLS/ by QcAnalyzer.
 */
struct QcResult {

    // -------------------------------------------------------------------------
    // Identity
    // -------------------------------------------------------------------------

    std::string componentName; ///< From TimeSeries::metadata().componentName.
    std::string datasetName;   ///< Stem of the input file path.

    // -------------------------------------------------------------------------
    // Section 1: Temporal coverage
    // -------------------------------------------------------------------------

    double      startMjd            {0.0};
    double      endMjd              {0.0};
    double      spanDays            {0.0};
    std::string startUtc;                    ///< UTC string of first epoch.
    std::string endUtc;                      ///< UTC string of last epoch.
    double      startGps            {0.0};   ///< GPS total seconds of first epoch.
    double      endGps              {0.0};   ///< GPS total seconds of last epoch.

    std::size_t nEpochs             {0};     ///< Actual number of epochs in series.
    std::size_t nExpected           {0};     ///< Expected epochs from span / medianStep.
    double      completenessFraction{0.0};   ///< nEpochs / nExpected.

    std::vector<GapInfo> gaps;               ///< All detected gaps (from GapFiller::detectGaps).
    std::size_t nGaps               {0};
    double      longestGapDays      {0.0};
    double      longestGapStartMjd  {0.0};
    double      medianGapDays       {0.0};   ///< NaN if nGaps == 0.

    double      uniformityFraction  {0.0};   ///< Fraction of steps > 1.1 * medianStep.

    // -------------------------------------------------------------------------
    // Section 4: Sampling rate (filled by _analyzeSampling, used by Section 1)
    // -------------------------------------------------------------------------

    double medianStepSeconds {0.0};
    double minStepSeconds    {0.0};
    double maxStepSeconds    {0.0};
    std::size_t nNonUniform  {0};    ///< Count of steps exceeding 1.1 * medianStep.

    // -------------------------------------------------------------------------
    // Section 2: Descriptive statistics
    // -------------------------------------------------------------------------

    std::size_t nValid   {0};        ///< Number of non-NaN observations.
    std::size_t nNan     {0};        ///< Number of NaN observations.

    double mean     {0.0};
    double median   {0.0};
    double stddev   {0.0};
    double iqrValue {0.0};
    double skewness {0.0};
    double kurtosis {0.0};
    double minVal   {0.0};
    double maxVal   {0.0};
    double p05      {0.0};
    double p25      {0.0};
    double p75      {0.0};
    double p95      {0.0};
    double hurstExp {0.0};           ///< NaN if hurstEnabled=false or n < 20.

    stats::HypothesisResult jbTest;  ///< Jarque-Bera normality test result.

    // -------------------------------------------------------------------------
    // Section 3: Outlier detection
    // -------------------------------------------------------------------------

    outlier::OutlierResult iqrResult;   ///< Empty if iqrEnabled=false.
    outlier::OutlierResult madResult;   ///< Empty if madEnabled=false.
    outlier::OutlierResult zscResult;   ///< Empty if zscoreEnabled=false.

    std::size_t nOutliersTotal {0};     ///< Union of all detected outlier indices.

    // -------------------------------------------------------------------------
    // Section 5: Seasonal consistency
    // -------------------------------------------------------------------------

    bool seasonalSectionRan    {false}; ///< False if auto-disabled for sub-hourly data.
    bool medianYearFeasible    {false}; ///< True if span >= minSpanYears and coverage ok.
    int  yearsWithPoorCoverage {0};     ///< Count of years below minMonthCoverage threshold.

    // -------------------------------------------------------------------------
    // Flagging
    // -------------------------------------------------------------------------

    /// One uint8_t per epoch in the original series. Bitfield of FLAG_* values.
    std::vector<uint8_t> flags;

    // -------------------------------------------------------------------------
    // Protocol text
    // -------------------------------------------------------------------------

    std::string recommendations; ///< Plain-text recommendation block appended to protocol.
};

} // namespace loki::qc
