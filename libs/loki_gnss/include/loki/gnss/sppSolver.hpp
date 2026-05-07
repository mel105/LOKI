#pragma once

#include <loki/gnss/gnssTypes.hpp>
#include <loki/gnss/keplerOrbit.hpp>
#include <loki/gnss/satVisibility.hpp>
#include <loki/core/config.hpp>

#include <array>
#include <string>
#include <vector>

namespace loki::gnss {

/**
 * @brief Result of SPP positioning for one epoch.
 */
struct SppResult {
    GpsTime time;
    double  x{0.0};
    double  y{0.0};
    double  z{0.0};
    double  clkBiasM{0.0};   ///< Receiver clock bias [m]
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
    int                      maxIterations         {10};
    double                   convergenceThresholdM {0.001};
    std::string              weighting             {"elevation"};
    std::string              ionosphere            {"klobuchar"};
    std::string              troposphere           {"saastamoinen"};
    bool                     sagnac                {true};
    bool                     relativistic          {true};
    double                   elevMaskDeg           {10.0};
    std::vector<std::string> constellations        {"GPS"};

    explicit SppSolverConfig(
        int                      maxIter_    = 10,
        double                   convThresh_ = 0.001,
        std::string              weight_     = "elevation",
        std::string              iono_       = "klobuchar",
        std::string              tropo_      = "saastamoinen",
        bool                     sagnac_     = true,
        bool                     relat_      = true,
        double                   elMask_     = 10.0,
        std::vector<std::string> const_      = {"GPS"})
        : maxIterations        (maxIter_)
        , convergenceThresholdM(convThresh_)
        , weighting            (std::move(weight_))
        , ionosphere           (std::move(iono_))
        , troposphere          (std::move(tropo_))
        , sagnac               (sagnac_)
        , relativistic         (relat_)
        , elevMaskDeg          (elMask_)
        , constellations       (std::move(const_))
    {}
};

/**
 * @brief Single Point Positioning solver using broadcast ephemeris.
 *
 * Implements iterative weighted least-squares SPP (IS-GPS-200):
 *   1. Compute satellite ECEF position + clock correction.
 *   2. Sagnac correction (Earth rotation during signal travel).
 *   3. Klobuchar ionosphere + Saastamoinen troposphere delays.
 *   4. Weighted LSQ: dx = (H^T W H)^-1 H^T W b.
 *   5. Iterate until convergence.
 */
class SppSolver {
public:
    using Config = SppSolverConfig;

    /// @brief Constructs the solver with the given configuration.
    explicit SppSolver(Config cfg = Config{});

    /**
     * @brief Solves SPP for a single observation epoch.
     *
     * @param epoch      One epoch of pseudorange observations.
     * @param nav        Navigation file with broadcast ephemerides.
     * @param approxPos  Initial ECEF position estimate [m].
     * @param ionoAlpha  Klobuchar alpha [4] from NAV header.
     * @param ionoBeta   Klobuchar beta  [4] from NAV header.
     */
    [[nodiscard]] SppResult solve(
        const ObsEpoch&              epoch,
        const NavFile&               nav,
        const std::array<double,3>&  approxPos,
        const std::array<double,4>&  ionoAlpha,
        const std::array<double,4>&  ionoBeta) const;

    /**
     * @brief Solves SPP for all epochs in an OBS file.
     */
    [[nodiscard]] std::vector<SppResult> solveAll(
        const ObsFile&    obs,
        const NavFile&    nav,
        const GnssConfig& cfg) const;

private:
    Config m_cfg;

    static constexpr double SPEED_OF_LIGHT = 299792458.0;

    static double klobucharDelay(const std::array<double,4>& alpha,
                                  const std::array<double,4>& beta,
                                  double lat, double lon,
                                  double az,  double el,
                                  double gpsSow);

    static double saastamoinenDelay(double lat, double h, double el);

    static void ecefToGeodetic(double x, double y, double z,
                                double& lat, double& lon, double& h);

    static double selectPseudorange(const SatObs& sat, GnssSystem system);

    static double elevationWeight(double elevRad);

    bool isEnabled(GnssSystem system) const;
};

} // namespace loki::gnss
