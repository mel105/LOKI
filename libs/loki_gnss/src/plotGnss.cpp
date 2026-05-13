#include <loki/gnss/plotGnss.hpp>
#include <Eigen/Dense>
#include <loki/gnss/keplerOrbit.hpp>
#include <loki/gnss/satVisibility.hpp>
#include <loki/geodesy/coordTransform.hpp>
#include <loki/math/ellipsoid.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/io/gnuplot.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <map>
#include <set>
#include <numbers>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

using namespace loki;
using namespace loki::gnss;

namespace fs = std::filesystem;

// =============================================================================
//  File-scope helpers
// =============================================================================

namespace {

std::string fmt(double v)
{
    std::ostringstream oss;
    oss << std::scientific << std::setprecision(10) << v;
    return oss.str();
}

// Terminal without size -- each plot specifies its own dimensions
std::string term(int w = 1200, int h = 600)
{
    return "set terminal pngcairo noenhanced font 'Sans,12' size "
           + std::to_string(w) + "," + std::to_string(h);
}

} // namespace

// =============================================================================
//  Constructor / helpers
// =============================================================================

PlotGnss::PlotGnss(const AppConfig& cfg, const std::string& station)
    : m_cfg(cfg)
    , m_station(station)
    , m_outDir(cfg.imgDir.string())
{
    fs::create_directories(m_outDir);
}

std::string PlotGnss::outPath(const std::string& param,
                               const std::string& plotType) const
{
    return m_outDir + "/gnss_" + m_station + "_" + param + "_" + plotType + ".png";
}

std::string PlotGnss::fwdSlash(const std::string& p)
{
    std::string r = p;
    for (char& c : r) if (c == '\\') c = '/';
    return r;
}

// =============================================================================
//  plotSatCount
// =============================================================================

void PlotGnss::plotSatCount(const ObsFile& obs) const
{
    if (!m_cfg.gnss.figures.satcount || obs.epochs.empty()) return;

    std::ostringstream blk;
    blk << "$SAT << EOD\n";
    for (const auto& ep : obs.epochs) {
        int gps = 0, glo = 0, gal = 0, bds = 0, tot = 0;
        for (const auto& sat : ep.satellites) {
            ++tot;
            switch (sat.system) {
                case GnssSystem::GPS:     ++gps; break;
                case GnssSystem::GLONASS: ++glo; break;
                case GnssSystem::GALILEO: ++gal; break;
                case GnssSystem::BEIDOU:  ++bds; break;
                default: break;
            }
        }
        blk << fmt(ep.time.toTimeStamp().mjd()) << " "
            << tot << " " << gps << " " << glo << " " << gal << " " << bds << "\n";
    }
    blk << "EOD";

    const std::string out = fwdSlash(outPath("satcount", "timeseries"));
    Gnuplot gp;
    gp(term(1200, 500));
    gp("set output '" + out + "'");
    gp("set title 'Satellites tracked per epoch -- " + m_station + "'");
    gp("set xlabel 'MJD'");
    gp("set ylabel 'Count'");
    gp("set yrange [0:*]");
    gp("set grid");
    gp("set key top right");
    gp(blk.str());
    gp("plot $SAT u 1:2 w l lc rgb '#333333' lw 1.5 title 'Total', "
       "     $SAT u 1:3 w l lc rgb '#1f77b4' lw 1.5 title 'GPS', "
       "     $SAT u 1:4 w l lc rgb '#ff7f0e' lw 1.5 title 'GLONASS', "
       "     $SAT u 1:5 w l lc rgb '#2ca02c' lw 1.5 title 'Galileo', "
       "     $SAT u 1:6 w l lc rgb '#d62728' lw 1.5 title 'BeiDou'");
    LOKI_INFO("PlotGnss: " + out);
}

// =============================================================================
//  plotElevation
//  Mean satellite elevation per epoch, computed from visibility.
// =============================================================================

