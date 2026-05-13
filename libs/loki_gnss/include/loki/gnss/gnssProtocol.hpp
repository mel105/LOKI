#pragma once

#include <loki/gnss/gnssResult.hpp>
#include <loki/core/config.hpp>

namespace loki::gnss {

/**
 * @brief Writes the loki_gnss text protocol to OUTPUT/PROTOCOLS/.
 *
 * Output file: gnss_<station>_spp_protocol.txt
 *
 * Protocol sections (present only when the corresponding pipeline stage ran):
 *   - Header        : run parameters, config summary.
 *   - PARSE SUMMARY : NAV ephemeris counts, OBS epoch stats, constellation counts.
 *   - SPP RESULTS   : aggregate statistics, position error vs reference if available.
 *
 * New sections will be added alongside new GnssResult fields (PPP, DD, ...).
 */
class GnssProtocol {
public:
    /**
     * @brief Constructs the protocol writer.
     * @param cfg  Full AppConfig (provides output paths, station name, config params).
     */
    explicit GnssProtocol(const loki::AppConfig& cfg);

    /**
     * @brief Writes the protocol file for the given result.
     * @param result  Completed GnssResult from GnssAnalyzer::run().
     * @throws IoException if the output file cannot be opened.
     */
    void write(const GnssResult& result) const;

private:
    loki::AppConfig m_cfg;

    void _writeHeader            (std::ostream& f) const;
    void _writeAppliedCorrections(std::ostream& f) const;
    void _writeParseSummary(std::ostream& f, const ParseResult&  parse)  const;
    void _writeSppResults  (std::ostream& f, const SppSummary&   spp,
                            const std::vector<SppResult>& epochs)         const;

    static void sep(std::ostream& f);
    static void thin(std::ostream& f);
};

} // namespace loki::gnss
