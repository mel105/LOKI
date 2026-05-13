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
    LOKI_INFO("GnssAnalyzer: parse summary built -- "
              + std::to_string(obs.epochs.size()) + " epochs, station "
              + result.parse.obs.station);

    const GnssConfig& gcfg = m_cfg.gnss;

    if (gcfg.task == "spp" || gcfg.spp.enabled) {
        result.spp        = _runSpp(nav, obs);
        result.sppSummary = _computeSppSummary(result.spp);
        result.hasSpp     = true;
        LOKI_INFO("GnssAnalyzer: SPP done -- "
                  + std::to_string(result.sppSummary.nEpochsValid)
                  + " / " + std::to_string(result.sppSummary.nEpochsTotal)
                  + " valid epochs");
    }

    GnssProtocol proto(m_cfg);
    proto.write(result);

    GnssCsvExport csvExport(m_cfg);
    csvExport.exportAll(result, obs);

    PlotGnss plotter(m_cfg, m_cfg.gnss.station);
    plotter.plotAll(result, nav, obs);

    return result;
}