void PlotGnss::plotElevation(const ObsFile& obs, const NavFile& nav) const
{
    if (!m_cfg.gnss.figures.elevation || obs.epochs.empty()) return;

    const std::array<double,3> staEcef{
        obs.receiver.approxX, obs.receiver.approxY, obs.receiver.approxZ};

    std::ostringstream blk;
    blk << "$ELV << EOD\n";

    // Sample every 10th epoch for speed
    for (std::size_t ei = 0; ei < obs.epochs.size(); ei += 10) {
        const auto& ep = obs.epochs[ei];
        double sumEl = 0.0;
        int    cnt   = 0;
        double minEl = 90.0, maxEl = 0.0;

        for (const auto& sat : ep.satellites) {
            const SatState st = KeplerOrbit::compute(nav, sat.system, sat.prn, ep.time);
            if (!st.valid) continue;
            double el = 0.0, az = 0.0;
            SatVisibility::ecefToElevAzim({st.x, st.y, st.z}, staEcef, el, az);
            if (el < 0.0) continue;
            sumEl += el;
            minEl  = std::min(minEl, el);
            maxEl  = std::max(maxEl, el);
            ++cnt;
        }
        if (cnt == 0) continue;
        blk << fmt(ep.time.toTimeStamp().mjd()) << " "
            << fmt(sumEl / cnt) << " " << fmt(minEl) << " " << fmt(maxEl) << "\n";
    }
    blk << "EOD";

    const std::string out = fwdSlash(outPath("elevation", "timeseries"));
    Gnuplot gp;
    gp(term(1200, 500));
    gp("set output '" + out + "'");
    gp("set title 'Satellite elevation -- " + m_station + "'");
    gp("set xlabel 'MJD'");
    gp("set ylabel 'Elevation [deg]'");
    gp("set yrange [0:90]");
    gp("set grid");
    gp("set key top right");
    gp(blk.str());
    gp("plot $ELV u 1:4 w l lc rgb '#aec7e8' lw 1 title 'Max', "
       "     $ELV u 1:2 w l lc rgb '#1f77b4' lw 2 title 'Mean', "
       "     $ELV u 1:3 w l lc rgb '#ff9896' lw 1 title 'Min'");
    LOKI_INFO("PlotGnss: " + out);
}

// =============================================================================
//  plotSkyplot
void PlotGnss::plotSkyplot(const ObsFile& obs, const NavFile& nav) const
{
    _plotSkyplotImpl(obs, nav, false);   // per-constellation colours
    _plotSkyplotImpl(obs, nav, true);    // per-PRN colours
}

// =============================================================================
//  _plotSkyplotImpl  -- shared sky-plot engine
//
//  perPrn = false : colour by constellation (4 colours), style by constellation
//  perPrn = true  : colour by PRN (cyklická 20-colour palette), PRN label at
//                   the last point of each track (horizon exit)
//
//  Coordinate system: x = (90-el)*sin(az),  y = (90-el)*cos(az)
//  Plot origin = zenith, rim = horizon (el=0 deg, r=90).
// =============================================================================

