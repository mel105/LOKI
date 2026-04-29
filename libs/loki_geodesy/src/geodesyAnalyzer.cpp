#include <loki/geodesy/geodesyAnalyzer.hpp>
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
// sysName helper
// ---------------------------------------------------------------------------

std::string GeodesyAnalyzer::sysName(InputCoordSystem s) noexcept
{
    switch (s) {
        case InputCoordSystem::ECEF:   return "ECEF";
        case InputCoordSystem::GEOD:   return "GEOD";
        case InputCoordSystem::SPHERE: return "SPHERE";
        case InputCoordSystem::ENU:    return "ENU";
    }
    return "UNKNOWN";
}

// ---------------------------------------------------------------------------
// Constructor -- resolve strings to enums once
// ---------------------------------------------------------------------------

GeodesyAnalyzer::GeodesyAnalyzer(const GeodesyConfig& cfg)
    : m_cfg(cfg)
    , m_ell(makeEllipsoid(cfg.ellipsoidModel))
    , m_inputSystem (inputCoordSystemFromString(cfg.inputSystem))
    , m_outputSystem(inputCoordSystemFromString(cfg.outputSystem))
    , m_refBody     (cfg.refBody == "sphere" ? RefBody::SPHERE : RefBody::ELLIPSOID)
{}

// ---------------------------------------------------------------------------
// transformPoint
// ---------------------------------------------------------------------------

TransformResult GeodesyAnalyzer::transformPoint(const Eigen::VectorXd& pos,
                                                  const Eigen::MatrixXd& cov) const
{
    TransformResult res;
    res.inputPos      = pos;
    res.inputCov      = cov;
    res.ellipsoidName = m_cfg.ellipsoidName;
    res.inputSystem   = sysName(m_inputSystem);
    res.outputSystem  = sysName(m_outputSystem);

    if (m_inputSystem == m_outputSystem) {
        res.outputPos = pos;
        res.outputCov = cov;
        return res;
    }

    Eigen::VectorXd outPos;
    Eigen::MatrixXd outCov;
    auto in  = m_inputSystem;
    auto out = m_outputSystem;

    if (in == InputCoordSystem::ECEF) {
        if      (out == InputCoordSystem::GEOD)
            trEcef2Geod(pos, cov, m_ell, outPos, outCov);
        else if (out == InputCoordSystem::ENU)
            trEcef2Enu(pos, cov, m_cfg.enuOrigin, m_refBody, m_ell, outPos, outCov);
        else if (out == InputCoordSystem::SPHERE)
            trEcef2Sphere(pos, cov, outPos, outCov);
    } else if (in == InputCoordSystem::GEOD) {
        if      (out == InputCoordSystem::ECEF)
            trGeod2Ecef(pos, cov, m_ell, outPos, outCov);
        else if (out == InputCoordSystem::ENU)
            trGeod2Enu(pos, cov, m_cfg.enuOrigin, m_ell, outPos, outCov);
        else if (out == InputCoordSystem::SPHERE)
            trGeod2Sphere(pos, cov, m_ell, outPos, outCov);
    } else if (in == InputCoordSystem::ENU) {
        if      (out == InputCoordSystem::ECEF)
            trEnu2Ecef(pos, cov, m_cfg.enuOrigin, m_refBody, m_ell, outPos, outCov);
        else if (out == InputCoordSystem::GEOD)
            trEnu2Geod(pos, cov, m_cfg.enuOrigin, m_ell, outPos, outCov);
        else if (out == InputCoordSystem::SPHERE)
            trEnu2Sphere(pos, cov, m_cfg.enuOrigin, m_ell, outPos, outCov);
    } else if (in == InputCoordSystem::SPHERE) {
        if      (out == InputCoordSystem::ECEF)
            trSphere2Ecef(pos, cov, outPos, outCov);
        else if (out == InputCoordSystem::GEOD)
            trSphere2Geod(pos, cov, m_ell, outPos, outCov);
        else if (out == InputCoordSystem::ENU)
            trSphere2Enu(pos, cov, m_cfg.enuOrigin, m_ell, outPos, outCov);
    }

    res.outputPos = outPos;
    res.outputCov = outCov;
    return res;
}

// ---------------------------------------------------------------------------
// monteCarlo
// ---------------------------------------------------------------------------

