# CLAUDE.md -- Project Instructions for LOKI

This file contains instructions for Claude when working on the LOKI project.
It should be placed in the root of the repository and kept up to date.

---

## Project Overview

LOKI is a professional C++ toolkit for statistical analysis of time series data.
The long-term goal is a modular ecosystem of analysis libraries with a GUI/IDE frontend.
Current focus: change point detection and homogeneity testing (`loki_homogeneity`).

---

## Workflow Rules

### One task per conversation thread
- Each new feature, refactoring task, or module gets its **own conversation thread**.
- Use this thread (the root project thread) only for **architecture, organization, and planning**.
- Start each task thread with a reference to this file: *"Working on LOKI -- see CLAUDE.md"*.

### Before writing any code
- Discuss the approach first. Do not jump into implementation without agreeing on design.
- If the task is ambiguous, ask clarifying questions before starting.
- Propose the function/class signatures and overall structure first, then wait for approval.

---

## Code Style and Standards

### Language and Standard
- Language: **C++20**
- All new code must compile cleanly with `-Wall -Wextra -Wpedantic` and no warnings.
- No compiler extensions (`CMAKE_CXX_EXTENSIONS OFF`).

### Naming Conventions
- Classes: `PascalCase` (e.g., `ChangePointDetector`)
- Functions and methods: `camelCase` (e.g., `detectChangePoints()`)
- Member variables: `m_camelCase` (e.g., `m_data`)
- Constants and enums: `UPPER_SNAKE_CASE`
- Files: `camelCase.cpp` / `camelCase.hpp`

### Headers
- Use `.hpp` extension for all C++ headers.
- Use `#pragma once` at the top of every header.
- Include order: own headers -> project headers -> third-party -> standard library.

### Comments
- **All comments must be written in English.**
- **All comments must use ASCII characters only** -- no Unicode box-drawing characters
  such as `--`, `|`, `+`, etc. Use plain `-` or `=` for decorative separators.
  Unicode in source files causes GCC on Windows to misparse tokens.
- Every class must have a Doxygen-style comment block explaining its purpose.
- Every public method must have a brief Doxygen comment.
- Inline comments should explain *why*, not *what*.

Example:
```cpp
/**
 * @brief Detects change points in a time series using the SNHT method.
 *
 * Implements the Standard Normal Homogeneity Test (Alexandersson, 1986).
 * The series must be detrended and deseasoned before calling this method.
 */
class SnhtDetector {
public:
    /**
     * @brief Runs the SNHT test and returns the index of the most significant change point.
     * @param series Input time series values.
     * @return Index of detected change point, or -1 if none found.
     * @throws LOKIException if the series has fewer than MIN_SERIES_LENGTH points.
     */
    int detect(const std::vector<double>& series) const;
};
```

---

## Error Handling

### Use exceptions -- no silent failures
- Define and use a project exception hierarchy rooted at `LOKIException`.
- Never return error codes from functions where an exception is more appropriate.
- Never silently ignore errors or return magic values like `-1` or `NaN` without documentation.

### Exception hierarchy (defined in `libs/loki_core/include/loki/core/exceptions.hpp`)
```
std::exception
+-- LOKIException                  <- base for all LOKI exceptions
    +-- DataException               <- invalid or missing data
    |   +-- SeriesTooShortException
    |   +-- MissingValueException
    +-- IoException                 <- file I/O errors
    |   +-- FileNotFoundException
    |   +-- ParseException
    +-- ConfigException             <- configuration errors
    +-- AlgorithmException          <- numerical or algorithmic failures
        +-- ConvergenceException
        +-- SingularMatrixException
```

### Namespace
- All exception classes live in `namespace loki` (defined in `exceptions.hpp`).
- In `.cpp` files, add `using namespace loki;` after the last `#include` so exception
  names can be used without the `loki::` prefix inside library code.
- In `apps/` (main), use the fully qualified `loki::LOKIException` in catch blocks.
- `exceptions.hpp` **must be included early** in the include chain. It is included by
  `logger.hpp`, which is included by `config.hpp` -- so any file that includes
  `config.hpp` or `logger.hpp` will have exceptions available automatically.