void PlotGnss::_plotSkyplotImpl(const ObsFile& obs, const NavFile& nav,
                                 bool perPrn) const
{
    if (obs.epochs.empty()) return;
    if (perPrn  && !m_cfg.gnss.figures.skyplotPrn)          return;
    if (!perPrn && !m_cfg.gnss.figures.skyplotConstellation) return;

    const std::array<double,3> staEcef{
        obs.receiver.approxX, obs.receiver.approxY, obs.receiver.approxZ};

    struct Track {
        std::string prn;
        char        sys;
        std::vector<std::pair<double,double>> pts;
    };
    std::map<std::string, Track> tracks;

    // Sample every 5th epoch
    for (std::size_t ei = 0; ei < obs.epochs.size(); ei += 5) {
        const auto& ep = obs.epochs[ei];
        for (const auto& sat : ep.satellites) {
            const SatState st = KeplerOrbit::compute(nav, sat.system, sat.prn, ep.time);
            if (!st.valid) continue;
            double el = 0.0, az = 0.0;
            SatVisibility::ecefToElevAzim({st.x, st.y, st.z}, staEcef, el, az);
            if (el < 0.0) continue;

            char sysChar = '?';
            switch (sat.system) {
                case GnssSystem::GPS:     sysChar = 'G'; break;
                case GnssSystem::GLONASS: sysChar = 'R'; break;
                case GnssSystem::GALILEO: sysChar = 'E'; break;
                case GnssSystem::BEIDOU:  sysChar = 'C'; break;
                default: break;
            }
            if (sysChar == '?') continue;

            std::ostringstream id;
            id << sysChar << std::setw(2) << std::setfill('0') << sat.prn;
            const std::string key = id.str();

            auto& tr  = tracks[key];
            tr.prn    = key;
            tr.sys    = sysChar;

            const double az_rad = az * (std::numbers::pi / 180.0);
            const double r      = 90.0 - el;
            tr.pts.emplace_back(r * std::sin(az_rad), r * std::cos(az_rad));
        }
    }
    if (tracks.empty()) return;

    // -------------------------------------------------------------------------
    //  Helper lambdas for colours / line styles
    // -------------------------------------------------------------------------

    // Per-constellation colour
    auto sysColor = [](char s) -> std::string {
        switch (s) {
            case 'G': return "#1f77b4";
            case 'R': return "#ff7f0e";
            case 'E': return "#2ca02c";
            case 'C': return "#d62728";
            default:  return "#888888";
        }
    };
    auto sysDt = [](char s) -> std::string {
        switch (s) {
            case 'G': return "1";
            case 'E': return "2";
            case 'R': return "3";
            case 'C': return "4";
            default:  return "1";
        }
    };

    // 20-colour cyklická paleta pre per-PRN mode
    static const std::vector<std::string> PRN_PALETTE = {
        "#1f77b4","#aec7e8","#ff7f0e","#ffbb78","#2ca02c",
        "#98df8a","#d62728","#ff9896","#9467bd","#c5b0d5",
        "#8c564b","#c49c94","#e377c2","#f7b6d2","#7f7f7f",
        "#c7c7c7","#bcbd22","#dbdb8d","#17becf","#9edae5"
    };

    // -------------------------------------------------------------------------
    //  Build elevation rings + tick datablocks (shared by both modes)
    // -------------------------------------------------------------------------

    auto makeCircle = [](double r, int n) -> std::string {
        std::ostringstream oss;
        for (int i = 0; i <= n; ++i) {
            const double t = 2.0 * std::numbers::pi * i / n;
            oss << fmt(r * std::sin(t)) << " " << fmt(r * std::cos(t)) << "\n";
        }
        return oss.str();
    };

    std::ostringstream datablocks;
    datablocks << "$C0  << EOD\n" << makeCircle(90.0, 360) << "EOD\n";
    datablocks << "$C30 << EOD\n" << makeCircle(60.0, 360) << "EOD\n";
    datablocks << "$C60 << EOD\n" << makeCircle(30.0, 360) << "EOD\n";

    datablocks << "$TICKS << EOD\n";
    for (int deg = 0; deg < 360; deg += 10) {
        const double a  = deg * (std::numbers::pi / 180.0);
        const double s  = std::sin(a), c = std::cos(a);
        const double r1 = (deg % 30 == 0) ? 84.0 : 87.0;
        datablocks << fmt(r1*s) << " " << fmt(r1*c) << "\n"
                   << fmt(90.0*s) << " " << fmt(90.0*c) << "\n\n";
    }
    datablocks << "EOD\n";

    // Track datablocks
    int prnIdx = 0;
    std::map<std::string,std::string> prnColorMap;
    for (const auto& [key, tr] : tracks) {
        if (tr.pts.empty()) continue;
        datablocks << "$T" << key << " << EOD\n";
        for (const auto& [x, y] : tr.pts)
            datablocks << fmt(x) << " " << fmt(y) << "\n";
        datablocks << "EOD\n";
        prnColorMap[key] = PRN_PALETTE[static_cast<std::size_t>(prnIdx)
                           % PRN_PALETTE.size()];
        ++prnIdx;
    }

    // -------------------------------------------------------------------------
    //  Build plot command
    // -------------------------------------------------------------------------

    std::ostringstream plotcmd;
    plotcmd << "plot ";
    plotcmd << "$C0    u 1:2 w l lc rgb '#aaaaaa' lw 1.0 dt 1 notitle, "
               "$C30   u 1:2 w l lc rgb '#aaaaaa' lw 0.5 dt 2 notitle, "
               "$C60   u 1:2 w l lc rgb '#aaaaaa' lw 0.5 dt 2 notitle, "
               "$TICKS u 1:2 w l lc rgb '#888888' lw 0.5 dt 1 notitle";

    for (const auto& [key, tr] : tracks) {
        if (tr.pts.empty()) continue;
        const std::string col = perPrn ? prnColorMap.at(key) : sysColor(tr.sys);
        const std::string dt  = perPrn ? "1"                 : sysDt(tr.sys);
        plotcmd << ", \\\n  $T" << key
                << " u 1:2 w l lc rgb '" << col
                << "' lw 0.8 dt " << dt << " notitle";
    }

    // -------------------------------------------------------------------------
    //  Gnuplot setup
    // -------------------------------------------------------------------------

    const std::string suffix = perPrn ? "prn" : "polar";
    const std::string title  = perPrn
        ? "Satellite sky coverage (per PRN) -- " + m_station
        : "Satellite sky coverage -- " + m_station;

    const std::string out = fwdSlash(outPath("skyplot", suffix));
    Gnuplot gp;
    gp(term(900, 950));
    gp("set output '" + out + "'");
    gp("set xrange [-95:95]");
    gp("set yrange [-108:95]");
    gp("set size square");
    gp("unset border");
    gp("unset xtics");
    gp("unset ytics");
    gp("set key off");
    gp("set title '" + title + "'");

    // Elevation ring labels
    gp("set label 100 '0'  at  0, 92 center font ',9' tc rgb '#666666'");
    gp("set label 101 '30' at  0, 62 center font ',9' tc rgb '#666666'");
    gp("set label 102 '60' at  0, 32 center font ',9' tc rgb '#666666'");
    gp("set label 103 '90' at  0,  2 center font ',9' tc rgb '#666666'");

    // Cardinal labels
    const std::vector<std::pair<std::string,std::pair<double,double>>> cardinals = {
        {"N",  { 0.0,  95.0}}, {"NE", { 68.0,  68.0}},
        {"E",  {95.0,   0.0}}, {"SE", { 68.0, -68.0}},
        {"S",  { 0.0, -95.0}}, {"SW", {-68.0, -68.0}},
        {"W",  {-95.0,  0.0}}, {"NW", {-68.0,  68.0}}
    };
    int lblId = 200;
    for (const auto& [lbl, pos] : cardinals) {
        gp("set label " + std::to_string(lblId++) + " '" + lbl + "' at "
           + fmt(pos.first) + "," + fmt(pos.second)
           + " center font ',10' tc rgb '#333333'");
    }

    // Azimuth degree tick labels every 30 deg
    for (int deg = 0; deg < 360; deg += 30) {
        const double a  = deg * (std::numbers::pi / 180.0);
        gp("set label " + std::to_string(lblId++)
           + " '" + std::to_string(deg) + "' at "
           + fmt(94.0 * std::sin(a)) + "," + fmt(94.0 * std::cos(a))
           + " center font ',8' tc rgb '#999999'");
    }

    if (!perPrn) {
        // Per-constellation legend: compact block below plot
        std::set<char> sysSeen;
        for (const auto& [key, tr] : tracks) sysSeen.insert(tr.sys);

        const std::map<char, std::string> sysNames{
            {'G',"GPS"},{'R',"GLONASS"},{'E',"Galileo"},{'C',"BeiDou"}};
        const std::map<char, std::string> sysStyleNames{
            {'G',"solid"},{'R',"dotted"},{'E',"dashed"},{'C',"dash-dot"}};

        double legX = -90.0;
        for (char s : sysSeen) {
            const std::string col = sysColor(s);
            auto nm = sysNames.find(s);
            auto st = sysStyleNames.find(s);
            const std::string nmStr = (nm != sysNames.end()) ? nm->second : std::string(1,s);
            const std::string stStr = (st != sysStyleNames.end()) ? " (" + st->second + ")" : "";
            gp("set label " + std::to_string(lblId++)
               + " '" + nmStr + stStr + "' at "
               + fmt(legX) + ",-101 left font ',9' tc rgb '" + col + "'");
            legX += 50.0;
        }
    } else {
        // Per-PRN mode: label each track at its LAST visible point
        for (const auto& [key, tr] : tracks) {
            if (tr.pts.empty()) continue;
            const auto& last = tr.pts.back();
            // Only label near the rim (el < 25 deg -> r > 65) to avoid clutter
            const double r = std::sqrt(last.first*last.first + last.second*last.second);
            if (r < 55.0) continue;   // satellite still high -- skip label
            const std::string col = prnColorMap.at(key);
            // Offset label slightly outward
            const double norm = (r > 0.01) ? r : 1.0;
            const double lx = last.first  + 5.0 * last.first  / norm;
            const double ly = last.second + 5.0 * last.second / norm;
            gp("set label " + std::to_string(lblId++)
               + " '" + key + "' at "
               + fmt(lx) + "," + fmt(ly)
               + " center font ',7' tc rgb '" + col + "'");
        }
    }

    gp(datablocks.str());
    gp(plotcmd.str());

    LOKI_INFO("PlotGnss: " + out);
}


