#pragma once

#include <loki/gnss/gnssTypes.hpp>
#include <loki/gnss/sppSolver.hpp>
#include <loki/gnss/pppSolver.hpp>

#include <map>
#include <string>
#include <vector>

namespace loki::gnss {

// =============================================================================
//  ParseResult
// =============================================================================

/**
 * @brief Per-constellation observation codes seen in the OBS file header.
 *
 * Key = constellation char ('G','R','E','C'), value = list of obs codes.
 * Example: { 'G': ["C1C","C2W","L1C","L2W"], 'E': ["C1X","C5X","L1X","L5X"] }
 */
using ObsCodeMap = std::map<char, std::vector<std::string>>;

/**
 * @brief Summary statistics computed from the parsed OBS file.
 */
struct ObsSummary {
    std::string station;
    std::string receiverType;
    std::string antennaType;
    std::size_t nEpochs{0};
    std::size_t minSatsPerEpoch{0};
    std::size_t maxSatsPerEpoch{0};
    double      meanSatsPerEpoch{0.0};
    double      intervalSec{0.0};
    double      spanStartMjd{0.0};
    double      spanEndMjd{0.0};

    std::size_t nGps{0};
    std::size_t nGlonass{0};
    std::size_t nGalileo{0};
    std::size_t nBeidou{0};
    std::size_t nSbas{0};
    std::size_t nQzss{0};

    ObsCodeMap  obsCodes;
};

/**
 * @brief Summary of what was parsed from NAV + OBS.
 */
struct ParseResult {
    ObsSummary  obs;
    std::size_t nGpsEph{0};
    std::size_t nGalEph{0};
    std::size_t nGloEph{0};
    std::size_t nBdsEph{0};
    bool        hasKlobuchar{false};
};

// =============================================================================
//  SppSummary
// =============================================================================

/**
 * @brief Aggregate statistics over all valid SPP epochs.
 */
struct SppSummary {
    std::size_t nEpochsTotal{0};
    std::size_t nEpochsValid{0};
    std::size_t nEpochsConverged{0};

    double meanX{0.0}, meanY{0.0}, meanZ{0.0};
    double stdX{0.0},  stdY{0.0},  stdZ{0.0};

    double meanLat{0.0};
    double meanLon{0.0};
    double meanH{0.0};

    double meanClkBiasM{0.0};
    double meanPdop{0.0};
    double meanNSats{0.0};

    bool   hasReference{false};
    double refX{0.0}, refY{0.0}, refZ{0.0};
    double refLat{0.0}, refLon{0.0}, refH{0.0};
    double mean3dErrorM{0.0};
    double std3dErrorM{0.0};
    double rmsErrorM{0.0};
    std::string referenceSource;
};

// =============================================================================
//  PppSummary
// =============================================================================

/**
 * @brief Aggregate statistics over all valid PPP epochs.
 *
 * Includes separate convergence-period and post-convergence statistics
 * so the protocol can report both.
 */
struct PppSummary {
    std::size_t nEpochsTotal{0};
    std::size_t nEpochsValid{0};
    std::size_t nEpochsConverged{0};

    // Post-convergence position mean and std.
    double meanX{0.0}, meanY{0.0}, meanZ{0.0};
    double stdX{0.0},  stdY{0.0},  stdZ{0.0};
    double meanLat{0.0}, meanLon{0.0}, meanH{0.0};

    double meanClkBiasM{0.0};
    double meanZtdWetM{0.0};
    double meanNSats{0.0};

    // Position error vs reference (filled only when referencePosition.enabled).
    bool   hasReference{false};
    double refX{0.0}, refY{0.0}, refZ{0.0};
    double refLat{0.0}, refLon{0.0}, refH{0.0};
    double mean3dErrorM{0.0};
    double std3dErrorM{0.0};
    double rmsErrorM{0.0};
    double convergenceTimeMin{0.0};  ///< Time to first converged epoch [min].
    std::string referenceSource;
};

// =============================================================================
//  GnssResult
// =============================================================================

/**
 * @brief Complete result of one loki_gnss pipeline run.
 *
 * GnssAnalyzer::run() fills this and passes it to GnssProtocol and PlotGnss.
 */
struct GnssResult {
    ParseResult              parse;

    std::vector<SppResult>   spp;
    SppSummary               sppSummary;
    bool                     hasSpp{false};

    std::vector<PppResult>   ppp;
    PppSummary               pppSummary;
    bool                     hasPpp{false};
};

} // namespace loki::gnss
