#include <loki/arima/arimaOrderSelector.hpp>
#include <loki/arima/arimaFitter.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <limits>
#include <string>

using namespace loki;
using namespace loki::arima;

// ---------------------------------------------------------------------------
//  Construction
// ---------------------------------------------------------------------------

ArimaOrderSelector::ArimaOrderSelector(Config cfg)
    : m_cfg(std::move(cfg))
{}

// ---------------------------------------------------------------------------
//  select
// ---------------------------------------------------------------------------

ArimaOrder ArimaOrderSelector::select(const std::vector<double>& y,
                                       int                        d,
                                       const SarimaOrder&         seasonal) const
{
    if (y.empty()) {
        throw SeriesTooShortException(
            "ArimaOrderSelector::select: input series is empty.");
    }

    const bool useAic = (m_cfg.criterion != "bic");  // default to AIC

    double     bestCriterion = std::numeric_limits<double>::infinity();
    ArimaOrder bestOrder{0, d, 0};
    bool       anyFit = false;

    for (int p = 0; p <= m_cfg.maxP; ++p) {
        for (int q = 0; q <= m_cfg.maxQ; ++q) {

            // Skip models with no parameters when d == 0 (degenerate white noise)
            // but always allow ARIMA(0,d,0) as the baseline.

            ArimaFitterConfig fitCfg;
            fitCfg.order    = ArimaOrder{p, d, q};
            fitCfg.seasonal = seasonal;
            fitCfg.method   = m_cfg.method;

            try {
                ArimaFitter fitter(fitCfg);
                // y is already differenced by the caller -- pass d=0, D=0 internally
                // by fitting on y directly. We set d=0 in the fitter order because
                // differencing has already been applied.
                ArimaFitterConfig fitCfgNoDiff = fitCfg;
                fitCfgNoDiff.order.d    = 0;
                fitCfgNoDiff.seasonal.D = 0;

                ArimaFitter fitterNoDiff(fitCfgNoDiff);
                const ArimaResult result = fitterNoDiff.fit(y);

                const double criterion = useAic ? result.aic : result.bic;

                if (criterion < bestCriterion) {
                    bestCriterion = criterion;
                    bestOrder     = ArimaOrder{p, d, q};
                    anyFit        = true;
                }

            } catch (const LOKIException& ex) {
                LOKI_WARNING("ArimaOrderSelector: ARIMA("
                             + std::to_string(p) + "," + std::to_string(d)
                             + "," + std::to_string(q) + ") failed: "
                             + std::string(ex.what()) + " -- skipped.");
            }
        }
    }

    if (!anyFit) {
        throw AlgorithmException(
            "ArimaOrderSelector::select: no candidate ARIMA model could be fitted.");
    }

    LOKI_INFO("ArimaOrderSelector: best order p=" + std::to_string(bestOrder.p)
              + " d=" + std::to_string(bestOrder.d)
              + " q=" + std::to_string(bestOrder.q)
              + "  " + m_cfg.criterion + "=" + std::to_string(bestCriterion));

    return bestOrder;
}
