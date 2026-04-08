#include <loki/homogeneity/homogeneityAnalyzer.hpp>
#include <loki/homogeneity/plotHomogeneity.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/stats/descriptive.hpp>
#include <loki/stats/hypothesis.hpp>
#include <loki/timeseries/gapFiller.hpp>
#include <loki/timeseries/deseasonalizer.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

using namespace loki;
using namespace loki::homogeneity;

// ---------------------------------------------------------------------------
//  Internal helpers
// ---------------------------------------------------------------------------

namespace {

/// Converts MJD to a UTC date string "YYYY-MM-DD".
std::string mjdToDate(double mjd)
{
    // MJD epoch: 1858-11-17 00:00:00 UTC.
    // Julian Day Number of MJD epoch: 2400000.5.
    const long jdn = static_cast<long>(mjd) + 2400001L;
    const long a   = jdn + 32044L;
    const long b   = (4L * a + 3L) / 146097L;
    const long c   = a - (146097L * b) / 4L;
    const long d   = (4L * c + 3L) / 1461L;
    const long e   = c - (1461L * d) / 4L;
    const long m   = (5L * e + 2L) / 153L;

    const int day   = static_cast<int>(e - (153L * m + 2L) / 5L + 1L);
    const int month = static_cast<int>(m + 3L - 12L * (m / 10L));
    const int year  = static_cast<int>(100L * b + d - 4800L + m / 10L);

    std::ostringstream oss;
    oss << year << "-"
        << std::setw(2) << std::setfill('0') << month << "-"
        << std::setw(2) << std::setfill('0') << day;
    return oss.str();
}

/// ACF at lag 1 of a vector.
double acfLag1(const std::vector<double>& v)
{
    if (v.size() < 2) return 0.0;
    const double n    = static_cast<double>(v.size());
    const double mean = std::accumulate(v.begin(), v.end(), 0.0) / n;
    double var = 0.0, cov = 0.0;
    for (double x : v) var += (x - mean) * (x - mean);
    for (std::size_t i = 1; i < v.size(); ++i)
        cov += (v[i] - mean) * (v[i - 1] - mean);
    if (var < 1e-15) return 0.0;
    return cov / var;
}

/// Formats a double with fixed width and precision.
std::string fmt(double v, int w, int p)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(p) << std::setw(w) << v;
    return oss.str();
}

/// Separator line.
constexpr const char* SEP  = " -------------------------------------------------------\n";
constexpr const char* HSEP = "============================================================\n";

/// Returns the human-readable method name for the protocol header.
std::string methodLabel(const std::string& method)
{
    if (method == "snht")     return "SNHT (Alexandersson 1986)";
    if (method == "pelt")     return "PELT (Killick et al. 2012)";
    if (method == "bocpd")    return "BOCPD (Adams & MacKay 2007)";
    return "Yao & Davis (1986)";
}

/// Returns the step label derived from the median time step of the series.
std::string stepLabel(const TimeSeries& ts)
{
    if (ts.size() < 2) return "unknown";
    // Median step in days from first 100 differences.
    const std::size_t nCheck = std::min(ts.size() - 1, std::size_t{100});
    std::vector<double> steps;
    steps.reserve(nCheck);
    for (std::size_t i = 0; i < nCheck; ++i) {
        steps.push_back(ts[i + 1].time.mjd() - ts[i].time.mjd());
    }
    std::sort(steps.begin(), steps.end());
    const double medStep = steps[steps.size() / 2];  // days

    if (medStep < 1.0 / 24.0)       return std::to_string(static_cast<int>(medStep * 1440.0)) + "min";
    if (medStep < 1.0)               return std::to_string(static_cast<int>(medStep * 24.0))   + "h";
    if (medStep < 8.0)               return std::to_string(static_cast<int>(medStep))          + "d";
    return fmt(medStep, 0, 1) + "d";
}

/// Segment statistics over the original (gap-filled) series.
struct SegStats {
    std::size_t begin{0};
    std::size_t end{0};
    double      mean{0.0};
    double      stddev{0.0};
    double      shift{0.0};       // shift introduced AT this segment's start CP
    double      correction{0.0};  // cumulative correction applied to this segment
};

