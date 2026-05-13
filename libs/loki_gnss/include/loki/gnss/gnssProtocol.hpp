#pragma once

#include <loki/gnss/gnssResult.hpp>
#include <loki/core/config.hpp>

namespace loki::gnss {

/**
 * @brief Writes the loki_gnss text protocol to OUTPUT/PROTOCOLS/.
 *
 * Output file name reflects the active task:
 *   gnss_<station>_ppp_protocol.txt  (when hasPpp == true)
 *   gnss_<station>_spp_protocol.txt  (when hasSpp == true and hasPpp == false)
 *   gnss_<station>_parse_protocol.txt (parse-only run)
 *
 * Sections present only when the corresponding pipeline stage ran:
 *   - Header, Applied Corrections, Parse Summary (always).
 *   - SPP RESULTS  (when hasSpp == true).
 *   - PPP RESULTS  (when hasPpp == true).
 */
class GnssProtocol {
public:
    explicit GnssProtocol(const loki::AppConfig& cfg);

    /**
     * @brief Writes the protocol file for the given result.
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
    void _writePppResults  (std::ostream& f, const PppSummary&   ppp,
                            const std::vector<PppResult>& epochs)         const;

    static void sep(std::ostream& f);
    static void thin(std::ostream& f);
};

} // namespace loki::gnss