MonteCarloResult GeodesyAnalyzer::monteCarlo(const Eigen::VectorXd& pos,
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

    TransformResult analytical = transformPoint(pos, cov);

    Eigen::MatrixXd zeroCov = Eigen::MatrixXd::Zero(N, N);
    std::mt19937_64            rng(42);
    std::normal_distribution<> nd(0.0, 1.0);

    Eigen::MatrixXd samples(N, m_cfg.mcSamples);
    for (int s = 0; s < m_cfg.mcSamples; ++s) {
        Eigen::VectorXd z(N);
        for (int i = 0; i < N; ++i) z(i) = nd(rng);
        TransformResult tr = transformPoint(pos + L * z, zeroCov);
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
// computeDistances
// ---------------------------------------------------------------------------

std::vector<GeodLineResult>
GeodesyAnalyzer::computeDistances(const loki::io::GeodesyLoadResult& ds) const
{
    std::vector<GeodLineResult> results;
    if (ds.positions.size() < 2) return results;

    auto toGeod = [&](const Eigen::VectorXd& p) -> GeodPoint {
        switch (m_inputSystem) {
            case InputCoordSystem::GEOD:
                return { p(0), p(1), p(2) };
            case InputCoordSystem::ECEF:
                return ecef2geod({ p(0), p(1), p(2) }, m_ell);
            case InputCoordSystem::SPHERE:
                return sphere2geod({ p(0), p(1), p(2) }, m_ell);
            case InputCoordSystem::ENU:
                return enu2geod({ p(0), p(1), p(2) }, m_cfg.enuOrigin, m_ell);
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
// writeProtocol
// ---------------------------------------------------------------------------

void GeodesyAnalyzer::writeProtocol(const GeodesyResult& result) const
{
    namespace fs = std::filesystem;
    fs::create_directories(m_cfg.outputDir + "PROTOCOLS/");
    std::string path = m_cfg.outputDir + "PROTOCOLS/geodesy_protocol.csv";

    std::ofstream f(path);
    if (!f.is_open())
        throw loki::IoException(
            "GeodesyAnalyzer: cannot write protocol to " + path);

    f << std::fixed << std::setprecision(9);
    f << "# loki_geodesy protocol\n";
    f << "# Ellipsoid;" << m_cfg.ellipsoidName << "\n";
    f << "# Input;"     << sysName(m_inputSystem)  << "\n";
    f << "# Output;"    << sysName(m_outputSystem) << "\n";

    f << "idx;in0;in1;in2;out0;out1;out2\n";
    for (std::size_t i = 0; i < result.transforms.size(); ++i) {
        const auto& tr = result.transforms[i];
        f << i << ";"
          << tr.inputPos(0)  << ";" << tr.inputPos(1)  << ";" << tr.inputPos(2)  << ";"
          << tr.outputPos(0) << ";" << tr.outputPos(1) << ";" << tr.outputPos(2) << "\n";
    }

    if (result.hasLines) {
        f << "# distances\nidx;distance_m;azFwd_deg;azRev_deg;method\n";
        for (std::size_t i = 0; i < result.lines.size(); ++i) {
            const auto& lr = result.lines[i];
            f << i << ";"
              << lr.geodesic.distance    << ";"
              << lr.geodesic.azimuthFwd  << ";"
              << lr.geodesic.azimuthRev  << ";"
              << lr.method << "\n";
        }
    }

    if (result.hasMonteCarlo) {
        const auto& mc = result.monteCarlo;
        f << "# monte_carlo\nsamples;" << mc.nSamples << "\n";
        f << "maxRelDiff;" << mc.maxRelDiff << "\n";
        f << "passed;"     << (mc.passed ? "YES" : "NO") << "\n";
        int N = static_cast<int>(mc.analyticalCov.rows());
        for (int i = 0; i < N; ++i)
            f << "ana_cov_" << i << ";" << mc.analyticalCov(i, i) << "\n";
        for (int i = 0; i < N; ++i)
            f << "emp_cov_" << i << ";" << mc.empiricalCov(i, i) << "\n";
    }

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
        result.transforms.push_back(transformPoint(loadResult.positions[i], cov));
    }

    if (m_cfg.task == GeodesyTask::MONTE_CARLO && !loadResult.positions.empty()) {
        const Eigen::MatrixXd& cov = loadResult.covariances.empty()
                                     ? Eigen::MatrixXd::Identity(N, N)
                                     : loadResult.covariances[0];
        result.monteCarlo    = monteCarlo(loadResult.positions[0], cov);
        result.hasMonteCarlo = true;
        if (!result.transforms.empty())
            result.transforms[0].empiricalCov = result.monteCarlo.empiricalCov;
    }

    if (m_cfg.task == GeodesyTask::DISTANCE) {
        result.lines    = computeDistances(loadResult);
        result.hasLines = !result.lines.empty();
    }

    writeProtocol(result);
    return result;
}
