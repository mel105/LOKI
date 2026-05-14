#include <loki/gnss/gnssAnalyzer.hpp>
#include <loki/gnss/broadcastOrbit.hpp>
#include <loki/gnss/correctionModel.hpp>
#include <loki/gnss/ionosphere.hpp>
#include <loki/gnss/troposphere.hpp>
#include <loki/gnss/solidTides.hpp>
#include <loki/gnss/sppSolver.hpp>
#include <loki/gnss/gnssProtocol.hpp>
#include <loki/gnss/plotGnss.hpp>
#include <loki/gnss/gnssCsvExport.hpp>
#include <loki/gnss/sp3Parser.hpp>
#include <loki/gnss/clkParser.hpp>
#include <loki/gnss/antexParser.hpp>
#include <loki/gnss/sp3Orbit.hpp>
#include <loki/gnss/pppSolver.hpp>
#include <loki/gnss/troposphere.hpp>
#include <loki/gnss/relativity.hpp>
#include <loki/gnss/obsBiasParser.hpp>
#include <loki/geodesy/coordTransform.hpp>
#include <loki/math/ellipsoid.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <algorithm>
#include <map>
#include <set>
#include <cmath>
#include <memory>
#include <numeric>

using namespace loki;
using namespace loki::gnss;

// =============================================================================
//  Constructor
// =============================================================================

GnssAnalyzer::GnssAnalyzer(const AppConfig& cfg)
    : m_cfg(cfg)
{}

// =============================================================================
//  _buildParseSummary
// =============================================================================

ParseResult GnssAnalyzer::_buildParseSummary(const NavFile& nav,
                                               const ObsFile& obs) const
{
    ParseResult pr;
    pr.nGpsEph      = nav.gpsEph.size();
    pr.nGalEph      = nav.galEph.size();
    pr.nGloEph      = nav.gloEph.size();
    pr.nBdsEph      = nav.bdsEph.size();
    pr.hasKlobuchar = (nav.ionoAlpha[0] != 0.0 || nav.ionoAlpha[1] != 0.0);

    ObsSummary& os  = pr.obs;
    os.station      = obs.receiver.markerName;
    os.receiverType = obs.receiver.receiverType;
    os.antennaType  = obs.receiver.antennaType;
    os.nEpochs      = obs.epochs.size();
    os.intervalSec  = obs.interval;

    // Collect observation codes from the first epoch that has each constellation.
    // ObsFile stores obsCodes per system in the header via the epoch satellite list.
    // We derive them from what satellites actually appear and what obs they carry.
    if (!obs.epochs.empty()) {
        os.spanStartMjd = obs.epochs.front().time.toTimeStamp().mjd();
        os.spanEndMjd   = obs.epochs.back().time.toTimeStamp().mjd();

        std::size_t minS = SIZE_MAX, maxS = 0;
        double sumS = 0.0;

        // Used to collect unique obs codes per constellation char
        std::map<char, std::set<std::string>> codesSeen;

        for (const auto& ep : obs.epochs) {
            const std::size_t n = ep.satellites.size();
            minS  = std::min(minS, n);
            maxS  = std::max(maxS, n);
            sumS += static_cast<double>(n);

            for (const auto& sat : ep.satellites) {
                char sysChar = '?';
                switch (sat.system) {
                    case GnssSystem::GPS:     ++os.nGps;     sysChar = 'G'; break;
                    case GnssSystem::GLONASS: ++os.nGlonass; sysChar = 'R'; break;
                    case GnssSystem::GALILEO: ++os.nGalileo; sysChar = 'E'; break;
                    case GnssSystem::BEIDOU:  ++os.nBeidou;  sysChar = 'C'; break;
                    case GnssSystem::SBAS:    ++os.nSbas;    sysChar = 'S'; break;
                    case GnssSystem::QZSS:    ++os.nQzss;    sysChar = 'J'; break;
                    default: break;
                }
                if (sysChar != '?') {
                    for (const auto& [code, val] : sat.obs)
                        codesSeen[sysChar].insert(code);
                }
            }
        }

        os.minSatsPerEpoch  = minS;
        os.maxSatsPerEpoch  = maxS;
        os.meanSatsPerEpoch = sumS / static_cast<double>(os.nEpochs);

        // Convert sets to sorted vectors
        for (const auto& [ch, codes] : codesSeen)
            os.obsCodes[ch] = std::vector<std::string>(codes.begin(), codes.end());
    }

    return pr;
}

