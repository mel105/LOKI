#include <loki/gnss/gnssCsvExport.hpp>
#include <loki/geodesy/coordTransform.hpp>
#include <loki/math/ellipsoid.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <numbers>

using namespace loki;
using namespace loki::gnss;

namespace fs = std::filesystem;

// =============================================================================
//  File-scope helpers
// =============================================================================

namespace {

// Saastamoinen dry and wet ZTD components at station (lat_rad, h_m).
// Returns {zhd_m, zwet_m}.
std::pair<double,double> saastamoinenComponents(double lat_rad, double h_m)
{
    const double h = std::max(h_m, -1000.0);
    const double P = 1013.25 * std::pow(1.0 - 2.2557e-5 * h, 5.2568);
    const double T = 288.15 - 6.5e-3 * h;
    const double e = 6.108 * std::exp(17.15 * (T - 273.15) / (T - 38.25));

    const double zhd = 0.0022768 * P
        / (1.0 - 0.00266 * std::cos(2.0 * lat_rad) - 2.8e-7 * h);
    const double zwet = 0.002277 * (1255.0 / T + 0.05) * e;
    return {zhd, zwet};
}

// GPS time -> calendar UTC string "YYYY MM DD HH MM SS.SSSSSS"
std::string gpsToUtcStr(const GpsTime& t)
{
    const auto ts = t.toTimeStamp();
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << ts.year()   << " "
        << std::setw(2) << ts.month()  << " "
        << std::setw(2) << ts.day()    << " "
        << std::setw(2) << ts.hour()   << " "
        << std::setw(2) << ts.minute() << " "
        << std::fixed << std::setprecision(6)
        << std::setw(9) << ts.second();
    return oss.str();
}

// DOY from GpsTime
int dayOfYear(const GpsTime& t)
{
    const auto ts = t.toTimeStamp();
    // Simple: count days from Jan 1
    int y = ts.year(), m = ts.month(), d = ts.day();
    static const int days[12] = {0,31,59,90,120,151,181,212,243,273,304,334};
    int doy = days[m - 1] + d;
    // Leap year check
    if (m > 2 && ((y%4==0 && y%100!=0) || y%400==0)) ++doy;
    return doy;
}

} // namespace

// =============================================================================
//  Constructor
// =============================================================================

GnssCsvExport::GnssCsvExport(const AppConfig& cfg)
    : m_cfg(cfg)
{}

std::string GnssCsvExport::outPath(const std::string& tag) const
{
    return m_cfg.csvDir.string()
           + "/gnss_" + m_cfg.gnss.station + "_" + tag + ".csv";
}

// =============================================================================
//  exportAll
// =============================================================================

void GnssCsvExport::exportAll(const GnssResult& result, const ObsFile& obs) const
{
    if (result.hasSpp) {
        exportSppEpochs(result);
        exportClk(result.spp);
        exportTropo(result.spp, obs);
    }
}

// =============================================================================
//  exportSppEpochs
//  One row per valid epoch: MJD; X; Y; Z; lat; lon; h; clk_m; clk_us;
//  pdop; n_sats; converged; mean_res_m; rms_res_m
// =============================================================================

