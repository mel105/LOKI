#include <loki/arima/arimaAnalyzer.hpp>
#include <loki/arima/arimaFitter.hpp>
#include <loki/arima/arimaOrderSelector.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/stats/descriptive.hpp>
#include <loki/stats/hypothesis.hpp>
#include <loki/stationarity/stationarityAnalyzer.hpp>

#include <algorithm>
#include <string>

using namespace loki;
using namespace loki::arima;

// ---------------------------------------------------------------------------
//  Construction
// ---------------------------------------------------------------------------

ArimaAnalyzer::ArimaAnalyzer(Config cfg)
    : m_cfg(std::move(cfg))
{}

// ---------------------------------------------------------------------------
//  determineDifferencingOrder
// ---------------------------------------------------------------------------

int ArimaAnalyzer::determineDifferencingOrder(const std::vector<double>& y) const
{
    if (m_cfg.d >= 0) {
        LOKI_INFO("ArimaAnalyzer: using configured d=" + std::to_string(m_cfg.d));
        return m_cfg.d;
    }

    // d == -1: auto-detect via StationarityAnalyzer
    LOKI_INFO("ArimaAnalyzer: d=-1, running StationarityAnalyzer to determine d.");

    loki::StationarityConfig stCfg;
    stCfg.significanceLevel = m_cfg.significanceLevel;
    // Use default test config (ADF + KPSS + PP all enabled)

    loki::stationarity::StationarityAnalyzer analyzer(stCfg);
    const loki::stationarity::StationarityResult stResult = analyzer.analyze(y);

    const int d = stResult.recommendedDiff;
    LOKI_INFO("ArimaAnalyzer: StationarityAnalyzer recommends d="
              + std::to_string(d) + "  (" + stResult.conclusion + ")");
    return d;
}

// ---------------------------------------------------------------------------
//  analyze
// ---------------------------------------------------------------------------

ArimaResult ArimaAnalyzer::analyze(const std::vector<double>& y) const
{
    if (y.size() < 10) {
        throw SeriesTooShortException(
            "ArimaAnalyzer::analyze: series must have at least 10 observations, got "
            + std::to_string(y.size()) + ".");
    }

    // -------------------------------------------------------------------------
    //  Step 1: Determine differencing order d
    // -------------------------------------------------------------------------

    const int d = determineDifferencingOrder(y);

    // -------------------------------------------------------------------------
    //  Step 2: Apply differencing to get the series for order selection + fitting
    // -------------------------------------------------------------------------

    const int D = m_cfg.seasonal.D;
    const int s = m_cfg.seasonal.s;

    // Apply seasonal differencing first, then ordinary
    std::vector<double> ydiff = y;

    if (s > 0 && D > 0) {
        for (int pass = 0; pass < D; ++pass) {
            ydiff = loki::stats::laggedDiff(ydiff, s, loki::NanPolicy::THROW);
        }
    }
    for (int pass = 0; pass < d; ++pass) {
        ydiff = loki::stats::diff(ydiff, loki::NanPolicy::THROW);
    }

    LOKI_INFO("ArimaAnalyzer: after differencing (d=" + std::to_string(d)
              + ", D=" + std::to_string(D) + ", s=" + std::to_string(s)
              + ") n=" + std::to_string(ydiff.size()));

    if (ydiff.size() < 10) {
        throw SeriesTooShortException(
            "ArimaAnalyzer::analyze: series too short after differencing (n="
            + std::to_string(ydiff.size()) + ").");
    }

    // -------------------------------------------------------------------------
    //  Step 3: Order selection (p, q)
    // -------------------------------------------------------------------------

    // const SarimaOrder seasonal = m_cfg.seasonal;
    SarimaOrder seasonal;
    seasonal.P = m_cfg.seasonal.P;
    seasonal.D = m_cfg.seasonal.D;
    seasonal.Q = m_cfg.seasonal.Q;
    seasonal.s = m_cfg.seasonal.s;

    ArimaOrder order{m_cfg.p, d, m_cfg.q};

    if (m_cfg.autoOrder) {
        ArimaOrderSelectorConfig selCfg;
        selCfg.maxP      = m_cfg.maxP;
        selCfg.maxQ      = m_cfg.maxQ;
        selCfg.criterion = m_cfg.criterion;
        selCfg.method    = m_cfg.fitter.method;

        ArimaOrderSelector selector(selCfg);
        order = selector.select(ydiff, d, seasonal);
    } else {
        LOKI_INFO("ArimaAnalyzer: using configured order p=" + std::to_string(order.p)
                  + " d=" + std::to_string(order.d)
                  + " q=" + std::to_string(order.q));
    }

    // -------------------------------------------------------------------------
    //  Step 4: Fit
    // -------------------------------------------------------------------------

    ArimaFitterConfig fitCfg;
    fitCfg.order    = ArimaOrder{order.p, 0, order.q};  // differencing already applied
    fitCfg.seasonal = SarimaOrder{seasonal.P, 0, seasonal.Q, seasonal.s};
    fitCfg.method   = m_cfg.fitter.method;
    fitCfg.maxIter  = m_cfg.fitter.maxIterations;
    fitCfg.tol      = m_cfg.fitter.tol;

    ArimaFitter fitter(fitCfg);
    ArimaResult result = fitter.fit(ydiff);

    // Restore the true differencing order in the result
    result.order.d    = d;
    result.seasonal.D = D;

    LOKI_INFO("ArimaAnalyzer: fit complete."
              + std::string("  ARIMA(") + std::to_string(order.p)
              + "," + std::to_string(d) + "," + std::to_string(order.q) + ")"
              + (s > 0 ? ("(" + std::to_string(seasonal.P)
                          + "," + std::to_string(D)
                          + "," + std::to_string(seasonal.Q)
                          + ")[" + std::to_string(s) + "]") : "")
              + "  AIC=" + std::to_string(result.aic)
              + "  BIC=" + std::to_string(result.bic)
              + "  sigma2=" + std::to_string(result.sigma2));

    // -------------------------------------------------------------------------
    //  Step 5: Ljung-Box diagnostic on model residuals
    // -------------------------------------------------------------------------

    try {
        const int maxLb = std::min(static_cast<int>(result.residuals.size()) / 5,
                                   std::max(10, order.p + order.q + 4));
        if (maxLb >= 1 && result.residuals.size() >= static_cast<std::size_t>(maxLb + 2)) {
            const auto lb = loki::stats::ljungBox(
                result.residuals, maxLb, m_cfg.significanceLevel,
                loki::NanPolicy::SKIP);
            LOKI_INFO("Ljung-Box(lag=" + std::to_string(maxLb)
                      + "): Q=" + std::to_string(lb.statistic)
                      + "  p=" + std::to_string(lb.pValue)
                      + (lb.rejected ? "  -> residuals NOT white noise" : "  -> residuals OK"));
        }
    } catch (const LOKIException& ex) {
        LOKI_WARNING("Ljung-Box test failed: " + std::string(ex.what()));
    }

    return result;
}
