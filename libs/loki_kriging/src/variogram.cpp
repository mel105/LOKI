#include <loki/kriging/variogram.hpp>

#include <loki/core/exceptions.hpp>
#include <loki/math/nelderMead.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <numeric>
#include <stdexcept>

using namespace loki;

namespace loki::kriging {

// =============================================================================
//  Empirical variogram
// =============================================================================

std::vector<VariogramPoint> computeEmpiricalVariogram(
    const TimeSeries&             ts,
    const KrigingVariogramConfig& cfg)
{
    // Collect valid observations
    std::vector<double> vals;
    std::vector<double> mjds;
    vals.reserve(ts.size());
    mjds.reserve(ts.size());

    for (std::size_t i = 0; i < ts.size(); ++i) {
        const double v = ts[i].value;
        if (!std::isnan(v)) {
            vals.push_back(v);
            mjds.push_back(ts[i].time.mjd());
        }
    }

    if (vals.size() < 3) {
        throw DataException(
            "computeEmpiricalVariogram: need at least 3 valid observations, got "
            + std::to_string(vals.size()) + ".");
    }

    const std::size_t n = vals.size();

    // Determine maximum lag
    double maxLag = cfg.maxLag;
    if (maxLag <= 0.0) {
        double minMjd = *std::min_element(mjds.begin(), mjds.end());
        double maxMjd = *std::max_element(mjds.begin(), mjds.end());
        maxLag = (maxMjd - minMjd) * 0.5; // standard: use half the total range
    }

    if (maxLag <= 0.0) {
        throw DataException(
            "computeEmpiricalVariogram: could not determine a positive maxLag "
            "(all observations at same time?).");
    }

    const int    nBins   = cfg.nLagBins;
    const double binSize = maxLag / static_cast<double>(nBins);

    // Accumulate squared differences per bin
    std::vector<double> sumSqDiff(static_cast<std::size_t>(nBins), 0.0);
    std::vector<int>    counts   (static_cast<std::size_t>(nBins), 0);

    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = i + 1; j < n; ++j) {
            const double h = std::abs(mjds[i] - mjds[j]);
            if (h >= maxLag) continue;

            const int bin = static_cast<int>(h / binSize);
            if (bin < 0 || bin >= nBins) continue;

            const double diff = vals[i] - vals[j];
            sumSqDiff[static_cast<std::size_t>(bin)] += diff * diff;
            counts   [static_cast<std::size_t>(bin)] += 1;
        }
    }

    // Build result -- skip bins with fewer than 2 pairs
    std::vector<VariogramPoint> result;
    result.reserve(static_cast<std::size_t>(nBins));

    for (int k = 0; k < nBins; ++k) {
        const int cnt = counts[static_cast<std::size_t>(k)];
        if (cnt < 2) continue;

        VariogramPoint pt;
        pt.lag   = (static_cast<double>(k) + 0.5) * binSize; // bin centre
        pt.gamma = 0.5 * sumSqDiff[static_cast<std::size_t>(k)]
                       / static_cast<double>(cnt);
        pt.count = cnt;
        result.push_back(pt);
    }

    return result;
}

// =============================================================================
//  Theoretical variogram models
// =============================================================================

double variogramSpherical(double h, double nugget, double sill, double range)
{
    if (h <= 0.0) return 0.0;
    if (range <= 0.0) return nugget + (sill - nugget); // degenerate
    const double partial = sill - nugget;
    if (h >= range) return nugget + partial;
    const double r = h / range;
    return nugget + partial * (1.5 * r - 0.5 * r * r * r);
}

double variogramExponential(double h, double nugget, double sill, double range)
{
    if (h <= 0.0) return 0.0;
    if (range <= 0.0) return nugget + (sill - nugget);
    return nugget + (sill - nugget) * (1.0 - std::exp(-h / range));
}

double variogramGaussian(double h, double nugget, double sill, double range)
{
    if (h <= 0.0) return 0.0;
    if (range <= 0.0) return nugget + (sill - nugget);
    const double r = h / range;
    return nugget + (sill - nugget) * (1.0 - std::exp(-(r * r)));
}

double variogramPower(double h, double nugget, double sill, double range)
{
    // sill = scaling coefficient a, range = power exponent alpha in (0,2)
    if (h <= 0.0) return 0.0;
    return nugget + sill * std::pow(h, range);
}

double variogramNugget(double h, double nugget)
{
    return (h > 0.0) ? nugget : 0.0;
}

double variogramEval(double h, const VariogramFitResult& fit)
{
    if      (fit.model == "spherical")   return variogramSpherical  (h, fit.nugget, fit.sill, fit.range);
    else if (fit.model == "exponential") return variogramExponential(h, fit.nugget, fit.sill, fit.range);
    else if (fit.model == "gaussian")    return variogramGaussian   (h, fit.nugget, fit.sill, fit.range);
    else if (fit.model == "power")       return variogramPower      (h, fit.nugget, fit.sill, fit.range);
    else if (fit.model == "nugget")      return variogramNugget     (h, fit.nugget);
    else {
        throw AlgorithmException(
            "variogramEval: unknown variogram model '" + fit.model + "'.");
    }
}

// =============================================================================
//  Variogram fitting (WLS via Nelder-Mead)
// =============================================================================