### Where to catch
- Catch exceptions as **high up** in the call stack as makes sense (ideally in `main` or app layer).
- In library code (`libs/`), throw -- do not catch and swallow.
- In `apps/` (main), catch and report cleanly to the user.

### Example pattern
```cpp
// In library code -- throw, do not catch
double computeMean(const std::vector<double>& data) {
    if (data.empty()) {
        throw DataException("Cannot compute mean of an empty series.");
    }
    // ...
}

// In app layer -- catch and handle
try {
    auto result = detector.detect(series);
} catch (const loki::LOKIException& ex) {
    std::cerr << "[LOKI ERROR] " << ex.what() << "\n";
    return EXIT_FAILURE;
} catch (const std::exception& ex) {
    std::cerr << "[UNEXPECTED ERROR] " << ex.what() << "\n";
    return EXIT_FAILURE;
}
```

---

## Code Delivery

### Always use artifacts
- All code (`.cpp`, `.hpp`, `CMakeLists.txt`) must be delivered in **artifacts**, never in plain chat text.
- Each artifact contains exactly **one file**.
- The artifact title must match the file name (e.g., `changePoint.hpp`).

### License header
- Do NOT add any license/copyright header block at the top of `.cpp` or `.hpp` files.
- License information is managed separately (LICENSE file in repository root).

### Robustness checklist before delivering code
Before presenting any code artifact, verify:
- [ ] No raw owning pointers -- use `std::unique_ptr` / `std::shared_ptr` / value types.
- [ ] No `using namespace std;` in headers.
- [ ] All inputs validated, exceptions thrown where appropriate.
- [ ] No magic numbers -- use named constants.
- [ ] No compiler warnings with `-Wall -Wextra -Wpedantic`.
- [ ] Doxygen comments on all public interfaces.
- [ ] Comments in English, ASCII characters only (no Unicode box-drawing).
- [ ] `using namespace loki;` present in `.cpp` files after the last `#include`.

---

## Project Structure Reference

