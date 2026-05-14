#include <loki/gnss/gnssProtocol.hpp>
#include <loki/geodesy/coordTransform.hpp>
#include <loki/math/ellipsoid.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>

using namespace loki;
using namespace loki::gnss;

namespace fs = std::filesystem;

// =============================================================================
//  Helpers
// =============================================================================

void GnssProtocol::sep(std::ostream& f)
{
    f << "================================================================================\n";
}

void GnssProtocol::thin(std::ostream& f)
{
    f << "--------------------------------------------------------------------------------\n";
}

static std::string toDms(double deg)
{
    const bool neg = (deg < 0.0);
    deg = std::fabs(deg);
    const int    d  = static_cast<int>(deg);
    const double rm = (deg - d) * 60.0;
    const int    m  = static_cast<int>(rm);
    const double s  = (rm - m) * 60.0;
    std::ostringstream oss;
    oss << (neg ? "-" : "") << d << " deg "
        << std::setw(2) << std::setfill('0') << m << "' "
        << std::fixed << std::setprecision(5) << s << "\"";
    return oss.str();
}

static loki::geodesy::GeodPoint toGeod(double x, double y, double z)
{
    static const loki::math::Ellipsoid wgs84 =
        loki::math::makeEllipsoid(loki::math::EllipsoidModel::WGS84);
    return loki::geodesy::ecef2geod({x, y, z}, wgs84);
}

// =============================================================================
//  Constructor
// =============================================================================

GnssProtocol::GnssProtocol(const AppConfig& cfg)
    : m_cfg(cfg)
{}

// =============================================================================
//  _writeHeader
// =============================================================================

void GnssProtocol::_writeHeader(std::ostream& f) const
{
    const GnssConfig& gcfg = m_cfg.gnss;
    sep(f);
    f << "  LOKI GNSS PROTOCOL\n";
    sep(f);
    f << "  Station       : " << gcfg.station        << "\n";
    f << "  Task          : " << gcfg.task            << "\n";
    f << "  NAV file      : " << gcfg.navFile         << "\n";
    f << "  OBS file      : " << gcfg.obsFile         << "\n";
    f << "  Constellations: ";
    for (const auto& c : gcfg.constellations) f << c << " ";
    f << "\n";
    f << "  Elevation mask: " << gcfg.elevationMaskDeg << " deg\n";
    f << "  Ionosphere    : " << gcfg.corrections.ionosphere  << "\n";
    f << "  Troposphere   : " << gcfg.corrections.troposphere << "\n";
    f << "  Sagnac        : " << (gcfg.corrections.sagnac ? "yes" : "no") << "\n";
    thin(f);
}

// =============================================================================
//  _writeAppliedCorrections
// =============================================================================

void GnssProtocol::_writeAppliedCorrections(std::ostream& f) const
{
    const GnssConfig& gcfg = m_cfg.gnss;
    auto yn = [](bool b) -> const char* { return b ? "yes" : "no"; };
    f << "\n";
    f << "APPLIED CORRECTIONS\n";
    f << "\n";
    f << "  Satellite clock (relativistic dtr) : always active (KeplerOrbit)\n";
    f << "  Sagnac (Earth rotation)            : " << yn(gcfg.corrections.sagnac)     << "\n";
    f << "  Ionosphere                         : " << gcfg.corrections.ionosphere      << "\n";
    f << "  Troposphere                        : " << gcfg.corrections.troposphere     << "\n";
    f << "  Solid Earth tides (IERS2010 step-1): " << yn(gcfg.corrections.solidTides) << "\n";
    f << "  Ocean loading                      : " << yn(gcfg.corrections.oceanLoading)<< "\n";
    f << "  Phase windup                       : " << yn(gcfg.corrections.phaseWindup) << "\n";
    f << "  PCO/PCV                            : " << yn(gcfg.corrections.pcoPcv)      << "\n";
}

// =============================================================================
//  _writeParseSummary
// =============================================================================

