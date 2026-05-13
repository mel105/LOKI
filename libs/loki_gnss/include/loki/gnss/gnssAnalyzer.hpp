#pragma once

#include <loki/gnss/gnssResult.hpp>
#include <loki/gnss/gnssTypes.hpp>
#include <loki/core/config.hpp>

namespace loki::gnss {

/**
 * @brief Main orchestrator for the loki_gnss processing pipeline.
 *
 * Responsibilities:
 *   1. Build parse summary from pre-loaded NAV + OBS data.
 *   2. Run SPP solver when gnss.spp.enabled == true.
 *   3. Run PPP solver when gnss.ppp.enabled == true.
 *   4. Write the text protocol (GnssProtocol).
 *   5. Produce all plots (PlotGnss).
 *   6. Export CSV files (GnssCsvExport).
 *
 * GnssAnalyzer does NOT own the parsers.  Parsing is done in main() and the
 * NavFile + ObsFile are passed directly to run().
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
     * Sequence (enabled stages only):
     *   _buildParseSummary()
     *   -> _runSpp()           (if spp.enabled)
     *   -> _runPpp()           (if ppp.enabled)
     *   -> GnssProtocol::write()
     *   -> PlotGnss::plotAll()
     *   -> GnssCsvExport::exportAll()
     *
     * @param nav  Parsed navigation file (broadcast ephemerides).
     * @param obs  Parsed observation file.
     * @return     GnssResult containing all computed results.
     */
    GnssResult run(const NavFile& nav, const ObsFile& obs) const;

private:
    loki::AppConfig m_cfg;

    ParseResult            _buildParseSummary(const NavFile& nav,
                                               const ObsFile& obs) const;

    std::vector<SppResult> _runSpp(const NavFile& nav,
                                    const ObsFile& obs) const;

    std::vector<PppResult> _runPpp(const NavFile& nav,
                                    const ObsFile& obs) const;

    SppSummary _computeSppSummary(const std::vector<SppResult>& results) const;

    PppSummary _computePppSummary(const std::vector<PppResult>& results) const;
};

} // namespace loki::gnss