```
loki/
+-- CLAUDE.md
+-- CMakeLists.txt
+-- CMakePresets.json
+-- cmake/
|   +-- CompilerOptions.cmake
|   +-- Dependencies.cmake
|   +-- Version.cmake
+-- apps/
|   +-- loki/
|   |   +-- CMakeLists.txt
|   |   +-- main.cpp
|   +-- loki_homogeneity/
|       +-- CMakeLists.txt
|       +-- main.cpp
+-- libs/
|   +-- loki_core/
|   |   +-- CMakeLists.txt
|   |   +-- include/
|   |   |   +-- loki/
|   |   |       +-- core/
|   |   |       |   +-- exceptions.hpp
|   |   |       |   +-- version.hpp
|   |   |       |   +-- config.hpp
|   |   |       |   +-- configLoader.hpp
|   |   |       |   +-- directoryScanner.hpp
|   |   |       |   +-- logger.hpp
|   |   |       |   +-- nanPolicy.hpp
|   |   |       +-- timeseries/
|   |   |       |   +-- timeSeries.hpp
|   |   |       |   +-- timeStamp.hpp
|   |   |       |   +-- gapFiller.hpp
|   |   |       +-- stats/
|   |   |       |   +-- descriptive.hpp
|   |   |       |   +-- filter.hpp
|   |   |       +-- io/
|   |   |           +-- loader.hpp         <- in io/, NOT timeseries/
|   |   |           +-- dataManager.hpp
|   |   |           +-- gnuplot.hpp
|   |   |           +-- plot.hpp
|   |   +-- src/
|   |       +-- core/
|   |       |   +-- configLoader.cpp
|   |       |   +-- directoryScanner.cpp
|   |       |   +-- logger.cpp
|   |       +-- timeseries/
|   |       |   +-- timeSeries.cpp
|   |       |   +-- timeStamp.cpp
|   |       |   +-- gapFiller.cpp
|   |       +-- stats/
|   |       |   +-- descriptive.cpp
|   |       |   +-- filter.cpp
|   |       +-- io/
|   |           +-- loader.cpp
|   |           +-- dataManager.cpp
|   |           +-- gnuplot.cpp
|   |           +-- plot.cpp
|   |
|   +-- loki_homogeneity/
|   |   +-- CMakeLists.txt
|   |   +-- include/
|   |   |   +-- loki/
|   |   |       +-- homogeneity/
|   |   |           +-- changePointResult.hpp
|   |   |           +-- changePointDetector.hpp
|   |   |           +-- multiChangePointDetector.hpp
|   |   |           +-- medianYearSeries.hpp
|   |   |           +-- deseasonalizer.hpp
|   |   |           +-- harmonicSeries.hpp
|   |   |           +-- seriesAdjuster.hpp
|   |   |           +-- homogenizer.hpp
|   |   +-- src/
|   |       +-- changePointDetector.cpp
|   |       +-- multiChangePointDetector.cpp
|   |       +-- medianYearSeries.cpp
|   |       +-- deseasonalizer.cpp
|   |       +-- harmonicSeries.cpp
|   |       +-- seriesAdjuster.cpp
|   |       +-- homogenizer.cpp
|   |
|   +-- loki_outlier/                      <- future module
|       +-- CMakeLists.txt
|       +-- include/loki/outlier/
|       +-- src/
|
+-- tests/
|   +-- CMakeLists.txt
|   +-- unit/
|   |   +-- core/
|   |   |   +-- test_exceptions.cpp
|   |   +-- timeseries/
|   |   |   +-- test_timeStamp.cpp
|   |   |   +-- test_timeSeries.cpp
|   |   |   +-- test_gapFiller.cpp
|   |   +-- stats/
|   |   |   +-- test_descriptive.cpp
|   |   |   +-- test_summarize.cpp
|   |   +-- homogeneity/
|   |       +-- test_changePointDetector.cpp
|   |       +-- test_multiChangePointDetector.cpp
|   |       +-- test_medianYearSeries.cpp
|   |       +-- test_deseasonalizer.cpp
|   |       +-- test_seriesAdjuster.cpp
|   |       +-- test_homogenizer.cpp
|   +-- integration/
|
+-- config/
|   +-- loki_homogeneity.json
+-- scripts/
|   +-- build.sh
|   +-- clean.sh
|   +-- run.sh
|   +-- test.sh
+-- docs/
```

---

## Dependencies

All dependencies managed via CMake `FetchContent` -- no vendored source code in repository.

| Library | Version | Purpose |
|---|---|---|
| `Eigen3` | 3.4.0 | Linear algebra, LSQ, SVD, Kalman |
| `nlohmann_json` | 3.11.3 | Configuration files |
| `Catch2` | 3.5.2 | Unit and integration tests |

**Removed:** `newmat` (obsolete), `Dependencies/json-3.5.0` (vendored, replaced by FetchContent).

---

## Planned Modules

| Module | Description |
|---|---|
| `loki_homogeneity` | Change point detection, homogenizacia -- **aktualny fokus** |
| `loki_outlier` | IQR, Z-score, Mahalanobis, hat matrix, clustering-based |
| `loki_decomposition` | Trend, seasonality, residuals (STL, classical) |
| `loki_svd` | SVD/PCA decomposition of time series |
| `loki_filter` | Moving average, Butterworth, Savitzky-Golay |
| `loki_kalman` | Kalman filter, smoother, Extended Kalman Filter |
| `loki_arima` | AR, ARMA, ARIMA, SARIMA modelling |
| `loki_spectral` | FFT, power spectrum, Lomb-Scargle (for unevenly sampled data) |
| `loki_stationarity` | ADF, KPSS, unit root tests |
| `loki_clustering` | k-means, DBSCAN, Hungarian algorithm |
| `loki_qc` | Quality control, gap filling, flagging |
| `loki_regression` | Linear, polynomial, robust regression |

---

## Build Instructions

### Platform: Windows MINGW64 shell, UCRT64 toolchain (GCC 13.2)

```bash
mkdir -p build/debug
cd build/debug

cmake ../.. \
  -G "MinGW Makefiles" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=/c/msys64/ucrt64/bin/g++.exe \
  -DCMAKE_MAKE_PROGRAM=/c/msys64/ucrt64/bin/mingw32-make.exe \
  -DLOKI_BUILD_TESTS=OFF

/c/msys64/ucrt64/bin/mingw32-make -j4
```

