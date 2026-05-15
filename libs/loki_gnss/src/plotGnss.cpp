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

// Format a double for gnuplot inline data (scientific notation, 10 digits).
std::string fmt(double v)
{
    std::ostringstream oss;
    oss << std::scientific << std::setprecision(10) << v;
    return oss.str();
}

// Terminal string for pngcairo.
std::string term(int w = 1200, int h = 600)
{
    return "set terminal pngcairo noenhanced font 'Sans,12' size "
           + std::to_string(w) + "," + std::to_string(h);
}

// 20-colour palette for per-PRN plots, matching the reference images.
const std::vector<std::string> PRN_PALETTE = {
    "#1f77b4","#aec7e8","#ff7f0e","#ffbb78","#2ca02c",
    "#98df8a","#d62728","#ff9896","#9467bd","#c5b0d5",
    "#8c564b","#c49c94","#e377c2","#f7b6d2","#7f7f7f",
    "#c7c7c7","#bcbd22","#dbdb8d","#17becf","#9edae5",
    "#393b79","#637939","#8c6d31","#843c39","#7b4173",
    "#5254a3","#6b6ecf","#b5cf6b","#e7cb94","#d6616b"
};

// PRN label: 'G' -> "GPS", sysChar -> constellation.
char sysChar(GnssSystem s)
{
    switch (s) {
        case GnssSystem::GPS:     return 'G';
        case GnssSystem::GLONASS: return 'R';
        case GnssSystem::GALILEO: return 'E';
        case GnssSystem::BEIDOU:  return 'C';
        default:                  return '?';
    }
}

std::string prnLabel(GnssSystem s, int prn)
{
    std::ostringstream oss;
    oss << sysChar(s) << std::setw(2) << std::setfill('0') << prn;
    return oss.str();
}

