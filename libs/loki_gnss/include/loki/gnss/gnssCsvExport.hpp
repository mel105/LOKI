#pragma once

#include <loki/gnss/gnssResult.hpp>
#include <loki/core/config.hpp>

namespace loki::gnss {

/**
 * @brief Exports loki_gnss pipeline results to CSV files.
 *
 * Output files written to OUTPUT/CSV/:
 *
 *   gnss_<station>_spp_epochs.csv
 *     One row per valid SPP epoch.
 *
 *   gnss_<station>_spp_clk.csv
 *     RINEX-CLOCK-inspired receiver clock file.
 *
 *   gnss_<station>_spp_tropo.csv
 *     Per-epoch Saastamoinen ZHD/ZWD (constant std atmosphere).
 *
 *   gnss_<station>_ppp_epochs.csv
 *     One row per valid PPP epoch: position, clock, ZTD wet, ZTD total,
 *     convergence flag, residual RMS (code and phase).
 *
 *   gnss_<station>_ppp_tropo.csv
 *     Per-epoch Kalman-estimated ZTD wet and total ZTD.
 */
class GnssCsvExport {
public:
    explicit GnssCsvExport(const loki::AppConfig& cfg);

    /**
     * @brief Exports all available CSV files for the completed result.
     * Individual exporters that have no data return immediately.
     */
    void exportAll(const GnssResult& result, const ObsFile& obs) const;

    // SPP exports.
    void exportSppEpochs(const GnssResult& result) const;
    void exportClk(const std::vector<SppResult>& spp) const;
    void exportTropo(const std::vector<SppResult>& spp, const ObsFile& obs) const;

    // PPP exports.
    void exportPppEpochs(const GnssResult& result) const;
    void exportPppTropo(const std::vector<PppResult>& ppp, const ObsFile& obs) const;

private:
    loki::AppConfig m_cfg;
    std::string outPath(const std::string& tag) const;
};

} // namespace loki::gnss