### First run -- copy runtime DLLs
```bash
cp /c/msys64/ucrt64/bin/libgcc_s_seh-1.dll   build/debug/apps/loki/
cp /c/msys64/ucrt64/bin/libstdc++-6.dll       build/debug/apps/loki/
cp /c/msys64/ucrt64/bin/libwinpthread-1.dll   build/debug/apps/loki/
cp /c/msys64/ucrt64/bin/libgcc_s_seh-1.dll   build/debug/tests/
cp /c/msys64/ucrt64/bin/libstdc++-6.dll       build/debug/tests/
cp /c/msys64/ucrt64/bin/libwinpthread-1.dll   build/debug/tests/
```

`build.sh --copy-dlls` handles this automatically.

---

## Scripts

```
./scripts/build.sh [debug|release] [--tests] [--copy-dlls]
./scripts/run.sh [config]
./scripts/clean.sh [debug|release|all|rebuild] [--tests]
./scripts/test.sh [--rebuild] [--filter <pat>] [--verbose] [--list]
```

---

## Test Infrastructure

### Framework
Catch2 v3.5.2 via FetchContent. Test executables in `build/debug/tests/`.

### CMakeLists structure
`tests/CMakeLists.txt` is the single file that owns all test targets.
Uses `loki_add_test_exe` macro. Always use `DISCOVERY_MODE PRE_TEST`.

### Adding a new test file
1. Create `tests/unit/<module>/test_<name>.cpp`.
2. Add entry in `tests/CMakeLists.txt`:
```cmake
loki_add_test_exe(
    NAME    test_<name>
    SOURCES unit/<module>/test_<name>.cpp
    LIBS    loki_core
)
```
3. Rebuild with `./scripts/test.sh --rebuild`.

### Current test coverage

| File | Covers | Tests |
|---|---|---|
| `unit/core/test_exceptions.cpp` | Full exception hierarchy | 14 |
| `unit/timeseries/test_timeStamp.cpp` | Calendar, MJD, Unix, GPS conversions | 12 |
| `unit/timeseries/test_timeSeries.cpp` | append, access, slice, metadata | 21 |
| `unit/stats/test_descriptive.cpp` | mean, median, variance, IQR, ACF, Hurst | 27 |
| `unit/stats/test_summarize.cpp` | summarize(), NanPolicy, formatSummary() | 13 |
| `unit/timeseries/test_gapFiller.cpp` | GapFiller: detectGaps, fill (all strategies), edges, maxFillLength | 25 |
| `unit/homogeneity/test_medianYearSeries.cpp` | MedianYearSeries: profile, slots, NaN, valueAt, resolution | 14 |

Total: **126 tests, 126 passing**.

---

## Workspace Layout

```
<workspace>/
+-- INPUT/
+-- OUTPUT/
    +-- LOG/
    +-- CSV/
    +-- IMG/
```

Example config (`config/loki_homogeneity.json`):
```json
{
    "workspace": "C:/Users/eliasmichal/Documents/Osobne",
    "input": {
        "file": "SENSOR_DATA.txt",
        "time_format": "gpst_seconds",
        "delimiter": ";",
        "comment_char": "%",
        "columns": [],
        "merge_strategy": "separate"
    },
    "output": {
        "log_level": "info"
    },
    "homogeneity": {
        "preprocessing": {
            "gap_filling": {
                "enabled": true,
                "strategy": "median_year",
                "min_years": 5
            },
            "outlier_removal": {
                "enabled": false,
                "strategy": "iqr",
                "threshold": 3.0
            }
        },
        "deseasonalization": {
            "enabled": true,
            "strategy": "median_year",
            "harmonic_order": 1,
            "ma_window_size": 365
        },
        "detection": {
            "enabled": true,
            "significance_level": 0.05,
            "acf_dependence_limit": 0.2,
            "min_segment_points": 60,
            "min_segment_seconds": 0
        },
        "adjustment": {
            "enabled": true
        }
    }
}
```