void GnssProtocol::_writeParseSummary(std::ostream& f,
                                       const ParseResult& pr) const
{
    f << "\n";
    f << "PARSE SUMMARY\n";
    f << "\n";

    const ObsSummary& os = pr.obs;
    f << "  Station       : " << os.station       << "\n";
    f << "  Receiver      : " << os.receiverType  << "\n";
    f << "  Antenna       : " << os.antennaType   << "\n";
    f << "  Interval      : " << std::fixed << std::setprecision(1)
                              << os.intervalSec   << " s\n";
    f << "  Epochs        : " << os.nEpochs       << "\n";

    if (os.nEpochs > 0) {
        const auto tsStart = ::TimeStamp::fromMjd(os.spanStartMjd);
        const auto tsEnd   = ::TimeStamp::fromMjd(os.spanEndMjd);
        f << "  Time span     : " << tsStart.utcString()
          << "  -->  " << tsEnd.utcString() << "\n";
        f << "  Sats/epoch    : min=" << os.minSatsPerEpoch
          << "  max=" << os.maxSatsPerEpoch
          << "  mean=" << std::fixed << std::setprecision(1)
          << os.meanSatsPerEpoch << "\n";
    }

    if (!os.obsCodes.empty()) {
        f << "\n";
        f << "  Observation codes:\n";
        const std::map<char, std::string> sysNames{
            {'G', "GPS    "}, {'R', "GLONASS"}, {'E', "Galileo"},
            {'C', "BeiDou "}, {'J', "QZSS   "}, {'S', "SBAS   "}};
        for (const auto& [ch, codes] : os.obsCodes) {
            auto it = sysNames.find(ch);
            const std::string sn = (it != sysNames.end()) ? it->second
                                                           : std::string(1,ch) + "      ";
            f << "    " << sn << " (" << ch << "): ";
            for (const auto& c : codes) f << c << " ";
            f << "\n";
        }
    }

    f << "\n";
    f << "  Observations per constellation:\n";
    if (os.nGps     > 0) f << "    GPS     : " << os.nGps     << "\n";
    if (os.nGlonass > 0) f << "    GLONASS : " << os.nGlonass << "\n";
    if (os.nGalileo > 0) f << "    Galileo : " << os.nGalileo << "\n";
    if (os.nBeidou  > 0) f << "    BeiDou  : " << os.nBeidou  << "\n";
    if (os.nSbas    > 0) f << "    SBAS    : " << os.nSbas    << "\n";
    if (os.nQzss    > 0) f << "    QZSS    : " << os.nQzss    << "\n";

    f << "\n";
    f << "  NAV ephemerides:\n";
    f << "    GPS     : " << pr.nGpsEph << "\n";
    f << "    Galileo : " << pr.nGalEph << "\n";
    f << "    GLONASS : " << pr.nGloEph << "\n";
    f << "    BeiDou  : " << pr.nBdsEph << "\n";
    f << "  Klobuchar : " << (pr.hasKlobuchar ? "present" : "absent") << "\n";
}

// =============================================================================
//  _writeSppResults
// =============================================================================