// =============================================================================
//  plotDop  -- 4-panel: GDOP / PDOP / HDOP / VDOP
// =============================================================================

void PlotGnss::plotDop(const std::vector<SppResult>& spp) const
{
    if (!m_cfg.gnss.figures.dop || spp.empty()) return;

    // SppResult only carries PDOP. For HDOP/VDOP we'd need SatVisibility.
    // For now plot PDOP alone with a good-geometry threshold line.
    std::ostringstream blk;
    blk << "$DOP << EOD\n";
    for (const auto& r : spp) {
        if (!r.valid) continue;
        blk << fmt(r.time.toTimeStamp().mjd()) << " " << fmt(r.pdop) << "\n";
    }
    blk << "EOD";

    const std::string out = fwdSlash(outPath("dop", "timeseries"));
    Gnuplot gp;
    gp(term(1200, 500));
    gp("set output '" + out + "'");
    gp("set title 'PDOP -- " + m_station + "'");
    gp("set xlabel 'MJD'");
    gp("set ylabel 'PDOP'");
    gp("set yrange [0:10]");
    gp("set grid");
    gp("set key top right");
    gp(blk.str());
    gp("plot $DOP u 1:2 w l lc rgb '#1f77b4' lw 1.5 title 'PDOP', "
       "     2.0 w l lc rgb '#2ca02c' lw 1 dt 2 title 'Good (2.0)', "
       "     5.0 w l lc rgb '#d62728' lw 1 dt 2 title 'Poor (5.0)'");
    LOKI_INFO("PlotGnss: " + out);
}