---

## loki_homogeneity -- Architecture

### Pipeline (in order, each step enabled/disabled via JSON)

```
1. GapFiller                (loki_core/timeseries)
2. OutlierRemover           (loki_outlier -- future)
3. Deseasonalizer           (loki_homogeneity)
4. MultiChangePointDetector (loki_homogeneity)
5. SeriesAdjuster           (loki_homogeneity)
```

### Deseasonalization strategies

| Strategy | When to use |
|---|---|
| `MEDIAN_YEAR` | Climatological / GNSS / economic data, requires UTC/GPST/MJD timestamp |
| `HARMONIC` | Same domain as MEDIAN_YEAR, LSQ harmonic model (a0 + a1*sin + b1*cos + ...) |
| `MOVING_AVERAGE` | Sensor / radio signals with ms-s resolution, no calendar meaning |
| `NONE` | Data already deseasonalized, or no seasonality present |

`MEDIAN_YEAR` and `HARMONIC` require a time axis with calendar meaning (UTC, GPST, MJD).
If the input has only an index (no timestamp), only `MOVING_AVERAGE` or `NONE` are valid.
`Deseasonalizer` throws `ConfigException` for incompatible strategy/input combinations.

### Key types

```cpp
namespace loki::homogeneity {

// Single change point detection result
struct ChangePointResult {
    bool   detected{false};
    int    index{-1};
    double shift{0.0};
    double meanBefore{0.0};
    double meanAfter{0.0};
    double maxTk{0.0};
    double criticalValue{0.0};
    double pValue{0.0};
    double acfLag1{0.0};
    double sigmaStar{1.0};
    int    confIntervalLow{-1};
    int    confIntervalHigh{-1};
};

// One detected change point in global coordinates
struct ChangePoint {
    std::size_t globalIndex;
    double      mjd;
    double      shift;
    double      pValue;
};

} // namespace loki::homogeneity
```

### Class signatures (agreed design)

```cpp
// --- ChangePointDetector ---
class ChangePointDetector {
public:
    struct Config {
        double significanceLevel{0.05};
        double acfDependenceLimit{0.2};
        bool   correctForDependence{true};
    };
    explicit ChangePointDetector(Config cfg = {});
    [[nodiscard]]
    ChangePointResult detect(const std::vector<double>& z,
                             std::size_t begin,
                             std::size_t end) const;
};

// --- MultiChangePointDetector ---
class MultiChangePointDetector {
public:
    struct Config {
        std::size_t minSegmentPoints{60};
        double      minSegmentSeconds{0.0};
        ChangePointDetector::Config detectorConfig{};
    };
    explicit MultiChangePointDetector(Config cfg = {});
    [[nodiscard]]
    std::vector<ChangePoint> detect(const std::vector<double>& z,
                                    const std::vector<double>& times) const;
};

// --- MedianYearSeries ---
// Computes median annual profile for gap filling and deseasonalization.
// Profile: flat vector of 366 * slotsPerDay medians.
// Resolution auto-detected from series; sub-hourly -> ConfigException.
// Slots with fewer than minYears values -> NaN (warning logged, no throw).
class MedianYearSeries {
public:
    struct Config {
        int    minYears{5};
        double maxResolutionDays{1.0 / 24.0};  // 1 hour
    };
    explicit MedianYearSeries(const TimeSeries& series, Config cfg = Config{});
    [[nodiscard]] double      valueAt(const TimeStamp& ts) const noexcept;
    [[nodiscard]] std::size_t profileSize()  const noexcept;
    [[nodiscard]] double      stepDays()     const noexcept;
    [[nodiscard]] int         slotsPerDay()  const noexcept;
};

// --- Deseasonalizer ---
class Deseasonalizer {
public:
    enum class Strategy { MEDIAN_YEAR, HARMONIC, MOVING_AVERAGE, NONE };
    struct Config {
        Strategy strategy{Strategy::MEDIAN_YEAR};
        int      harmonicOrder{1};
        int      maWindowSize{365};
    };
    explicit Deseasonalizer(Config cfg = {});
    [[nodiscard]]
    std::vector<double> deseasonalize(const TimeSeries& series) const;
};

// --- SeriesAdjuster ---
class SeriesAdjuster {
public:
    [[nodiscard]]
    TimeSeries adjust(const TimeSeries& original,
                      const std::vector<ChangePoint>& changePoints) const;
};

// --- Homogenizer ---
class Homogenizer {
public:
    struct Config {
        Deseasonalizer::Config           deseasonalizer{};
        MultiChangePointDetector::Config detector{};
        bool applyAdjustment{true};
    };
    struct Result {
        std::vector<ChangePoint> changePoints;
        TimeSeries               adjustedSeries;
        std::vector<double>      deseasonalizedValues;
    };
    explicit Homogenizer(Config cfg = {});
    [[nodiscard]]
    Result process(const TimeSeries& input) const;
};
```

