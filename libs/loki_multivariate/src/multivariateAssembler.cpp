#include "loki/multivariate/multivariateAssembler.hpp"

#include "loki/core/exceptions.hpp"
#include "loki/core/logger.hpp"
#include "loki/timeseries/gapFiller.hpp"
#include "loki/timeseries/timeSeries.hpp"
#include "loki/timeseries/timeStamp.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <string>

using namespace loki;
using namespace loki::multivariate;

namespace {

// Returns true if v is NaN or Inf.
bool isBad(double v) noexcept
{
    return !std::isfinite(v);
}

} // anonymous namespace

// -----------------------------------------------------------------------------
//  Construction
// -----------------------------------------------------------------------------

MultivariateAssembler::MultivariateAssembler(const AppConfig& cfg)
    : m_cfg(cfg)
{}

// -----------------------------------------------------------------------------
//  assemble
// -----------------------------------------------------------------------------

MultivariateSeries MultivariateAssembler::assemble() const
{
    const auto& mcfg = m_cfg.multivariate;

    if (mcfg.input.files.empty()) {
        throw DataException(
            "MultivariateAssembler: no input files configured.");
    }

    const double toleranceMjd =
        mcfg.input.syncToleranceSeconds / 86400.0;

    // -- Step 1: load all files, collect individual TimeSeries ----------------

    std::vector<TimeSeries>  channels;
    std::vector<std::string> channelNames;

    for (const auto& fc : mcfg.input.files) {
        const InputConfig ic = _buildInputConfig(fc);
        const Loader      loader(ic);

        const std::filesystem::path fp(fc.path);
        const std::string           stem = fp.stem().string();

        LOKI_INFO("MultivariateAssembler: loading '" + fp.filename().string() + "'.");

        LoadResult lr = loader.load(fp);

        for (std::size_t k = 0; k < lr.series.size(); ++k) {
            // Derive channel name: header name or fallback.
            std::string name;
            if (k < lr.columnNames.size() && !lr.columnNames[k].empty()) {
                name = lr.columnNames[k];
            } else {
                // 1-based column index relative to file.
                const int colIdx = ic.columns.empty()
                    ? static_cast<int>(k) + 1
                    : ic.columns[k];
                name = stem + "_col_" + std::to_string(colIdx);
            }

            LOKI_INFO("MultivariateAssembler:   channel '" + name
                      + "' n=" + std::to_string(lr.series[k].size()));

            channels.push_back(std::move(lr.series[k]));
            channelNames.push_back(std::move(name));
        }
    }

    if (channels.size() < 2) {
        throw DataException(
            "MultivariateAssembler: at least 2 channels are required, got "
            + std::to_string(channels.size()) + ".");
    }

    LOKI_INFO("MultivariateAssembler: " + std::to_string(channels.size())
              + " channels loaded. Sync strategy: " + mcfg.input.syncStrategy);

    // -- Step 2: build common time axis ---------------------------------------

    std::vector<double> commonMjd;

    if (mcfg.input.syncStrategy == "inner") {
        commonMjd = _innerJoin(channels, toleranceMjd);
    } else if (mcfg.input.syncStrategy == "outer") {
        commonMjd = _outerJoin(channels, toleranceMjd);
    } else {
        throw ConfigException(
            "MultivariateAssembler: unknown sync_strategy '"
            + mcfg.input.syncStrategy + "'. Use 'inner' or 'outer'.");
    }

    if (commonMjd.empty()) {
        throw DataException(
            "MultivariateAssembler: no common timestamps found after "
            + mcfg.input.syncStrategy + " join. "
            "Check that input files overlap in time.");
    }

    LOKI_INFO("MultivariateAssembler: common time axis has "
              + std::to_string(commonMjd.size()) + " epochs.");

    // -- Step 3: align each channel to common axis, gap fill, assemble -------

    const auto nObs      = static_cast<Eigen::Index>(commonMjd.size());
    const auto nChannels = static_cast<Eigen::Index>(channels.size());

    Eigen::MatrixXd mat(nObs, nChannels);

    for (Eigen::Index j = 0; j < nChannels; ++j) {
        std::vector<double> aligned =
            _alignToGrid(channels[static_cast<std::size_t>(j)],
                         commonMjd, toleranceMjd);

        // Replace Inf with NaN for uniform handling.
        for (double& v : aligned) {
            if (std::isinf(v)) v = std::numeric_limits<double>::quiet_NaN();
        }

        // Gap fill if requested (outer join always has gaps; inner may also
        // have NaN from bad values in the source file).
        if (mcfg.preprocessing.applyGapFilling) {
            aligned = _applyGapFill(aligned, commonMjd,
                                    channelNames[static_cast<std::size_t>(j)]);
        }

        // Copy into matrix column.
        for (Eigen::Index i = 0; i < nObs; ++i) {
            mat(i, j) = aligned[static_cast<std::size_t>(i)];
        }
    }

    // -- Step 4: standardise if requested -------------------------------------

    if (mcfg.preprocessing.standardize) {
        for (Eigen::Index j = 0; j < nChannels; ++j) {
            Eigen::VectorXd col = mat.col(j);
            _standardise(col, channelNames[static_cast<std::size_t>(j)]);
            mat.col(j) = col;
        }
        LOKI_INFO("MultivariateAssembler: channels standardised to zero mean "
                  "and unit variance.");
    }

    // -- Step 5: build timestamp vector from common MJD ----------------------

    std::vector<TimeStamp> timestamps;
    timestamps.reserve(static_cast<std::size_t>(nObs));
    for (double mjd : commonMjd) {
        timestamps.push_back(TimeStamp::fromMjd(mjd));
    }

    LOKI_INFO("MultivariateAssembler: assembly complete. "
              "Matrix shape " + std::to_string(nObs)
              + " x " + std::to_string(nChannels) + ".");

    return MultivariateSeries(std::move(mat),
                              std::move(timestamps),
                              std::move(channelNames));
}