// =============================================================================
//  _runSpp
// =============================================================================

std::vector<SppResult> GnssAnalyzer::_runSpp(const NavFile& nav,
                                               const ObsFile& obs) const
{
    const GnssConfig& gcfg = m_cfg.gnss;

    auto orbit = std::make_unique<BroadcastOrbit>();

    std::vector<std::unique_ptr<CorrectionModel>> corr;
    if (gcfg.corrections.ionosphere == "klobuchar") {
        corr.push_back(std::make_unique<KlobucharModel>(
            nav.ionoAlpha, nav.ionoBeta));
        LOKI_INFO("GnssAnalyzer: ionosphere = Klobuchar");
    }
    if (gcfg.corrections.troposphere == "saastamoinen") {
        corr.push_back(std::make_unique<SaastamoinenModel>());
        LOKI_INFO("GnssAnalyzer: troposphere = Saastamoinen");
    }
    if (gcfg.corrections.solidTides) {
        corr.push_back(std::make_unique<SolidTidesModel>());
        LOKI_INFO("GnssAnalyzer: solid Earth tides = IERS2010 step-1");
    }

    SppSolverConfig solverCfg;
    solverCfg.maxIterations         = gcfg.spp.maxIterations;
    solverCfg.convergenceThresholdM = gcfg.spp.convergenceThresholdM;
    solverCfg.weighting             = gcfg.spp.weighting;
    solverCfg.sagnac                = gcfg.corrections.sagnac;
    solverCfg.elevMaskDeg           = gcfg.elevationMaskDeg;
    solverCfg.constellations        = gcfg.constellations;

    SppSolver solver(solverCfg, std::move(orbit), std::move(corr));

    LOKI_INFO("GnssAnalyzer: running SPP for "
              + std::to_string(obs.epochs.size()) + " epochs...");

    return solver.solveAll(obs, nav, gcfg);
}

// =============================================================================
//  _runPpp
// =============================================================================
std::vector<PppResult> GnssAnalyzer::_runPpp(const NavFile& /*nav*/,
                                               const ObsFile&  obs) const
{
    const GnssPppConfig& pppCfg = m_cfg.gnss.ppp;
 
    LOKI_INFO("PPP: loading SP3: " + pppCfg.sp3File);
    Sp3Parser sp3Parser;
    Sp3File sp3 = sp3Parser.parseGz(pppCfg.sp3File);
 
    LOKI_INFO("PPP: loading CLK: " + pppCfg.clkFile);
    ClkParser clkParser;
    ClkFile clk = clkParser.parseGz(pppCfg.clkFile);
 
    LOKI_INFO("PPP: SP3 epochs: " + std::to_string(sp3.epochs.size())
              + "  CLK records: " + std::to_string(clk.records.size()));
 
    // Load OSB (Observable-Specific Biases) if available.
    OsbFile osb;
    if (!pppCfg.osbFile.empty()) {
        try {
            ObsBiasParser osbParser;
            osb = osbParser.parseGz(pppCfg.osbFile);
            LOKI_INFO("PPP: OSB records: " + std::to_string(osb.records.size()));
        } catch (const LOKIException& ex) {
            LOKI_WARNING("PPP: OSB load failed: " + std::string(ex.what())
                         + " -- proceeding without bias correction.");
        }
    }
 
    auto orbit = std::make_shared<Sp3Orbit>(std::move(sp3), std::move(clk));
 
    PppSolverConfig solverCfg;
    solverCfg.constellations = m_cfg.gnss.constellations;
    solverCfg.elevMaskDeg    = m_cfg.gnss.elevationMaskDeg;
    solverCfg.phaseWindup    = m_cfg.gnss.corrections.phaseWindup;
    solverCfg.pcoPcv         = m_cfg.gnss.corrections.pcoPcv;
    solverCfg.applyOsb       = !osb.records.empty();
 
    PppSolver solver(solverCfg, orbit, std::move(osb));
    return solver.solveAll(obs, {}, m_cfg.gnss);
}
 