### apps/loki_homogeneity/main.cpp
Thin orchestration layer:
1. Parse CLI args, load JSON config.
2. `Logger::initDefault()`.
3. `DataManager::load()`.
4. For each series: `Homogenizer::process()`.
5. Log results, write CSV, optionally plot via `Plot`.
6. Catch `loki::LOKIException` and `std::exception`.

---

## Known Issues and Workarounds

### Unicode in source files (Windows GCC)
GCC on Windows misparsed tokens when source files contain UTF-8 box-drawing
characters in comments. **Rule: all comments ASCII only.**

### exceptions.hpp must be included early
`logger.hpp` includes `exceptions.hpp`. Including `logger.hpp` is sufficient.

### namespace loki in .cpp files
All library `.cpp` files add `using namespace loki;` after the last `#include`.

### DLL mismatch on Windows (exit code 0xc0000139)
Three DLLs must be present next to every executable:
`libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`.
`build.sh --copy-dlls` handles both `apps/` and `tests/`.

### catch_discover_tests and DLLs
Always use `DISCOVERY_MODE PRE_TEST` in `tests/CMakeLists.txt`.

### TimeStamp not in namespace loki
Known inconsistency -- to be fixed during full refactoring.

### Catch2 test name s bodkociarkami (test #95)
Test case z H1 obsahuje bodkociarkamy v nazve -- Catch2 PRE_TEST discovery
ho registruje ako jeden dlhy nazov. Nie je to bug v kode, len kosmeticky
problem s pomenovanim. Opravit pri refaktore testov H1.

### Logger macro names
Correct macros: LOKI_INFO, LOKI_WARNING, LOKI_ERROR (not LOKI_WARN, not LOG_INFO).

### Config struct with enum member -- GCC 13 aggregate init
In-class initializers on structs with enum members fail with `= {}` as default
argument on GCC 13/Windows. Fix: use explicit constructor with member-initializer
list, and `Config cfg = Config{}` as the default argument.

### ::TimeStamp in loki namespace code
TimeStamp is not in namespace loki. When used alongside loki:: types,
always qualify as ::TimeStamp (global scope). This applies to function
signatures, lambdas, and local variables.

---

## Implemented Modules

### loki::core -- complete
- `exceptions.hpp` -- full hierarchy
- `version.hpp` -- `0.1.0`
- `nanPolicy.hpp` -- `NanPolicy { THROW, SKIP, PROPAGATE }`
- `logger.hpp / .cpp` -- Meyers singleton, file + stdout/stderr, macros
- `config.hpp` -- all config structs
- `configLoader.hpp / .cpp` -- JSON loader, path resolution

### loki::timeseries -- complete
- `timeStamp.hpp / .cpp` -- MJD internal, GPS/UTC/Unix conversions
- `timeSeries.hpp / .cpp` -- Observation, SeriesMetadata, append, slice, indexOf
- `gapFiller.hpp / .cpp` -- LINEAR, FORWARD_FILL, MEAN, NONE strategies;
                             MEDIAN_YEAR blocked (ConfigException) until integrated
                             with MedianYearSeries; detects NaN and time-jump gaps;
                             bfill/ffill on leading/trailing edges; maxFillLength limit

### loki::stats -- complete
- `descriptive.hpp / .cpp` -- mean, median, variance, IQR, ACF, Hurst, summarize()