void GnssProtocol::_writeSppResults(std::ostream& f,
                                     const SppSummary& s,
                                     const std::vector<SppResult>& epochs) const
{
    f << "\n";
    f << "SPP RESULTS\n";
    f << "\n";

    f << "  Valid epochs  : " << s.nEpochsValid    << " / " << s.nEpochsTotal    << "\n";
    f << "  Converged     : " << s.nEpochsConverged << "\n";
    f << "  Mean sats     : " << std::fixed << std::setprecision(1) << s.meanNSats << "\n";
    f << "  Mean PDOP     : " << std::setprecision(2) << s.meanPdop               << "\n";

    f << "\n";
    f << "  Mean ECEF position (WGS-84):\n";
    f << std::fixed << std::setprecision(4);
    f << "    X = " << s.meanX << " +/- " << s.stdX << " [m]\n";
    f << "    Y = " << s.meanY << " +/- " << s.stdY << " [m]\n";
    f << "    Z = " << s.meanZ << " +/- " << s.stdZ << " [m]\n";

    f << "\n";
    f << "  Mean geodetic position (WGS-84):\n";
    f << "    Lat = " << toDms(s.meanLat) << "  (" << std::setprecision(9) << s.meanLat << " deg)\n";
    f << "    Lon = " << toDms(s.meanLon) << "  (" << std::setprecision(9) << s.meanLon << " deg)\n";
    f << "    h   = " << std::setprecision(4) << s.meanH << " [m]\n";

    f << "\n" << std::setprecision(3);
    f << "  Mean clk bias : " << s.meanClkBiasM
      << " [m]  = " << s.meanClkBiasM / 299792458.0 * 1.0e6 << " [us]\n";

    if (s.hasReference) {
        f << "\n";
        f << "  Reference (" << s.referenceSource << "):\n";
        f << std::setprecision(4);
        f << "    ECEF: X = " << s.refX << "  Y = " << s.refY
          << "  Z = " << s.refZ << " [m]\n";
        f << "    Lat = " << toDms(s.refLat) << "\n";
        f << "    Lon = " << toDms(s.refLon) << "\n";
        f << "    h   = " << std::setprecision(4) << s.refH << " [m]\n";
        f << std::setprecision(3);
        f << "\n";
        f << "  Position error vs reference:\n";
        f << "    Mean 3D error : " << s.mean3dErrorM << " m\n";
        f << "    Std  3D error : " << s.std3dErrorM  << " m\n";
        f << "    RMS  3D error : " << s.rmsErrorM    << " m\n";
    }

    f << "\n";
    f << "  Per-epoch sample (first 20 valid):\n";
    f << "  " << std::left
      << std::setw(14) << "MJD"
      << std::setw(16) << "X [m]"
      << std::setw(16) << "Y [m]"
      << std::setw(16) << "Z [m]"
      << std::setw(16) << "Lat [deg]"
      << std::setw(16) << "Lon [deg]"
      << std::setw(12) << "h [m]"
      << std::setw(12) << "clk [m]"
      << std::setw(8)  << "PDOP"
      << std::setw(6)  << "nSat"
      << "\n";
    f << "  " << std::string(132, '-') << "\n";

    int printed = 0;
    for (const auto& r : epochs) {
        if (!r.valid) continue;
        if (printed >= 20) break;
        const auto gp = toGeod(r.x, r.y, r.z);
        f << "  " << std::left << std::fixed
          << std::setprecision(8) << std::setw(14) << r.time.toTimeStamp().mjd()
          << std::setprecision(3)
          << std::setw(16) << r.x << std::setw(16) << r.y << std::setw(16) << r.z
          << std::setprecision(9)
          << std::setw(16) << gp.lat << std::setw(16) << gp.lon
          << std::setprecision(3)
          << std::setw(12) << gp.h << std::setw(12) << r.clkBiasM
          << std::setprecision(2) << std::setw(8) << r.pdop
          << std::setw(6) << r.nSats << "\n";
        ++printed;
    }
}

// =============================================================================
//  _writePppResults
// =============================================================================

