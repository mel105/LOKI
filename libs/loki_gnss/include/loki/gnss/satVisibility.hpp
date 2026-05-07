#pragma once

#include <loki/gnss/gnssTypes.hpp>
#include <loki/gnss/keplerOrbit.hpp>

#include <array>
#include <vector>

namespace loki::gnss {

/**
 * @brief Elevation and azimuth of one satellite in one epoch.
 */
struct SatElevAzim {
    GnssSystem system{GnssSystem::UNKNOWN};
    int        prn{0};
    double     elevation{0.0};   ///< Elevation angle [deg], 0 = horizon, 90 = zenith.
    double     azimuth{0.0};     ///< Azimuth angle [deg], 0 = North, clockwise.
    SatState   state{};          ///< ECEF position and clock bias from KeplerOrbit.
    bool       visible{false};   ///< True if elevation >= elevation mask.
};

/**
 * @brief Visibility results for one observation epoch.
 *
 * Contains elevation/azimuth for all satellites tracked in the epoch,
 * plus DOP values computed from the visible subset.
 */
struct VisibilityEpoch {
    GpsTime                  time;
    std::vector<SatElevAzim> satellites;
    double gdop{0.0};
    double pdop{0.0};
    double hdop{0.0};
    double vdop{0.0};
    int    nVisible{0};   ///< Number of satellites above elevation mask.
};

/**
 * @brief Computes satellite visibility (elevation, azimuth, DOP) for all epochs.
 *
 * For each epoch in ObsFile:
 *   1. Computes satellite ECEF position via KeplerOrbit::compute().
 *   2. Transforms to ENU relative to the station.
 *   3. Computes elevation and azimuth.
 *   4. Computes GDOP/PDOP/HDOP/VDOP from visible satellites.
 *
 * The ECEF -> ENU transformation is self-contained (no loki_geodesy dependency).
 * loki_geodesy will be used in PlotGnss for more precise geodetic operations.
 */
class SatVisibility {
public:
    /**
     * @brief Computes visibility for all epochs in an OBS file.
     *
     * @param obs           Parsed OBS file.
     * @param nav           Parsed NAV file.
     * @param stationEcef   Station ECEF coordinates [m] (from OBS header).
     * @param elevMaskDeg   Elevation mask [deg]. Satellites below are marked not visible.
     * @return              One VisibilityEpoch per OBS epoch.
     */
    [[nodiscard]] std::vector<VisibilityEpoch> compute(
        const ObsFile&              obs,
        const NavFile&              nav,
        const std::array<double,3>& stationEcef,
        double                      elevMaskDeg = 10.0) const;

    /**
     * @brief Converts ECEF satellite position to elevation and azimuth
     *        as seen from a ground station.
     *
     * @param satEcef       Satellite ECEF position [m].
     * @param staEcef       Station ECEF position [m].
     * @param elevation     Output elevation [deg].
     * @param azimuth       Output azimuth [deg].
     */
    static void ecefToElevAzim(const std::array<double,3>& satEcef,
                                const std::array<double,3>& staEcef,
                                double& elevation,
                                double& azimuth);

private:
    /**
     * @brief Computes DOP values from unit vectors to visible satellites.
     *
     * @param epoch  Visibility epoch with satellites already filled.
     */
    static void computeDop(VisibilityEpoch& epoch);
};

} // namespace loki::gnss
