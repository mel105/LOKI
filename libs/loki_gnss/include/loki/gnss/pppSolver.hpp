#pragma once

#include <loki/gnss/gnssTypes.hpp>
#include <loki/gnss/sp3Orbit.hpp>
#include <loki/gnss/pppFilter.hpp>
#include <loki/gnss/phaseWindup.hpp>
#include <loki/gnss/correctionModel.hpp>
#include <loki/gnss/obsBiasParser.hpp>
#include <loki/core/config.hpp>

#include <memory>
#include <vector>

namespace loki::gnss {

// =============================================================================
//  PppResult
// =============================================================================

struct PppResult {
    GpsTime time;

    // Position [m] ECEF.
    double  x{0.0}, y{0.0}, z{0.0};

    // Formal position sigmas [m] from Kalman covariance diagonal.
    double  sigmaXm{0.0}, sigmaYm{0.0}, sigmaZm{0.0};

    // Receiver clock [m].
    double  clkBiasM{0.0};
    double  sigmaClkM{0.0};

    // Troposphere [m].
    double  ztdWetM{0.0};
    double  ztdTotalM{0.0};
    double  sigmaZtdM{0.0};

    double  isbGalM{0.0};
    double  pdop{0.0};
    int     nSats{0};
    bool    valid{false};
    bool    converged{false};

    // Per-satellite post-fit residuals for this epoch.
    std::vector<PppSatResidual> satResiduals;
};

// =============================================================================
//  PppSolverConfig
// =============================================================================

struct PppSolverConfig {
    std::vector<std::string> constellations{"GPS"};
    double elevMaskDeg{10.0};
    bool   phaseWindup{true};
    bool   pcoPcv{false};
    bool   applyOsb{true};

    PppFilterConfig filter{};

    explicit PppSolverConfig() = default;
};

// =============================================================================
//  PppSolver
// =============================================================================

/**
 * @brief Precise Point Positioning solver (ionosphere-free combination).
 *
 * Corrections applied per observation:
 *   - Satellite clock from RINEX CLK (AS records, O(log n) lookup).
 *   - Ionosphere-free combination eliminates first-order iono.
 *   - Troposphere: ZHD fixed (Saastamoinen + NMF); ZWD estimated by filter.
 *   - Satellite PCO: SP3 orbits are to Centre of Mass (CoM); CLK products
 *     are generated consistent with the Antenna Phase Centre (APC).
 *   - Receiver antenna height offset (antDeltaH from OBS header) applied
 *     to translate from marker to ARP before processing.
 *   - Phase windup (carrier phase only, lambda = c/(f1+f2)).
 *   - OSB code bias alignment to CLK reference signals.
 */
class PppSolver {
public:
    using Config = PppSolverConfig;

    explicit PppSolver(Config                                         cfg,
                       std::shared_ptr<Sp3Orbit>                      orbit,
                       OsbFile                                        osb   = {},
                       AntexFile                                      antex = {},
                       std::vector<std::unique_ptr<CorrectionModel>>  corrections = {});

    [[nodiscard]] std::vector<PppResult> solveAll(
        const ObsFile&    obs,
        const NavFile&    nav,
        const GnssConfig& gnssCfg) const;

private:
    Config                                        m_cfg;
    std::shared_ptr<Sp3Orbit>                     m_orbit;
    OsbFile                                       m_osb;
    AntexFile                                     m_antex;
    std::vector<std::unique_ptr<CorrectionModel>> m_corrections;

    static constexpr double GPS_F1 = 1575.42e6;
    static constexpr double GPS_F2 = 1227.60e6;
    static constexpr double GAL_F1 = 1575.42e6;
    static constexpr double GAL_F5 = 1176.45e6;
    static constexpr double SPEED_OF_LIGHT = 299792458.0;

    static constexpr double GPS_ALPHA = (GPS_F1*GPS_F1) / (GPS_F1*GPS_F1 - GPS_F2*GPS_F2);
    static constexpr double GPS_BETA  = (GPS_F2*GPS_F2) / (GPS_F1*GPS_F1 - GPS_F2*GPS_F2);
    static constexpr double GAL_ALPHA = (GAL_F1*GAL_F1) / (GAL_F1*GAL_F1 - GAL_F5*GAL_F5);
    static constexpr double GAL_BETA  = (GAL_F5*GAL_F5) / (GAL_F1*GAL_F1 - GAL_F5*GAL_F5);

    static constexpr double GPS_LAMBDA_NL =
        SPEED_OF_LIGHT / (GPS_F1 + GPS_F2);
    static constexpr double GAL_LAMBDA_NL =
        SPEED_OF_LIGHT / (GAL_F1 + GAL_F5);

    bool isEnabled(GnssSystem system) const;

    PppObservation formIfObs(const SatObs& satObs, GnssSystem system) const;

    static double selectPhase(const SatObs& satObs, GnssSystem system,
                               int freqIdx);

    double osbCorrection(GnssSystem system, int prn,
                          const std::string& codeL1,
                          const std::string& codeL2) const;

    double satPcoCorrection(GnssSystem system,
                             double satX, double satY, double satZ,
                             double rxX,  double rxY,  double rxZ) const;
};

} // namespace loki::gnss