// -----------------------------------------------------------------------------
//  _buildInputConfig
// -----------------------------------------------------------------------------

InputConfig MultivariateAssembler::_buildInputConfig(
    const MultivariateFileConfig& fc) const
{
    // Convert time format string to enum locally.
    TimeFormat tf = TimeFormat::MJD;
    const std::string& tfs = fc.timeFormat;
    if      (tfs == "mjd")               tf = TimeFormat::MJD;
    else if (tfs == "utc")               tf = TimeFormat::UTC;
    else if (tfs == "gps_total_seconds") tf = TimeFormat::GPS_TOTAL_SECONDS;
    else if (tfs == "gps_week_sow")      tf = TimeFormat::GPS_WEEK_SOW;
    else if (tfs == "unix")              tf = TimeFormat::UNIX;
    else if (tfs == "index")             tf = TimeFormat::INDEX;
    else throw ConfigException(
        "MultivariateAssembler: unknown time_format '" + tfs + "'.");

    InputConfig ic;
    ic.timeFormat = tf;

    // Delimiter: single char or tab.
    const std::string& ds = fc.delimiter;
    ic.delimiter = (ds == "\t" || ds == "\\t") ? '\t'
                 : ds.empty()                  ? ' '
                 : ds.front();

    ic.commentChar  = fc.commentChar;
    ic.columns      = fc.columns;
    ic.timeColumns  = fc.timeColumns;
    ic.mode         = InputMode::SINGLE_FILE;

    return ic;
}

// -----------------------------------------------------------------------------
//  _innerJoin
// -----------------------------------------------------------------------------

std::vector<double> MultivariateAssembler::_innerJoin(
    const std::vector<TimeSeries>& channels,
    double                          tolerance) const
{
    if (channels.empty()) return {};

    // Start with the MJD values of the first channel as candidates.
    std::vector<double> candidates;
    candidates.reserve(channels[0].size());
    for (std::size_t i = 0; i < channels[0].size(); ++i) {
        candidates.push_back(channels[0][i].time.mjd());
    }

    // For each subsequent channel, keep only candidates that have a match.
    for (std::size_t c = 1; c < channels.size(); ++c) {
        std::vector<double> chMjd;
        chMjd.reserve(channels[c].size());
        for (std::size_t i = 0; i < channels[c].size(); ++i) {
            chMjd.push_back(channels[c][i].time.mjd());
        }

        std::vector<double> surviving;
        surviving.reserve(candidates.size());

        std::size_t ci = 0; // pointer into sorted chMjd
        for (double cand : candidates) {
            // Binary search for nearest in chMjd.
            auto it = std::lower_bound(chMjd.begin(), chMjd.end(), cand);

            bool matched = false;
            if (it != chMjd.end() && std::abs(*it - cand) <= tolerance) {
                matched = true;
            }
            if (!matched && it != chMjd.begin()) {
                --it;
                if (std::abs(*it - cand) <= tolerance) matched = true;
            }
            (void)ci;
            if (matched) surviving.push_back(cand);
        }

        candidates = std::move(surviving);
        if (candidates.empty()) break;
    }

    return candidates; // already sorted (derived from sorted channel[0])
}

// -----------------------------------------------------------------------------
//  _outerJoin
// -----------------------------------------------------------------------------

std::vector<double> MultivariateAssembler::_outerJoin(
    const std::vector<TimeSeries>& channels,
    double                          tolerance) const
{
    // Collect all MJD values from all channels into one sorted vector,
    // then merge values within tolerance into a single representative.
    std::vector<double> all;
    for (const auto& ts : channels) {
        for (std::size_t i = 0; i < ts.size(); ++i) {
            all.push_back(ts[i].time.mjd());
        }
    }
    std::sort(all.begin(), all.end());

    if (all.empty()) return {};

    std::vector<double> merged;
    merged.reserve(all.size());

    double groupStart = all[0];
    double groupSum   = all[0];
    int    groupCount = 1;

    for (std::size_t k = 1; k < all.size(); ++k) {
        if (all[k] - groupStart <= tolerance) {
            // Same group -- accumulate for mean representative.
            groupSum   += all[k];
            groupCount += 1;
        } else {
            merged.push_back(groupSum / groupCount);
            groupStart = all[k];
            groupSum   = all[k];
            groupCount = 1;
        }
    }
    merged.push_back(groupSum / groupCount);

    return merged;
}

