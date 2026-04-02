#include <loki/stationarity/stationarityAnalyzer.hpp>
#include <loki/stats/hypothesis.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <sstream>
#include <string>
#include <vector>

using namespace loki;

namespace loki::stationarity {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

StationarityAnalyzer::StationarityAnalyzer(Config cfg)
    : m_cfg(std::move(cfg))
{}

// ---------------------------------------------------------------------------
// analyze
// ---------------------------------------------------------------------------

StationarityResult StationarityAnalyzer::analyze(const std::vector<double>& y) const
{
    if (y.size() < 10) {
        throw DataException(
            "StationarityAnalyzer: series must have at least 10 observations, got " +
            std::to_string(y.size()) + ".");
    }

    StationarityResult result;
    result.n = y.size();

    // -----------------------------------------------------------------------
    // ADF test
    // -----------------------------------------------------------------------
    if (m_cfg.tests.adfEnabled) {
        try {
            AdfTest adf(m_cfg.tests.adf);
            result.adf = adf.test(y);
        } catch (const LOKIException& e) {
            LOKI_WARNING("StationarityAnalyzer: ADF test failed -- " +
                         std::string(e.what()));
        }
    }

    // -----------------------------------------------------------------------
    // KPSS test
    // -----------------------------------------------------------------------
    if (m_cfg.tests.kpssEnabled) {
        try {
            KpssTest kpss(m_cfg.tests.kpss);
            result.kpss = kpss.test(y);
        } catch (const LOKIException& e) {
            LOKI_WARNING("StationarityAnalyzer: KPSS test failed -- " +
                         std::string(e.what()));
        }
    }

    // -----------------------------------------------------------------------
    // PP test
    // -----------------------------------------------------------------------
    if (m_cfg.tests.ppEnabled) {
        try {
            PpTest pp(m_cfg.tests.pp);
            result.pp = pp.test(y);
        } catch (const LOKIException& e) {
            LOKI_WARNING("StationarityAnalyzer: PP test failed -- " +
                         std::string(e.what()));
        }
    }

    // -----------------------------------------------------------------------
    // Runs test (supplementary)
    // -----------------------------------------------------------------------
    if (m_cfg.tests.runsTestEnabled) {
        try {
            result.runsTest = loki::stats::runsTest(
                y, m_cfg.significanceLevel, NanPolicy::SKIP);
        } catch (const LOKIException& e) {
            LOKI_WARNING("StationarityAnalyzer: Runs test failed -- " +
                         std::string(e.what()));
        }
    }

    // -----------------------------------------------------------------------
    // Joint conclusion
    //
    // adfPpEvidence = true  means ADF or PP rejected the unit root hypothesis
    //                        (evidence FOR stationarity from ADF/PP side).
    // kpssEvidence  = true  means KPSS did NOT reject stationarity
    //                        (evidence FOR stationarity from KPSS side).
    // -----------------------------------------------------------------------
    const bool haveAdf  = result.adf.has_value();
    const bool haveKpss = result.kpss.has_value();
    const bool havePp   = result.pp.has_value();

    // ADF/PP: rejected means unit root rejected = evidence of stationarity.
    bool adfPpEvidence = false;
    int  adfPpCount    = 0;
    int  adfPpReject   = 0;
    if (haveAdf) { ++adfPpCount; if (result.adf->rejected) ++adfPpReject; }
    if (havePp)  { ++adfPpCount; if (result.pp->rejected)  ++adfPpReject; }
    if (adfPpCount > 0) {
        // Majority vote: stationarity evidence if more than half reject unit root.
        adfPpEvidence = (adfPpReject * 2 > adfPpCount);
    }

    // KPSS: NOT rejected means evidence of stationarity.
    bool kpssEvidence = false;
    if (haveKpss) {
        kpssEvidence = !result.kpss->rejected;
    }

    // Joint decision.
    const bool haveAdfPp = (adfPpCount > 0);

    if (haveAdfPp && haveKpss) {
        if (adfPpEvidence && kpssEvidence) {
            // Both agree: stationary.
            result.isStationary    = true;
            result.recommendedDiff = 0;
        } else {
            // Both agree non-stationary, or conflicting: conservative -> non-stationary.
            result.isStationary    = false;
            result.recommendedDiff = 1;
        }
    } else if (haveAdfPp) {
        result.isStationary    = adfPpEvidence;
        result.recommendedDiff = adfPpEvidence ? 0 : 1;
    } else if (haveKpss) {
        result.isStationary    = kpssEvidence;
        result.recommendedDiff = kpssEvidence ? 0 : 1;
    } else {
        // No tests ran -- cannot determine.
        result.isStationary    = false;
        result.recommendedDiff = 0;
    }

    result.conclusion = buildConclusion(result, adfPpEvidence, kpssEvidence);

    return result;
}

// ---------------------------------------------------------------------------
// buildConclusion
// ---------------------------------------------------------------------------

std::string StationarityAnalyzer::buildConclusion(const StationarityResult& r,
                                                   bool adfPpEvidence,
                                                   bool kpssEvidence)
{
    std::ostringstream oss;

    // ADF
    if (r.adf.has_value()) {
        const auto& a = *r.adf;
        oss << "ADF(" << a.trendType << ", lags=" << a.lags
            << "): tau=" << a.statistic
            << " [cv1%=" << a.critVal1pct
            << " cv5%=" << a.critVal5pct
            << " cv10%=" << a.critVal10pct << "]"
            << (a.rejected ? " -> unit root REJECTED" : " -> unit root not rejected")
            << "\n";
    }

    // PP
    if (r.pp.has_value()) {
        const auto& p = *r.pp;
        oss << "PP(" << p.trendType << ", lags=" << p.lags
            << "): Zt=" << p.statistic
            << " [cv5%=" << p.critVal5pct << "]"
            << (p.rejected ? " -> unit root REJECTED" : " -> unit root not rejected")
            << "\n";
    }

    // KPSS
    if (r.kpss.has_value()) {
        const auto& k = *r.kpss;
        oss << "KPSS(" << k.trendType << ", lags=" << k.lags
            << "): eta=" << k.statistic
            << " [cv5%=" << k.critVal5pct << "]"
            << (k.rejected ? " -> stationarity REJECTED" : " -> stationarity not rejected")
            << "\n";
    }

    // Runs test (supplementary)
    if (r.runsTest.has_value()) {
        const auto& rt = *r.runsTest;
        oss << "Runs test: z=" << rt.statistic
            << " p=" << rt.pValue
            << (rt.rejected ? " -> randomness REJECTED" : " -> randomness not rejected")
            << "\n";
    }

    // Joint verdict
    const bool haveAdfPp = r.adf.has_value() || r.pp.has_value();
    const bool haveKpss  = r.kpss.has_value();

    if (haveAdfPp && haveKpss && adfPpEvidence != kpssEvidence) {
        oss << "CONFLICTING evidence (ADF/PP and KPSS disagree).\n";
    }

    oss << "Overall: "
        << (r.isStationary ? "STATIONARY" : "NON-STATIONARY")
        << ". Recommended differencing order d=" << r.recommendedDiff << ".";

    return oss.str();
}

} // namespace loki::stationarity