void GnssCsvExport::exportSppEpochs(const GnssResult& result) const
{
    fs::create_directories(m_cfg.csvDir);
    const std::string path = outPath("spp_epochs");
    std::ofstream f(path);
    if (!f.is_open())
        throw IoException("GnssCsvExport: cannot open " + path);

    static const loki::math::Ellipsoid wgs84 =
        loki::math::makeEllipsoid(loki::math::EllipsoidModel::WGS84);

    constexpr double C = 299792458.0;

    f << "mjd;x_m;y_m;z_m;lat_deg;lon_deg;h_m;"
         "clk_m;clk_us;pdop;n_sats;converged;"
         "mean_residual_m;rms_residual_m\n";

    for (const auto& r : result.spp) {
        if (!r.valid) continue;

        const loki::geodesy::EcefPoint ep{r.x, r.y, r.z};
        const loki::geodesy::GeodPoint gp = loki::geodesy::ecef2geod(ep, wgs84);

        double meanRes = 0.0, rmsRes = 0.0;
        if (!r.residuals.empty()) {
            double sumAbs = 0.0, sumSq = 0.0;
            for (double res : r.residuals) {
                sumAbs += std::fabs(res);
                sumSq  += res * res;
            }
            const double n = static_cast<double>(r.residuals.size());
            meanRes = sumAbs / n;
            rmsRes  = std::sqrt(sumSq / n);
        }

        f << std::fixed << std::setprecision(9)
          << r.time.toTimeStamp().mjd()  << ";"
          << std::setprecision(4)
          << r.x   << ";" << r.y   << ";" << r.z   << ";"
          << std::setprecision(9)
          << gp.lat << ";" << gp.lon << ";"
          << std::setprecision(4)
          << gp.h   << ";"
          << r.clkBiasM << ";"
          << r.clkBiasM / C * 1.0e6 << ";"
          << std::setprecision(3)
          << r.pdop << ";"
          << r.nSats << ";"
          << (r.converged ? 1 : 0) << ";"
          << std::setprecision(4)
          << meanRes << ";" << rmsRes
          << "\n";
    }

    LOKI_INFO("GnssCsvExport: " + path);
}

// =============================================================================
//  exportClk
//  RINEX-CLOCK-inspired format with # header comments.
//  Clock sigma estimated as residual RMS / c (rough approximation for SPP).
// =============================================================================

void GnssCsvExport::exportClk(const std::vector<SppResult>& spp) const
{
    fs::create_directories(m_cfg.csvDir);
    const std::string path = outPath("spp_clk");
    std::ofstream f(path);
    if (!f.is_open())
        throw IoException("GnssCsvExport: cannot open " + path);

    constexpr double C = 299792458.0;
    const std::string& sta = m_cfg.gnss.station;

    // Header (# prefix as agreed)
    f << "# LOKI GNSS -- RECEIVER CLOCK OFFSET\n";
    f << "# Station   : " << sta << "\n";
    f << "# Format    : AR <station> <UTC epoch>  2  <clk_s>  <sigma_s>\n";
    f << "# Clock bias : GPS receiver clock from SPP LSQ [seconds]\n";
    f << "# Sigma      : estimated from pseudorange residual RMS / c\n";
    f << "# NOTE       : SPP sigma is approximate; use PPP for rigorous uncertainty\n";
    f << "#\n";

    for (const auto& r : spp) {
        if (!r.valid) continue;

        const double clk_s = r.clkBiasM / C;

        // Sigma from residual RMS; fallback to 1 m / c if no residuals
        double sigma_s = 1.0 / C;
        if (!r.residuals.empty()) {
            double sumSq = 0.0;
            for (double res : r.residuals) sumSq += res * res;
            sigma_s = std::sqrt(sumSq / static_cast<double>(r.residuals.size())) / C;
        }

        f << "AR " << std::left << std::setw(5) << sta << " "
          << gpsToUtcStr(r.time) << "  2  "
          << std::scientific << std::setprecision(12)
          << clk_s << "  " << sigma_s << "\n";
    }

    LOKI_INFO("GnssCsvExport: " + path);
}

// =============================================================================
//  exportTropo
//  SINEX-TRO-inspired format with # header.
//  Computes Saastamoinen Zhd and Zwet at station position.
//  Slant delay = mapping(Zhd + Zwet) where mapping = 1/sin(mean_elev).
// =============================================================================