// -----------------------------------------------------------------------------
//  _alignToGrid
// -----------------------------------------------------------------------------

std::vector<double> MultivariateAssembler::_alignToGrid(
    const TimeSeries&          ts,
    const std::vector<double>& targetMjd,
    double                      tolerance) const
{
    const std::size_t nTarget = targetMjd.size();
    std::vector<double> out(nTarget, std::numeric_limits<double>::quiet_NaN());

    if (ts.empty()) return out;

    // Build sorted MJD index for ts.
    std::vector<double> tsMjd;
    tsMjd.reserve(ts.size());
    for (std::size_t i = 0; i < ts.size(); ++i) {
        tsMjd.push_back(ts[i].time.mjd());
    }

    for (std::size_t k = 0; k < nTarget; ++k) {
        const double target = targetMjd[k];

        // Binary search for nearest observation.
        auto it = std::lower_bound(tsMjd.begin(), tsMjd.end(), target);

        double bestDist = std::numeric_limits<double>::max();
        std::size_t bestIdx = ts.size();

        if (it != tsMjd.end()) {
            const double d = std::abs(*it - target);
            if (d < bestDist) { bestDist = d; bestIdx = static_cast<std::size_t>(it - tsMjd.begin()); }
        }
        if (it != tsMjd.begin()) {
            --it;
            const double d = std::abs(*it - target);
            if (d < bestDist) { bestDist = d; bestIdx = static_cast<std::size_t>(it - tsMjd.begin()); }
        }

        if (bestDist <= tolerance && bestIdx < ts.size()) {
            const double v = ts[bestIdx].value;
            out[k] = isBad(v) ? std::numeric_limits<double>::quiet_NaN() : v;
        }
        // else: out[k] stays NaN (missing epoch for this channel)
    }

    return out;
}

// -----------------------------------------------------------------------------
//  _applyGapFill
// -----------------------------------------------------------------------------

std::vector<double> MultivariateAssembler::_applyGapFill(
    const std::vector<double>& values,
    const std::vector<double>& targetMjd,
    const std::string&         name) const
{
    // Build a temporary TimeSeries from (targetMjd, values).
    TimeSeries tmp;
    for (std::size_t i = 0; i < targetMjd.size(); ++i) {
        tmp.append(TimeStamp::fromMjd(targetMjd[i]), values[i]);
    }

    // Configure GapFiller from preprocessing config.
    const auto& pp = m_cfg.multivariate.preprocessing;

    GapFiller::Config gfc;
    gfc.maxFillLength = pp.maxFillLength;

    const std::string& strat = pp.gapStrategy;
    if      (strat == "linear")       gfc.strategy = GapFiller::Strategy::LINEAR;
    else if (strat == "forward_fill") gfc.strategy = GapFiller::Strategy::FORWARD_FILL;
    else if (strat == "mean")         gfc.strategy = GapFiller::Strategy::MEAN;
    else if (strat == "none")         gfc.strategy = GapFiller::Strategy::NONE;
    else {
        LOKI_WARNING("MultivariateAssembler: unknown gap strategy '"
                     + strat + "' for channel '" + name
                     + "' -- using LINEAR.");
        gfc.strategy = GapFiller::Strategy::LINEAR;
    }

    GapFiller filler(gfc);

    TimeSeries filled;
    try {
        filled = filler.fill(tmp);
    } catch (const LOKIException& ex) {
        LOKI_WARNING("MultivariateAssembler: GapFiller failed for channel '"
                     + name + "': " + ex.what() + " -- leaving NaNs.");
        filled = tmp;
    }

    // Extract values back to vector.
    std::vector<double> out;
    out.reserve(filled.size());
    for (std::size_t i = 0; i < filled.size(); ++i) {
        out.push_back(filled[i].value);
    }
    return out;
}

// -----------------------------------------------------------------------------
//  _standardise
// -----------------------------------------------------------------------------

void MultivariateAssembler::_standardise(Eigen::VectorXd& col,
                                         const std::string& name) const
{
    // Compute mean and std ignoring NaN.
    double sum   = 0.0;
    double sum2  = 0.0;
    int    count = 0;

    for (Eigen::Index i = 0; i < col.size(); ++i) {
        const double v = col(i);
        if (std::isfinite(v)) {
            sum  += v;
            sum2 += v * v;
            ++count;
        }
    }

    if (count < 2) {
        LOKI_WARNING("MultivariateAssembler: channel '" + name
                     + "' has fewer than 2 finite values -- skipping standardisation.");
        return;
    }

    const double mean = sum / count;
    const double var  = sum2 / count - mean * mean;
    const double std  = (var > 0.0) ? std::sqrt(var) : 0.0;

    if (std == 0.0) {
        LOKI_WARNING("MultivariateAssembler: channel '" + name
                     + "' is constant -- skipping standardisation.");
        return;
    }

    for (Eigen::Index i = 0; i < col.size(); ++i) {
        if (std::isfinite(col(i))) {
            col(i) = (col(i) - mean) / std;
        }
    }
}
