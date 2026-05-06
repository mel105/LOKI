#pragma once

#include <loki/core/exceptions.hpp>
#include <loki/timeseries/timeStamp.hpp>

#include <array>
#include <map>
#include <string>
#include <vector>

/**
 * @file gnssTypes.hpp
 * @brief Core GNSS data structures used by all loki_gnss parsers and algorithms.
 *
 * All types live in namespace loki::gnss.
 *
 * Time convention:
 *   - GpsTime holds native GPS week + seconds-of-week (no leap seconds).
 *   - Conversion to/from ::TimeStamp (UTC/MJD) is done via GpsTime helpers.
 *   - GLONASS toe is stored in GpsTime after conversion in the parser.
 *
 * Coordinate convention:
 *   - SP3, GLO state vectors: ECEF [km].
 *   - Antenna offsets: [mm].
 *   - Delays: [s] unless noted otherwise.
 */

namespace loki::gnss {

// =============================================================================
//  Enumerations
// =============================================================================

/// @brief GNSS constellation identifier.
enum class GnssSystem {
    GPS,
    GLONASS,
    GALILEO,
    BEIDOU,
    QZSS,
    SBAS,
    UNKNOWN
};

/// @brief Observation type (first character of a RINEX 3 observation code).
enum class ObsType {
    PSEUDORANGE,    // C
    CARRIER_PHASE,  // L
    DOPPLER,        // D
    SNR,            // S
    UNKNOWN
};

/// @brief Navigation message type.
enum class NavMsgType {
    LNAV,    // GPS legacy
    CNAV,    // GPS civil
    CNAV2,   // GPS civil (L1C)
    FNAV,    // Galileo E5a
    INAV,    // Galileo E1/E5b
    D1,      // BeiDou MEO/IGSO
    D2,      // BeiDou GEO
    UNKNOWN
};

/// @brief SP3 file format version.
enum class Sp3Version { SP3A, SP3C, SP3D, UNKNOWN };

/// @brief RINEX CLK record type.
enum class ClkType {
    AR,   // analysis center receiver clock
    AS,   // analysis center satellite clock
    CR,   // calibration receiver
    DR,   // derived receiver
    MS    // monitor station
};

// =============================================================================
//  GPS Time
// =============================================================================

/**
 * @brief Native GPS time representation: week number + seconds of week.
 *
 * GPS time has no leap seconds. Week 0 started 1980-01-06 00:00:00 UTC.
 * Use toTimeStamp() to convert to UTC for LOKI TimeSeries integration.
 */
struct GpsTime {
    int    week{0};
    double sow{0.0};   ///< Seconds of week [0, 604800).

    /// @brief Returns total GPS seconds since GPS epoch (1980-01-06).
    [[nodiscard]] double totalSeconds() const {
        return static_cast<double>(week) * 604800.0 + sow;
    }

    /// @brief Converts to ::TimeStamp (UTC).
    [[nodiscard]] ::TimeStamp toTimeStamp() const {
        return ::TimeStamp::fromGpsWeekSow(week, sow);
    }

    /// @brief Constructs a GpsTime from a ::TimeStamp (UTC).
    [[nodiscard]] static GpsTime fromTimeStamp(const ::TimeStamp& ts) {
        return GpsTime{ ts.gpsWeek(), ts.gpsSecondsOfWeek() };
    }