// =============================================================================
//  plotClockBias  -- 2-panel: [m] and [us]
// =============================================================================

void PlotGnss::plotClockBias(const std::vector<SppResult>& spp) const
{
    if (!m_cfg.gnss.figures.spp.clockbias || spp.empty()) return;

    constexpr double C = 299792458.0;
    std::ostringstream blk;
    blk << "$CLK << EOD\n";
    for (const auto& r : spp) {
        if (!r.valid) continue;
        blk << fmt(r.time.toTimeStamp().mjd()) << " "
            << fmt(r.clkBiasM) << " "
            << fmt(r.clkBiasM / C * 1.0e6) << "\n";
    }
    blk << "EOD";

    const std::string out = fwdSlash(outPath("spp", "clockbias"));
    Gnuplot gp;
    gp(term(1200, 700));
    gp("set output '" + out + "'");
    gp("set multiplot layout 2,1 title 'GPS receiver clock bias -- " + m_station + "'");
    gp(blk.str());
    gp("set xlabel ''");
    gp("set ylabel 'Clock bias [m]'");
    gp("set grid");
    gp("set key off");
    gp("plot $CLK u 1:2 w l lc rgb '#1f77b4' lw 1.5 notitle");
    gp("set xlabel 'MJD'");
    gp("set ylabel 'Clock bias [us]'");
    gp("plot $CLK u 1:3 w l lc rgb '#ff7f0e' lw 1.5 notitle");
    gp("unset multiplot");
    LOKI_INFO("PlotGnss: " + out);
}