### loki::io -- complete
- `loader.hpp / .cpp` -- file loading (in io/, NOT timeseries/)
- `dataManager.hpp / .cpp`
- `gnuplot.hpp / .cpp`
- `plot.hpp / .cpp`

### apps/loki -- complete
Full pipeline: CLI -> ConfigLoader -> Logger -> DataManager -> stats -> Plot.

### loki_homogeneity -- partially implemented
- `changePointResult.hpp` -- ChangePointResult + ChangePoint structs
- `changePointDetector.hpp / .cpp` -- single segment, O(n) prefix sums, sigmaStar
- `multiChangePointDetector.hpp / .cpp` -- recursive binary splitting
- `medianYearSeries.hpp / .cpp` -- median annual profile, auto-detected resolution
                                    (daily=366 slots, 6h=1464 slots, etc.);
                                    sub-hourly -> ConfigException;
                                    under-populated slots -> NaN + warning (no throw);
                                    short series -> warning + continues with available data
- `filter.hpp / .cpp` -- movingAverage (centered, O(n) prefix sums),
                         exponentialMovingAverage, weightedMovingAverage;
                         all reject NaN input (DataException); checkNoNaN helper
- `deseasonalizer.hpp / .cpp` -- Strategy: MOVING_AVERAGE (default), MEDIAN_YEAR, NONE;
                                  MEDIAN_YEAR requires profileLookup lambda + step >= 1h;
                                  Result: {residuals, seasonal, series with _deseas suffix}

### Not yet implemented
- `loki_homogeneity/deseasonalizer` -- H5
- `loki_homogeneity/harmonicSeries` -- H4
- `loki_homogeneity/seriesAdjuster` -- H6
- `loki_homogeneity/homogenizer` -- H7
- `apps/loki_homogeneity` -- H8
- `loki_outlier` -- future
- All other planned modules

---

## Planned Thread Sequence for loki_homogeneity

| Thread | Scope | Deliverables | Status |
|---|---|---|---|
| H1 | `ChangePointResult`, `ChangePointDetector` | `.hpp`, `.cpp`, `test_changePointDetector.cpp` | DONE |
| H2 | `MultiChangePointDetector` (binary splitting) | `.hpp`, `.cpp`, `test_multiChangePointDetector.cpp` | DONE |
| G  | `GapFiller` (loki_core/timeseries) | `.hpp`, `.cpp`, `test_gapFiller.cpp` | DONE |
| H3 | `MedianYearSeries` | `.hpp`, `.cpp`, `test_medianYearSeries.cpp` | DONE |
| H4 | `HarmonicSeries` (LSQ, replaces old fit+deseas) | `.hpp`, `.cpp` | POSTPONED - urobime v loki_spectral (FFT) |
| H5 | `MA filter + Deseasonalizer` | `filter.hpp/.cpp`, `deseasonalizer.hpp/.cpp`, `test_filter.cpp, test_deseasonalizer.cpp` | DONE |
| H6 | `SeriesAdjuster` | `.hpp`, `.cpp`, `test_seriesAdjuster.cpp` | next |
| H7 | `Homogenizer` + `CMakeLists.txt` for `loki_homogeneity` | `.hpp`, `.cpp` | |
| H8 | `apps/loki_homogeneity/main.cpp` + `CMakeLists.txt` | pipeline app | |

Recommended order: H5 -> H6 -> H7 -> H8.

### How to start each thread
Paste this at the beginning of each new conversation:

```
Working on LOKI -- see CLAUDE.md [paste full CLAUDE.md content here]

Thread <ID>: <one-line description>

[Attach relevant old code if refactoring]
```

---

## Design Decisions

### T-statistics computation -- O(n) via prefix sums
Old code used O(n^2) nested loops. New implementation uses prefix sums:
- `prefixSum[k] = sum of z[0..k-1]`
- `meanBefore(k) = prefixSum[k] / k`
- `meanAfter(k) = (prefixSum[n] - prefixSum[k]) / (n - k)`
This is critical for series with n > 10000 (NWM data had n > 36000).

