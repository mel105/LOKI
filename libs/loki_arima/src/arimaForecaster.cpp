#include <loki/arima/arimaForecaster.hpp>
#include <loki/core/exceptions.hpp>

#include <cmath>
#include <string>

using namespace loki;
using namespace loki::arima;

// ---------------------------------------------------------------------------
//  Construction
// ---------------------------------------------------------------------------

ArimaForecaster::ArimaForecaster(const ArimaResult& result)
    : m_result(result)
{
    if (result.fitted.empty()) {
        throw AlgorithmException(
            "ArimaForecaster: cannot construct from an empty ArimaResult "
            "(fitted vector is empty).");
    }
}

// ---------------------------------------------------------------------------
//  computePsiWeights
// ---------------------------------------------------------------------------

std::vector<double> ArimaForecaster::computePsiWeights(int horizon) const
{
    // psi[0] = 1 always.
    // For h >= 1:
    //   psi[h] = sum_{i: arLags[i] <= h} arCoeffs[i] * psi[h - arLags[i]]
    //            + (h is in maLags ? maCoeffs[maLags.index(h)] : 0)
    //
    // This is the exact recursion for the MA(inf) representation.

    std::vector<double> psi(static_cast<std::size_t>(horizon), 0.0);
    if (horizon < 1) { return psi; }
    psi[0] = 1.0;

    const auto& arIdx   = m_result.arLags;
    const auto& arCoeff = m_result.arCoeffs;
    const auto& maIdx   = m_result.maLags;
    const auto& maCoeff = m_result.maCoeffs;

    for (int h = 1; h < horizon; ++h) {
        double val = 0.0;

        // AR contribution
        for (std::size_t i = 0; i < arIdx.size(); ++i) {
            const int lag = arIdx[i];
            if (lag <= h) {
                val += arCoeff[i] * psi[static_cast<std::size_t>(h - lag)];
            }
        }

        // MA contribution: theta coefficient at lag h (if it exists)
        for (std::size_t j = 0; j < maIdx.size(); ++j) {
            if (maIdx[j] == h) {
                val += maCoeff[j];
                break;
            }
        }

        psi[static_cast<std::size_t>(h)] = val;
    }

    return psi;
}

// ---------------------------------------------------------------------------
//  forecast
// ---------------------------------------------------------------------------

ForecastResult ArimaForecaster::forecast(int horizon) const
{
    if (horizon < 1) {
        throw AlgorithmException(
            "ArimaForecaster::forecast: horizon must be >= 1, got "
            + std::to_string(horizon) + ".");
    }

    const auto& arIdx   = m_result.arLags;
    const auto& arCoeff = m_result.arCoeffs;
    const auto& maIdx   = m_result.maLags;
    const auto& maCoeff = m_result.maCoeffs;

    const std::size_t nObs = m_result.fitted.size();

    // Build extended history: observed differenced series (proxy via fitted + residuals)
    // y_history[i] = fitted[i] + residuals[i] = original differenced value
    // For indices beyond n, we use the forecast values.
    std::vector<double> yHist(nObs + static_cast<std::size_t>(horizon), 0.0);
    for (std::size_t i = 0; i < nObs; ++i) {
        yHist[i] = m_result.fitted[i] + m_result.residuals[i];
    }

    // Epsilon history: model residuals for observed period; 0 for future steps.
    std::vector<double> eHist(nObs + static_cast<std::size_t>(horizon), 0.0);
    for (std::size_t i = 0; i < nObs; ++i) {
        eHist[i] = m_result.residuals[i];
    }

    ForecastResult result;
    result.horizon = horizon;
    result.forecast.resize(static_cast<std::size_t>(horizon));
    result.lower95 .resize(static_cast<std::size_t>(horizon));
    result.upper95 .resize(static_cast<std::size_t>(horizon));

    // Psi weights for variance accumulation
    const std::vector<double> psi = computePsiWeights(horizon + 1);

    double cumPsiSq = 0.0;  // sum_{j=0}^{h-1} psi[j]^2 at step h

    for (int h = 1; h <= horizon; ++h) {
        const std::size_t tNext = nObs + static_cast<std::size_t>(h) - 1;

        double f = m_result.intercept;

        // AR contribution from yHist
        for (std::size_t i = 0; i < arIdx.size(); ++i) {
            const int lag = arIdx[i];
            if (tNext >= static_cast<std::size_t>(lag)) {
                f += arCoeff[i] * yHist[tNext - static_cast<std::size_t>(lag)];
            }
        }

        // MA contribution from eHist (zero for future steps)
        for (std::size_t j = 0; j < maIdx.size(); ++j) {
            const int lag = maIdx[j];
            if (tNext >= static_cast<std::size_t>(lag)) {
                f += maCoeff[j] * eHist[tNext - static_cast<std::size_t>(lag)];
            }
        }

        yHist[tNext] = f;

        result.forecast[static_cast<std::size_t>(h - 1)] = f;

        // Prediction interval variance: sigma2 * sum_{j=0}^{h-1} psi[j]^2
        const double psih = psi[static_cast<std::size_t>(h - 1)];
        cumPsiSq += psih * psih;

        const double stdErr = std::sqrt(m_result.sigma2 * cumPsiSq);
        constexpr double Z95 = 1.959963985;  // normal quantile at 0.975
        result.lower95[static_cast<std::size_t>(h - 1)] = f - Z95 * stdErr;
        result.upper95[static_cast<std::size_t>(h - 1)] = f + Z95 * stdErr;
    }

    return result;
}