    /// @brief Returns true if both fields are zero (default-constructed).
    [[nodiscard]] bool isZero() const { return week == 0 && sow == 0.0; }
};

// =============================================================================
//  OBS file structures
// =============================================================================

/**
 * @brief A single observation value with associated quality indicators.
 *
 * Corresponds to one cell in a RINEX 3 observation record.
 */
struct ObsValue {
    double value{0.0};
    int    lli{0};      ///< Loss-of-lock indicator (carrier phase only, 0 = OK).
    int    snr{0};      ///< Signal strength indicator (0 = not present, 1-9 scale).
    bool   valid{false};
};

/**
 * @brief All observations for one satellite in one epoch.
 *
 * Observation codes follow RINEX 3 convention: three characters,
 * e.g. "C1C" (GPS L1 C/A pseudorange), "L2W" (L2 carrier phase).
 */
struct SatObs {
    GnssSystem                   system{GnssSystem::UNKNOWN};
    int                          prn{0};
    std::map<std::string, ObsValue> obs;  ///< Key = RINEX 3 obs code ("C1C", "L1P", ...).
};

/**
 * @brief One observation epoch from a RINEX OBS file.
 *
 * epochFlag meanings (RINEX 3):
 *   0 = OK, 1 = power failure between previous and current epoch,
 *   2-6 = special events (header records follow).
 */
struct ObsEpoch {
    GpsTime             time;
    int                 epochFlag{0};
    int                 numSat{0};
    std::vector<SatObs> satellites;
};

/**
 * @brief Receiver and site metadata parsed from a RINEX OBS header.
 */
struct ReceiverInfo {
    std::string markerName;
    std::string markerNumber;
    std::string receiverType;
    std::string receiverSerial;
    std::string receiverFirmware;
    std::string antennaType;
    std::string antennaSerial;
    double      antDeltaH{0.0};   ///< Antenna height above marker [m].
    double      antDeltaE{0.0};   ///< East eccentricity [m].
    double      antDeltaN{0.0};   ///< North eccentricity [m].
    double      approxX{0.0};     ///< Approximate ECEF X [m].
    double      approxY{0.0};     ///< Approximate ECEF Y [m].
    double      approxZ{0.0};     ///< Approximate ECEF Z [m].
};

/**
 * @brief Complete RINEX OBS file.
 */
struct ObsFile {
    std::string           rinexVersion;
    ReceiverInfo          receiver;
    double                interval{0.0};   ///< Nominal sampling interval [s], 0 = unknown.
    std::vector<ObsEpoch> epochs;
};

// =============================================================================
//  NAV file structures
// =============================================================================

/**
 * @brief GPS / QZSS broadcast ephemeris (IS-GPS-200).
 *
 * Parametrization: Keplerian elements + harmonic corrections at toe.
 */
struct GpsBroadcastEph {
    int        prn{0};
    GpsTime    toc;            ///< Clock correction reference time.
    GpsTime    toe;            ///< Ephemeris reference time.
    NavMsgType msgType{NavMsgType::LNAV};

    // Clock corrections
    double af0{0.0};           ///< Clock bias [s].
    double af1{0.0};           ///< Clock drift [s/s].
    double af2{0.0};           ///< Clock drift rate [s/s^2].
    double TGD{0.0};           ///< Group delay L1-L2 [s].

    // Keplerian elements at toe
    double sqrtA{0.0};         ///< Square root of semi-major axis [m^0.5].
    double e{0.0};             ///< Eccentricity [-].
    double i0{0.0};            ///< Inclination at toe [rad].
    double Omega0{0.0};        ///< RAAN at weekly epoch [rad].
    double omega{0.0};         ///< Argument of perigee [rad].
    double M0{0.0};            ///< Mean anomaly at toe [rad].

    // Rates and harmonic corrections
    double deltaN{0.0};        ///< Mean motion correction [rad/s].
    double IDOT{0.0};          ///< Rate of inclination angle [rad/s].
    double OmegaDot{0.0};      ///< Rate of RAAN [rad/s].
    double Crc{0.0}, Crs{0.0}; ///< Radius harmonic corrections [m].
    double Cuc{0.0}, Cus{0.0}; ///< Argument of latitude corrections [rad].
    double Cic{0.0}, Cis{0.0}; ///< Inclination corrections [rad].

    // Metadata
    double IODE{0.0};          ///< Issue of data, ephemeris.
    double IODC{0.0};          ///< Issue of data, clock.
    int    SVhealth{0};        ///< SV health (0 = healthy).
    double URA{0.0};           ///< User range accuracy index.
    double fitInterval{0.0};   ///< Curve fit interval [h], 0 = 4h default.
    double week{0.0};          ///< GPS week of toe (may differ from toc week).
};

/**
 * @brief Galileo broadcast ephemeris (OS-SIS-ICD).
 *
 * Same Keplerian parametrization as GPS. TGD replaced by BGD fields.
 */
struct GalBroadcastEph {
    int        prn{0};
    GpsTime    toc;
    GpsTime    toe;
    NavMsgType msgType{NavMsgType::FNAV};

    // Clock corrections
    double af0{0.0};
    double af1{0.0};
    double af2{0.0};
    double BGDe5a{0.0};        ///< E1-E5a broadcast group delay [s].
    double BGDe5b{0.0};        ///< E1-E5b broadcast group delay [s].

