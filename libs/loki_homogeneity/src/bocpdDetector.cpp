#include <loki/homogeneity/bocpdDetector.hpp>
#include <loki/core/exceptions.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <numeric>
#include <vector>

using namespace loki;

namespace loki::homogeneity {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

BocpdDetector::BocpdDetector(Config cfg)
    : m_cfg{std::move(cfg)}
{}

// ---------------------------------------------------------------------------
// Public interface -- posteriorChangeProb
// ---------------------------------------------------------------------------

std::vector<double> BocpdDetector::posteriorChangeProb(
        const std::vector<double>& z) const
{
    const std::size_t n = z.size();

    if (n < MIN_SERIES) {
        throw DataException(
            "BocpdDetector: series too short (n=" +
            std::to_string(n) + ", minimum=" + std::to_string(MIN_SERIES) + ").");
    }

    const double hazard    = 1.0 / m_cfg.hazardLambda;
    const double logHazard = std::log(hazard);
    const double logSurv   = std::log(1.0 - hazard);

    // Run-length posterior: R[r] = log P(run_length = r | data_1..t).
    // At t=0, run length is 0 with probability 1.
    // We maintain a vector of (log_prob, NixStats) pairs, one per active run length.

    struct RunState {
        double   logProb;
        NixStats stats;
    };

    // Initialise: one state at run length 0 with log prob = 0.
    std::vector<RunState> states;
    states.reserve(512);
    states.push_back({0.0, priorStats()});

    std::vector<double> cpProb(n, 0.0);

    // Normalisation constant (log-sum-exp of all state probs).
    // After each step we renormalise to prevent underflow.

    for (std::size_t t = 0; t < n; ++t) {
        const double xt = z[t];

        // --- Compute log predictive for each current run state ---------------
        std::vector<double> logPred(states.size());
        for (std::size_t r = 0; r < states.size(); ++r) {
            logPred[r] = logPredictive(states[r].stats, xt);
        }

        // --- Grow: run continues (hazard not triggered) ----------------------
        // New state for run length r+1 gets:
        //   log P(r+1 | data_1..t+1) = logProb[r] + logPred[r] + logSurv
        std::vector<RunState> newStates;
        newStates.reserve(states.size() + 1);

        for (std::size_t r = 0; r < states.size(); ++r) {
            newStates.push_back({
                states[r].logProb + logPred[r] + logSurv,
                updateStats(states[r].stats, xt)
            });
        }

        // --- Reset: change point (hazard triggered) --------------------------
        // Collapse all run lengths into a new run length 0:
        //   log P(0 | data_1..t+1) = log( sum_r P(r | data_1..t) * pred(r) * hazard )
        //                           = log-sum-exp(logProb[r] + logPred[r]) + logHazard

        // Compute log-sum-exp of (logProb[r] + logPred[r]).
        double maxVal = -std::numeric_limits<double>::infinity();
        for (std::size_t r = 0; r < states.size(); ++r) {
            const double v = states[r].logProb + logPred[r];
            if (v > maxVal) maxVal = v;
        }

        double sumExp = 0.0;
        for (std::size_t r = 0; r < states.size(); ++r) {
            sumExp += std::exp(states[r].logProb + logPred[r] - maxVal);
        }
        const double logResetProb = maxVal + std::log(sumExp) + logHazard;

        // Insert the reset state at position 0 (run length 0).
        newStates.insert(newStates.begin(), {logResetProb, priorStats()});

        // --- Normalise -------------------------------------------------------
        double maxLogProb = -std::numeric_limits<double>::infinity();
        for (const auto& s : newStates) {
            if (s.logProb > maxLogProb) maxLogProb = s.logProb;
        }
        double logNorm = 0.0;
        for (const auto& s : newStates) {
            logNorm += std::exp(s.logProb - maxLogProb);
        }
        logNorm = maxLogProb + std::log(logNorm);

        for (auto& s : newStates) {
            s.logProb -= logNorm;
        }

        // --- Record P(run_length = 0 | data_1..t+1) as CP probability -------
        cpProb[t] = std::exp(newStates[0].logProb);

        states = std::move(newStates);

        // --- Prune states with negligible probability (log prob < -50) -------
        // This keeps memory and compute bounded for long series.
        static constexpr double LOG_PROB_THRESHOLD = -50.0;
        std::vector<RunState> pruned;
        pruned.reserve(states.size());
        for (auto& s : states) {
            if (s.logProb > LOG_PROB_THRESHOLD) {
                pruned.push_back(std::move(s));
            }
        }
        if (!pruned.empty()) {
            states = std::move(pruned);
        }
    }

    return cpProb;
}

// ---------------------------------------------------------------------------
// Public interface -- detectAll
// ---------------------------------------------------------------------------

std::vector<ChangePoint> BocpdDetector::detectAll(
        const std::vector<double>& z,
        const std::vector<double>& times) const
{
    const std::size_t n = z.size();

    if (!times.empty() && times.size() != n) {
        throw DataException(
            "BocpdDetector::detectAll: times.size() != z.size().");
    }

    // Get posterior CP probability for every time step.
    const std::vector<double> prob = posteriorChangeProb(z);

    // Threshold + minSegmentLength suppression to get discrete CPs.
    // We declare a CP at the local maximum within each region where prob >= threshold,
    // subject to a minimum gap of minSegmentLength samples between CPs.

    const int    minSeg   = std::max(1, m_cfg.minSegmentLength);
    const double thresh   = m_cfg.threshold;

    std::vector<ChangePoint> result;

    int lastCp = -minSeg - 1;  // tracks index of last accepted CP

    for (std::size_t t = 0; t < n; ++t) {
        if (prob[t] < thresh) continue;
        if (static_cast<int>(t) - lastCp < minSeg) continue;

        // Find the local peak within a window of minSeg around t.
        // Scan forward until prob drops below threshold to find the peak.
        std::size_t peakIdx = t;
        double      peakVal = prob[t];

        for (std::size_t tt = t + 1;
             tt < n && prob[tt] >= thresh && static_cast<int>(tt - t) < minSeg;
             ++tt)
        {
            if (prob[tt] > peakVal) {
                peakVal  = prob[tt];
                peakIdx  = tt;
            }
        }

        // Compute shift at peakIdx: use a window of minSeg on each side.
        const std::size_t winBefore = static_cast<std::size_t>(minSeg);
        const std::size_t winAfter  = static_cast<std::size_t>(minSeg);

        const std::size_t bBegin = (peakIdx >= winBefore) ? peakIdx - winBefore : 0;
        const std::size_t bEnd   = peakIdx;
        const std::size_t aBegin = peakIdx;
        const std::size_t aEnd   = std::min(n, peakIdx + winAfter);

        double meanBefore = 0.0;
        if (bEnd > bBegin) {
            for (std::size_t i = bBegin; i < bEnd; ++i) meanBefore += z[i];
            meanBefore /= static_cast<double>(bEnd - bBegin);
        }

        double meanAfter = 0.0;
        if (aEnd > aBegin) {
            for (std::size_t i = aBegin; i < aEnd; ++i) meanAfter += z[i];
            meanAfter /= static_cast<double>(aEnd - aBegin);
        }

        const double shift = meanAfter - meanBefore;
        const double mjd   = times.empty() ? 0.0 : times[peakIdx];

        result.push_back(ChangePoint{peakIdx, mjd, shift, peakVal});

        lastCp = static_cast<int>(peakIdx);

        // Skip to after the detected peak.
        t = peakIdx;
    }

    return result;
}

// ---------------------------------------------------------------------------
// NIX model helpers
// ---------------------------------------------------------------------------

BocpdDetector::NixStats BocpdDetector::priorStats() const
{
    return NixStats{
        m_cfg.priorVar,   // kappa_0
        m_cfg.priorMean,  // mu_0
        m_cfg.priorAlpha, // alpha_0
        m_cfg.priorBeta,  // beta_0
        0                 // count
    };
}

// ---------------------------------------------------------------------------

BocpdDetector::NixStats BocpdDetector::updateStats(const NixStats& s, double x)
{
    // Normal-InverseGamma (NIX) conjugate update for one new observation x.
    // Parameters follow the standard parameterisation in Murphy (2007).
    const double kappaN = s.kappa + 1.0;
    const double muN    = (s.kappa * s.mu + x) / kappaN;
    const double alphaN = s.alpha + 0.5;
    const double betaN  = s.beta
                        + (s.kappa * (x - s.mu) * (x - s.mu)) / (2.0 * kappaN);

    return NixStats{kappaN, muN, alphaN, betaN, s.count + 1};
}

// ---------------------------------------------------------------------------

double BocpdDetector::logPredictive(const NixStats& s, double x)
{
    // Predictive distribution is Student-t with:
    //   df   = 2 * alpha
    //   loc  = mu
    //   scale^2 = beta * (kappa + 1) / (alpha * kappa)
    //
    // log p(x | s) = log Gamma((df+1)/2) - log Gamma(df/2)
    //              - 0.5 * log(pi * df * scale^2)
    //              - (df+1)/2 * log(1 + (x - mu)^2 / (df * scale^2))

    const double df     = 2.0 * s.alpha;
    const double scale2 = s.beta * (s.kappa + 1.0) / (s.alpha * s.kappa);

    if (scale2 <= 0.0 || df <= 0.0) {
        return -std::numeric_limits<double>::infinity();
    }

    const double z     = x - s.mu;
    const double ratio = z * z / (df * scale2);

    // Use lgamma for numerical stability.
    const double logP = std::lgamma((df + 1.0) / 2.0)
                      - std::lgamma(df / 2.0)
                      - 0.5 * std::log(std::numbers::pi * df * scale2)
                      - ((df + 1.0) / 2.0) * std::log(1.0 + ratio);

    return logP;
}

} // namespace loki::homogeneity