void GnssCsvExport::exportTropo(const std::vector<SppResult>& spp,
                                  const ObsFile& obs) const
{
    if (spp.empty()) return;
    fs::create_directories(m_cfg.csvDir);
    const std::string path = outPath("spp_tropo");
    std::ofstream f(path);
    if (!f.is_open())
        throw IoException("GnssCsvExport: cannot open " + path);

    static const loki::math::Ellipsoid wgs84 =
        loki::math::makeEllipsoid(loki::math::EllipsoidModel::WGS84);

    // Station geodetic position (from OBS approx ECEF)
    const loki::geodesy::EcefPoint staEcef{
        obs.receiver.approxX, obs.receiver.approxY, obs.receiver.approxZ};
    const loki::geodesy::GeodPoint staGeod = loki::geodesy::ecef2geod(staEcef, wgs84);
    const double lat_rad = staGeod.lat * (std::numbers::pi / 180.0);
    const double h_m     = staGeod.h;

    const auto [zhd, zwet] = saastamoinenComponents(lat_rad, h_m);
    const double ztd = zhd + zwet;

    const std::string& sta = m_cfg.gnss.station;

    // Header
    f << "# LOKI GNSS -- TROPOSPHERE ZENITH DELAYS (Saastamoinen standard atmosphere)\n";
    f << "# Station     : " << sta << "\n";
    f << std::fixed << std::setprecision(4);
    f << "# ECEF X [m]  : " << obs.receiver.approxX << "\n";
    f << "# ECEF Y [m]  : " << obs.receiver.approxY << "\n";
    f << "# ECEF Z [m]  : " << obs.receiver.approxZ << "\n";
    f << std::setprecision(9);
    f << "# Lat [deg]   : " << staGeod.lat << "\n";
    f << "# Lon [deg]   : " << staGeod.lon << "\n";
    f << std::setprecision(3);
    f << "# Height [m]  : " << h_m << "\n";
    f << "# Model       : Saastamoinen (standard atmosphere)\n";
    f << "# NOTE        : Zhd and Zwet are constant (standard atm), slant varies with elevation\n";
    f << "# NOTE        : For epoch-varying wet delay use PPP + VMF3\n";
    f << "# Zhd [mm]    : " << zhd * 1000.0 << "\n";
    f << "# Zwet [mm]   : " << zwet * 1000.0 << "\n";
    f << "# ZTD [mm]    : " << ztd * 1000.0  << "\n";
    f << "#\n";
    f << "# epoch_utc;doy;sow;zhd_mm;zwet_mm;ztd_mm;"
         "mean_elev_deg;slant_delay_m\n";

    // Per-epoch: mean elevation from usedSats, then slant = ZTD / sin(mean_el)
    for (const auto& r : spp) {
        if (!r.valid) continue;

        double sumEl = 0.0;
        int    nEl   = 0;
        for (const auto& us : r.usedSats) {
            sumEl += us.elevation;
            ++nEl;
        }
        const double meanElDeg = (nEl > 0) ? sumEl / nEl : 15.0;
        const double meanElRad = meanElDeg * (std::numbers::pi / 180.0);
        const double sinEl     = std::max(std::sin(meanElRad), 0.05);
        const double slant     = ztd / sinEl;

        const auto ts = r.time.toTimeStamp();
        // Format UTC epoch as "YYYY-MM-DD HH:MM:SS"
        std::ostringstream epoch;
        epoch << std::setfill('0')
              << std::setw(4) << ts.year()   << "-"
              << std::setw(2) << ts.month()  << "-"
              << std::setw(2) << ts.day()    << " "
              << std::setw(2) << ts.hour()   << ":"
              << std::setw(2) << ts.minute() << ":"
              << std::fixed << std::setw(6) << std::setprecision(3) << ts.second();

        f << epoch.str() << ";"
          << dayOfYear(r.time) << ";"
          << std::fixed << std::setprecision(1) << r.time.sow << ";"
          << std::setprecision(3)
          << zhd  * 1000.0 << ";"
          << zwet * 1000.0 << ";"
          << ztd  * 1000.0 << ";"
          << std::setprecision(2) << meanElDeg << ";"
          << std::setprecision(5) << slant
          << "\n";
    }

    LOKI_INFO("GnssCsvExport: " + path);
}