    // Keplerian elements at toe
    double sqrtA{0.0};
    double e{0.0};
    double i0{0.0};
    double Omega0{0.0};
    double omega{0.0};
    double M0{0.0};

    // Rates and harmonic corrections
    double deltaN{0.0};
    double IDOT{0.0};
    double OmegaDot{0.0};
    double Crc{0.0}, Crs{0.0};
    double Cuc{0.0}, Cus{0.0};
    double Cic{0.0}, Cis{0.0};

    // Metadata
    double IODNAV{0.0};        ///< Issue of data, navigation.
    int    SVhealth{0};
    double SISA{0.0};          ///< Signal-in-space accuracy [m].
    double week{0.0};
};

/**
 * @brief GLONASS broadcast ephemeris (ICD-GLONASS edition 5.1).
 *
 * State vector parametrization in PZ-90.11 ECEF.
 * toe is stored in GPS time after parser conversion from GLONASS UTC.
 */
struct GloBroadcastEph {
    int     prn{0};            ///< Slot number (1-24).
    int     freqCh{0};         ///< Frequency channel number (-7 to +13).
    GpsTime toe;               ///< Reference epoch (converted from GLONASS UTC to GPST).

    // State vector in PZ-90.11 ECEF at toe
    double x{0.0},  y{0.0},  z{0.0};    ///< Position [km].
    double vx{0.0}, vy{0.0}, vz{0.0};   ///< Velocity [km/s].
    double ax{0.0}, ay{0.0}, az{0.0};   ///< Lunisolar acceleration [km/s^2].

    // Clock
    double tauN{0.0};          ///< SV clock correction (negative sign: dt = -tauN + gammaN*t) [s].
    double gammaN{0.0};        ///< Relative frequency deviation [-].
    double dtau{0.0};          ///< L2-L1 time difference [s].

    // Metadata
    int health{0};             ///< SV health (0 = healthy).
    int age{0};                ///< Age of operational information [days].
    int P4{0};                 ///< Flag for updated ephemeris (1 = updated).
};

/**
 * @brief BeiDou broadcast ephemeris (BDS-SIS-ICD-2.1).
 *
 * Same Keplerian parametrization as GPS.
 * MEO/IGSO satellites use D1 message; GEO satellites use D2.
 */
struct BdsBroadcastEph {
    int        prn{0};
    GpsTime    toc;
    GpsTime    toe;
    NavMsgType msgType{NavMsgType::D1};

    // Clock corrections
    double af0{0.0};
    double af1{0.0};
    double af2{0.0};
    double TGD1{0.0};          ///< B1/B3 group delay [s].
    double TGD2{0.0};          ///< B2/B3 group delay [s].

    // Keplerian elements at toe
    double sqrtA{0.0};
    double e{0.0};
    double i0{0.0};
    double Omega0{0.0};
    double omega{0.0};
    double M0{0.0};

    // Rates and harmonic corrections
    double deltaN{0.0};
    double IDOT{0.0};
    double OmegaDot{0.0};
    double Crc{0.0}, Crs{0.0};
    double Cuc{0.0}, Cus{0.0};
    double Cic{0.0}, Cis{0.0};

    // Metadata
    double AODE{0.0};          ///< Age of data, ephemeris [h].
    double AODC{0.0};          ///< Age of data, clock [h].
    int    SVhealth{0};
    double URA{0.0};
    double week{0.0};
};

/**
 * @brief SBAS geostationary satellite broadcast ephemeris (RTCA DO-229).
 *
 * State vector parametrization (same as GLONASS, different reference frame).
 */
struct SbasBroadcastEph {
    int     prn{0};
    GpsTime t0;                ///< Reference epoch.

    // State vector in WGS-84 ECEF
    double x{0.0},  y{0.0},  z{0.0};    ///< Position [km].
    double vx{0.0}, vy{0.0}, vz{0.0};   ///< Velocity [km/s].
    double ax{0.0}, ay{0.0}, az{0.0};   ///< Acceleration [km/s^2].

    // Clock
    double agf0{0.0};          ///< Clock offset [s].
    double agf1{0.0};          ///< Clock drift [s/s].