// =============================================================================
//  plotResiduals  -- mean absolute + RMS per epoch
// =============================================================================

void PlotGnss::plotResiduals(const std::vector<SppResult>& spp) const
{
    if (!m_cfg.gnss.figures.spp.residuals || spp.empty()) return;

    std::ostringstream blk;
    blk << "$RES << EOD\n";
    for (const auto& r : spp) {
        if (!r.valid || r.residuals.empty()) continue;
        double sumAbs = 0.0, sumSq = 0.0;
        for (double res : r.residuals) { sumAbs += std::fabs(res); sumSq += res * res; }
        const double n    = static_cast<double>(r.residuals.size());
        const double mean = sumAbs / n;
        const double rms  = std::sqrt(sumSq / n);
        blk << fmt(r.time.toTimeStamp().mjd()) << " "
            << fmt(mean) << " " << fmt(rms) << "\n";
    }
    blk << "EOD";

    const std::string out = fwdSlash(outPath("spp", "residuals"));
    Gnuplot gp;
    gp(term(1200, 500));
    gp("set output '" + out + "'");
    gp("set title 'Pseudorange residuals -- " + m_station + "'");
    gp("set xlabel 'MJD'");
    gp("set ylabel 'Residual [m]'");
    gp("set yrange [0:*]");
    gp("set grid");
    gp("set key top right");
    gp(blk.str());
    gp("plot $RES u 1:3 w l lc rgb '#aec7e8' lw 1.5 title 'RMS', "
       "     $RES u 1:2 w l lc rgb '#1f77b4' lw 2   title 'Mean abs'");
    LOKI_INFO("PlotGnss: " + out);
}

// =============================================================================
//  plotIsb  -- inter-system biases for non-GPS constellations
// =============================================================================

void PlotGnss::plotIsb(const std::vector<SppResult>& spp) const
{
    if (!m_cfg.gnss.figures.spp.isb) return;
    // Check if any epoch has ISB data
    bool hasGlo = false, hasGal = false, hasBds = false;
    for (const auto& r : spp) {
        if (!r.valid) continue;
        if (r.isbM.count("GLONASS")) hasGlo = true;
        if (r.isbM.count("GALILEO")) hasGal = true;
        if (r.isbM.count("BEIDOU"))  hasBds = true;
    }
    if (!hasGlo && !hasGal && !hasBds) return;

    std::ostringstream blk;
    blk << "$ISB << EOD\n";
    for (const auto& r : spp) {
        if (!r.valid) continue;
        auto get = [&](const std::string& k) -> double {
            auto it = r.isbM.find(k);
            return (it != r.isbM.end()) ? it->second : std::numeric_limits<double>::quiet_NaN();
        };
        blk << fmt(r.time.toTimeStamp().mjd()) << " "
            << fmt(get("GLONASS")) << " "
            << fmt(get("GALILEO")) << " "
            << fmt(get("BEIDOU"))  << "\n";
    }
    blk << "EOD";

    const std::string out = fwdSlash(outPath("spp", "isb"));
    Gnuplot gp;
    gp(term(1200, 600));
    gp("set output '" + out + "'");
    gp("set title 'Inter-system biases -- " + m_station + "'");
    gp("set xlabel 'MJD'");
    gp("set ylabel 'ISB [m]'");
    gp("set grid");
    gp("set key top right");
    gp(blk.str());

    std::string plotcmd = "plot ";
    bool first = true;
    if (hasGlo) {
        plotcmd += "$ISB u 1:2 w l lc rgb '#ff7f0e' lw 1.5 title 'GLONASS'";
        first = false;
    }
    if (hasGal) {
        if (!first) plotcmd += ", ";
        plotcmd += "$ISB u 1:3 w l lc rgb '#2ca02c' lw 1.5 title 'Galileo'";
        first = false;
    }
    if (hasBds) {
        if (!first) plotcmd += ", ";
        plotcmd += "$ISB u 1:4 w l lc rgb '#d62728' lw 1.5 title 'BeiDou'";
    }
    gp(plotcmd);
    LOKI_INFO("PlotGnss: " + out);
}