namespace {

// Derive initial parameter estimates from the empirical variogram
void deriveInitialParams(const std::vector<VariogramPoint>& empirical,
                         const KrigingVariogramConfig&      cfg,
                         double& nugget0, double& sill0, double& range0)
{
    // Use config overrides if provided (non-zero)
    nugget0 = (cfg.nugget > 0.0) ? cfg.nugget : 0.0;
    sill0   = (cfg.sill   > 0.0) ? cfg.sill   : 0.0;
    range0  = (cfg.range  > 0.0) ? cfg.range  : 0.0;

    if (sill0 <= 0.0 || range0 <= 0.0) {
        // Estimate from empirical data
        const double maxGamma = empirical.back().gamma;
        const double maxLag   = empirical.back().lag;

        if (sill0   <= 0.0) sill0   = maxGamma;
        if (range0  <= 0.0) range0  = maxLag * 0.5;  // practical range heuristic
        if (nugget0 <= 0.0) {
            // Nugget: extrapolate from first bin toward h=0
            if (empirical.size() >= 2) {
                const double h1 = empirical[0].lag;
                const double g1 = empirical[0].gamma;
                const double h2 = empirical[1].lag;
                const double g2 = empirical[1].gamma;
                const double slope = (g2 - g1) / (h2 - h1 + 1e-12);
                const double extrap = g1 - slope * h1;
                nugget0 = std::max(0.0, extrap);
            } else {
                nugget0 = 0.0;
            }
        }
    }

    // Safety: ensure nugget < sill
    if (nugget0 >= sill0) nugget0 = sill0 * 0.1;
}

// WLS objective function (weights = pair counts)
double wlsObjective(const std::vector<VariogramPoint>& emp,
                    const std::string& model,
                    double nugget, double sill, double range)
{
    // Penalty for invalid parameter combinations
    const double BIG = 1.0e12;
    if (nugget  <  0.0)    return BIG;
    if (range   <= 0.0)    return BIG;

    // For power model: sill is the scaling coeff (must be > 0), range is
    // the exponent (must be in (0, 2)).
    if (model == "power") {
        if (sill  <= 0.0)   return BIG;
        if (range <= 0.0 || range >= 2.0) return BIG;
    } else {
        if (sill  <= nugget) return BIG;
    }

    VariogramFitResult tmp;
    tmp.model  = model;
    tmp.nugget = nugget;
    tmp.sill   = sill;
    tmp.range  = range;

    double obj = 0.0;
    for (const auto& pt : emp) {
        const double res = pt.gamma - variogramEval(pt.lag, tmp);
        obj += static_cast<double>(pt.count) * res * res;
    }
    return obj;
}

} // anonymous namespace

VariogramFitResult fitVariogram(const std::vector<VariogramPoint>& empirical,
                                const KrigingVariogramConfig&      cfg)
{
    if (empirical.size() < 3) {
        throw DataException(
            "fitVariogram: need at least 3 valid variogram bins, got "
            + std::to_string(empirical.size()) + ".");
    }
    if (cfg.model != "spherical"   &&
        cfg.model != "exponential" &&
        cfg.model != "gaussian"    &&
        cfg.model != "power"       &&
        cfg.model != "nugget")
    {
        throw AlgorithmException(
            "fitVariogram: unknown variogram model '" + cfg.model + "'. "
            "Valid: spherical, exponential, gaussian, power, nugget.");
    }

    VariogramFitResult result;
    result.model     = cfg.model;
    result.converged = false;

    // Pure nugget -- no fitting needed
    if (cfg.model == "nugget") {
        result.nugget = (cfg.nugget > 0.0)
            ? cfg.nugget
            : empirical.back().gamma;
        result.sill  = result.nugget;
        result.range = 0.0;
        // Compute RMSE
        double sse = 0.0;
        for (const auto& pt : empirical) {
            const double res = pt.gamma - result.nugget;
            sse += res * res;
        }
        result.rmse      = std::sqrt(sse / static_cast<double>(empirical.size()));
        result.converged = true;
        return result;
    }

    // Derive initial parameters
    double nugget0 = 0.0;
    double sill0   = 0.0;
    double range0  = 0.0;
    deriveInitialParams(empirical, cfg, nugget0, sill0, range0);

    // Build Nelder-Mead objective (3 free parameters: nugget, sill, range)
    const std::string& modelName = cfg.model;
    auto objective = [&](const std::vector<double>& p) -> double {
        return wlsObjective(empirical, modelName, p[0], p[1], p[2]);
    };

    const std::vector<double> x0 = {nugget0, sill0, range0};

    const auto nmResult = loki::math::nelderMead(objective, x0, 2000, 1.0e-9);

    result.nugget    = nmResult.params[0];
    result.sill      = nmResult.params[1];
    result.range     = nmResult.params[2];
    result.converged = nmResult.converged;

    if (!result.converged) {
        LOKI_WARNING("fitVariogram: Nelder-Mead did not converge for model '"
                     + cfg.model + "'. Using best estimate found.");
    }

    // Clamp to valid domain (in case of near-boundary convergence)
    result.nugget = std::max(0.0, result.nugget);
    result.range  = std::max(1.0e-12, result.range);
    if (modelName != "power") {
        result.sill = std::max(result.nugget + 1.0e-12, result.sill);
    } else {
        result.sill = std::max(1.0e-12, result.sill);
    }

    // Compute RMSE
    double sse = 0.0;
    for (const auto& pt : empirical) {
        const double res = pt.gamma - variogramEval(pt.lag, result);
        sse += res * res;
    }
    result.rmse = std::sqrt(sse / static_cast<double>(empirical.size()));

    return result;
}

} // namespace loki::kriging
