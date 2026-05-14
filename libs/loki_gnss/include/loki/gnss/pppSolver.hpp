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
    double  x{0.0}, y{0.0}, z{0.0};
    double  clkBiasM{0.0};
    double  ztdWetM{0.0};
    double  ztdTotalM{0.0};
    double  isbGalM{0.0};
    double  pdop{0.0};
    int     nSats{0};
    bool    valid{false};
    bool    converged{false};

    std::vector<double> residualsCode;
    std::vector<double> residualsPhase;
};

// =============================================================================
//  PppSolverConfig
// =============================================================================

struct PppSolverConfig {
    std::vector<std::string> constellations{"GPS"};
    double elevMaskDeg{10.0};
    bool   phaseWindup{true};
    bool   pcoPcv{false};
    bool   applyOsb{true};    ///< Apply OSB code bias corrections.

    PppFilterConfig filter{};

    explicit PppSolverConfig() = default;
};

// =============================================================================
//  PppSolver
// =============================================================================

/**
 * @brief Precise Point Positioning solver (ionosphere-free combination).
 *
 * OSB correction:
 *   CODE MGEX CLK products are consistent with C1W/C2W observations.
 *   When C1C is used (TRIMBLE ALLOY and most receivers), a systematic
 *   bias of alpha*(OSB_C1C - OSB_C1W) must be applied to each satellite.
 *   Values are typically 0.3-3 m per satellite and differ between SVs,
 *   causing ~1-7 m code residuals if uncorrected.
 *
 *   The correction applied to IF pseudorange:
 *     P_IF_corrected = P_IF_raw
 *                    - [alpha*OSB_P1 - beta*OSB_P2] * c/1e9   (user signals)
 *                    + [alpha*OSB_C1W - beta*OSB_C2W] * c/1e9 (CLK ref signals)
 *
 *   Simplified (since C2W is used for both user and CLK reference on L2):
 *     correction = -alpha * (OSB_C1C - OSB_C1W) * c/1e9   [m]
 *
 *   For Galileo C1X/C5X, the CLK products are generated with C1X/C5X
 *   so no OSB correction is needed (OSB_C1X - OSB_C1X = 0).
 */
class PppSolver {
public:
    using Config = PppSolverConfig;

    /**
     * @brief Constructs the PPP solver.
     * @param osb  Optional OSB file for code bias corrections.
     *             Pass empty OsbFile{} if not available.
     */
    explicit PppSolver(Config                                         cfg,
                       std::shared_ptr<Sp3Orbit>                      orbit,
                       OsbFile                                        osb = {},
                       std::vector<std::unique_ptr<CorrectionModel>>  corrections = {});

    [[nodiscard]] std::vector<PppResult> solveAll(
        const ObsFile&    obs,
        const NavFile&    nav,
        const GnssConfig& gnssCfg) const;

private:
    Config                                        m_cfg;
    std::shared_ptr<Sp3Orbit>                     m_orbit;
    OsbFile                                       m_osb;
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

    bool isEnabled(GnssSystem system) const;

    /**
     * @brief Forms IF-combined observations and applies OSB correction.
     *
     * @param osb   OSB table (may be empty -- no correction applied).
     */
    PppObservation formIfObs(const SatObs& satObs, GnssSystem system) const;

    static double selectPhase(const SatObs& satObs, GnssSystem system,
                               int freqIdx);

    /**
     * @brief Computes IF code bias correction [m] using OSB records.
     *
     * For GPS using C1C + C2W:
     *   correction = -(alpha * OSB_C1C - beta * OSB_C2W) * c/1e9
     *               + (alpha * OSB_C1W - beta * OSB_C2W) * c/1e9
     *             = -alpha * (OSB_C1C - OSB_C1W) * c/1e9
     *
     * For Galileo using C1X + C5X: correction ≈ 0 (CLK consistent with C1X).
     *
     * @return Correction to ADD to raw IF pseudorange [m].
     */
    double osbCorrection(GnssSystem system, int prn,
                          const std::string& codeL1,
                          const std::string& codeL2) const;
};

} // namespace loki::gnss