// =============================================================================
//  plotPositionEcef  -- X, Y, Z time series (3-panel)
// =============================================================================

void PlotGnss::plotPositionEcef(const std::vector<SppResult>& spp) const
{
    if (!m_cfg.gnss.figures.spp.positionEcef || spp.empty()) return;

    std::ostringstream blk;
    blk << "$POS << EOD\n";
    for (const auto& r : spp) {
        if (!r.valid) continue;
        blk << fmt(r.time.toTimeStamp().mjd()) << " "
            << fmt(r.x) << " " << fmt(r.y) << " " << fmt(r.z) << "\n";
    }
    blk << "EOD";

    const std::string out = fwdSlash(outPath("spp", "position_ecef"));
    Gnuplot gp;
    gp(term(1200, 900));
    gp("set output '" + out + "'");
    gp("set multiplot layout 3,1 title 'SPP ECEF position -- " + m_station + "'");
    gp(blk.str());

    const std::vector<std::string> cols   = {"2", "3", "4"};
    const std::vector<std::string> labels = {"X [m]", "Y [m]", "Z [m]"};
    const std::vector<std::string> colors = {"#1f77b4", "#ff7f0e", "#2ca02c"};

    for (int i = 0; i < 3; ++i) {
        gp("set ylabel '" + labels[static_cast<std::size_t>(i)] + "'");
        gp((i == 2) ? "set xlabel 'MJD'" : "set xlabel ''");
        gp("set grid");
        gp("set key off");
        gp("plot $POS u 1:" + cols[static_cast<std::size_t>(i)]
           + " w l lc rgb '" + colors[static_cast<std::size_t>(i)]
           + "' lw 1.5 notitle");
    }
    gp("unset multiplot");
    LOKI_INFO("PlotGnss: " + out);
}

// =============================================================================
//  plotPositionError  -- dX/dY/dZ + 3D error (4-panel)
// =============================================================================

