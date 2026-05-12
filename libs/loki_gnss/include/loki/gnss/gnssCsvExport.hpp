#pragma once

#include <loki/gnss/gnssResult.hpp>
#include <loki/core/config.hpp>

namespace loki::gnss {

/**
 * @brief Exports loki_gnss pipeline results to CSV and standardised text files.
 *
 * Output files written to OUTPUT/CSV/:
 *
 *   gnss_<station>_spp_epochs.csv
 *     One row per valid epoch: MJD, X, Y, Z, lat, lon, h, clk_m, clk_us,
 *     pdop, n_sats, converged, mean_residual_m, rms_residual_m.
 *
 *   gnss_<station>_spp_clk.csv
 *     RINEX-CLOCK-inspired format (# header, data rows):
 *     # AR <station> <year> <month> <day> <hh> <mm> <ss>  2  <clk_s>  <sigma_s>
 *     sigma estimated from epoch LSQ residual RMS / speed_of_light.
 *
 *   gnss_<station>_spp_tropo.csv
 *     Per-epoch Saastamoinen components: epoch, zhd_mm, zwet_mm, ztd_mm, slant_m.
 *     Useful for comparison with PPP TRO files and VMF3 products.
 *
 * New exporters will be added as pipeline stages grow (PPP, DD, ...).
 */
class GnssCsvExport {
public:
    explicit GnssCsvExport(const loki::AppConfig& cfg);

    /**
     * @brief Exports all available CSV files for the completed result.
     * Individual exporters that have no data return immediately.
     */
    void exportAll(const GnssResult& result, const ObsFile& obs) const;

    /// SPP per-epoch positions, clocks, quality.
    void exportSppEpochs(const GnssResult& result) const;

    /// RINEX-CLOCK-inspired receiver clock file.
    void exportClk(const std::vector<SppResult>& spp) const;

    /**
     * @brief Per-epoch Saastamoinen dry/wet ZTD components.
     *
     * Requires the station ECEF position from the OBS header to derive
     * geodetic lat/lon/h for the Saastamoinen model.
     */
    void exportTropo(const std::vector<SppResult>& spp, const ObsFile& obs) const;

private:
    loki::AppConfig m_cfg;

    std::string outPath(const std::string& tag) const;
};

} // namespace loki::gnss
