#include <loki/gnss/sp3Orbit.hpp>
#include <loki/math/interpolation.hpp>
#include <loki/core/exceptions.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

using namespace loki;
using namespace loki::gnss;

// =============================================================================
//  Constructor -- builds clock index
// =============================================================================

Sp3Orbit::Sp3Orbit(Sp3File sp3, ClkFile clk)
    : m_sp3(std::move(sp3))
    , m_clk(std::move(clk))
{
    buildClockIndex();
}

// =============================================================================
//  buildClockIndex
//
//  Iterates once over all CLK records (O(n)) and inserts each AS record into
//  the per-satellite vector.  Each vector is then sorted by time (O(m log m)
//  per satellite).  Total: O(n log n).
//
//  After this, every clock query is O(log m) via std::lower_bound.
// =============================================================================

void Sp3Orbit::buildClockIndex()
{
    for (const auto& rec : m_clk.records) {
        if (rec.type != ClkType::AS) continue;
        m_clkIndex[rec.name].emplace_back(rec.time.totalSeconds(), rec.bias);
    }

    // Sort each per-satellite vector by time (they are usually already sorted,
    // but the CLK format does not strictly require it).
    for (auto& kv : m_clkIndex) {
        auto& vec = kv.second;
        std::sort(vec.begin(), vec.end(),
                  [](const ClkEntry& a, const ClkEntry& b) {
                      return a.first < b.first;
                  });
    }
}

// =============================================================================
//  compute
// =============================================================================

SatState Sp3Orbit::compute(const NavFile& /*nav*/,
                            GnssSystem      system,
                            int             prn,
                            const GpsTime&  t) const
{
    SatState state;

    std::vector<double>               times;
    std::vector<std::array<double,3>> positions;
    collectSp3(system, prn, times, positions);

    if (times.size() < static_cast<std::size_t>(POS_INTERP_ORDER + 1))
        return state;

    const double tSec = t.totalSeconds();
    const double dt   = m_sp3.interval > 0.0 ? m_sp3.interval : 300.0;

    if (tSec < times.front() - dt || tSec > times.back() + dt)
        return state;

    try {
        const auto pos = loki::math::lagrangeInterp3(times, positions, tSec,
                                                      POS_INTERP_ORDER);
        state.x = pos[0] * KM_TO_M;
        state.y = pos[1] * KM_TO_M;
        state.z = pos[2] * KM_TO_M;
    } catch (const AlgorithmException&) {
        return state;
    }

    state.clkBias = interpolateClock(system, prn, t);
    state.valid   = true;
    return state;
}

// =============================================================================
//  velocity
// =============================================================================

std::array<double, 3> Sp3Orbit::velocity(GnssSystem system,
                                          int        prn,
                                          const GpsTime& t) const
{
    std::vector<double>               times;
    std::vector<std::array<double,3>> positions;
    collectSp3(system, prn, times, positions);

    if (times.size() < static_cast<std::size_t>(POS_INTERP_ORDER + 1))
        return {0.0, 0.0, 0.0};

    const double tSec = t.totalSeconds();
    const double h    = VEL_DT_S;

    try {
        const auto p1 = loki::math::lagrangeInterp3(
            times, positions, tSec - h, POS_INTERP_ORDER);
        const auto p2 = loki::math::lagrangeInterp3(
            times, positions, tSec + h, POS_INTERP_ORDER);
        return {
            (p2[0] - p1[0]) / (2.0 * h) * KM_TO_M,
            (p2[1] - p1[1]) / (2.0 * h) * KM_TO_M,
            (p2[2] - p1[2]) / (2.0 * h) * KM_TO_M
        };
    } catch (const AlgorithmException&) {
        return {0.0, 0.0, 0.0};
    }
}

// =============================================================================
//  collectSp3
// =============================================================================

void Sp3Orbit::collectSp3(GnssSystem system, int prn,
                           std::vector<double>&               times,
                           std::vector<std::array<double,3>>& positions) const
{
    times.reserve(m_sp3.epochs.size());
    positions.reserve(m_sp3.epochs.size());

    for (const auto& epoch : m_sp3.epochs) {
        for (const auto& sat : epoch.satellites) {
            if (sat.system == system && sat.prn == prn && !sat.posMissing) {
                times.push_back(epoch.time.totalSeconds());
                positions.push_back({sat.x, sat.y, sat.z});
                break;
            }
        }
    }
}

// =============================================================================
//  interpolateClock  -- O(log n) using pre-built index
// =============================================================================

double Sp3Orbit::interpolateClock(GnssSystem system, int prn,
                                   const GpsTime& t) const
{
    const std::string id   = satId(system, prn);
    const double      tSec = t.totalSeconds();

    const auto idxIt = m_clkIndex.find(id);
    if (idxIt != m_clkIndex.end() && idxIt->second.size() >= 2) {
        const auto& vec = idxIt->second;

        // Find first entry >= tSec.
        const auto it = std::lower_bound(
            vec.begin(), vec.end(), ClkEntry{tSec, 0.0},
            [](const ClkEntry& a, const ClkEntry& b) {
                return a.first < b.first;
            });

        if (it != vec.end() && it != vec.begin()) {
            const auto prev = std::prev(it);
            const double t0 = prev->first,  b0 = prev->second;
            const double t1 = it->first,    b1 = it->second;
            const double dtt = t1 - t0;
            if (dtt > 1.0e-9) {
                return b0 + (tSec - t0) / dtt * (b1 - b0);
            }
            return b0;
        }
        // Near boundary: allow small extrapolation (< 60 s).
        if (it == vec.begin() && std::abs(tSec - vec.front().first) < 60.0)
            return vec.front().second;
        if (it == vec.end() && std::abs(tSec - vec.back().first) < 60.0)
            return vec.back().second;
    }

    // Fallback: SP3 clock (microseconds -> seconds), lower precision (~10 ns).
    for (const auto& epoch : m_sp3.epochs) {
        if (std::abs(epoch.time.totalSeconds() - tSec) > 300.0) continue;
        for (const auto& sat : epoch.satellites) {
            if (sat.system == system && sat.prn == prn && !sat.clockMissing)
                return sat.clk * US_TO_S;
        }
    }

    return 0.0;
}

// =============================================================================
//  satId
// =============================================================================

std::string Sp3Orbit::satId(GnssSystem system, int prn)
{
    char c = '?';
    switch (system) {
        case GnssSystem::GPS:     c = 'G'; break;
        case GnssSystem::GLONASS: c = 'R'; break;
        case GnssSystem::GALILEO: c = 'E'; break;
        case GnssSystem::BEIDOU:  c = 'C'; break;
        case GnssSystem::QZSS:    c = 'J'; break;
        default: break;
    }
    std::ostringstream oss;
    oss << c << std::setw(2) << std::setfill('0') << prn;
    return oss.str();
}