### sigmaStar -- Antoch et al. noise correction
After detecting the candidate change point at index `m`:
1. Center each segment around its own mean -> `VALSIGMA`.
2. Compute ACF of `VALSIGMA` using `loki::stats::acf()`.
3. Estimate `f0` using Bartlett window of width `L = ceil(n^(1/3))`.
4. `sigmaStar = sqrt(|f0|)`.
5. If `acfLag1 > acfDependenceLimit`: multiply critical value by `sigmaStar`.

### Binary splitting -- min segment constraint
Both conditions must be satisfied before recursing:
- `(end - begin) >= minSegmentPoints`
- If timestamps available: time span >= `minSegmentSeconds`

### Homogenization direction
Reference segment = **last (rightmost)**. Adjust all previous segments
left-to-right by cumulative shift sum. This matches the original approach.

### GapFiller -- gap detection
Two sources of gaps detected simultaneously:
- NaN value in Observation (time present, value missing).
- Time jump > expectedStep * gapThresholdFactor (entire rows absent).
Expected step = median of consecutive MJD differences (robust to outlier jumps).
Absent-row gaps use sentinel endIndex < startIndex to distinguish from NaN gaps.
Leading/trailing NaN always filled (bfill/ffill) regardless of maxFillLength.

### GapFiller -- MEDIAN_YEAR integration
GapFiller::Strategy::MEDIAN_YEAR is defined but blocked with ConfigException.
When MedianYearSeries is integrated (post-H3), add:
  fillMedianYear(series, gaps, const MedianYearSeries&) const
to GapFiller without changing the public interface.

### MedianYearSeries -- profile structure
Flat vector of 366 * slotsPerDay medians. Slot index:
  slot = (doy - 1) * slotsPerDay + slotOfDay
doy in [1, 366], slotOfDay in [0, slotsPerDay).
slotsPerDay = round(1 / stepDays), clamped to [1, 24].
Leap year DOY 366 slot allocated but only populated in leap years.
Sub-hourly resolution (step < 1h) rejected -- median year loses statistical meaning.
Short series (span < minYears) -> LOKI_WARNING, continues with available data.
Under-populated slots (count < minYears) -> NaN, LOKI_WARNING logged.
minYears default = 5, configurable (future: from JSON).

### MedianYearSeries location
Lives in `loki_homogeneity`, not `loki_core`, because it is specific to the
homogeneity analysis pipeline. GapFiller (loki_core) receives it by reference.

### HarmonicSeries -- LSQ, replaces old newmat-based fit.cpp
Model: `y = a0 + sum_{j=1}^{order} [a_j * sin(2*pi*j*t/P) + b_j * cos(2*pi*j*t/P)]`
Period P = 365.25 days (climatological). Uses Eigen3 instead of newmat.

### loki::stats ACF -- already implemented
`loki::stats::acf(x, maxLag)` exists in `descriptive.hpp`. Do NOT reimplement
in `loki_homogeneity`. Import `loki_core` and call directly.

### HarmonicSeries -- POSTPONED
Bez FFT/spektralnej analyzy nie je mozne odhadnut periody spolahlivo.
Bude implementovana po loki_spectral: FFT -> dominant periods -> harmonic fit.

### Deseasonalizer strategies (revised)
MEDIAN_YEAR: signal - medianYear[slot], pouziva MedianYearSeries cez referenciu
MOVING_AVERAGE: centered simple MA, windowSize konfigurovatelny (default 365)
NONE: no-op
MA filter implementovany v loki_core/stats (nie loki_homogeneity).

---

## Notes and Reminders

- Data files and plot outputs are **not** committed to the repository.
- Third-party dependencies via CMake `FetchContent` only -- no vendored source.
- Build directory `build/` is gitignored.
- `newmat` replaced by `Eigen3` throughout.
- Do NOT add license/copyright blocks at the top of source files.
- gnuplot font: `'Sans,12'` (not `'Helvetica,12'`).
- gnuplot terminal: `'noenhanced'`.
- `loader.hpp` is in `loki_core/io/`, NOT in `timeseries/`.
- `SeriesMetadata` must be populated by Loader after loading.
- Plot output -> `OUTPUT/IMG/`, temp files use `.tmp_` prefix.