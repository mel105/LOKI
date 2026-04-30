#include <loki/geodesy/geodesyAnalyzer.hpp>
#include <loki/geodesy/coordTransform.hpp>
#include <loki/geodesy/geodesicLine.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <Eigen/Dense>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <numbers>
#include <random>
#include <string>

using namespace loki::geodesy;
using namespace loki::math;

// ---------------------------------------------------------------------------
// String parsers
// ---------------------------------------------------------------------------

GeodesyTask loki::geodesy::geodesyTaskFromString(const std::string& s)
{
    if (s == "transform")   return GeodesyTask::TRANSFORM;
    if (s == "distance")    return GeodesyTask::DISTANCE;
    if (s == "monte_carlo") return GeodesyTask::MONTE_CARLO;
    throw loki::ConfigException("Unknown geodesy task: " + s);
}

DistanceMethod loki::geodesy::distanceMethodFromString(const std::string& s)
{
    if (s == "vincenty")  return DistanceMethod::VINCENTY;
    if (s == "haversine") return DistanceMethod::HAVERSINE;
    throw loki::ConfigException("Unknown distance method: " + s);
}

// ---------------------------------------------------------------------------
// Local helpers (file-scope only)
// ---------------------------------------------------------------------------

namespace {

std::string sysName(InputCoordSystem s) noexcept
{
    switch (s) {
        case InputCoordSystem::ECEF:   return "ECEF";
        case InputCoordSystem::GEOD:   return "GEOD";
        case InputCoordSystem::SPHERE: return "SPHERE";
        case InputCoordSystem::ENU:    return "ENU";
    }
    return "UNKNOWN";
}

// Build GeodPoint from the three separate origin doubles in GeodesyConfig
GeodPoint originFromCfg(const GeodesyConfig& cfg) noexcept
{
    return { cfg.enuOriginLat, cfg.enuOriginLon, cfg.enuOriginH };
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

GeodesyAnalyzer::GeodesyAnalyzer(const GeodesyConfig& cfg)
    : m_cfg(cfg)
    , m_ell(makeEllipsoid(cfg.ellipsoidModel))
{}

// ---------------------------------------------------------------------------
// _transformPoint
// ---------------------------------------------------------------------------

TransformResult GeodesyAnalyzer::_transformPoint(const Eigen::VectorXd& pos,
                                                   const Eigen::MatrixXd& cov) const
{
    InputCoordSystem in  = inputCoordSystemFromString(m_cfg.inputSystem);
    InputCoordSystem out = inputCoordSystemFromString(m_cfg.outputSystem);
    RefBody          ref = (m_cfg.refBody == "sphere")
                           ? RefBody::SPHERE : RefBody::ELLIPSOID;
    GeodPoint        org = originFromCfg(m_cfg);

    TransformResult res;
    res.inputPos      = pos;
    res.inputCov      = cov;
    res.ellipsoidName = m_cfg.ellipsoidName;
    res.inputSystem   = sysName(in);
    res.outputSystem  = sysName(out);

    if (in == out) {
        res.outputPos = pos;
        res.outputCov = cov;
        return res;
    }

    Eigen::VectorXd outPos;
    Eigen::MatrixXd outCov;

    if (in == InputCoordSystem::ECEF) {
        if      (out == InputCoordSystem::GEOD)
            trEcef2Geod  (pos, cov, m_ell,      outPos, outCov);
        else if (out == InputCoordSystem::ENU)
            trEcef2Enu   (pos, cov, org, ref, m_ell, outPos, outCov);
        else if (out == InputCoordSystem::SPHERE)
            trEcef2Sphere(pos, cov,             outPos, outCov);
    } else if (in == InputCoordSystem::GEOD) {
        if      (out == InputCoordSystem::ECEF)
            trGeod2Ecef  (pos, cov, m_ell,      outPos, outCov);
        else if (out == InputCoordSystem::ENU)
            trGeod2Enu   (pos, cov, org, m_ell, outPos, outCov);
        else if (out == InputCoordSystem::SPHERE)
            trGeod2Sphere(pos, cov, m_ell,      outPos, outCov);
    } else if (in == InputCoordSystem::ENU) {
        if      (out == InputCoordSystem::ECEF)
            trEnu2Ecef   (pos, cov, org, ref, m_ell, outPos, outCov);
        else if (out == InputCoordSystem::GEOD)
            trEnu2Geod   (pos, cov, org, m_ell, outPos, outCov);
        else if (out == InputCoordSystem::SPHERE)
            trEnu2Sphere (pos, cov, org, m_ell, outPos, outCov);
    } else if (in == InputCoordSystem::SPHERE) {
        if      (out == InputCoordSystem::ECEF)
            trSphere2Ecef(pos, cov,             outPos, outCov);
        else if (out == InputCoordSystem::GEOD)
            trSphere2Geod(pos, cov, m_ell,      outPos, outCov);
        else if (out == InputCoordSystem::ENU)
            trSphere2Enu (pos, cov, org, m_ell, outPos, outCov);
    }

    res.outputPos = outPos;
    res.outputCov = outCov;
    return res;
}

// ---------------------------------------------------------------------------
// _monteCarlo
// ---------------------------------------------------------------------------

MonteCarloResult GeodesyAnalyzer::_monteCarlo(const Eigen::VectorXd& pos,
                                                const Eigen::MatrixXd& cov) const
{
    int N = static_cast<int>(pos.size());

    Eigen::MatrixXd L;
    Eigen::LLT<Eigen::MatrixXd> llt(cov);
    if (llt.info() == Eigen::Success) {
        L = llt.matrixL();
    } else {
        LOKI_WARNING("GeodesyAnalyzer: covariance not positive definite -- "
                     "using identity for Monte Carlo sampling");
        L = Eigen::MatrixXd::Identity(N, N);
    }

    TransformResult analytical = _transformPoint(pos, cov);

    Eigen::MatrixXd zeroCov = Eigen::MatrixXd::Zero(N, N);
    std::mt19937_64            rng(42);
    std::normal_distribution<> nd(0.0, 1.0);

    Eigen::MatrixXd samples(N, m_cfg.mcSamples);
    for (int s = 0; s < m_cfg.mcSamples; ++s) {
        Eigen::VectorXd z(N);
        for (int i = 0; i < N; ++i) z(i) = nd(rng);
        TransformResult tr = _transformPoint(pos + L * z, zeroCov);
        samples.col(s) = tr.outputPos;
    }

    Eigen::VectorXd mean    = samples.rowwise().mean();
    Eigen::MatrixXd centred = samples.colwise() - mean;
    Eigen::MatrixXd empCov  = (centred * centred.transpose())
                               / static_cast<double>(m_cfg.mcSamples - 1);

    double maxRel = 0.0;
    for (int i = 0; i < N; ++i) {
        double an = analytical.outputCov(i, i);
        double em = empCov(i, i);
        if (std::fabs(an) > 1e-30)
            maxRel = std::max(maxRel, std::fabs(an - em) / std::fabs(an));
    }

    MonteCarloResult mc;
    mc.analyticalCov = analytical.outputCov;
    mc.empiricalCov  = empCov;
    mc.empiricalMean = mean;
    mc.samples       = samples;
    mc.nSamples      = m_cfg.mcSamples;
    mc.maxRelDiff    = maxRel;
    mc.passed        = (maxRel < m_cfg.mcTolerance);

    if (mc.passed)
        LOKI_INFO("GeodesyAnalyzer: Monte Carlo passed (maxRelDiff="
                  + std::to_string(maxRel) + ")");
    else
        LOKI_WARNING("GeodesyAnalyzer: Monte Carlo FAILED (maxRelDiff="
                     + std::to_string(maxRel) + " >= tol="
                     + std::to_string(m_cfg.mcTolerance) + ")");

    return mc;
}

// ---------------------------------------------------------------------------
// _computeDistances
// ---------------------------------------------------------------------------

std::vector<GeodLineResult>
GeodesyAnalyzer::_computeDistances(const loki::io::GeodesyLoadResult& ds) const
{
    std::vector<GeodLineResult> results;
    if (ds.positions.size() < 2) return results;

    InputCoordSystem in  = inputCoordSystemFromString(m_cfg.inputSystem);
    GeodPoint        org = originFromCfg(m_cfg);

    auto toGeod = [&](const Eigen::VectorXd& p) -> GeodPoint {
        switch (in) {
            case InputCoordSystem::GEOD:
                return { p(0), p(1), p(2) };
            case InputCoordSystem::ECEF:
                return ecef2geod({ p(0), p(1), p(2) }, m_ell);
            case InputCoordSystem::SPHERE:
                return sphere2geod({ p(0), p(1), p(2) }, m_ell);
            case InputCoordSystem::ENU:
                return enu2geod({ p(0), p(1), p(2) }, org, m_ell);
        }
        return {};
    };

    for (std::size_t i = 0; i + 1 < ds.positions.size(); ++i) {
        GeodPoint gp1 = toGeod(ds.positions[i]);
        GeodPoint gp2 = toGeod(ds.positions[i + 1]);

        GeodLineResult lr;
        lr.ellipsoidName = m_cfg.ellipsoidName;
        if (m_cfg.distMethod == DistanceMethod::VINCENTY) {
            lr.geodesic = vincentyInverse(gp1.lat, gp1.lon,
                                          gp2.lat, gp2.lon, m_ell);
            lr.method = "vincenty";
        } else {
            lr.geodesic = haversine(gp1.lat, gp1.lon, gp2.lat, gp2.lon);
            lr.method = "haversine";
        }
        results.push_back(lr);
    }
    return results;
}

// ---------------------------------------------------------------------------
// _writeProtocol
// ---------------------------------------------------------------------------

void GeodesyAnalyzer::_writeProtocol(const GeodesyResult& result) const
{
    namespace fs = std::filesystem;
    fs::create_directories(m_cfg.protocolDir);
    std::string path = m_cfg.protocolDir + "/geodesy_protocol.txt";

    std::ofstream f(path);
    if (!f.is_open())
        throw loki::IoException(
            "GeodesyAnalyzer: cannot write protocol to " + path);

    InputCoordSystem in  = inputCoordSystemFromString(m_cfg.inputSystem);
    InputCoordSystem out = inputCoordSystemFromString(m_cfg.outputSystem);

    f << std::fixed << std::setprecision(9);
    f << "================================================================================\n";
    f << "  LOKI GEODESY PROTOCOL\n";
    f << "================================================================================\n";
    f << "  Ellipsoid    : " << m_cfg.ellipsoidName << "\n";
    f << "  Input system : " << sysName(in)  << "\n";
    f << "  Output system: " << sysName(out) << "\n";
    f << "  Points       : " << result.transforms.size() << "\n";
    f << "--------------------------------------------------------------------------------\n";
    f << "\n";

    f << "COORDINATE TRANSFORMATIONS\n";
    f << "\n";
    f << "  " << std::left
      << std::setw(5)  << "idx"
      << std::setw(20) << ("in_0 [" + sysName(in) + "]")
      << std::setw(20) << "in_1"
      << std::setw(20) << "in_2"
      << std::setw(20) << ("out_0 [" + sysName(out) + "]")
      << std::setw(20) << "out_1"
      << std::setw(20) << "out_2"
      << "\n";
    f << "  " << std::string(125, '-') << "\n";

    for (std::size_t i = 0; i < result.transforms.size(); ++i) {
        const auto& tr = result.transforms[i];
        f << "  " << std::left << std::setw(5) << i
          << std::setw(20) << tr.inputPos(0)
          << std::setw(20) << tr.inputPos(1)
          << std::setw(20) << tr.inputPos(2)
          << std::setw(20) << tr.outputPos(0)
          << std::setw(20) << tr.outputPos(1)
          << std::setw(20) << tr.outputPos(2)
          << "\n";

        // Print input covariance if available
        bool hasCov = (tr.inputCov.rows() > 0 && tr.inputCov.norm() > 1e-30);
        bool hasOutCov = (tr.outputCov.rows() > 0 && tr.outputCov.norm() > 1e-30);
        if (hasCov || hasOutCov) {
            int Nc = static_cast<int>(tr.inputPos.size());
            f << "       Input  sigma  : ";
            for (int c = 0; c < Nc; ++c) {
                double var = hasCov ? tr.inputCov(c, c) : 0.0;
                f << std::scientific << std::setprecision(4)
                  << std::sqrt(var);
                if (c < Nc - 1) f << "  ";
            }
            f << "\n";
            f << "       Output sigma  : ";
            for (int c = 0; c < Nc; ++c) {
                double var = hasOutCov ? tr.outputCov(c, c) : 0.0;
                f << std::scientific << std::setprecision(4)
                  << std::sqrt(var);
                if (c < Nc - 1) f << "  ";
            }
            f << "\n";
            f << "       Input  cov    : [";
            for (int r = 0; r < Nc; ++r)
                for (int c = 0; c < Nc; ++c) {
                    f << std::scientific << std::setprecision(4)
                      << (hasCov ? tr.inputCov(r,c) : 0.0);
                    if (r < Nc-1 || c < Nc-1) f << "  ";
                }
            f << "]\n";
            f << "       Output cov    : [";
            for (int r = 0; r < Nc; ++r)
                for (int c = 0; c < Nc; ++c) {
                    f << std::scientific << std::setprecision(4)
                      << (hasOutCov ? tr.outputCov(r,c) : 0.0);
                    if (r < Nc-1 || c < Nc-1) f << "  ";
                }
            f << "]\n";
            f << std::fixed << std::setprecision(9);
        }
    }

    if (result.hasLines) {
        f << "\n";
        f << "GEODESIC DISTANCES\n";
        f << "\n";
        f << "  " << std::left
          << std::setw(5)  << "idx"
          << std::setw(18) << "distance [m]"
          << std::setw(18) << "az_fwd [deg]"
          << std::setw(18) << "az_rev [deg]"
          << std::setw(12) << "method"
          << "\n";
        f << "  " << std::string(71, '-') << "\n";
        for (std::size_t i = 0; i < result.lines.size(); ++i) {
            const auto& lr = result.lines[i];
            f << "  " << std::left << std::setw(5) << i
              << std::setw(18) << lr.geodesic.distance
              << std::setw(18) << lr.geodesic.azimuthFwd
              << std::setw(18) << lr.geodesic.azimuthRev
              << std::setw(12) << lr.method
              << "\n";
        }
    }

    if (result.hasMonteCarlo) {
        const auto& mc = result.monteCarlo;
        f << "\n";
        f << "MONTE CARLO VALIDATION\n";
        f << "\n";
        f << "  Samples      : " << mc.nSamples   << "\n";
        f << "  Max rel diff : " << mc.maxRelDiff  << "\n";
        f << "  Result       : " << (mc.passed ? "PASSED" : "FAILED") << "\n";
        f << "\n";
        int Nmc = static_cast<int>(mc.analyticalCov.rows());
        f << "  " << std::left << std::setw(5) << "comp"
          << std::setw(22) << "analytical_var"
          << std::setw(22) << "empirical_var"
          << std::setw(14) << "rel_diff"
          << "\n";
        f << "  " << std::string(63, '-') << "\n";
        for (int i = 0; i < Nmc; ++i) {
            double an = mc.analyticalCov(i, i);
            double em = mc.empiricalCov(i, i);
            double rd = (std::fabs(an) > 1e-30) ? std::fabs(an - em) / std::fabs(an) : 0.0;
            f << "  " << std::left << std::setw(5) << i
              << std::setw(22) << an
              << std::setw(22) << em
              << std::setw(14) << rd
              << "\n";
        }
    }

    f << "\n";
    f << "================================================================================\n";

    LOKI_INFO("GeodesyAnalyzer: protocol written to " + path);
}

// ---------------------------------------------------------------------------
// run()
// ---------------------------------------------------------------------------

GeodesyResult GeodesyAnalyzer::run(const loki::io::GeodesyLoadResult& loadResult) const
{
    LOKI_INFO("GeodesyAnalyzer: " + std::to_string(loadResult.positions.size())
              + " points, task=" + std::to_string(static_cast<int>(m_cfg.task)));

    GeodesyResult result;
    int N = m_cfg.stateSize;
    Eigen::MatrixXd zeroCov = Eigen::MatrixXd::Zero(N, N);

    for (std::size_t i = 0; i < loadResult.positions.size(); ++i) {
        const Eigen::MatrixXd& cov = loadResult.covariances.empty()
                                     ? zeroCov
                                     : loadResult.covariances[i];
        TransformResult tr = _transformPoint(loadResult.positions[i], cov);

        if (m_cfg.task == GeodesyTask::MONTE_CARLO) {
            const Eigen::MatrixXd& mcCov = loadResult.covariances.empty()
                                           ? Eigen::MatrixXd::Identity(N, N)
                                           : loadResult.covariances[i];
            MonteCarloResult mc = _monteCarlo(loadResult.positions[i], mcCov);
            tr.empiricalCov  = mc.empiricalCov;
            tr.empiricalMean = mc.empiricalMean;
            tr.mcSamples     = mc.samples;

            // Store first point MC in top-level for protocol
            if (i == 0) {
                result.monteCarlo    = mc;
                result.hasMonteCarlo = true;
            }
        }

        result.transforms.push_back(std::move(tr));
    }

    if (m_cfg.task == GeodesyTask::DISTANCE) {
        result.lines    = _computeDistances(loadResult);
        result.hasLines = !result.lines.empty();
    }

    _writeProtocol(result);
    return result;
}