std::vector<SegStats> buildSegmentStats(const TimeSeries&              ts,
                                        const std::vector<ChangePoint>& cps)
{
    const std::size_t n   = ts.size();
    const std::size_t ncp = cps.size();

    // Segment boundaries: [0, cp0), [cp0, cp1), ..., [cp_{k-1}, n)
    std::vector<std::size_t> bounds;
    bounds.reserve(ncp + 2);
    bounds.push_back(0);
    for (const auto& cp : cps) bounds.push_back(cp.globalIndex);
    bounds.push_back(n);

    // Cumulative correction per segment.
    std::vector<double> cumCorr(bounds.size() - 1, 0.0);
    for (std::size_t j = 1; j < bounds.size() - 1; ++j) {
        cumCorr[j] = cumCorr[j - 1] + cps[j - 1].shift;
    }

    std::vector<SegStats> segs;
    segs.reserve(bounds.size() - 1);

    for (std::size_t j = 0; j + 1 < bounds.size(); ++j) {
        const std::size_t beg = bounds[j];
        const std::size_t end = bounds[j + 1];

        double sum = 0.0, ss = 0.0;
        const double cnt = static_cast<double>(end - beg);
        for (std::size_t i = beg; i < end; ++i) sum += ts[i].value;
        const double mean = (cnt > 0) ? sum / cnt : 0.0;
        for (std::size_t i = beg; i < end; ++i) {
            const double d = ts[i].value - mean;
            ss += d * d;
        }
        const double stddev = (cnt > 1) ? std::sqrt(ss / (cnt - 1.0)) : 0.0;

        const double shift = (j > 0 && j - 1 < cps.size())
                             ? cps[j - 1].shift : 0.0;

        segs.push_back(SegStats{beg, end, mean, stddev, shift, cumCorr[j]});
    }

    return segs;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
//  Construction
// ---------------------------------------------------------------------------

HomogeneityAnalyzer::HomogeneityAnalyzer(const AppConfig& cfg)
    : m_cfg{cfg}
{}

// ---------------------------------------------------------------------------
//  _buildHomogenizerConfig
// ---------------------------------------------------------------------------

HomogenizerConfig HomogeneityAnalyzer::_buildHomogenizerConfig() const
{
    const auto& h = m_cfg.homogeneity;
    HomogenizerConfig hcfg{};

    hcfg.applyGapFilling = h.applyGapFilling;
    {
        const std::string& s = h.gapFilling.strategy;
        if      (s == "linear")       hcfg.gapFiller.strategy = GapFiller::Strategy::LINEAR;
        else if (s == "forward_fill") hcfg.gapFiller.strategy = GapFiller::Strategy::FORWARD_FILL;
        else if (s == "mean")         hcfg.gapFiller.strategy = GapFiller::Strategy::MEAN;
        else if (s == "spline")       hcfg.gapFiller.strategy = GapFiller::Strategy::SPLINE;
        else                          hcfg.gapFiller.strategy = GapFiller::Strategy::NONE;
        hcfg.gapFiller.maxFillLength = static_cast<std::size_t>(
            std::max(0, h.gapFilling.maxFillLength));
    }

    auto mapOutlier = [](const OutlierFilterConfig& src,
                         double defaultMul) -> OutlierConfig {
        OutlierConfig o;
        o.enabled             = src.enabled;
        o.method              = src.method;
        o.madMultiplier       = (src.madMultiplier > 0.0) ? src.madMultiplier : defaultMul;
        o.iqrMultiplier       = src.iqrMultiplier;
        o.zscoreThreshold     = src.zscoreThreshold;
        o.replacementStrategy = src.replacementStrategy;
        o.maxFillLength       = src.maxFillLength;
        return o;
    };

    hcfg.preOutlier  = mapOutlier(h.preOutlier,  5.0);
    hcfg.postOutlier = mapOutlier(h.postOutlier, 3.0);

    {
        const std::string& s = h.deseasonalization.strategy;
        if      (s == "median_year")    hcfg.deseasonalizer.strategy = Deseasonalizer::Strategy::MEDIAN_YEAR;
        else if (s == "moving_average") hcfg.deseasonalizer.strategy = Deseasonalizer::Strategy::MOVING_AVERAGE;
        else                            hcfg.deseasonalizer.strategy = Deseasonalizer::Strategy::NONE;
        hcfg.deseasonalizer.maWindowSize = h.deseasonalization.maWindowSize;
    }

    hcfg.detector.method           = h.detection.method;
    hcfg.detector.minSegmentPoints = static_cast<std::size_t>(
        std::max(0, h.detection.minSegmentPoints));
    hcfg.detector.minSegmentSeconds = h.detection.minSegmentSeconds;

    hcfg.detector.detectorConfig.significanceLevel  = h.detection.significanceLevel;
    hcfg.detector.detectorConfig.acfDependenceLimit = h.detection.acfDependenceLimit;

    hcfg.detector.snhtConfig.significance     = h.detection.significanceLevel;
    hcfg.detector.snhtConfig.nPermutations    = h.detection.snht.nPermutations;
    hcfg.detector.snhtConfig.seed             = h.detection.snht.seed;
    hcfg.detector.snhtConfig.minSegmentLength = std::max(0, h.detection.minSegmentPoints);

    hcfg.detector.peltConfig.penaltyType      = h.detection.pelt.penaltyType;
    hcfg.detector.peltConfig.fixedPenalty     = h.detection.pelt.fixedPenalty;
    hcfg.detector.peltConfig.minSegmentLength = h.detection.pelt.minSegmentLength;

    hcfg.detector.bocpdConfig.hazardLambda     = h.detection.bocpd.hazardLambda;
    hcfg.detector.bocpdConfig.priorMean        = h.detection.bocpd.priorMean;
    hcfg.detector.bocpdConfig.priorVar         = h.detection.bocpd.priorVar;
    hcfg.detector.bocpdConfig.priorAlpha       = h.detection.bocpd.priorAlpha;
    hcfg.detector.bocpdConfig.priorBeta        = h.detection.bocpd.priorBeta;
    hcfg.detector.bocpdConfig.threshold        = h.detection.bocpd.threshold;
    hcfg.detector.bocpdConfig.minSegmentLength = h.detection.bocpd.minSegmentLength;

    hcfg.applyAdjustment = h.applyAdjustment;

    return hcfg;
}

// ---------------------------------------------------------------------------
//  run
// ---------------------------------------------------------------------------

void HomogeneityAnalyzer::run(const TimeSeries& ts,
                               const std::string& datasetName) const
{
    const std::string compName = ts.metadata().componentName;
    LOKI_INFO("HomogeneityAnalyzer: processing '" + compName + "'");

    std::filesystem::create_directories(m_cfg.protocolsDir);
    std::filesystem::create_directories(m_cfg.csvDir);
    std::filesystem::create_directories(m_cfg.imgDir);

    const HomogenizerConfig hcfg = _buildHomogenizerConfig();
    const Homogenizer homogenizer{hcfg};

    HomogenizerResult result;
    try {
        result = homogenizer.process(ts);
    } catch (const LOKIException& ex) {
        LOKI_ERROR("HomogeneityAnalyzer: Homogenizer::process failed for '"
                   + compName + "': " + ex.what());
        throw;
    }

    LOKI_INFO("HomogeneityAnalyzer: detected "
              + std::to_string(result.changePoints.size()) + " change point(s).");

    for (const auto& cp : result.changePoints) {
        std::ostringstream oss;
        oss << "  index=" << cp.globalIndex
            << "  mjd="   << cp.mjd
            << "  shift=" << cp.shift
            << "  p="     << cp.pValue;
        LOKI_INFO(oss.str());
    }

    _writeProtocol(result.preOutlierCleaned, result, hcfg, datasetName, compName);
    _writeCsv(ts, result, datasetName, compName);

    PlotHomogeneity plotter{m_cfg};
    plotter.plotAll(ts, result, result.seasonal);
}

// ---------------------------------------------------------------------------
//  _writeProtocol
// ---------------------------------------------------------------------------

void HomogeneityAnalyzer::_writeProtocol(const TimeSeries&       original,
                                          const HomogenizerResult& result,
                                          const HomogenizerConfig& hcfg,
                                          const std::string&       datasetName,
                                          const std::string&       compName) const
{
    const std::string fname = "homogeneity_" + datasetName + "_" + compName + ".txt";
    const std::filesystem::path outPath = m_cfg.protocolsDir / fname;

    std::ofstream ofs(outPath);
    if (!ofs.is_open()) {
        throw IoException("HomogeneityAnalyzer: cannot write protocol to '"
                          + outPath.string() + "'.");
    }

    const auto& h     = m_cfg.homogeneity;
    const std::size_t n = original.size();
    const auto& cps   = result.changePoints;

    // Period strings.
    const std::string dateFirst = (n > 0) ? mjdToDate(original[0].time.mjd())       : "?";
    const std::string dateLast  = (n > 0) ? mjdToDate(original[n - 1].time.mjd())   : "?";
    const std::string step      = stepLabel(original);

    ofs << std::fixed;

    // -----------------------------------------------------------------------
    // Header
    // -----------------------------------------------------------------------
    ofs << HSEP;
    ofs << " HOMOGENEITY PROTOCOL -- " << methodLabel(h.detection.method) << "\n";
    ofs << HSEP;
    ofs << " Dataset:      " << datasetName << "    Series: " << compName << "\n";
    ofs << " Period:       " << dateFirst << "  --  " << dateLast << "\n";
    ofs << " Observations: " << n << "    Step: " << step << "\n";

    // -----------------------------------------------------------------------
    // Pre-processing
    // -----------------------------------------------------------------------
    ofs << "\n PRE-PROCESSING\n" << SEP;

    // Gap filling.
    const std::string gfStrat = h.gapFilling.strategy;
    if (hcfg.applyGapFilling && gfStrat != "none") {
        ofs << " Gap filling:           " << gfStrat << "\n";
    } else {
        ofs << " Gap filling:           disabled\n";
    }

    // Pre-outlier.
    if (hcfg.preOutlier.enabled) {
        ofs << " Pre-outliers removed:  "
            << result.preOutlierDetection.nOutliers
            << "    (method: " << hcfg.preOutlier.method
            << ", k=" << std::setprecision(1) << hcfg.preOutlier.madMultiplier << ")\n";
    } else {
        ofs << " Pre-outliers removed:  disabled\n";
    }

    // Deseasonalization.
    const std::string dsStrat = h.deseasonalization.strategy;
    ofs << " Deseasonalization:     ";
    if      (dsStrat == "median_year")    ofs << "MEDIAN_YEAR"
        << "    (min_years=" << h.deseasonalization.medianYearMinYears << ")\n";
    else if (dsStrat == "moving_average") ofs << "MOVING_AVERAGE"
        << "    (window=" << h.deseasonalization.maWindowSize << " samples)\n";
    else                                   ofs << "none\n";

    // Post-outlier.
    if (hcfg.postOutlier.enabled) {
        ofs << " Post-outliers removed: "
            << result.postOutlierDetection.nOutliers
            << "    (method: " << hcfg.postOutlier.method
            << ", k=" << std::setprecision(1) << hcfg.postOutlier.madMultiplier << ")\n";
    } else {
        ofs << " Post-outliers removed: disabled\n";
    }

    // -----------------------------------------------------------------------
    // Detection parameters (method-specific)
    // -----------------------------------------------------------------------
    ofs << "\n DETECTION PARAMETERS\n" << SEP;
    ofs << " Method:                " << methodLabel(h.detection.method) << "\n";

    const std::string& method = h.detection.method;

    if (method == "yao_davis" || method == "snht") {
        ofs << " Significance level:    " << std::setprecision(4)
            << h.detection.significanceLevel << "\n";
        ofs << " Min segment:           "
            << h.detection.minSegmentPoints << " samples";
        // Approximate in years if step is known.
        if (step.find('h') != std::string::npos) {
            const double hoursPerSample = [&]() {
                if (step == "6h") return 6.0;
                if (step == "3h") return 3.0;
                if (step == "1h") return 1.0;
                return 6.0;
            }();
            const double years = static_cast<double>(h.detection.minSegmentPoints)
                                 * hoursPerSample / 8765.81;
            ofs << "  (~" << std::setprecision(2) << years << " years)";
        }
        ofs << "\n";
        if (method == "yao_davis") {
            ofs << " ACF dependence corr.:  "
                << (h.detection.correctForDependence ? "yes" : "no") << "\n";
            ofs << " ACF dependence limit:  " << std::setprecision(2)
                << h.detection.acfDependenceLimit << "\n";
        }
        if (method == "snht") {
            ofs << " Permutations:          " << h.detection.snht.nPermutations << "\n";
            ofs << " RNG seed:              "
                << (h.detection.snht.seed == 0 ? "random" : std::to_string(h.detection.snht.seed))
                << "\n";
        }
    } else if (method == "pelt") {
        ofs << " Penalty type:          " << h.detection.pelt.penaltyType << "\n";
        if (h.detection.pelt.penaltyType == "fixed") {
            ofs << " Fixed penalty:         " << std::setprecision(6)
                << h.detection.pelt.fixedPenalty << "\n";
        }
        ofs << " Min segment length:    " << h.detection.pelt.minSegmentLength << " samples";
        if (step.find('h') != std::string::npos) {
            const double hoursPerSample = (step == "6h") ? 6.0 : (step == "3h") ? 3.0 : 1.0;
            const double years = static_cast<double>(h.detection.pelt.minSegmentLength)
                                 * hoursPerSample / 8765.81;
            ofs << "  (~" << std::setprecision(2) << years << " years)";
        }
        ofs << "\n";
    } else if (method == "bocpd") {
        ofs << " Hazard lambda:         " << std::setprecision(1)
            << h.detection.bocpd.hazardLambda << " samples\n";
        ofs << " Prior mean:            " << std::setprecision(4)
            << h.detection.bocpd.priorMean << "\n";
        ofs << " Prior var (kappa0):    " << h.detection.bocpd.priorVar << "\n";
        ofs << " Prior alpha:           " << h.detection.bocpd.priorAlpha << "\n";
        ofs << " Prior beta:            " << h.detection.bocpd.priorBeta << "\n";
        ofs << " Threshold:             " << h.detection.bocpd.threshold << "\n";
        ofs << " Min segment length:    " << h.detection.bocpd.minSegmentLength << " samples\n";
    }

    // -----------------------------------------------------------------------
    // Change points table
    // -----------------------------------------------------------------------
    ofs << "\n CHANGE POINTS DETECTED: " << cps.size() << "\n" << SEP;

    const bool hasPValue = (method == "yao_davis" || method == "snht");

    ofs << " #    Index      MJD         Date (UTC)      Shift     ";
    if (hasPValue) ofs << "  p-value";
    ofs << "\n";

    for (std::size_t i = 0; i < cps.size(); ++i) {
        const auto& cp = cps[i];
        ofs << " " << std::setw(2) << (i + 1)
            << "   " << std::setw(6) << cp.globalIndex
            << "   " << std::setw(10) << std::setprecision(2) << cp.mjd
            << "   " << mjdToDate(cp.mjd)
            << "   " << std::setw(10) << std::setprecision(6)
                     << std::showpos << cp.shift << std::noshowpos;
        if (hasPValue) {
            if (cp.pValue >= 0.001)
                ofs << "   " << std::setprecision(4) << cp.pValue;
            else if (cp.pValue > 0.0)
                ofs << "    < 0.001";
            else
                ofs << "         --";
        }
        ofs << "\n";
    }

    if (cps.empty()) {
        ofs << " No change points detected -- series is homogeneous.\n";
    }

    // -----------------------------------------------------------------------
    // Segment statistics
    // -----------------------------------------------------------------------
    ofs << "\n SEGMENT STATISTICS\n" << SEP;
    ofs << " Seg  Date range                             n        Mean      Std dev    Shift in\n";

    const auto segs = buildSegmentStats(original, cps);

    for (std::size_t j = 0; j < segs.size(); ++j) {
        const auto& seg = segs[j];
        const std::string bDate = mjdToDate(original[seg.begin].time.mjd());
        const std::string eDate = (seg.end < n)
                                  ? mjdToDate(original[seg.end - 1].time.mjd())
                                  : mjdToDate(original[n - 1].time.mjd());
        const std::size_t segN = seg.end - seg.begin;

        ofs << " " << std::setw(3) << j
            << "   " << bDate << "  --  " << eDate
            << "   " << std::setw(6) << segN
            << "   " << std::setw(9) << std::setprecision(4) << seg.mean
            << "   " << std::setw(9) << std::setprecision(4) << seg.stddev;

        if (j == 0) {
            ofs << "    ref\n";
        } else {
            ofs << "   " << std::showpos << std::setprecision(6) << seg.shift
                << std::noshowpos << "\n";
        }
    }

    // -----------------------------------------------------------------------
    // Adjustments applied
    // -----------------------------------------------------------------------
    ofs << "\n ADJUSTMENTS APPLIED\n" << SEP;
    ofs << " Reference segment:         0 (oldest / leftmost)\n";
    ofs << " Applied adjustment:        " << (hcfg.applyAdjustment ? "yes" : "no") << "\n";

    if (!cps.empty() && hcfg.applyAdjustment) {
        // Total cumulative correction = sum of all shifts.
        double total = 0.0;
        for (const auto& cp : cps) total += cp.shift;
        ofs << " Total cumulative corr.:    " << std::showpos << std::setprecision(6)
            << total << std::noshowpos << "\n\n";

        ofs << " Seg  Correction\n";
        ofs << " 0      0.000000  (reference)\n";
        double cumCorr = 0.0;
        for (std::size_t j = 0; j < cps.size(); ++j) {
            cumCorr += cps[j].shift;
            ofs << " " << std::setw(2) << (j + 1)
                << "   " << std::showpos << std::setprecision(6) << cumCorr
                << std::noshowpos << "\n";
        }
    } else if (cps.empty()) {
        ofs << " No adjustments applied (no change points detected).\n";
    } else {
        ofs << " Adjustment disabled in config (apply_adjustment=false).\n";
    }

    // -----------------------------------------------------------------------
    // Residual diagnostics (deseasonalized series)
    // -----------------------------------------------------------------------
    ofs << "\n RESIDUAL DIAGNOSTICS  (deseasonalized series)\n" << SEP;

    const std::vector<double>& resid = result.deseasonalizedValues;

    if (!resid.empty()) {
        const auto summary = loki::stats::summarize(resid, NanPolicy::SKIP, false);
        const auto jb      = loki::stats::jarqueBera(resid, 0.05, NanPolicy::SKIP);
        const double dw    = loki::stats::durbinWatson(resid, NanPolicy::SKIP);
        const double acf1  = acfLag1(resid);

        ofs << " Mean:      " << std::setw(10) << std::setprecision(6) << summary.mean
            << "    Std dev: " << std::setw(10) << summary.stddev << "\n";
        ofs << " MAD:       " << std::setw(10) << summary.mad
            << "    Min:     " << std::setw(10) << summary.min
            << "    Max: "     << std::setw(10) << summary.max << "\n";
        ofs << " ACF lag-1: " << std::setw(10) << std::setprecision(4) << acf1 << "\n";

        // Normality.
        ofs << " Normality (J-B):   p=" << std::setprecision(4) << jb.pValue;
        ofs << (jb.rejected ? "   [FAIL -- non-normal residuals]\n"
                            : "   [OK]\n");

        // Durbin-Watson.
        const bool dwOk = (dw > 1.5 && dw < 2.5);
        ofs << " Autocorr. (D-W):   " << std::setprecision(3) << dw;
        if (dwOk)       ofs << "   [OK]\n";
        else if (dw < 1.5) ofs << "   [FAIL -- positive autocorrelation]\n";
        else               ofs << "   [FAIL -- negative autocorrelation]\n";
    } else {
        ofs << " No residuals available.\n";
    }

    // -----------------------------------------------------------------------
    // Tuning hints
    // -----------------------------------------------------------------------
    ofs << "\n TUNING HINTS\n" << SEP;

    if (!resid.empty()) {
        const double acf1check = acfLag1(resid);
        const double dw2       = loki::stats::durbinWatson(resid, NanPolicy::SKIP);
        bool         hinted    = false;

        if (acf1check > 0.5) {
            ofs << " High ACF lag-1 (" << std::setprecision(3) << acf1check
                << ") in deseasonalized residuals.\n"
                << " This is expected for sub-daily climatological data\n"
                << " (synoptic variability dominates). The yao_davis and snht\n"
                << " detectors apply a sigmaStar correction for this.\n"
                << " If using pelt, consider increasing min_segment_length.\n";
            hinted = true;
        }

        if (dw2 < 1.5) {
            ofs << " D-W = " << std::setprecision(3) << dw2
                << " -- positive autocorrelation remains in residuals.\n"
                << " This does not indicate a problem with the homogenization;\n"
                << " it reflects the natural autocorrelation of the underlying series.\n";
            hinted = true;
        }

        const std::string& detMethod = h.detection.method;
        if (detMethod == "snht" && cps.size() > 10) {
            ofs << " SNHT detected " << cps.size() << " change points -- consider:\n"
                << "   - increasing min_segment_points (current: "
                << h.detection.minSegmentPoints << ")\n"
                << "   - decreasing significance_level (current: "
                << std::setprecision(4) << h.detection.significanceLevel << ")\n";
            hinted = true;
        }

        if (detMethod == "pelt" && cps.size() > 10) {
            ofs << " PELT detected " << cps.size() << " change points -- consider:\n"
                << "   - switching penalty_type from '" << h.detection.pelt.penaltyType
                << "' to 'mbic'\n"
                << "   - increasing min_segment_length (current: "
                << h.detection.pelt.minSegmentLength << ")\n";
            hinted = true;
        }

        if (cps.empty()) {
            ofs << " No change points detected.\n"
                << " If breaks are expected, try:\n"
                << "   - decreasing min_segment_points\n"
                << "   - increasing significance_level\n";
            hinted = true;
        }

        if (!hinted) {
            ofs << " No specific tuning required.\n";
        }
    } else {
        ofs << " No residuals available for diagnostic hints.\n";
    }

    // -----------------------------------------------------------------------
    // Homogenized series statistics
    // -----------------------------------------------------------------------
    ofs << "\n HOMOGENIZED SERIES STATISTICS\n" << SEP;

    if (result.adjustedSeries.size() > 0) {
        std::vector<double> adjVals;
        adjVals.reserve(result.adjustedSeries.size());
        for (std::size_t i = 0; i < result.adjustedSeries.size(); ++i) {
            adjVals.push_back(result.adjustedSeries[i].value);
        }
        const auto adjSum = loki::stats::summarize(adjVals, NanPolicy::SKIP, false);

        ofs << " Mean:      " << std::setw(10) << std::setprecision(6) << adjSum.mean
            << "    Std dev: " << std::setw(10) << adjSum.stddev << "\n";
        ofs << " MAD:       " << std::setw(10) << adjSum.mad
            << "    Min:     " << std::setw(10) << adjSum.min
            << "    Max: "     << std::setw(10) << adjSum.max << "\n";
    } else {
        ofs << " Adjusted series not available.\n";
    }

    ofs << HSEP;
    ofs.close();

    LOKI_INFO("HomogeneityAnalyzer: protocol written -> " + outPath.string());
}

// ---------------------------------------------------------------------------
//  _writeCsv
// ---------------------------------------------------------------------------

void HomogeneityAnalyzer::_writeCsv(const TimeSeries&       original,
                                     const HomogenizerResult& result,
                                     const std::string&       datasetName,
                                     const std::string&       compName) const
{
    const std::string fname = "homogeneity_" + datasetName + "_" + compName + ".csv";
    const std::filesystem::path outPath = m_cfg.csvDir / fname;

    std::ofstream ofs(outPath);
    if (!ofs.is_open()) {
        throw IoException("HomogeneityAnalyzer: cannot write CSV to '"
                          + outPath.string() + "'.");
    }

    ofs << "mjd;original;seasonal;deseasonalized;adjusted;flag;change_point\n";
    ofs << std::fixed << std::setprecision(10);

    const std::size_t n = original.size();
    for (std::size_t i = 0; i < n; ++i) {
        const double mjd    = original[i].time.mjd();
        const double orig   = original[i].value;
        const double seas   = (i < result.seasonal.size())
                              ? result.seasonal[i]
                              : std::numeric_limits<double>::quiet_NaN();
        const double deseas = (i < result.deseasonalizedValues.size())
                              ? result.deseasonalizedValues[i]
                              : std::numeric_limits<double>::quiet_NaN();
        const double adj    = (i < result.adjustedSeries.size())
                              ? result.adjustedSeries[i].value
                              : std::numeric_limits<double>::quiet_NaN();
        const int    flag   = original[i].flag;

        bool isCp = false;
        for (const auto& cp : result.changePoints) {
            if (cp.globalIndex == i) { isCp = true; break; }
        }

        ofs << mjd    << ";"
            << orig   << ";"
            << seas   << ";"
            << deseas << ";"
            << adj    << ";"
            << flag   << ";"
            << (isCp ? 1 : 0) << "\n";
    }

    LOKI_INFO("HomogeneityAnalyzer: CSV written -> " + outPath.string());
}