void GnssProtocol::_writePppResults(std::ostream& f,
                                     const PppSummary& s,
                                     const std::vector<PppResult>& epochs) const
{
    f << "\n";
    f << "PPP RESULTS\n";
    f << "\n";

    f << "  Valid epochs      : " << s.nEpochsValid    << " / " << s.nEpochsTotal    << "\n";
    f << "  Converged epochs  : " << s.nEpochsConverged << "\n";
    f << "  Convergence time  : " << std::fixed << std::setprecision(1)
                                  << s.convergenceTimeMin << " min\n";
    f << "  Mean sats         : " << std::setprecision(1) << s.meanNSats             << "\n";

    f << "\n";
    f << "  Post-convergence mean ECEF position (WGS-84):\n";
    f << std::fixed << std::setprecision(4);
    f << "    X = " << s.meanX << " +/- " << s.stdX << " [m]\n";
    f << "    Y = " << s.meanY << " +/- " << s.stdY << " [m]\n";
    f << "    Z = " << s.meanZ << " +/- " << s.stdZ << " [m]\n";

    // Mean geodetic.
    if (s.meanX != 0.0 || s.meanY != 0.0 || s.meanZ != 0.0) {
        const auto gm = toGeod(s.meanX, s.meanY, s.meanZ);
        f << "\n";
        f << "  Post-convergence mean geodetic position (WGS-84):\n";
        f << "    Lat = " << toDms(gm.lat)
          << "  (" << std::setprecision(9) << gm.lat << " deg)\n";
        f << "    Lon = " << toDms(gm.lon)
          << "  (" << std::setprecision(9) << gm.lon << " deg)\n";
        f << "    h   = " << std::setprecision(4) << gm.h << " [m]\n";
    }

    f << "\n" << std::setprecision(3);
    f << "  Mean clk bias     : " << s.meanClkBiasM
      << " [m]  = " << s.meanClkBiasM / 299792458.0 * 1.0e6 << " [us]\n";
    f << "  Mean ZTD wet      : " << s.meanZtdWetM * 1000.0 << " [mm]\n";

    if (s.hasReference) {
        f << "\n";
        f << "  Reference (" << s.referenceSource << "):\n";
        const auto gr = toGeod(s.refX, s.refY, s.refZ);
        f << std::setprecision(4);
        f << "    ECEF: X = " << s.refX << "  Y = " << s.refY
          << "  Z = " << s.refZ << " [m]\n";
        f << "    Lat = " << toDms(gr.lat)
          << "  (" << std::setprecision(9) << gr.lat << " deg)\n";
        f << "    Lon = " << toDms(gr.lon)
          << "  (" << std::setprecision(9) << gr.lon << " deg)\n";
        f << "    h   = " << std::setprecision(4) << gr.h << " [m]\n";
        f << std::setprecision(3);
        f << "\n";
        f << "  Post-convergence position error vs reference:\n";
        f << "    Mean 3D error : " << s.mean3dErrorM << " m\n";
        f << "    Std  3D error : " << s.std3dErrorM  << " m\n";
        f << "    RMS  3D error : " << s.rmsErrorM    << " m\n";
    }

    // Per-epoch table (first 20 converged) -- ECEF + geodetic.
    f << "\n";
    f << "  Per-epoch sample (first 20 converged):\n";
    f << "  " << std::left
      << std::setw(14) << "MJD"
      << std::setw(14) << "X [m]"
      << std::setw(14) << "Y [m]"
      << std::setw(14) << "Z [m]"
      << std::setw(14) << "Lat [deg]"
      << std::setw(14) << "Lon [deg]"
      << std::setw(10) << "h [m]"
      << std::setw(12) << "clk [m]"
      << std::setw(11) << "ZTD_wet[m]"
      << std::setw(11) << "ZTD_tot[m]"
      << std::setw(6)  << "nSat"
      << "\n";
    f << "  " << std::string(144, '-') << "\n";

    int printed = 0;
    for (const auto& r : epochs) {
        if (!r.valid || !r.converged) continue;
        if (printed >= 20) break;
        const auto gp = toGeod(r.x, r.y, r.z);
        f << "  " << std::left << std::fixed
          << std::setprecision(8) << std::setw(14) << r.time.toTimeStamp().mjd()
          << std::setprecision(3)
          << std::setw(14) << r.x << std::setw(14) << r.y << std::setw(14) << r.z
          << std::setprecision(9)
          << std::setw(14) << gp.lat << std::setw(14) << gp.lon
          << std::setprecision(3)
          << std::setw(10) << gp.h
          << std::setw(12) << r.clkBiasM
          << std::setprecision(5)
          << std::setw(11) << r.ztdWetM
          << std::setw(11) << r.ztdTotalM
          << std::setw(6)  << r.nSats
          << "\n";
        ++printed;
    }
}

// =============================================================================
//  write
// =============================================================================

void GnssProtocol::write(const GnssResult& result) const
{
    fs::create_directories(m_cfg.protocolsDir);

    const std::string task = result.hasPpp ? "ppp" :
                             result.hasSpp ? "spp" : "parse";

    const std::string path = m_cfg.protocolsDir.string()
        + "/gnss_" + m_cfg.gnss.station + "_" + task + "_protocol.txt";

    std::ofstream f(path);
    if (!f.is_open())
        throw IoException("GnssProtocol: cannot open " + path);

    _writeHeader(f);
    _writeAppliedCorrections(f);
    _writeParseSummary(f, result.parse);

    if (result.hasSpp)
        _writeSppResults(f, result.sppSummary, result.spp);

    if (result.hasPpp)
        _writePppResults(f, result.pppSummary, result.ppp);

    f << "\n";
    sep(f);

    LOKI_INFO("GnssProtocol: written to " + path);
}
