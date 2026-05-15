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
 * Corrections applied per observation:
 *   - Satellite clock from RINEX CLK (AS records, O(log n) lookup).
 *   - Ionosphere-free combination eliminates first-order iono.
 *   - Troposphere: ZHD fixed (Saastamoinen + NMF); ZWD estimated by filter.
 *   - Satellite PCO: SP3 orbits are to Centre of Mass (CoM); CLK products
 *     are generated consistent with the Antenna Phase Centre (APC).
 *     The correction moves the CoM position to APC:
 *       sat_apc = sat_com + R_body * pco_body
 *     where R_body is constructed from the nadir and solar-panel unit
 *     vectors.  PCO values [mm] are taken from the ANTEX file (IGS20).
 *     IF-combined PCO = alpha*PCO_L1 - beta*PCO_L2 applied as a scalar
 *     range correction (dot product with LoS unit vector).
 *   - Receiver antenna height offset (antDeltaH from OBS header) applied
 *     to translate from marker to ARP before processing.
 *   - Phase windup (carrier phase only, lambda = c/(f1+f2)).
 *   - OSB code bias alignment to CLK reference signals.
 *
 * OSB correction:
 *   CODE MGEX CLK products are consistent with C1W/C2W observations.
 *   When C1C is used, a systematic bias alpha*(OSB_C1C - OSB_C1W) must
 *   be applied per satellite.  Values are 0.3-3 m and satellite-specific.
 */
class PppSolver {
public:
    using Config = PppSolverConfig;

    /**
     * @brief Constructs the PPP solver.
     * @param osb    Optional OSB file for code bias corrections.
     * @param antex  Optional ANTEX file for satellite PCO corrections.
     */
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

    // Narrow-lane wavelength for IF phase windup [m].
    // lambda_NL = c / (f1 + f2).  For GPS: ~0.1070 m.
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

    /**
     * @brief Returns the IF-combined satellite PCO range correction [m].
     *
     * Looks up PCO for L1 and L2 (or E1/E5a) from the ANTEX file by
     * antenna type prefix (e.g. "BLOCK II", "GAL-").  Applies
     * IF combination: pco_if = alpha*pco_L1 - beta*pco_L2.
     * Projects the result onto the satellite-to-receiver line-of-sight.
     *
     * The correction is ADDED to the modeled range (moving CoM -> APC
     * increases the range from receiver to satellite phase centre).
     *
     * @param system   Constellation.
     * @param satX/Y/Z Satellite ECEF position [m] (CoM).
     * @param rxX/Y/Z  Receiver ECEF position [m].
     * @return         Range correction [m] to ADD to modeled range.
     */
    double satPcoCorrection(GnssSystem system,
                             double satX, double satY, double satZ,
                             double rxX,  double rxY,  double rxZ) const;
};

} // namespace loki::gnss
