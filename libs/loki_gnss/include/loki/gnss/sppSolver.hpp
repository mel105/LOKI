#pragma once

#include <loki/gnss/gnssTypes.hpp>
#include <loki/gnss/orbitModel.hpp>
#include <loki/gnss/correctionModel.hpp>
#include <loki/gnss/satVisibility.hpp>
#include <loki/core/config.hpp>

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace loki::gnss {

/**
 * @brief Result of SPP positioning for one epoch.
 *
 * clkBiasM holds the GPS receiver clock bias [m].
 * isbM holds inter-system biases relative to GPS [m] for each non-GPS
 * constellation that contributed measurements: keys are "GLONASS",
 * "GALILEO", "BEIDOU". Absent key means constellation had no measurements.
 */
struct SppResult {
    GpsTime time;
    double  x{0.0};
    double  y{0.0};
    double  z{0.0};
    double  clkBiasM{0.0};              ///< GPS receiver clock bias [m].
    std::map<std::string, double> isbM; ///< Inter-system biases [m].
    double  pdop{0.0};
    int     nSats{0};
    int     iterations{0};
    bool    valid{false};
    bool    converged{false};

    struct UsedSat {
        GnssSystem  system;
        int         prn;
        double      elevation;  ///< [deg]
        double      weight;
    };
    std::vector<UsedSat> usedSats;
    std::vector<double>  residuals;
};

/**
 * @brief Configuration for SppSolver.
 *
 * Defined outside SppSolver to work around GCC 13 / Windows MINGW64 bug
 * where nested structs with default member initialisers cannot be used as
 * default arguments in the enclosing class constructor.
 */
struct SppSolverConfig {
    int                      maxIterations         {20};
    double                   convergenceThresholdM {0.01};
    std::string              weighting             {"elevation"};
    bool                     sagnac                {false};
    double                   elevMaskDeg           {10.0};
    std::vector<std::string> constellations        {"GPS"};

    explicit SppSolverConfig(
        int                      maxIter_    = 20,
        double                   convThresh_ = 0.01,
        std::string              weight_     = "elevation",
        bool                     sagnac_     = false,
        double                   elMask_     = 10.0,
        std::vector<std::string> const_      = {"GPS"})
        : maxIterations        (maxIter_)
        , convergenceThresholdM(convThresh_)
        , weighting            (std::move(weight_))
        , sagnac               (sagnac_)
        , elevMaskDeg          (elMask_)
        , constellations       (std::move(const_))
    {}
};

/**
 * @brief Single Point Positioning solver using broadcast ephemeris.
 *
 * Implements iterative weighted least-squares SPP with multi-constellation
 * support via per-constellation inter-system bias (ISB) parameters.
 *
 * Parameter vector: [X, Y, Z, clk_GPS, ISB_GLO, ISB_GAL, ISB_BDS]
 * where ISB columns are added dynamically for each non-GPS constellation
 * that has at least one valid measurement above the elevation mask.
 * GPS and QZSS share the GPS clock column (QZSS uses GPS time system).
 *
 * Orbit propagation and signal corrections are fully injected via
 * OrbitModel and CorrectionModel interfaces. The solver contains no
 * physics -- it is a pure LSQ engine.
 *
 * References:
 *   IS-GPS-200 Table 20-IV (Keplerian propagation)
 *   Hofmann-Wellenhof et al., GPS: Theory and Practice
 */
class SppSolver {
public:
    using Config = SppSolverConfig;

    /**
     * @brief Constructs the solver with injected orbit model and corrections.
     *
     * @param cfg          Solver configuration.
     * @param orbit        Orbit propagator (e.g. BroadcastOrbit). Must not be null.
     * @param corrections  Optional correction models applied in order (iono, tropo, ...).
     *                     Each model's delay() result is subtracted from the pseudorange.
     */
    explicit SppSolver(
        Config                                              cfg,
        std::unique_ptr<OrbitModel>                         orbit,
        std::vector<std::unique_ptr<CorrectionModel>>       corrections = {});

    /**
     * @brief Solves SPP for a single observation epoch.
     *
     * @param epoch      One epoch of pseudorange observations.
     * @param nav        Navigation file with broadcast ephemerides.
     * @param approxPos  Initial ECEF position estimate [m].
     */
    [[nodiscard]] SppResult solve(
        const ObsEpoch&              epoch,
        const NavFile&               nav,
        const std::array<double,3>&  approxPos) const;

    /**
     * @brief Solves SPP for all epochs in an OBS file.
     */
    [[nodiscard]] std::vector<SppResult> solveAll(
        const ObsFile&    obs,
        const NavFile&    nav,
        const GnssConfig& cfg) const;

private:
    Config                                        m_cfg;
    std::unique_ptr<OrbitModel>                   m_orbit;
    std::vector<std::unique_ptr<CorrectionModel>> m_corrections;

    static constexpr double SPEED_OF_LIGHT = 299792458.0;

    bool isEnabled(GnssSystem system) const;
};

} // namespace loki::gnss
