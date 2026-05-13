#pragma once

#include <loki/gnss/orbitModel.hpp>
#include <loki/gnss/gnssTypes.hpp>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace loki::gnss {

/**
 * @brief Precise orbit model backed by SP3 + RINEX CLK products.
 *
 * Implements OrbitModel so it can be drop-in substituted for BroadcastOrbit
 * inside SppSolver or PppSolver with no solver code changes.
 *
 * Satellite position: Lagrange order-9 interpolation of SP3 ECEF [km -> m].
 *
 * Satellite clock: linear interpolation of RINEX CLK AS records.
 * The CLK records are pre-indexed at construction time into a per-satellite
 * sorted vector so each query is O(log n) via std::lower_bound.
 * For a 24-h 30s CLK file (~346 000 records, ~30 satellites) this reduces
 * per-epoch clock lookup from O(346 000) to O(log 11 500) ~ 14 comparisons,
 * cutting total processing time from ~15 min to ~30 s.
 *
 * Velocity for phase windup: finite difference of Lagrange-interpolated
 * positions at (t - 1s, t + 1s).
 */
class Sp3Orbit : public OrbitModel {
public:
    /**
     * @brief Constructs the orbit model from parsed SP3 and CLK files.
     *
     * Pre-builds the per-satellite clock index (O(n log n), done once).
     *
     * @param sp3  Parsed precise orbit file.
     * @param clk  Parsed precise clock file (AS records used).
     */
    Sp3Orbit(Sp3File sp3, ClkFile clk);

    /**
     * @brief Computes satellite ECEF [m] and clock bias [s] at t.
     *
     * NavFile is ignored -- SP3 contains all orbit data.
     */
    SatState compute(const NavFile&  nav,
                     GnssSystem      system,
                     int             prn,
                     const GpsTime&  t) const override;

    /**
     * @brief Returns satellite ECEF velocity [m/s] at t via finite difference.
     */
    std::array<double, 3> velocity(GnssSystem      system,
                                   int             prn,
                                   const GpsTime&  t) const;

private:
    Sp3File m_sp3;
    ClkFile m_clk;  // kept for SP3 clock fallback only

    // Pre-built clock index: satId -> sorted (gps_total_sec, bias_s).
    using ClkEntry = std::pair<double, double>;
    std::map<std::string, std::vector<ClkEntry>> m_clkIndex;

    static constexpr int    POS_INTERP_ORDER = 9;
    static constexpr double KM_TO_M          = 1000.0;
    static constexpr double US_TO_S          = 1.0e-6;
    static constexpr double VEL_DT_S         = 1.0;  // finite-diff step [s]

    /// @brief Builds m_clkIndex from CLK AS records. Called by constructor.
    void buildClockIndex();

    /// @brief Collects SP3 time+position arrays for one satellite.
    void collectSp3(GnssSystem system, int prn,
                    std::vector<double>&               times,
                    std::vector<std::array<double,3>>& positions) const;

    /// @brief O(log n) clock interpolation using pre-built index.
    double interpolateClock(GnssSystem system, int prn,
                             const GpsTime& t) const;

    /// @brief Returns satellite ID string, e.g. "G01", "E03".
    static std::string satId(GnssSystem system, int prn);
};

} // namespace loki::gnss