PppSummary GnssAnalyzer::_computePppSummary(
    const std::vector<PppResult>& results) const
{
    PppSummary s;
    s.nEpochsTotal = results.size();
 
    // Reference position.
    const bool hasRef = m_cfg.gnss.referencePosition.enabled;
    if (hasRef) {
        s.hasReference  = true;
        s.refX          = m_cfg.gnss.referencePosition.x;
        s.refY          = m_cfg.gnss.referencePosition.y;
        s.refZ          = m_cfg.gnss.referencePosition.z;
        s.referenceSource = m_cfg.gnss.referencePosition.source;
    }
 
    // Collect post-convergence epochs.
    std::vector<double> xs, ys, zs, clks, ztds, nsats;
    std::vector<double> errors3d;
    double firstConvSec = -1.0;
 
    for (const auto& r : results) {
        if (!r.valid) continue;
        ++s.nEpochsValid;
        if (r.converged) {
            ++s.nEpochsConverged;
            if (firstConvSec < 0.0)
                firstConvSec = r.time.totalSeconds();
            xs.push_back(r.x);
            ys.push_back(r.y);
            zs.push_back(r.z);
            clks.push_back(r.clkBiasM);
            ztds.push_back(r.ztdWetM);
            nsats.push_back(static_cast<double>(r.nSats));
 
            if (hasRef) {
                const double ex = r.x - s.refX;
                const double ey = r.y - s.refY;
                const double ez = r.z - s.refZ;
                errors3d.push_back(std::sqrt(ex*ex + ey*ey + ez*ez));
            }
        }
    }
 
    if (!results.empty() && firstConvSec > 0.0) {
        const double startSec = results.front().time.totalSeconds();
        s.convergenceTimeMin = (firstConvSec - startSec) / 60.0;
    }
 
    auto mean = [](const std::vector<double>& v) {
        if (v.empty()) return 0.0;
        double sum = 0.0;
        for (double x : v) sum += x;
        return sum / static_cast<double>(v.size());
    };
    auto stddev = [&mean](const std::vector<double>& v) {
        if (v.size() < 2) return 0.0;
        const double m = mean(v);
        double ss = 0.0;
        for (double x : v) ss += (x-m)*(x-m);
        return std::sqrt(ss / static_cast<double>(v.size() - 1));
    };
    auto rms = [](const std::vector<double>& v) {
        if (v.empty()) return 0.0;
        double ss = 0.0;
        for (double x : v) ss += x*x;
        return std::sqrt(ss / static_cast<double>(v.size()));
    };
 
    s.meanX        = mean(xs);
    s.meanY        = mean(ys);
    s.meanZ        = mean(zs);
    s.stdX         = stddev(xs);
    s.stdY         = stddev(ys);
    s.stdZ         = stddev(zs);
    s.meanClkBiasM = mean(clks);
    s.meanZtdWetM  = mean(ztds);
    s.meanNSats    = mean(nsats);
 
    if (hasRef && !errors3d.empty()) {
        s.mean3dErrorM = mean(errors3d);
        s.std3dErrorM  = stddev(errors3d);
        s.rmsErrorM    = rms(errors3d);
    }
 
    // Geodetic mean (approximate: convert mean ECEF).
    // (Proper conversion uses ecef2geod -- left to caller if needed.)
 
    return s;
}

// =============================================================================
//  _computeSppSummary
// =============================================================================