// Constellation-level colour (for sky plot per-constellation).
std::string sysColor(char s)
{
    switch (s) {
        case 'G': return "#1f77b4";
        case 'R': return "#ff7f0e";
        case 'E': return "#2ca02c";
        case 'C': return "#d62728";
        default:  return "#888888";
    }
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

double PlotGnss::toSecOfDay(const GpsTime& t)
{
    const auto ts = t.toTimeStamp();
    return ts.hour() * 3600.0 + ts.minute() * 60.0 + ts.second();
}

std::string PlotGnss::utcDateStr(const GpsTime& t)
{
    // GPS time 00:00:00 converts to UTC 23:59:42 of the *previous* day
    // (18 leap seconds behind).  Add 30 s before flooring so that any
    // epoch within the GPS day rounds to the correct UTC calendar date.
    const ::TimeStamp ts = t.toTimeStamp();
    const double mjdRounded = std::floor(ts.mjd() + 30.0 / 86400.0);
    const ::TimeStamp tsDay = ::TimeStamp::fromMjd(mjdRounded + 0.5);
    std::ostringstream oss;
    oss << std::setw(2) << std::setfill('0') << tsDay.day()   << "-"
        << std::setw(2) << std::setfill('0') << tsDay.month() << "-"
        << tsDay.year();
    return oss.str();
}

std::string PlotGnss::xAxisUtcSetup(const std::string& xlabel)
{
    // Use gnuplot's time axis with seconds-of-day as the raw value.
    // Format: HH:MM.  Tics every 3 hours.
    std::ostringstream s;
    s << "set xdata time\n"
      << "set timefmt '%s'\n"
      << "set format x '%H:%M'\n"
      << "set xtics 10800\n"          // 3-hour tics
      << "set mxtics 3\n"             // minor tics every hour
      << "set xlabel '" << xlabel << "'\n";
    return s.str();
}

// =============================================================================
//  plotSatCount  (observation-level)
// =============================================================================

void PlotGnss::plotSatCount(const ObsFile& obs) const
{
    if (!m_cfg.gnss.figures.satcount || obs.epochs.empty()) return;

    const std::string dateStr = utcDateStr(obs.epochs.front().time);

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
        blk << fmt(toSecOfDay(ep.time)) << " "
            << tot << " " << gps << " " << glo << " " << gal << " " << bds << "\n";
    }
    blk << "EOD";

    const std::string out = fwdSlash(outPath("satcount", "timeseries"));
    Gnuplot gp;
    gp(term(1200, 500));
    gp("set output '" + out + "'");
    gp("set title 'Satellites tracked per epoch (" + dateStr + ") -- " + m_station + "'");
    gp(xAxisUtcSetup());
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
// =============================================================================

void PlotGnss::plotElevation(const ObsFile& obs, const NavFile& nav) const
{
    if (!m_cfg.gnss.figures.elevation || obs.epochs.empty()) return;

    const std::array<double,3> staEcef{
        obs.receiver.approxX, obs.receiver.approxY, obs.receiver.approxZ};
    const std::string dateStr = utcDateStr(obs.epochs.front().time);

    std::ostringstream blk;
    blk << "$ELV << EOD\n";

    for (std::size_t ei = 0; ei < obs.epochs.size(); ei += 10) {
        const auto& ep = obs.epochs[ei];
        double sumEl = 0.0, minEl = 90.0, maxEl = 0.0;
        int cnt = 0;

        for (const auto& sat : ep.satellites) {
            const SatState st = KeplerOrbit::compute(nav, sat.system, sat.prn, ep.time);
            if (!st.valid) continue;
            double el = 0.0, az = 0.0;
            SatVisibility::ecefToElevAzim({st.x, st.y, st.z}, staEcef, el, az);
            if (el < 0.0) continue;
            sumEl += el; minEl = std::min(minEl, el); maxEl = std::max(maxEl, el);
            ++cnt;
        }
        if (cnt == 0) continue;
        blk << fmt(toSecOfDay(ep.time)) << " "
            << fmt(sumEl / cnt) << " " << fmt(minEl) << " " << fmt(maxEl) << "\n";
    }
    blk << "EOD";

    const std::string out = fwdSlash(outPath("elevation", "timeseries"));
    Gnuplot gp;
    gp(term(1200, 500));
    gp("set output '" + out + "'");
    gp("set title 'Satellite elevation (" + dateStr + ") -- " + m_station + "'");
    gp(xAxisUtcSetup());
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
//  plotSkyplot / _plotSkyplotImpl
// =============================================================================

void PlotGnss::plotSkyplot(const ObsFile& obs, const NavFile& nav) const
{
    _plotSkyplotImpl(obs, nav, false);
    _plotSkyplotImpl(obs, nav, true);
}

void PlotGnss::_plotSkyplotImpl(const ObsFile& obs, const NavFile& nav,
                                 bool perPrn) const
{
    if (obs.epochs.empty()) return;
    if ( perPrn && !m_cfg.gnss.figures.skyplotPrn)          return;
    if (!perPrn && !m_cfg.gnss.figures.skyplotConstellation) return;

    const std::array<double,3> staEcef{
        obs.receiver.approxX, obs.receiver.approxY, obs.receiver.approxZ};

    struct Track {
        std::string prn;
        char        sys;
        std::vector<std::pair<double,double>> pts;
    };
    std::map<std::string, Track> tracks;

    for (std::size_t ei = 0; ei < obs.epochs.size(); ei += 5) {
        const auto& ep = obs.epochs[ei];
        for (const auto& sat : ep.satellites) {
            const SatState st = KeplerOrbit::compute(nav, sat.system, sat.prn, ep.time);
            if (!st.valid) continue;
            double el = 0.0, az = 0.0;
            SatVisibility::ecefToElevAzim({st.x, st.y, st.z}, staEcef, el, az);
            if (el < 0.0) continue;

            const char sc = sysChar(sat.system);
            if (sc == '?') continue;

            std::ostringstream id;
            id << sc << std::setw(2) << std::setfill('0') << sat.prn;
            const std::string key = id.str();

            auto& tr = tracks[key];
            tr.prn = key; tr.sys = sc;

            const double az_rad = az * (std::numbers::pi / 180.0);
            const double r      = 90.0 - el;
            tr.pts.emplace_back(r * std::sin(az_rad), r * std::cos(az_rad));
        }
    }
    if (tracks.empty()) return;

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
    datablocks << "$C80 << EOD\n" << makeCircle(10.0, 360) << "EOD\n";

    datablocks << "$TICKS << EOD\n";
    for (int deg = 0; deg < 360; deg += 10) {
        const double a  = deg * (std::numbers::pi / 180.0);
        const double s  = std::sin(a), c = std::cos(a);
        const double r1 = (deg % 30 == 0) ? 84.0 : 87.0;
        datablocks << fmt(r1*s) << " " << fmt(r1*c) << "\n"
                   << fmt(90.0*s) << " " << fmt(90.0*c) << "\n\n";
    }
    datablocks << "EOD\n";

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

    std::ostringstream plotcmd;
    plotcmd << "plot ";
    plotcmd << "$C0   u 1:2 w l lc rgb '#aaaaaa' lw 1.0 dt 1 notitle, "
               "$C30  u 1:2 w l lc rgb '#aaaaaa' lw 0.5 dt 2 notitle, "
               "$C60  u 1:2 w l lc rgb '#aaaaaa' lw 0.5 dt 2 notitle, "
               "$C80  u 1:2 w l lc rgb '#aaaaaa' lw 0.5 dt 2 notitle, "
               "$TICKS u 1:2 w l lc rgb '#888888' lw 0.5 dt 1 notitle";

    for (const auto& [key, tr] : tracks) {
        if (tr.pts.empty()) continue;
        const std::string col = perPrn ? prnColorMap.at(key) : sysColor(tr.sys);
        plotcmd << ", \\\n  $T" << key
                << " u 1:2 w l lc rgb '" << col
                << "' lw 0.8 dt 1 notitle";
    }

    // Build legend from unique PRNs in a compact multi-column box.
    // We label the endpoint of each track.
    const std::string suffix = perPrn ? "prn" : "polar";
    const std::string title  = perPrn
        ? "Satellite Sky Distribution (" + utcDateStr(obs.epochs.front().time) + ")"
        : "Satellite Sky Distribution (" + utcDateStr(obs.epochs.front().time) + ")";

    const std::string out = fwdSlash(
        perPrn ? outPath("ppp", "sky") : outPath("skyplot", "polar"));
    Gnuplot gp;
    gp(term(700, 950));
    gp("set output '" + out + "'");
    gp("set xrange [-95:95]");
    gp("set yrange [-130:95]");   // extra bottom margin for legend
    gp("set size square");
    gp("unset border");
    gp("unset xtics");
    gp("unset ytics");

    // Disable automatic key; we'll add a manual label grid below.
    gp("set key off");
    gp("set title '" + title + "'");

    // Elevation ring labels.
    gp("set label 100 '90' at  0, 92 center font ',9' tc rgb '#666666'");
    gp("set label 101 '80' at  0, 82 center font ',9' tc rgb '#666666'");
    gp("set label 102 '70' at  0, 72 center font ',9' tc rgb '#666666'");
    gp("set label 103 '60' at  0, 62 center font ',9' tc rgb '#666666'");
    gp("set label 104 '50' at  0, 52 center font ',9' tc rgb '#666666'");
    gp("set label 105 '40' at  0, 42 center font ',9' tc rgb '#666666'");
    gp("set label 106 '30' at  0, 32 center font ',9' tc rgb '#666666'");
    gp("set label 107 '20' at  0, 22 center font ',9' tc rgb '#666666'");
    gp("set label 108 '10' at  0, 12 center font ',9' tc rgb '#666666'");

    // Cardinal directions.
    const std::vector<std::pair<std::string,std::pair<double,double>>> cardinals = {
        {"N",  { 0.0,  95.0}}, {"E",  {95.0,  0.0}},
        {"S",  { 0.0, -95.0}}, {"W",  {-95.0, 0.0}}
    };
    int lblId = 200;
    for (const auto& [lbl, pos] : cardinals) {
        gp("set label " + std::to_string(lblId++) + " '" + lbl + "' at "
           + fmt(pos.first) + "," + fmt(pos.second)
           + " center font ',11' tc rgb '#333333'");
    }

    // Manual legend grid at the bottom (4 columns).
    // Each entry: coloured dot + PRN label.
    if (perPrn) {
        const std::vector<std::string> keys = [&]{
            std::vector<std::string> v;
            for (const auto& [k, tr] : tracks)
                if (!tr.pts.empty()) v.push_back(k);
            return v;
        }();

        constexpr int   COLS      = 5;
        constexpr double COL_W    = 36.0;   // x spacing per column [plot units]
        constexpr double ROW_H    = 6.0;    // y spacing per row
        const double startX       = -88.0;
        const double startY       = -105.0;

        for (std::size_t i = 0; i < keys.size(); ++i) {
            const std::string& k   = keys[i];
            const std::string& col = prnColorMap.at(k);
            const int    col_i = static_cast<int>(i) % COLS;
            const int    row_i = static_cast<int>(i) / COLS;
            const double lx    = startX + col_i * COL_W;
            const double ly    = startY - row_i * ROW_H;

            gp("set label " + std::to_string(lblId++)
               + " '+' at " + fmt(lx) + "," + fmt(ly)
               + " left font ',8' tc rgb '" + col + "'");
            gp("set label " + std::to_string(lblId++)
               + " '" + k + "' at " + fmt(lx + 4.5) + "," + fmt(ly)
               + " left font ',8' tc rgb '#333333'");
        }
    }

    gp(datablocks.str());
    gp(plotcmd.str());
    LOKI_INFO("PlotGnss: " + out);
}

// =============================================================================
//  plotDop  (SPP)
// =============================================================================

void PlotGnss::plotDop(const std::vector<SppResult>& spp) const
{
    if (!m_cfg.gnss.figures.dop || spp.empty()) return;

    const std::string dateStr = utcDateStr(spp.front().time);
    std::ostringstream blk;
    blk << "$DOP << EOD\n";
    for (const auto& r : spp) {
        if (!r.valid) continue;
        blk << fmt(toSecOfDay(r.time)) << " " << fmt(r.pdop) << "\n";
    }
    blk << "EOD";

    const std::string out = fwdSlash(outPath("dop", "timeseries"));
    Gnuplot gp;
    gp(term(1200, 500));
    gp("set output '" + out + "'");
    gp("set title 'PDOP (" + dateStr + ") -- " + m_station + "'");
    gp(xAxisUtcSetup());
    gp("set ylabel 'PDOP'");
    gp("set yrange [0:10]");
    gp("set grid");
    gp("set key off");
    gp(blk.str());
    gp("plot $DOP u 1:2 w l lc rgb '#1f77b4' lw 1.5 notitle");
    LOKI_INFO("PlotGnss: " + out);
}

// =============================================================================
//  plotClockBias  (SPP)
// =============================================================================

void PlotGnss::plotClockBias(const std::vector<SppResult>& spp) const
{
    if (!m_cfg.gnss.figures.spp.clockbias || spp.empty()) return;

    constexpr double C = 299792458.0;
    const std::string dateStr = utcDateStr(spp.front().time);
    std::ostringstream blk;
    blk << "$CLK << EOD\n";
    for (const auto& r : spp) {
        if (!r.valid) continue;
        blk << fmt(toSecOfDay(r.time)) << " "
            << fmt(r.clkBiasM) << " "
            << fmt(r.clkBiasM / C * 1.0e6) << "\n";
    }
    blk << "EOD";

    const std::string out = fwdSlash(outPath("spp", "clockbias"));
    Gnuplot gp;
    gp(term(1200, 700));
    gp("set output '" + out + "'");
    gp("set multiplot layout 2,1 title 'GPS receiver clock bias (SPP) ("
       + dateStr + ") -- " + m_station + "'");
    gp(blk.str());
    gp(xAxisUtcSetup(""));
    gp("set ylabel 'Clock bias [m]'");
    gp("set grid");
    gp("set key off");
    gp("plot $CLK u 1:2 w l lc rgb '#1f77b4' lw 1.5 notitle");
    gp(xAxisUtcSetup());
    gp("set ylabel 'Clock bias [us]'");
    gp("plot $CLK u 1:3 w l lc rgb '#ff7f0e' lw 1.5 notitle");
    gp("unset multiplot");
    LOKI_INFO("PlotGnss: " + out);
}

// =============================================================================
//  plotResiduals  (SPP)
// =============================================================================

void PlotGnss::plotResiduals(const std::vector<SppResult>& spp) const
{
    if (!m_cfg.gnss.figures.spp.residuals || spp.empty()) return;

    const std::string dateStr = utcDateStr(spp.front().time);
    std::ostringstream blk;
    blk << "$RES << EOD\n";
    for (const auto& r : spp) {
        if (!r.valid || r.residuals.empty()) continue;
        double sumAbs = 0.0, sumSq = 0.0;
        for (double res : r.residuals) { sumAbs += std::fabs(res); sumSq += res * res; }
        const double n   = static_cast<double>(r.residuals.size());
        const double rms = std::sqrt(sumSq / n);
        blk << fmt(toSecOfDay(r.time)) << " "
            << fmt(sumAbs / n) << " " << fmt(rms) << "\n";
    }
    blk << "EOD";

    const std::string out = fwdSlash(outPath("spp", "residuals"));
    Gnuplot gp;
    gp(term(1200, 500));
    gp("set output '" + out + "'");
    gp("set title 'Pseudorange residuals SPP (" + dateStr + ") -- " + m_station + "'");
    gp(xAxisUtcSetup());
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
//  plotIsb  (SPP)
// =============================================================================

void PlotGnss::plotIsb(const std::vector<SppResult>& spp) const
{
    if (!m_cfg.gnss.figures.spp.isb) return;

    bool hasGlo = false, hasGal = false, hasBds = false;
    for (const auto& r : spp) {
        if (!r.valid) continue;
        if (r.isbM.count("GLONASS")) hasGlo = true;
        if (r.isbM.count("GALILEO")) hasGal = true;
        if (r.isbM.count("BEIDOU"))  hasBds = true;
    }
    if (!hasGlo && !hasGal && !hasBds) return;

    const std::string dateStr = utcDateStr(spp.front().time);
    std::ostringstream blk;
    blk << "$ISB << EOD\n";
    for (const auto& r : spp) {
        if (!r.valid) continue;
        auto get = [&](const std::string& k) -> double {
            auto it = r.isbM.find(k);
            return (it != r.isbM.end()) ? it->second
                                        : std::numeric_limits<double>::quiet_NaN();
        };
        blk << fmt(toSecOfDay(r.time)) << " "
            << fmt(get("GLONASS")) << " "
            << fmt(get("GALILEO")) << " "
            << fmt(get("BEIDOU"))  << "\n";
    }
    blk << "EOD";

    const std::string out = fwdSlash(outPath("spp", "isb"));
    Gnuplot gp;
    gp(term(1200, 600));
    gp("set output '" + out + "'");
    gp("set title 'Inter-system biases SPP (" + dateStr + ") -- " + m_station + "'");
    gp(xAxisUtcSetup());
    gp("set ylabel 'ISB [m]'");
    gp("set grid");
    gp("set key top right");
    gp(blk.str());
    std::string plotcmd = "plot ";
    bool first = true;
    if (hasGlo) { plotcmd += "$ISB u 1:2 w l lc rgb '#ff7f0e' lw 1.5 title 'GLONASS'"; first = false; }
    if (hasGal) { if (!first) plotcmd += ", "; plotcmd += "$ISB u 1:3 w l lc rgb '#2ca02c' lw 1.5 title 'Galileo'"; first = false; }
    if (hasBds) { if (!first) plotcmd += ", "; plotcmd += "$ISB u 1:4 w l lc rgb '#d62728' lw 1.5 title 'BeiDou'"; }
    gp(plotcmd);
    LOKI_INFO("PlotGnss: " + out);
}

// =============================================================================
//  plotPositionEcef  (SPP)
// =============================================================================

void PlotGnss::plotPositionEcef(const std::vector<SppResult>& spp) const
{
    if (!m_cfg.gnss.figures.spp.positionEcef || spp.empty()) return;

    const std::string dateStr = utcDateStr(spp.front().time);
    std::ostringstream blk;
    blk << "$POS << EOD\n";
    for (const auto& r : spp) {
        if (!r.valid) continue;
        blk << fmt(toSecOfDay(r.time)) << " "
            << fmt(r.x) << " " << fmt(r.y) << " " << fmt(r.z) << "\n";
    }
    blk << "EOD";

    const std::string out = fwdSlash(outPath("spp", "position_ecef"));
    Gnuplot gp;
    gp(term(1200, 900));
    gp("set output '" + out + "'");
    gp("set multiplot layout 3,1 title 'SPP ECEF position ("
       + dateStr + ") -- " + m_station + "'");
    gp(blk.str());

    const std::vector<std::string> cols   = {"2", "3", "4"};
    const std::vector<std::string> labels = {"X [m]", "Y [m]", "Z [m]"};
    const std::vector<std::string> colors = {"#1f77b4", "#ff7f0e", "#2ca02c"};

    for (int i = 0; i < 3; ++i) {
        gp(xAxisUtcSetup(i == 2 ? "UTC" : ""));
        gp("set ylabel '" + labels[static_cast<std::size_t>(i)] + "'");
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
//  plotPositionError  (SPP)
// =============================================================================

void PlotGnss::plotPositionError(const std::vector<SppResult>& spp,
                                  const SppSummary&             summary) const
{
    if (!m_cfg.gnss.figures.spp.positionError || !summary.hasReference || spp.empty()) return;

    const std::string dateStr = utcDateStr(spp.front().time);
    std::ostringstream blk;
    blk << "$ERR << EOD\n";
    for (const auto& r : spp) {
        if (!r.valid) continue;
        const double dX = r.x - summary.refX;
        const double dY = r.y - summary.refY;
        const double dZ = r.z - summary.refZ;
        const double d3 = std::sqrt(dX*dX + dY*dY + dZ*dZ);
        blk << fmt(toSecOfDay(r.time)) << " "
            << fmt(dX) << " " << fmt(dY) << " " << fmt(dZ) << " " << fmt(d3) << "\n";
    }
    blk << "EOD";

    const std::string out = fwdSlash(outPath("spp", "position_error"));
    Gnuplot gp;
    gp(term(1200, 1000));
    gp("set output '" + out + "'");
    gp("set multiplot layout 4,1 title 'SPP position error vs "
       + summary.referenceSource + " (" + dateStr + ") -- " + m_station + "'");
    gp(blk.str());

    const std::vector<std::string> cols   = {"2","3","4","5"};
    const std::vector<std::string> labels = {"dX [m]","dY [m]","dZ [m]","3D error [m]"};
    const std::vector<std::string> colors = {"#1f77b4","#ff7f0e","#2ca02c","#d62728"};

    for (int i = 0; i < 4; ++i) {
        gp(xAxisUtcSetup(i == 3 ? "UTC" : ""));
        gp("set ylabel '" + labels[static_cast<std::size_t>(i)] + "'");
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
//  plotPositionScatter  (SPP)
// =============================================================================

void PlotGnss::plotPositionScatter(const std::vector<SppResult>& spp,
                                    const SppSummary&             summary) const
{
    if (!m_cfg.gnss.figures.spp.positionScatter || !summary.hasReference || spp.empty()) return;

    static const loki::math::Ellipsoid wgs84 =
        loki::math::makeEllipsoid(loki::math::EllipsoidModel::WGS84);

    const loki::geodesy::EcefPoint refEp{summary.refX, summary.refY, summary.refZ};
    const loki::geodesy::GeodPoint refGp = loki::geodesy::ecef2geod(refEp, wgs84);
    const Eigen::Matrix3d R = loki::geodesy::ecefEnuRotMat(refGp.lat, refGp.lon);

    std::ostringstream blk;
    blk << "$SCAT << EOD\n";
    for (const auto& r : spp) {
        if (!r.valid) continue;
        const Eigen::Vector3d dEcef{r.x - summary.refX, r.y - summary.refY,
                                    r.z - summary.refZ};
        const Eigen::Vector3d enu = R * dEcef;
        blk << fmt(enu(0)) << " " << fmt(enu(1)) << "\n";
    }
    blk << "EOD";

    const std::string out = fwdSlash(outPath("spp", "position_scatter"));
    Gnuplot gp;
    gp(term(700, 700));
    gp("set output '" + out + "'");
    gp("set title 'SPP horizontal scatter ENU -- " + m_station + "'");
    gp("set xlabel 'dE [m]'");
    gp("set ylabel 'dN [m]'");
    gp("set grid");
    gp("set size square");
    gp("set key off");
    gp(blk.str());
    gp("plot $SCAT u 1:2 w p pt 7 ps 0.3 lc rgb '#1f77b460' notitle, "
       "     0,0 w p pt 2 ps 2 lc rgb '#d62728' lw 2 notitle");
    LOKI_INFO("PlotGnss: " + out);
}

// =============================================================================
//  Helpers: ENU diff from ECEF PPP result vs reference
// =============================================================================

namespace {

struct EnuDiff { double dLat{0.0}, dLon{0.0}, dHgt{0.0}; };

EnuDiff ecefToEnuDiff(double x, double y, double z,
                       double refX, double refY, double refZ,
                       const loki::math::Ellipsoid& wgs84)
{
    const loki::geodesy::EcefPoint rp{refX, refY, refZ};
    const loki::geodesy::GeodPoint rg = loki::geodesy::ecef2geod(rp, wgs84);
    const Eigen::Matrix3d R = loki::geodesy::ecefEnuRotMat(rg.lat, rg.lon);
    const Eigen::Vector3d dEcef{x - refX, y - refY, z - refZ};
    const Eigen::Vector3d enu = R * dEcef;
    return {enu(1), enu(0), enu(2)};   // lat=N, lon=E, hgt=U
}

} // namespace

// =============================================================================
//  plotPppSky
// =============================================================================

void PlotGnss::plotPppSky(const ObsFile& obs, const NavFile& nav) const
{
    // Reuse per-PRN skyplot logic; it writes to gnss_STATION_ppp_sky.png
    // when called for the PPP context.  We call the implementation directly
    // with perPrn=true and bypass the figure flag.
    if (obs.epochs.empty()) return;

    const std::array<double,3> staEcef{
        obs.receiver.approxX, obs.receiver.approxY, obs.receiver.approxZ};

    struct Track {
        std::string prn;
        char        sys;
        std::vector<std::pair<double,double>> pts;
    };
    std::map<std::string, Track> tracks;

    for (std::size_t ei = 0; ei < obs.epochs.size(); ei += 5) {
        const auto& ep = obs.epochs[ei];
        for (const auto& sat : ep.satellites) {
            const SatState st = KeplerOrbit::compute(nav, sat.system, sat.prn, ep.time);
            if (!st.valid) continue;
            double el = 0.0, az = 0.0;
            SatVisibility::ecefToElevAzim({st.x, st.y, st.z}, staEcef, el, az);
            if (el < 0.0) continue;

            const char sc = sysChar(sat.system);
            if (sc == '?') continue;

            std::ostringstream id;
            id << sc << std::setw(2) << std::setfill('0') << sat.prn;
            const std::string key = id.str();
            auto& tr = tracks[key];
            tr.prn = key; tr.sys = sc;
            const double az_rad = az * (std::numbers::pi / 180.0);
            const double r      = 90.0 - el;
            tr.pts.emplace_back(r * std::sin(az_rad), r * std::cos(az_rad));
        }
    }
    if (tracks.empty()) return;

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
    datablocks << "$C80 << EOD\n" << makeCircle(10.0, 360) << "EOD\n";

    datablocks << "$TICKS << EOD\n";
    for (int deg = 0; deg < 360; deg += 10) {
        const double a = deg * (std::numbers::pi / 180.0);
        const double s = std::sin(a), c = std::cos(a);
        const double r1 = (deg % 30 == 0) ? 84.0 : 87.0;
        datablocks << fmt(r1*s) << " " << fmt(r1*c) << "\n"
                   << fmt(90.0*s) << " " << fmt(90.0*c) << "\n\n";
    }
    datablocks << "EOD\n";

    int prnIdx = 0;
    std::map<std::string,std::string> prnColorMap;
    for (const auto& [key, tr] : tracks) {
        if (tr.pts.empty()) continue;
        datablocks << "$T" << key << " << EOD\n";
        for (const auto& [x, y] : tr.pts)
            datablocks << fmt(x) << " " << fmt(y) << "\n";
        datablocks << "EOD\n";
        prnColorMap[key] = PRN_PALETTE[static_cast<std::size_t>(prnIdx) % PRN_PALETTE.size()];
        ++prnIdx;
    }

    std::ostringstream plotcmd;
    plotcmd << "plot $C0 u 1:2 w l lc rgb '#aaaaaa' lw 1.0 dt 1 notitle, "
               "$C30 u 1:2 w l lc rgb '#aaaaaa' lw 0.5 dt 2 notitle, "
               "$C60 u 1:2 w l lc rgb '#aaaaaa' lw 0.5 dt 2 notitle, "
               "$C80 u 1:2 w l lc rgb '#aaaaaa' lw 0.5 dt 2 notitle, "
               "$TICKS u 1:2 w l lc rgb '#888888' lw 0.5 notitle";

    for (const auto& [key, tr] : tracks) {
        if (tr.pts.empty()) continue;
        plotcmd << ", \\\n  $T" << key
                << " u 1:2 w l lc rgb '" << prnColorMap.at(key)
                << "' lw 0.8 notitle";
    }

    const std::string out = fwdSlash(outPath("ppp", "sky"));
    Gnuplot gp;
    gp(term(700, 950));
    gp("set output '" + out + "'");
    gp("set xrange [-95:95]");
    gp("set yrange [-130:95]");
    gp("set size square");
    gp("unset border"); gp("unset xtics"); gp("unset ytics");
    gp("set key off");
    gp("set title 'Satellite Sky Distribution ("
       + utcDateStr(obs.epochs.front().time) + ")'");

    gp("set label 100 '90' at  0, 92 center font ',9' tc rgb '#666666'");
    gp("set label 101 '80' at  0, 82 center font ',9' tc rgb '#666666'");
    gp("set label 102 '70' at  0, 72 center font ',9' tc rgb '#666666'");
    gp("set label 103 '60' at  0, 62 center font ',9' tc rgb '#666666'");
    gp("set label 104 '50' at  0, 52 center font ',9' tc rgb '#666666'");
    gp("set label 105 '40' at  0, 42 center font ',9' tc rgb '#666666'");
    gp("set label 106 '30' at  0, 32 center font ',9' tc rgb '#666666'");
    gp("set label 107 '20' at  0, 22 center font ',9' tc rgb '#666666'");
    gp("set label 108 '10' at  0, 12 center font ',9' tc rgb '#666666'");

    const std::vector<std::pair<std::string,std::pair<double,double>>> cardinals = {
        {"N",{0.0,95.0}},{"E",{95.0,0.0}},{"S",{0.0,-95.0}},{"W",{-95.0,0.0}}
    };
    int lblId = 200;
    for (const auto& [lbl, pos] : cardinals) {
        gp("set label " + std::to_string(lblId++) + " '" + lbl + "' at "
           + fmt(pos.first) + "," + fmt(pos.second)
           + " center font ',11' tc rgb '#333333'");
    }

    // Legend grid (5 columns).
    {
        const std::vector<std::string> keys = [&]{
            std::vector<std::string> v;
            for (const auto& [k, tr] : tracks)
                if (!tr.pts.empty()) v.push_back(k);
            return v;
        }();
        constexpr int    COLS   = 5;
        constexpr double COL_W  = 36.0;
        constexpr double ROW_H  = 6.5;
        const double startX     = -88.0;
        const double startY     = -105.0;

        for (std::size_t i = 0; i < keys.size(); ++i) {
            const std::string& k   = keys[i];
            const std::string& col = prnColorMap.at(k);
            const int    ci = static_cast<int>(i) % COLS;
            const int    ri = static_cast<int>(i) / COLS;
            const double lx = startX + ci * COL_W;
            const double ly = startY - ri * ROW_H;
            gp("set label " + std::to_string(lblId++)
               + " '+' at " + fmt(lx) + "," + fmt(ly)
               + " left font ',8' tc rgb '" + col + "'");
            gp("set label " + std::to_string(lblId++)
               + " '" + k + "' at " + fmt(lx + 4.5) + "," + fmt(ly)
               + " left font ',8' tc rgb '#333333'");
        }
    }

    gp(datablocks.str());
    gp(plotcmd.str());
    LOKI_INFO("PlotGnss: " + out);
}

// =============================================================================
//  plotPppCoordDiff  (lat, lon, hgt — 3 separate files)
// =============================================================================

void PlotGnss::plotPppCoordDiff(const std::vector<PppResult>& ppp,
                                  const PppSummary& summary) const
{
    if (ppp.empty() || !summary.hasReference) return;

    static const loki::math::Ellipsoid wgs84 =
        loki::math::makeEllipsoid(loki::math::EllipsoidModel::WGS84);

    const std::string dateStr = utcDateStr(ppp.front().time);

    // Split into three separate datablocks to avoid column() inside
    // using-clauses with multiple axes -- not supported in gnuplot 5.x.
    //   CONV: sod dLat dLon dHgt   (converged epochs)
    //   PREC: sod dLat dLon dHgt   (pre-convergence epochs)
    //   SIG:  sod sigLat sigLon sigHgt  (all epochs, right axis)
    std::ostringstream blkConv, blkPrec, blkSig;
    blkConv << "$CONV << EOD\n";
    blkPrec << "$PREC << EOD\n";
    blkSig  << "$SIG  << EOD\n";

    double prevSodConv = -1e9, prevSodPrec = -1e9;
    for (const auto& r : ppp) {
        if (!r.valid) continue;
        const EnuDiff d = ecefToEnuDiff(r.x, r.y, r.z,
                                         summary.refX, summary.refY, summary.refZ,
                                         wgs84);
        const double sod = toSecOfDay(r.time);
        blkSig << fmt(sod) << " "
               << fmt(r.sigmaYm) << " "
               << fmt(r.sigmaXm) << " "
               << fmt(r.sigmaZm) << "\n";
        if (r.converged) {
            // Insert blank line on gap > 60 s to break gnuplot line segment.
            if (prevSodConv > -1e8 && (sod - prevSodConv) > 60.0)
                blkConv << "\n";
            blkConv << fmt(sod) << " "
                    << fmt(d.dLat) << " "
                    << fmt(d.dLon) << " "
                    << fmt(d.dHgt) << "\n";
            prevSodConv = sod;
        } else {
            if (prevSodPrec > -1e8 && (sod - prevSodPrec) > 60.0)
                blkPrec << "\n";
            blkPrec << fmt(sod) << " "
                    << fmt(d.dLat) << " "
                    << fmt(d.dLon) << " "
                    << fmt(d.dHgt) << "\n";
            prevSodPrec = sod;
        }
    }
    blkConv << "EOD";
    blkPrec << "EOD";
    blkSig  << "EOD";

    const std::string allBlocks = blkConv.str() + "\n"
                                + blkPrec.str() + "\n"
                                + blkSig.str();

    struct CoordSpec {
        const char* suffix;
        const char* title;
        const char* colC;
        const char* colS;
    };
    const std::array<CoordSpec, 3> specs = {{
        {"coord_lat", "Latitude Differences",  "2", "2"},
        {"coord_lon", "Longitude Differences", "3", "3"},
        {"coord_hgt", "Height Differences",    "4", "4"},
    }};

    for (const auto& sp : specs) {
        const std::string out = fwdSlash(outPath("ppp", sp.suffix));
        Gnuplot gp;
        gp(term(1200, 450));
        gp("set output '" + out + "'");
        gp(std::string("set title '") + sp.title + " (" + dateStr + ")'");
        gp(xAxisUtcSetup());
        gp("set ylabel 'metres'");
        gp("set y2label 'Sigma (metres)'");
        gp("set ytics nomirror");
        gp("set y2tics");
        gp("set y2range [0:*]");
        gp("set grid");
        gp("set key top left");
        gp(allBlocks);
        gp(std::string("plot $PREC u 1:") + sp.colC
           + " axes x1y1 w p pt 7 ps 0.3 lc rgb '#aaaaaa' lw 1.0 title 'Pre-convergence', "
           "     $CONV u 1:" + sp.colC
           + " axes x1y1 w p pt 7 ps 0.3 lc rgb '#1f77b4' lw 1.5 title 'Final solution', "
           "     $SIG  u 1:" + sp.colS
           + " axes x1y2 w p pt 7 ps 0.3 lc rgb '#d62728' lw 1.2 title 'SIG_PPP'");
        LOKI_INFO("PlotGnss: " + out);
    }
}

// =============================================================================
//  plotPppTroposphere
// =============================================================================

void PlotGnss::plotPppTroposphere(const std::vector<PppResult>& ppp) const
{
    if (ppp.empty()) return;

    const std::string dateStr = utcDateStr(ppp.front().time);

    // col: 1=sod  2=ztdTotal  3=sigma
    std::ostringstream blk;
    blk << "$TRP << EOD\n";
    for (const auto& r : ppp) {
        if (!r.valid) continue;
        blk << fmt(toSecOfDay(r.time)) << " "
            << fmt(r.ztdTotalM) << " "
            << fmt(r.sigmaZtdM) << "\n";
    }
    blk << "EOD";

    const std::string out = fwdSlash(outPath("ppp", "troposphere"));
    Gnuplot gp;
    gp(term(1200, 500));
    gp("set output '" + out + "'");
    gp("set title 'Estimated Tropospheric Zenith Delay (" + dateStr + ")'");
    gp(xAxisUtcSetup());
    gp("set ylabel 'metres'");
    gp("set grid");
    gp("set key top right");
    // Grey sigma envelope, blue ZTD line.
    gp(blk.str());
    gp("set y2label 'Sigma (metres)'");
    gp("set ytics nomirror");
    gp("set y2tics");
    gp("set y2range [0:*]");
    // gp("plot $CLK u 1:2 axes x1y1 w p pt 7 ps 0.3 lc rgb '#1f77b4' title 'Clock offset', "
    // "     $CLK u 1:3 axes x1y2 w p pt 7 ps 0.2 lc rgb '#d62728' title 'Sigma'");
    gp("plot $TRP u 1:3 axes x1y2 w filledcurves y1=0 lc rgb '#dddddd' title 'Sigma', "
       "     $TRP u 1:2 axes x1y1 w p pt 7 ps 0.3 lc rgb '#1f77b4' lw 2 title 'ZTD total'");
    LOKI_INFO("PlotGnss: " + out);
}

// =============================================================================
//  plotPppClock
// =============================================================================

void PlotGnss::plotPppClock(const std::vector<PppResult>& ppp) const
{
    if (ppp.empty()) return;

    constexpr double C   = 299792458.0;
    constexpr double M2NS = 1.0e9 / C;   // metres -> nanoseconds

    const std::string dateStr = utcDateStr(ppp.front().time);

    // col: 1=sod  2=clk_ns  3=sigma_ns
    std::ostringstream blk;
    blk << "$CLK << EOD\n";
    for (const auto& r : ppp) {
        if (!r.valid) continue;
        blk << fmt(toSecOfDay(r.time)) << " "
            << fmt(r.clkBiasM * M2NS) << " "
            << fmt(r.sigmaClkM * M2NS) << "\n";
    }
    blk << "EOD";

    const std::string out = fwdSlash(outPath("ppp", "clock"));
    Gnuplot gp;
    gp(term(1200, 500));
    gp("set output '" + out + "'");
    gp("set title 'Station Clock Offset (" + dateStr + ")'");
    gp(xAxisUtcSetup());
    gp("set ylabel 'nanoseconds'");
    gp("set y2label 'Sigma (nanoseconds)'");
    gp("set ytics nomirror");
    gp("set y2tics");
    gp("set y2range [0:*]");
    gp("set grid");
    gp("set key top right");
    gp(blk.str());
    // Blue dots = clock offset (left axis), red = sigma (right axis).
    gp("plot $CLK u 1:2 axes x1y1 w p pt 7 ps 0.3 lc rgb '#1f77b4' title 'Clock offset', "
    "     $CLK u 1:3 axes x1y2 w p pt 7 ps 0.2 lc rgb '#d62728' title 'Sigma'");
    LOKI_INFO("PlotGnss: " + out);
}

// =============================================================================
//  plotPppSatCount
// =============================================================================

void PlotGnss::plotPppSatCount(const std::vector<PppResult>& ppp) const
{
    if (ppp.empty()) return;

    const std::string dateStr = utcDateStr(ppp.front().time);

    // Count ambiguity resets per epoch: a satellite is "new" when it appears
    // in satResiduals for the first time after being absent.
    // Here we track how many residuals were present in a short time window.
    // As a simpler proxy: count epochs where nSats changes abruptly.
    // We compute % ambiguity reset as (nNewArcs / nSats) * 100.
    // A new arc: satellite present but had a residual in previous epoch with
    // a large phase jump (abs phase residual > threshold).
    // Because this information is already baked into the Kalman via
    // resetAmbiguity(), we approximate: any epoch where nSats decreases then
    // increases counts as a reset epoch, and we flag the delta.
    // For now we output nSats per epoch; ambiguity resets are approximated
    // as epochs where the number of tracked sats changed.

    // col: 1=sod  2=nSats  3=pctAmbReset (0 or rough estimate)
    std::ostringstream blk;
    blk << "$SC << EOD\n";
    int prevNSats = -1;
    for (const auto& r : ppp) {
        if (!r.valid) continue;
        // Ambiguity reset: satellite appears in residuals after absence.
        // We don't have enough info here without tracking per-sat history,
        // so we flag epochs where nSats < prevNSats - 1 (satellite was lost
        // and likely re-acquired in the same epoch).
        double pctReset = 0.0;
        if (prevNSats > 0 && r.nSats < prevNSats) {
            pctReset = static_cast<double>(prevNSats - r.nSats)
                       / static_cast<double>(prevNSats) * 100.0;
        }
        blk << fmt(toSecOfDay(r.time)) << " "
            << r.nSats << " "
            << fmt(pctReset) << "\n";
        prevNSats = r.nSats;
    }
    blk << "EOD";

    const std::string out = fwdSlash(outPath("ppp", "satcount"));
    Gnuplot gp;
    gp(term(1200, 500));
    gp("set output '" + out + "'");
    gp("set title 'Tracked Satellites and Reset Ambiguities (" + dateStr + ")'");
    gp(xAxisUtcSetup());
    gp("set ylabel 'Number of satellites'");
    gp("set y2label '% Ambiguity Reset'");
    gp("set ytics nomirror");
    gp("set y2tics");
    gp("set y2range [0:100]");
    gp("set grid");
    gp("set key top right");
    gp(blk.str());
    gp("plot $SC u 1:2 axes x1y1 w p pt 7 ps 0.4 lc rgb '#1f77b4' title 'Satellites', "
       "     $SC u 1:3 axes x1y2 w p pt 2 ps 0.8 lc rgb '#d62728' title '% amb. reset'");
    LOKI_INFO("PlotGnss: " + out);
}

// =============================================================================
//  plotPppResidualsPhase / plotPppResidualsCode
// =============================================================================

namespace {

// Build per-PRN datablock map from ppp results.
// Returns: map<prnLabel, vector<pair<sod, residual>>>
// residualType: 0 = code, 1 = phase.
std::map<std::string, std::vector<std::pair<double,double>>>
buildResidualMap(const std::vector<PppResult>& ppp, int residualType)
{
    std::map<std::string, std::vector<std::pair<double,double>>> m;
    for (const auto& r : ppp) {
        if (!r.valid || r.satResiduals.empty()) continue;
        const auto   ts  = r.time.toTimeStamp();
        const double sod = ts.hour() * 3600.0 + ts.minute() * 60.0 + ts.second();
        for (const auto& sr : r.satResiduals) {
            if (residualType == 1 && !sr.hasPhase) continue;
            const double res = (residualType == 0) ? sr.codeResM : sr.phaseResM;
            m[prnLabel(sr.system, sr.prn)].emplace_back(sod, res);
        }
    }
    return m;
}

} // namespace

void PlotGnss::plotPppResidualsPhase(const std::vector<PppResult>& ppp) const
{
    if (ppp.empty()) return;
    const auto rmap = buildResidualMap(ppp, 1);
    if (rmap.empty()) return;

    const std::string dateStr = utcDateStr(ppp.front().time);
    std::ostringstream datablocks;
    int idx = 0;
    for (const auto& [prn, pts] : rmap) {
        datablocks << "$R" << prn << " << EOD\n";
        for (const auto& [sod, res] : pts)
            datablocks << fmt(sod) << " " << fmt(res) << "\n";
        datablocks << "EOD\n";
        ++idx;
    }

    std::ostringstream plotcmd;
    plotcmd << "plot ";
    bool first = true;
    idx = 0;
    for (const auto& [prn, pts] : rmap) {
        if (!first) plotcmd << ", \\\n     ";
        first = false;
        const std::string col = PRN_PALETTE[static_cast<std::size_t>(idx) % PRN_PALETTE.size()];
        // Use crosshair point style like reference image.
        plotcmd << "$R" << prn << " u 1:2 w p pt 1 ps 0.4 lc rgb '"
                << col << "' title '" << prn << "'";
        ++idx;
    }

    const std::string out = fwdSlash(outPath("ppp", "residuals_phase"));
    Gnuplot gp;
    gp(term(1400, 700));
    gp("set output '" + out + "'");
    gp("set title 'Carrier-Phase Residuals (" + dateStr + ")'");
    gp(xAxisUtcSetup());
    gp("set ylabel 'metres'");
    gp("set grid");
    gp("set key outside below center horizontal maxcols 8 font ',9'");
    gp(datablocks.str());
    gp(plotcmd.str());
    LOKI_INFO("PlotGnss: " + out);
}

void PlotGnss::plotPppResidualsCode(const std::vector<PppResult>& ppp) const
{
    if (ppp.empty()) return;
    const auto rmap = buildResidualMap(ppp, 0);
    if (rmap.empty()) return;

    const std::string dateStr = utcDateStr(ppp.front().time);
    std::ostringstream datablocks;
    int idx = 0;
    for (const auto& [prn, pts] : rmap) {
        datablocks << "$R" << prn << " << EOD\n";
        for (const auto& [sod, res] : pts)
            datablocks << fmt(sod) << " " << fmt(res) << "\n";
        datablocks << "EOD\n";
        ++idx;
    }

    std::ostringstream plotcmd;
    plotcmd << "plot ";
    bool first = true;
    idx = 0;
    for (const auto& [prn, pts] : rmap) {
        if (!first) plotcmd << ", \\\n     ";
        first = false;
        const std::string col = PRN_PALETTE[static_cast<std::size_t>(idx) % PRN_PALETTE.size()];
        plotcmd << "$R" << prn << " u 1:2 w p pt 1 ps 0.4 lc rgb '"
                << col << "' title '" << prn << "'";
        ++idx;
    }

    const std::string out = fwdSlash(outPath("ppp", "residuals_code"));
    Gnuplot gp;
    gp(term(1400, 700));
    gp("set output '" + out + "'");
    gp("set title 'Pseudo-Range Residuals (" + dateStr + ")'");
    gp(xAxisUtcSetup());
    gp("set ylabel 'metres'");
    gp("set grid");
    gp("set key outside below center horizontal maxcols 8 font ',9'");
    gp(datablocks.str());
    gp(plotcmd.str());
    LOKI_INFO("PlotGnss: " + out);
}

// =============================================================================
//  plotPppAmbiguity  (Gantt-style)
// =============================================================================

void PlotGnss::plotPppAmbiguity(const std::vector<PppResult>& ppp) const
{
    if (ppp.empty()) return;

    const std::string dateStr = utcDateStr(ppp.front().time);

    // Collect the set of all PRN labels seen across all epochs.
    std::set<std::string> prnSet;
    for (const auto& r : ppp) {
        if (!r.valid) continue;
        for (const auto& sr : r.satResiduals)
            prnSet.insert(prnLabel(sr.system, sr.prn));
    }
    if (prnSet.empty()) return;

    // Assign a y-index to each PRN (sorted: E first, then G, then R).
    std::vector<std::string> prnList(prnSet.begin(), prnSet.end());
    std::sort(prnList.begin(), prnList.end());
    std::map<std::string,int> prnY;
    for (int i = 0; i < static_cast<int>(prnList.size()); ++i)
        prnY[prnList[static_cast<std::size_t>(i)]] = i;

    // We draw arcs as horizontal line segments.
    // Colour: converged epoch -> green (#2ca02c); pre-conv -> yellow (#ffbb78).
    // An arc break (reset) is drawn as a red cross at the reset epoch.
    //
    // Strategy: track the last seen epoch for each PRN.  Emit a segment from
    // lastSod to currentSod.  If gap > 2*interval, treat as arc break (red tick).

    const double interval = [&]() -> double {
        if (ppp.size() < 2) return 30.0;
        return toSecOfDay(ppp[1].time) - toSecOfDay(ppp[0].time);
    }();

    // Separate data for green arcs, yellow arcs, and red reset ticks.
    std::ostringstream blkGreen, blkYellow, blkRed;
    blkGreen  << "$AG << EOD\n";
    blkYellow << "$AY << EOD\n";
    blkRed    << "$AR << EOD\n";

    struct ArcState {
        double lastSod{-1e9};
        bool   lastConv{false};
    };
    std::map<std::string, ArcState> arcStates;

    for (const auto& r : ppp) {
        if (!r.valid) continue;
        const double sod = toSecOfDay(r.time);

        std::set<std::string> seenThisEpoch;
        for (const auto& sr : r.satResiduals) {
            const std::string lbl = prnLabel(sr.system, sr.prn);
            seenThisEpoch.insert(lbl);
            const int yi = prnY.at(lbl);

            auto& as = arcStates[lbl];

            if (as.lastSod < -1e8) {
                // First appearance.
                blkRed << fmt(as.lastSod < -1e8 ? sod : as.lastSod)
                       << " " << yi << "\n";
                as.lastSod  = sod;
                as.lastConv = r.converged;
                continue;
            }

            const double gap = sod - as.lastSod;
            const bool   isReset = gap > 1.8 * interval;

            if (isReset) {
                // Red tick at the new arc start.
                blkRed << fmt(sod) << " " << yi << "\n";
            }

            // Emit segment from lastSod to sod.
            auto& blk = as.lastConv ? blkGreen : blkYellow;
            blk << fmt(as.lastSod) << " " << (yi + 0.3) << "\n"
                << fmt(sod)        << " " << (yi + 0.3) << "\n\n";

            as.lastSod  = sod;
            as.lastConv = r.converged;
        }
    }
    blkGreen  << "EOD";
    blkYellow << "EOD";
    blkRed    << "EOD";

    const int nPrn  = static_cast<int>(prnList.size());
    const int height = std::max(400, nPrn * 14 + 100);

    const std::string out = fwdSlash(outPath("ppp", "ambiguity"));
    Gnuplot gp;
    gp(term(1200, height));
    gp("set output '" + out + "'");
    gp("set title 'Phase Ambiguity Status (" + dateStr + ")'");
    gp(xAxisUtcSetup());
    gp("set ylabel ''");
    gp("set yrange [-0.5:" + std::to_string(nPrn) + "]");
    gp("set grid xtics");

    // Y-axis: PRN labels.
    std::ostringstream ytics;
    ytics << "set ytics (";
    for (int i = 0; i < nPrn; ++i) {
        if (i > 0) ytics << ", ";
        ytics << "'" << prnList[static_cast<std::size_t>(i)]
              << "' " << (i + 0.3);
    }
    ytics << ")";
    gp(ytics.str());
    gp("set ytics font ',8'");

    // gp("set key outside right top font ',10'");
    gp("set key outside below center vertical maxrows '4' width '-5'");
    gp("set rmargin 14");
    gp(blkGreen.str());
    gp(blkYellow.str());
    gp(blkRed.str());

    gp("plot $AG u 1:2 w l lc rgb '#2ca02c' lw 4 title 'Fixed ambiguity', "
       "     $AY u 1:2 w l lc rgb '#ffbb78' lw 4 title 'Float ambiguity', "
       "     $AR u 1:2 w p pt 2 ps 0.8 lc rgb '#d62728' title 'New arc'");
    LOKI_INFO("PlotGnss: " + out);
}

// =============================================================================
//  plotPppPositionError  (legacy combined — kept for protocol backward compat)
// =============================================================================

void PlotGnss::plotPppPositionError(const std::vector<PppResult>& ppp,
                                     const PppSummary& summary) const
{
    if (!m_cfg.gnss.figures.ppp.positionError || !summary.hasReference || ppp.empty()) return;

    static const loki::math::Ellipsoid wgs84 =
        loki::math::makeEllipsoid(loki::math::EllipsoidModel::WGS84);

    const std::string dateStr = utcDateStr(ppp.front().time);

    std::ostringstream blk;
    blk << "$ERR << EOD\n";
    for (const auto& r : ppp) {
        if (!r.valid) continue;
        const double dX = r.x - summary.refX;
        const double dY = r.y - summary.refY;
        const double dZ = r.z - summary.refZ;
        const double d3 = std::sqrt(dX*dX + dY*dY + dZ*dZ);
        blk << fmt(toSecOfDay(r.time)) << " "
            << fmt(dX) << " " << fmt(dY) << " " << fmt(dZ) << " "
            << fmt(d3) << " " << (r.converged ? 1 : 0) << "\n";
    }
    blk << "EOD";

    const std::string out = fwdSlash(outPath("ppp", "position_error"));
    Gnuplot gp;
    gp(term(1200, 1000));
    gp("set output '" + out + "'");
    gp("set multiplot layout 4,1 title 'PPP position error vs "
       + summary.referenceSource + " (" + dateStr + ")'");
    gp(blk.str());

    const std::vector<std::string> cols   = {"2","3","4","5"};
    const std::vector<std::string> labels = {"dX [m]","dY [m]","dZ [m]","3D error [m]"};
    const std::vector<std::string> colors = {"#1f77b4","#ff7f0e","#2ca02c","#d62728"};

    for (int i = 0; i < 4; ++i) {
        gp(xAxisUtcSetup(i == 3 ? "UTC" : ""));
        gp("set ylabel '" + labels[static_cast<std::size_t>(i)] + "'");
        gp("set grid");
        gp("set key top right");
        gp("plot $ERR u 1:(($6==1)?$" + cols[static_cast<std::size_t>(i)] + ":1/0)"
           " w l lc rgb '" + colors[static_cast<std::size_t>(i)] + "' lw 1.5 title 'converged', "
           "     $ERR u 1:(($6==0)?$" + cols[static_cast<std::size_t>(i)] + ":1/0)"
           " w l lc rgb '#aaaaaa' lw 0.8 title 'pre-conv'");
    }
    gp("unset multiplot");
    LOKI_INFO("PlotGnss: " + out);
}

// =============================================================================
//  plotPppClockBias  (legacy)
// =============================================================================

void PlotGnss::plotPppClockBias(const std::vector<PppResult>& ppp) const
{
    if (!m_cfg.gnss.figures.ppp.clockBias || ppp.empty()) return;

    constexpr double C = 299792458.0;
    const std::string dateStr = utcDateStr(ppp.front().time);
    std::ostringstream blk;
    blk << "$CLK << EOD\n";
    for (const auto& r : ppp) {
        if (!r.valid) continue;
        blk << fmt(toSecOfDay(r.time)) << " "
            << fmt(r.clkBiasM) << " "
            << fmt(r.clkBiasM / C * 1.0e6) << " "
            << (r.converged ? 1 : 0) << "\n";
    }
    blk << "EOD";

    const std::string out = fwdSlash(outPath("ppp", "clockbias"));
    Gnuplot gp;
    gp(term(1200, 700));
    gp("set output '" + out + "'");
    gp("set multiplot layout 2,1 title 'PPP receiver clock bias ("
       + dateStr + ")'");
    gp(blk.str());

    gp(xAxisUtcSetup(""));
    gp("set ylabel 'Clock bias [m]'");
    gp("set grid");
    gp("set key top right");
    gp("plot $CLK u 1:(($4==1)?$2:1/0) w l lc rgb '#1f77b4' lw 1.5 title 'converged', "
       "     $CLK u 1:(($4==0)?$2:1/0) w l lc rgb '#aaaaaa' lw 0.8 title 'pre-conv'");

    gp(xAxisUtcSetup());
    gp("set ylabel 'Clock bias [us]'");
    gp("set grid");
    gp("plot $CLK u 1:(($4==1)?$3:1/0) w l lc rgb '#ff7f0e' lw 1.5 title 'converged', "
       "     $CLK u 1:(($4==0)?$3:1/0) w l lc rgb '#aaaaaa' lw 0.8 title 'pre-conv'");
    gp("unset multiplot");
    LOKI_INFO("PlotGnss: " + out);
}

// =============================================================================
//  plotAll
// =============================================================================

void PlotGnss::plotAll(const GnssResult& result,
                        const NavFile&    nav,
                        const ObsFile&    obs) const
{
    // Observation-level.
    plotSatCount(obs);
    plotElevation(obs, nav);
    plotSkyplot(obs, nav);

    // SPP.
    if (result.hasSpp) {
        plotDop            (result.spp);
        plotClockBias      (result.spp);
        plotResiduals      (result.spp);
        plotIsb            (result.spp);
        plotPositionEcef   (result.spp);
        plotPositionError  (result.spp, result.sppSummary);
        plotPositionScatter(result.spp, result.sppSummary);
    }

    // PPP: diagnostic plots always produced (for debugging).
    if (result.hasPpp) {
        // New diagnostic suite.
        plotPppSky             (obs, nav);
        plotPppCoordDiff       (result.ppp, result.pppSummary);
        plotPppTroposphere     (result.ppp);
        plotPppClock           (result.ppp);
        plotPppSatCount        (result.ppp);
        plotPppResidualsPhase  (result.ppp);
        plotPppResidualsCode   (result.ppp);
        plotPppAmbiguity       (result.ppp);

        // Legacy plots (flag-controlled).
        plotPppPositionError   (result.ppp, result.pppSummary);
        plotPppClockBias       (result.ppp);
    }
}
