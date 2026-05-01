#pragma once

#include "loki/multivariate/multivariateSeries.hpp"
#include "loki/core/config.hpp"
#include "loki/io/loader.hpp"

#include <vector>

namespace loki::multivariate {

/**
 * @brief Assembles a synchronised MultivariateSeries from multiple input files.
 *
 * MultivariateAssembler is the preprocessing stage of the loki_multivariate
 * pipeline. It bridges the generic loki_core Loader (which reads one file into
 * one or more TimeSeries) and the analysis methods that require a single
 * synchronised data matrix.
 *
 * ### Pipeline
 *  1. For each file entry in MultivariateInputConfig, construct an InputConfig
 *     and call Loader::load(). Each selected column becomes one channel.
 *  2. Build a common time axis from all channels using the configured
 *     sync_strategy ("inner" or "outer") and sync_tolerance_seconds.
 *  3. For "outer" sync: insert NaN observations for missing epochs per channel,
 *     then apply GapFiller per channel.
 *  4. For "inner" sync: keep only timestamps present in every channel
 *     (within tolerance). No gap filling required.
 *  5. Optionally standardise each channel to zero mean and unit variance.
 *  6. Return a MultivariateSeries.
 *
 * ### Channel naming
 * Channel names are derived from LoadResult::columnNames[i]. If a column name
 * is absent, the fallback is "<fileStem>_col_<N>" where N is the 1-based
 * column index. This produces unique, informative labels for biplot and CCF
 * heatmap axes.
 *
 * ### Time scale unification
 * All timestamps are stored as TimeStamp objects internally (MJD double).
 * Regardless of the input time format (UTC, GPS, MJD, Unix), Loader already
 * converts to TimeStamp on load. The assembler therefore works exclusively
 * in MJD space and never needs to know the original time format.
 */
class MultivariateAssembler {
public:

    // -------------------------------------------------------------------------
    //  Construction
    // -------------------------------------------------------------------------

    /**
     * @brief Constructs an assembler with the given application configuration.
     * @param cfg Full AppConfig. Uses cfg.multivariate for all parameters.
     */
    explicit MultivariateAssembler(const AppConfig& cfg);

    // -------------------------------------------------------------------------
    //  Public interface
    // -------------------------------------------------------------------------

    /**
     * @brief Loads all files, synchronises channels, and returns a MultivariateSeries.
     *
     * The workspace INPUT directory is used to resolve relative file paths.
     *
     * @return Synchronised, optionally gap-filled and standardised MultivariateSeries.
     * @throws FileNotFoundException if any input file does not exist.
     * @throws ParseException if any file contains no valid data.
     * @throws DataException if fewer than 2 channels are available after loading.
     * @throws ConfigException if sync_strategy is unrecognised.
     */
    [[nodiscard]] MultivariateSeries assemble() const;

private:

    const AppConfig& m_cfg;

    // -------------------------------------------------------------------------
    //  Private helpers
    // -------------------------------------------------------------------------

    /**
     * @brief Converts a MultivariateFileConfig to an InputConfig for Loader.
     *
     * The per-file time_format, delimiter, comment_char, columns, and
     * time_columns are mapped to the corresponding InputConfig fields.
     */
    [[nodiscard]]
    InputConfig _buildInputConfig(const MultivariateFileConfig& fc) const;

    /**
     * @brief Builds the inner-join common time axis.
     *
     * Returns the sorted set of MJD values that appear in every channel
     * within sync_tolerance_seconds / 86400 MJD tolerance.
     *
     * Complexity: O(N * C) where N = observations per channel, C = channels.
     *
     * @param channels  One TimeSeries per channel.
     * @param tolerance Tolerance in MJD days.
     * @return Sorted common MJD values.
     */
    [[nodiscard]]
    std::vector<double> _innerJoin(const std::vector<TimeSeries>& channels,
                                   double                          tolerance) const;

    /**
     * @brief Builds the outer-join time axis (union of all timestamps).
     *
     * Returns the sorted union of all MJD values across all channels,
     * merging values within sync_tolerance_seconds / 86400 MJD tolerance.
     *
     * @param channels  One TimeSeries per channel.
     * @param tolerance Tolerance in MJD days.
     * @return Sorted union MJD values.
     */
    [[nodiscard]]
    std::vector<double> _outerJoin(const std::vector<TimeSeries>& channels,
                                   double                          tolerance) const;

    /**
     * @brief Interpolates a single TimeSeries onto the target MJD grid.
     *
     * For each target MJD, finds the closest observation within tolerance.
     * If none is found, inserts NaN. Used for both inner and outer joins
     * to extract aligned values.
     *
     * @param ts        Source TimeSeries (sorted).
     * @param targetMjd Target MJD grid (sorted).
     * @param tolerance Match tolerance in MJD days.
     * @return Vector of values aligned to targetMjd, NaN for missing epochs.
     */
    [[nodiscard]]
    std::vector<double> _alignToGrid(const TimeSeries&           ts,
                                     const std::vector<double>&  targetMjd,
                                     double                       tolerance) const;

    /**
     * @brief Applies GapFiller to a value vector on the common time axis.
     *
     * Constructs a temporary TimeSeries from (targetMjd, values), runs
     * GapFiller with the configured strategy, and returns the filled values.
     *
     * @param values    Values aligned to targetMjd (may contain NaN).
     * @param targetMjd Common MJD grid.
     * @param name      Channel name for logging.
     * @return Gap-filled value vector.
     */
    [[nodiscard]]
    std::vector<double> _applyGapFill(const std::vector<double>& values,
                                      const std::vector<double>& targetMjd,
                                      const std::string&         name) const;

    /**
     * @brief Standardises a column vector to zero mean and unit variance.
     *
     * Skips NaN values when computing mean and std. If std is zero
     * (constant channel), the column is left unchanged and a warning is logged.
     *
     * @param col    Column to standardise (modified in place).
     * @param name   Channel name for logging.
     */
    void _standardise(Eigen::VectorXd& col, const std::string& name) const;
};

} // namespace loki::multivariate