    int    health{0};
    double URA{0.0};
};

/**
 * @brief Complete RINEX NAV file (all constellations, mixed or single).
 *
 * Ionosphere model coefficients are stored here because RINEX 3 places them
 * in the navigation file header.
 */
struct NavFile {
    std::string rinexVersion;

    // Ephemerides per constellation
    std::vector<GpsBroadcastEph>  gpsEph;
    std::vector<GalBroadcastEph>  galEph;
    std::vector<GloBroadcastEph>  gloEph;
    std::vector<BdsBroadcastEph>  bdsEph;
    std::vector<SbasBroadcastEph> sbasEph;

    // Ionosphere model coefficients from header
    std::array<double, 4> ionoAlpha{};   ///< Klobuchar alpha [GPS], units: s, s/semi-circle, ...
    std::array<double, 4> ionoBeta{};    ///< Klobuchar beta  [GPS], units: s, s/semi-circle, ...
    std::array<double, 3> nequickAi{};   ///< NeQuick ai0 [sfu], ai1 [sfu/deg], ai2 [sfu/deg^2].
};

// =============================================================================
//  SP3 file structures
// =============================================================================

/**
 * @brief Position and clock state for one satellite in one SP3 epoch.
 *
 * The SP3 sentinel value 999999.999999 for clock is represented via
 * the clockMissing flag; posMissing flags all-zero or 0.000000 positions.
 */
struct Sp3SatState {
    GnssSystem system{GnssSystem::UNKNOWN};
    int        prn{0};
    double     x{0.0}, y{0.0}, z{0.0};   ///< ECEF position [km].
    double     clk{0.0};                  ///< Clock offset [microseconds].
    double     sigX{0.0}, sigY{0.0}, sigZ{0.0};   ///< Position sigma [mm], 0 = absent.
    double     sigClk{0.0};               ///< Clock sigma [picoseconds], 0 = absent.
    bool       clockMissing{false};       ///< True if clock = 999999.999999 sentinel.
    bool       posMissing{false};         ///< True if position record absent this epoch.
};

/// @brief One SP3 epoch: timestamp + all satellite states.
struct Sp3Epoch {
    GpsTime                  time;
    std::vector<Sp3SatState> satellites;
};

/**
 * @brief Complete SP3 precise orbit file.
 */
struct Sp3File {
    Sp3Version               version{Sp3Version::UNKNOWN};
    std::string              coordinateSystem;   ///< e.g. "IGS20", "ITRF2014".
    std::string              orbitType;          ///< e.g. "FIT", "HLM", "BCT".
    std::string              agency;
    double                   interval{0.0};      ///< Epoch interval [s].
    std::vector<std::string> satList;            ///< Satellite IDs from header, e.g. "G01".
    std::vector<Sp3Epoch>    epochs;
};

// =============================================================================
//  CLK file structures
// =============================================================================

/**
 * @brief One clock record from a RINEX CLK file.
 */
struct ClkRecord {
    ClkType     type{ClkType::AS};
    std::string name;             ///< Satellite ID (e.g. "G01") or receiver name.
    GpsTime     time;
    double      bias{0.0};        ///< Clock bias [s].
    double      biasRMS{0.0};     ///< Clock bias RMS [s], 0 = not present.
    double      drift{0.0};       ///< Clock drift [s/s], 0 = not present.
    double      driftRMS{0.0};    ///< Clock drift RMS [s/s], 0 = not present.
};

/**
 * @brief Complete RINEX CLK file.
 */
struct ClkFile {
    std::string            rinexVersion;
    std::vector<ClkRecord> records;
};

// =============================================================================
//  IONEX file structures
// =============================================================================

/**
 * @brief One TEC or RMS map from an IONEX file.
 *
 * Grid is row-major: tec[i][j] corresponds to lat[i], lon[j].
 * Raw integer values from file are scaled by 10^exponent to produce TECU.
 */
struct TecMap {
    GpsTime                          time;
    double                           rms{0.0};
    int                              exponent{-1};   ///< TEC scaling: value_TECU = raw * 10^exponent.
    std::vector<double>              lats;            ///< Latitude grid [deg], descending.
    std::vector<double>              lons;            ///< Longitude grid [deg], ascending.
    std::vector<std::vector<double>> tec;             ///< [TECU], rows = lat, cols = lon.
};

/**
 * @brief Complete IONEX TEC map file.
 */
struct IonexFile {
    std::string           version;
    std::string           mappingFunction;    ///< e.g. "COSZ", "QFAC", "MSLM".
    double                baseRadius{6371.0}; ///< Mean Earth radius [km].
    double                mapHeight{450.0};   ///< Single-layer ionosphere height [km].
    std::vector<TecMap>   tecMaps;
    std::vector<TecMap>   rmsMaps;
};

// =============================================================================
//  ANTEX file structures
// =============================================================================

/**
 * @brief PCV pattern for one frequency/azimuth combination.
 *
 * If azimuths is empty, the pattern is non-azimuth-dependent (NOAZI line).
 * elevations runs from 0 to 90 deg in steps given by the ANTEX header.
 */
struct PcvPattern {
    std::vector<double>              azimuths;    ///< [deg], empty = non-azimuth dependent.
    std::vector<double>              elevations;  ///< [deg], 0..90.
    std::vector<std::vector<double>> pcv;         ///< [mm], rows = azimuth, cols = elevation.
};

/**
 * @brief Phase center offset and variation for one frequency of one antenna.
 */
struct AntennaFreq {
    std::string obsCode;       ///< Frequency label, e.g. "G01" (satellite) or "L1" (receiver).
    double      pcoE{0.0};     ///< Phase center offset East  [mm] (receiver) or X [mm] (satellite).
    double      pcoN{0.0};     ///< North [mm] or Y [mm].
    double      pcoU{0.0};     ///< Up [mm] or Z [mm] (radial for satellite).
    PcvPattern  pcv;
};

/**
 * @brief Antenna calibration entry from an ANTEX file.
 *
 * Covers both receiver antennas (type-mean or individual) and
 * satellite antennas (satellite == true).
 */
struct AntennaCalib {
    std::string              antennaType;   ///< IGS antenna type code (20 chars).
    std::string              serialNo;      ///< Serial number; empty = type-mean calibration.
    bool                     satellite{false};
    std::string              svCode;        ///< Satellite code, e.g. "G032" (if satellite == true).
    std::vector<AntennaFreq> freqs;
};

/**
 * @brief Complete ANTEX antenna calibration file.
 */
struct AntexFile {
    std::string                  version;
    std::vector<AntennaCalib>    antennas;
};

// =============================================================================
//  DCB file structures
// =============================================================================

/**
 * @brief One differential code bias record.
 *
 * bias = code1 - code2 pseudorange bias for a given satellite or receiver.
 * Units: [ns].
 */
struct DcbRecord {
    std::string svn;           ///< Satellite ID (e.g. "G01") or receiver name.
    std::string obsCode1;      ///< First observation code (e.g. "C1C").
    std::string obsCode2;      ///< Second observation code (e.g. "C2W").
    double      bias{0.0};     ///< DCB value [ns].
    double      biasSigma{0.0};///< DCB RMS [ns], 0 = not present.
};

/**
 * @brief Complete DCB product file (CODE Bern P1C1/P1P2 format).
 */
struct DcbFile {
    std::string            description;
    GpsTime                startTime;
    GpsTime                endTime;
    std::vector<DcbRecord> records;
};

// =============================================================================
//  VMF3 file structures
// =============================================================================

/**
 * @brief One VMF3 record for a single station and epoch.
 *
 * VMF3 provides empirical mapping function coefficients (ah, aw) and
 * a priori zenith delays (zhd, zwd) derived from NWM fields.
 */
struct Vmf3Record {
    double mjd{0.0};     ///< Epoch in MJD (UTC).
    double lat{0.0};     ///< Station latitude [deg].
    double lon{0.0};     ///< Station longitude [deg].
    double ah{0.0};      ///< Hydrostatic mapping function coefficient.
    double aw{0.0};      ///< Wet mapping function coefficient.
    double zhd{0.0};     ///< Zenith hydrostatic delay [m].
    double zwd{0.0};     ///< Zenith wet delay [m].
};

/**
 * @brief Complete VMF3 file (station or grid).
 *
 * For single-station files, stationName is set from the header.
 * For grid files, stationName is empty and records cover the full grid per epoch.
 */
struct Vmf3File {
    std::string             stationName;   ///< Empty for grid files.
    std::vector<Vmf3Record> records;
};

} // namespace loki::gnss