SppSummary GnssAnalyzer::_computeSppSummary(
    const std::vector<SppResult>& results) const
{
    SppSummary s;
    s.nEpochsTotal = results.size();

    static const loki::math::Ellipsoid wgs84 =
        loki::math::makeEllipsoid(loki::math::EllipsoidModel::WGS84);

    std::vector<double> xs, ys, zs, errs;

    for (const auto& r : results) {
        if (!r.valid) continue;
        ++s.nEpochsValid;
        if (r.converged) ++s.nEpochsConverged;
        xs.push_back(r.x);
        ys.push_back(r.y);
        zs.push_back(r.z);
        s.meanClkBiasM += r.clkBiasM;
        s.meanPdop     += r.pdop;
        s.meanNSats    += static_cast<double>(r.nSats);
    }

    if (s.nEpochsValid == 0) return s;

    const double n = static_cast<double>(s.nEpochsValid);

    auto mean = [&](const std::vector<double>& v) {
        return std::accumulate(v.begin(), v.end(), 0.0) / n;
    };
    auto stddev = [&](const std::vector<double>& v, double m) {
        double acc = 0.0;
        for (double x : v) acc += (x - m) * (x - m);
        return std::sqrt(acc / n);
    };

    s.meanX = mean(xs); s.meanY = mean(ys); s.meanZ = mean(zs);
    s.stdX  = stddev(xs, s.meanX);
    s.stdY  = stddev(ys, s.meanY);
    s.stdZ  = stddev(zs, s.meanZ);
    s.meanClkBiasM /= n;
    s.meanPdop     /= n;
    s.meanNSats    /= n;

    // Mean geodetic position from mean ECEF
    {
        const loki::geodesy::EcefPoint ep{s.meanX, s.meanY, s.meanZ};
        const loki::geodesy::GeodPoint gp = loki::geodesy::ecef2geod(ep, wgs84);
        s.meanLat = gp.lat;
        s.meanLon = gp.lon;
        s.meanH   = gp.h;
    }

    const GnssConfig& gcfg = m_cfg.gnss;
    if (gcfg.referencePosition.enabled) {
        s.hasReference    = true;
        s.refX            = gcfg.referencePosition.x;
        s.refY            = gcfg.referencePosition.y;
        s.refZ            = gcfg.referencePosition.z;
        s.referenceSource = gcfg.referencePosition.source;

        // Reference geodetic
        {
            const loki::geodesy::EcefPoint ep{s.refX, s.refY, s.refZ};
            const loki::geodesy::GeodPoint gp = loki::geodesy::ecef2geod(ep, wgs84);
            s.refLat = gp.lat;
            s.refLon = gp.lon;
            s.refH   = gp.h;
        }

        double sumErr2 = 0.0;
        for (const auto& r : results) {
            if (!r.valid) continue;
            const double dX = r.x - s.refX;
            const double dY = r.y - s.refY;
            const double dZ = r.z - s.refZ;
            const double e  = std::sqrt(dX*dX + dY*dY + dZ*dZ);
            errs.push_back(e);
            sumErr2 += e * e;
        }

        s.mean3dErrorM = mean(errs);
        s.std3dErrorM  = stddev(errs, s.mean3dErrorM);
        s.rmsErrorM    = std::sqrt(sumErr2 / n);
    }

    return s;
}

// =============================================================================
//  run
// =============================================================================

GnssResult GnssAnalyzer::run(const NavFile& nav, const ObsFile& obs) const
{
    GnssResult result;
    result.parse = _buildParseSummary(nav, obs);

    if (m_cfg.gnss.spp.enabled) {
        result.spp        = _runSpp(nav, obs);
        result.sppSummary = _computeSppSummary(result.spp);
        result.hasSpp     = true;
    }

    if (m_cfg.gnss.ppp.enabled) {
        result.ppp        = _runPpp(nav, obs);
        result.pppSummary = _computePppSummary(result.ppp);
        result.hasPpp     = true;
    }

    GnssProtocol protocol(m_cfg);
    protocol.write(result);

    PlotGnss plotter(m_cfg, m_cfg.gnss.station);
    plotter.plotAll(result, nav, obs);

    GnssCsvExport exporter(m_cfg);
    exporter.exportAll(result, obs);

    return result;
}