void PlotGnss::plotPositionError(const std::vector<SppResult>& spp,
                                  const SppSummary&             summary) const
{
    if (!m_cfg.gnss.figures.spp.positionError || !summary.hasReference || spp.empty()) return;

    std::ostringstream blk;
    blk << "$ERR << EOD\n";
    for (const auto& r : spp) {
        if (!r.valid) continue;
        const double dX = r.x - summary.refX;
        const double dY = r.y - summary.refY;
        const double dZ = r.z - summary.refZ;
        const double d3 = std::sqrt(dX*dX + dY*dY + dZ*dZ);
        blk << fmt(r.time.toTimeStamp().mjd()) << " "
            << fmt(dX) << " " << fmt(dY) << " " << fmt(dZ) << " " << fmt(d3) << "\n";
    }
    blk << "EOD";

    const std::string out = fwdSlash(outPath("spp", "position_error"));
    const std::string ref = "vs " + summary.referenceSource;

    Gnuplot gp;
    gp(term(1200, 1000));
    gp("set output '" + out + "'");
    gp("set multiplot layout 4,1 title 'SPP position error ("
       + ref + ") -- " + m_station + "'");
    gp(blk.str());

    const std::vector<std::string> cols   = {"2","3","4","5"};
    const std::vector<std::string> labels = {"dX [m]","dY [m]","dZ [m]","3D error [m]"};
    const std::vector<std::string> colors = {"#1f77b4","#ff7f0e","#2ca02c","#d62728"};

    for (int i = 0; i < 4; ++i) {
        gp("set ylabel '" + labels[static_cast<std::size_t>(i)] + "'");
        gp((i == 3) ? "set xlabel 'MJD'" : "set xlabel ''");
        gp("set grid");
        gp("set key off");
        gp("plot $ERR u 1:" + cols[static_cast<std::size_t>(i)]
           + " w l lc rgb '" + colors[static_cast<std::size_t>(i)]
           + "' lw 1.5 notitle");
    }
    gp("unset multiplot");
    LOKI_INFO("PlotGnss: " + out);
}

// =============================================================================
//  plotPositionScatter  -- horizontal dE vs dN scatter in local ENU
// =============================================================================

void PlotGnss::plotPositionScatter(const std::vector<SppResult>& spp,
                                    const SppSummary&             summary) const
{
    if (!m_cfg.gnss.figures.spp.positionScatter || !summary.hasReference || spp.empty()) return;

    static const loki::math::Ellipsoid wgs84 =
        loki::math::makeEllipsoid(loki::math::EllipsoidModel::WGS84);

    // ENU rotation matrix at reference point
    const loki::geodesy::EcefPoint refEp{summary.refX, summary.refY, summary.refZ};
    const loki::geodesy::GeodPoint refGp = loki::geodesy::ecef2geod(refEp, wgs84);
    const Eigen::Matrix3d R = loki::geodesy::ecefEnuRotMat(refGp.lat, refGp.lon);

    std::ostringstream blk;
    blk << "$SCAT << EOD\n";
    for (const auto& r : spp) {
        if (!r.valid) continue;
        const Eigen::Vector3d dEcef{
            r.x - summary.refX,
            r.y - summary.refY,
            r.z - summary.refZ};
        const Eigen::Vector3d enu = R * dEcef;
        blk << fmt(enu(0)) << " " << fmt(enu(1)) << "\n";   // E, N
    }
    blk << "EOD";

    const std::string out = fwdSlash(outPath("spp", "position_scatter"));
    Gnuplot gp;
    gp(term(700, 700));
    gp("set output '" + out + "'");
    gp("set title 'SPP horizontal scatter (ENU) -- " + m_station + "'");
    gp("set xlabel 'dE [m]'");
    gp("set ylabel 'dN [m]'");
    gp("set grid");
    gp("set size square");
    gp("set key off");
    gp("set style circle radius 0.2");
    gp(blk.str());
    gp("plot $SCAT u 1:2 w p pt 7 ps 0.3 lc rgb '#1f77b460' notitle, "
       "     0,0 w p pt 2 ps 2 lc rgb '#d62728' lw 2 notitle");
    LOKI_INFO("PlotGnss: " + out);
}

// =============================================================================
//  plotAll
// =============================================================================

void PlotGnss::plotAll(const GnssResult& result,
                        const NavFile&    nav,
                        const ObsFile&    obs) const
{
    // --- Observation-level plots (always) ---
    plotSatCount(obs);
    plotElevation(obs, nav);
    plotSkyplot(obs, nav);

    // --- SPP plots ---
    if (result.hasSpp) {
        plotDop          (result.spp);
        plotClockBias    (result.spp);
        plotResiduals    (result.spp);
        plotIsb          (result.spp);
        plotPositionEcef (result.spp);
        plotPositionError  (result.spp, result.sppSummary);
        plotPositionScatter(result.spp, result.sppSummary);
    }
}
