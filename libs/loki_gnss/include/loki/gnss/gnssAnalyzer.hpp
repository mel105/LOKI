#pragma once

#include <loki/gnss/gnssResult.hpp>
#include <loki/gnss/gnssTypes.hpp>
#include <loki/core/config.hpp>

namespace loki::gnss {

/**
 * @brief Main orchestrator for the loki_gnss processing pipeline.
 *
 * Responsibilities:
 *   1. Compute parse summary (ObsSummary, ParseResult) from pre-loaded data.
 *   2. Build and run the SPP solver (SppSolver) for all epochs.
 *   3. Compute SppSummary aggregate statistics.
 *   4. Write the text protocol (GnssProtocol).
 *   5. Produce all plots (PlotGnss).
 *
 * GnssAnalyzer does NOT own the parsers. Parsing is done in main() and the
 * NavFile + ObsFile are passed directly to run(). This keeps the analyzer
 * focused on computation and output, not I/O.
 *
 * New pipeline stages (PPP, DD, seismology events, ...) will be added as
 * additional private methods called from run() and filling new fields in
 * GnssResult.
 */
class GnssAnalyzer {
public:
    /**
     * @brief Constructs the analyzer with the full application configuration.
     * @param cfg  Fully populated AppConfig (contains GnssConfig, output paths).
     */
    explicit GnssAnalyzer(const loki::AppConfig& cfg);

    /**
     * @brief Runs the full GNSS pipeline on pre-parsed NAV + OBS data.
     *
     * Sequence:
     *   _buildParseSummary() -> _runSpp() -> _computeSppSummary()
     *   -> GnssProtocol::write() -> PlotGnss::plotAll()
     *
     * @param nav  Parsed navigation file.
     * @param obs  Parsed observation file.
     * @return     GnssResult containing all computed results.
     */
    GnssResult run(const NavFile& nav, const ObsFile& obs) const;

private:
    loki::AppConfig m_cfg;

    /// @brief Fills ParseResult from nav + obs metadata.
    ParseResult _buildParseSummary(const NavFile& nav, const ObsFile& obs) const;

    /// @brief Runs SPP solver for all epochs; returns raw SppResult vector.
    std::vector<SppResult> _runSpp(const NavFile& nav, const ObsFile& obs) const;

    /// @brief Computes aggregate SppSummary from raw results.
    SppSummary _computeSppSummary(const std::vector<SppResult>& results) const;
};

} // namespace loki::gnss
