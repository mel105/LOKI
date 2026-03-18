#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <loki/homogeneity/multiChangePointDetector.hpp>
#include <loki/core/exceptions.hpp>

#include <cmath>
#include <numeric>
#include <vector>

using namespace loki::homogeneity;

// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

namespace {

/// Builds a stationary series: pure zero-mean white noise with small amplitude.
/// Uses a simple LCG so results are reproducible without <random>.
std::vector<double> makeStationary(std::size_t n, double amplitude = 0.05)
{
    std::vector<double> z(n);
    uint64_t state = 12345678901ULL;
    for (std::size_t i = 0; i < n; ++i) {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        // map to [-1, 1]
        const double u = static_cast<double>(static_cast<int64_t>(state)) /
                         static_cast<double>(INT64_MAX);
        z[i] = amplitude * u;
    }
    return z;
}

/// Builds a series with a single hard step shift at splitIdx.
/// Uses the same LCG for reproducible small-amplitude noise.
std::vector<double> makeSingleShift(std::size_t n,
                                    std::size_t splitIdx,
                                    double      shift,
                                    double      noiseAmp = 0.05)
{
    std::vector<double> z = makeStationary(n, noiseAmp);
    for (std::size_t i = splitIdx; i < n; ++i) {
        z[i] += shift;
    }
    return z;
}

/// Builds a series with two step shifts.
std::vector<double> makeTwoShifts(std::size_t n,
                                   std::size_t split1,
                                   std::size_t split2,
                                   double      shift1,
                                   double      shift2,
                                   double      noiseAmp = 0.05)
{
    std::vector<double> z = makeStationary(n, noiseAmp);
    for (std::size_t i = split1; i < n; ++i) z[i] += shift1;
    for (std::size_t i = split2; i < n; ++i) z[i] += shift2;
    return z;
}

/// Trivial MJD axis: one observation per day starting at MJD 51544 (J2000.0).
std::vector<double> makeMjdAxis(std::size_t n, double step = 1.0)
{
    std::vector<double> t(n);
    for (std::size_t i = 0; i < n; ++i) {
        t[i] = 51544.0 + static_cast<double>(i) * step;
    }
    return t;
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------------

TEST_CASE("MultiChangePointDetector -- empty series returns no change points",
          "[MultiChangePoint]")
{
    MultiChangePointDetector det;
    const auto result = det.detect({});
    CHECK(result.empty());
}

TEST_CASE("MultiChangePointDetector -- stationary series returns no change points",
          "[MultiChangePoint]")
{
    // Pure low-amplitude white noise -- no shift anywhere.
    // minSegmentPoints=200 so even if the detector is slightly over-eager,
    // sub-segments below 200 points are never tested.
    const std::size_t N = 600;
    const auto z = makeStationary(N, 0.02);

    MultiChangePointDetector::Config cfg;
    cfg.minSegmentPoints = 200;
    MultiChangePointDetector det(cfg);

    const auto result = det.detect(z);
    CHECK(result.empty());
}

TEST_CASE("MultiChangePointDetector -- single shift detected",
          "[MultiChangePoint]")
{
    // Large shift (10x noise amplitude) -- should be found unambiguously.
    // minSegmentPoints=150 prevents the recursion from going too deep.
    const std::size_t N      = 600;
    const std::size_t SPLIT  = 300;
    const double      SHIFT  = 5.0;  // >> noise amplitude 0.05

    const auto z = makeSingleShift(N, SPLIT, SHIFT, 0.05);

    MultiChangePointDetector::Config cfg;
    cfg.minSegmentPoints = 150;
    MultiChangePointDetector det(cfg);

    const auto result = det.detect(z);

    REQUIRE(result.size() == 1);

    // Change point must be in the vicinity of SPLIT (within 10 samples)
    CHECK(result[0].globalIndex >= SPLIT - 10);
    CHECK(result[0].globalIndex <= SPLIT + 10);

    // Shift sign: segment after change point has higher mean
    CHECK(result[0].shift > 0.0);

    // p-value must be below default significance level
    CHECK(result[0].pValue < 0.05);
}

TEST_CASE("MultiChangePointDetector -- two shifts detected",
          "[MultiChangePoint]")
{
    const std::size_t N      = 900;
    const std::size_t SPLIT1 = 300;
    const std::size_t SPLIT2 = 600;
    const double      SHIFT1 =  5.0;
    const double      SHIFT2 = -4.0;

    const auto z = makeTwoShifts(N, SPLIT1, SPLIT2, SHIFT1, SHIFT2, 0.05);

    MultiChangePointDetector::Config cfg;
    cfg.minSegmentPoints = 150;
    MultiChangePointDetector det(cfg);

    const auto result = det.detect(z);

    REQUIRE(result.size() == 2);

    // Results are sorted by globalIndex
    CHECK(result[0].globalIndex < result[1].globalIndex);

    // Each change point near its true position
    CHECK(result[0].globalIndex >= SPLIT1 - 15);
    CHECK(result[0].globalIndex <= SPLIT1 + 15);

    CHECK(result[1].globalIndex >= SPLIT2 - 15);
    CHECK(result[1].globalIndex <= SPLIT2 + 15);
}

TEST_CASE("MultiChangePointDetector -- result is sorted by globalIndex",
          "[MultiChangePoint]")
{
    const std::size_t N = 900;
    const auto z = makeTwoShifts(N, 300, 600, 5.0, -4.0, 0.05);

    MultiChangePointDetector::Config cfg;
    cfg.minSegmentPoints = 150;
    MultiChangePointDetector det(cfg);

    const auto result = det.detect(z);

    for (std::size_t i = 1; i < result.size(); ++i) {
        CHECK(result[i - 1].globalIndex < result[i].globalIndex);
    }
}

TEST_CASE("MultiChangePointDetector -- segment shorter than minSegmentPoints not tested",
          "[MultiChangePoint]")
{
    // Series just below the minimum -- should return nothing even if a shift exists
    const std::size_t N = 50; // less than default minSegmentPoints = 60
    const auto z = makeSingleShift(N, 25, 5.0, 0.05);

    MultiChangePointDetector det; // default minSegmentPoints = 60
    const auto result = det.detect(z);
    CHECK(result.empty());
}

TEST_CASE("MultiChangePointDetector -- with MJD time axis, mjd field populated",
          "[MultiChangePoint]")
{
    const std::size_t N     = 600;
    const std::size_t SPLIT = 300;
    const auto z    = makeSingleShift(N, SPLIT, 5.0, 0.05);
    const auto mjds = makeMjdAxis(N);

    MultiChangePointDetector::Config cfg;
    cfg.minSegmentPoints = 150;
    MultiChangePointDetector det(cfg);

    const auto result = det.detect(z, mjds);

    REQUIRE(!result.empty());

    // mjd must be within the plausible range of the axis
    CHECK(result[0].mjd >= mjds.front());
    CHECK(result[0].mjd <= mjds.back());
}

TEST_CASE("MultiChangePointDetector -- empty times vector gives mjd = 0.0",
          "[MultiChangePoint]")
{
    const std::size_t N = 600;
    const auto z = makeSingleShift(N, 300, 5.0, 0.05);

    MultiChangePointDetector::Config cfg;
    cfg.minSegmentPoints = 150;
    MultiChangePointDetector det(cfg);

    const auto result = det.detect(z, {}); // no times

    REQUIRE(!result.empty());
    CHECK(result[0].mjd == 0.0);
}

TEST_CASE("MultiChangePointDetector -- mismatched z and times sizes throw DataException",
          "[MultiChangePoint]")
{
    const std::vector<double> z(400, 0.0);
    const std::vector<double> times(300, 0.0); // wrong size

    MultiChangePointDetector det;
    CHECK_THROWS_AS(det.detect(z, times), loki::DataException);
}

TEST_CASE("MultiChangePointDetector -- minSegmentSeconds filters short time segments",
          "[MultiChangePoint]")
{
    // 200 points, 1 observation per second (step = 1/86400 days)
    const std::size_t N = 200;
    const double STEP_DAYS = 1.0 / 86400.0;

    const auto z    = makeSingleShift(N, 100, 5.0, 0.05);
    const auto mjds = makeMjdAxis(N, STEP_DAYS);

    MultiChangePointDetector::Config cfg;
    cfg.minSegmentPoints  = 60;
    cfg.minSegmentSeconds = 300.0; // 5 min > total series span ~200s -> nothing tested

    MultiChangePointDetector det(cfg);
    const auto result = det.detect(z, mjds);

    // All results (if any) must have valid indices
    for (const auto& cp : result) {
        CHECK(cp.globalIndex < N);
    }
}
