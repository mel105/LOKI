# CLAUDE.md -- Project Instructions for LOKI

This file contains instructions for Claude when working on the LOKI project.
It should be placed in the root of the repository and kept up to date.

---

## Project Overview

LOKI is a professional C++ toolkit for statistical analysis of time series data.
The long-term goal is a modular ecosystem of analysis libraries with a GUI/IDE frontend.
Current focus: outlier detection and removal (`loki_outlier`).

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

### No code duplication
- Before implementing any functionality, check if it already exists in `loki_core` or
  another module. Reuse and extend existing classes rather than duplicating logic.
- If a module needs slightly different behaviour from an existing class (e.g. GapFiller),
  extend the existing class with a new strategy or method -- do not copy it.

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

## Library Architecture

### Dependency graph
```
loki_core
    ^
    |
loki_outlier          (depends on loki_core only)
    ^
    |
loki_homogeneity      (depends on loki_core + loki_outlier)
    ^
    |
loki_spectral         (depends on loki_core)
... (other future modules depend on loki_core; cross-dependencies only when justified)
```

### Rules
- Every module is a **separate CMake library** with its own `include/` and `src/` tree.
- Include paths follow `#include <loki/<module>/<class>.hpp>`.
- No circular dependencies between modules.
- `loki_core` must not depend on any other loki module.
- `loki_homogeneity` may depend on `loki_outlier` once it is implemented.
  Until then, `preOutlier` and `postOutlier` remain `enabled=false` placeholders.

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
|   |   +-- CMakeLists.txt
|   |   +-- main.cpp
|   +-- loki_outlier/
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
|   |   |           +-- loader.hpp
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
|   +-- loki_outlier/
|   |   +-- CMakeLists.txt
|   |   +-- include/
|   |   |   +-- loki/
|   |   |       +-- outlier/
|   |   |           +-- outlierResult.hpp
|   |   |           +-- iqrDetector.hpp
|   |   |           +-- madDetector.hpp
|   |   |           +-- zScoreDetector.hpp
|   |   |           +-- hatMatrixDetector.hpp   <- O4, later
|   |   |           +-- outlierCleaner.hpp
|   |   +-- src/
|   |       +-- iqrDetector.cpp
|   |       +-- madDetector.cpp
|   |       +-- zScoreDetector.cpp
|   |       +-- hatMatrixDetector.cpp           <- O4, later
|   |       +-- outlierCleaner.cpp
|   |
|   +-- loki_homogeneity/
|       +-- CMakeLists.txt
|       +-- include/
|       |   +-- loki/
|       |       +-- homogeneity/
|       |           +-- changePointResult.hpp
|       |           +-- changePointDetector.hpp
|       |           +-- multiChangePointDetector.hpp
|       |           +-- medianYearSeries.hpp
|       |           +-- deseasonalizer.hpp
|       |           +-- seriesAdjuster.hpp
|       |           +-- homogenizer.hpp
|       |           +-- plotHomogeneity.hpp
|       +-- src/
|           +-- changePointDetector.cpp
|           +-- multiChangePointDetector.cpp
|           +-- medianYearSeries.cpp
|           +-- deseasonalizer.cpp
|           +-- seriesAdjuster.cpp
|           +-- homogenizer.cpp
|           +-- plotHomogeneity.cpp

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
|   |   |   +-- test_changePointDetector.cpp
|   |   |   +-- test_multiChangePointDetector.cpp
|   |   |   +-- test_medianYearSeries.cpp
|   |   |   +-- test_deseasonalizer.cpp
|   |   |   +-- test_seriesAdjuster.cpp
|   |   |   +-- test_homogenizer.cpp
|   |   +-- outlier/
|   |       +-- test_iqrDetector.cpp
|   |       +-- test_madDetector.cpp
|   |       +-- test_zScoreDetector.cpp
|   |       +-- test_outlierCleaner.cpp
|   +-- integration/
|
+-- config/
|   +-- homogenization.json
|   +-- outlier.json
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

| Module | Description | Status |
|---|---|---|
| `loki_homogeneity` | Change point detection, SNHT, binary splitting, adjustment | active |
| `loki_outlier` | IQR, MAD, Z-score, hat matrix, outlier replacement | **current focus** |
| `loki_decomposition` | Trend, seasonality, residuals (STL, classical) | planned |
| `loki_svd` | SVD/PCA decomposition of time series | planned |
| `loki_filter` | Moving average, Butterworth, Savitzky-Golay | planned |
| `loki_kalman` | Kalman filter, smoother, Extended Kalman Filter | planned |
| `loki_arima` | AR, ARMA, ARIMA, SARIMA modelling | planned |
| `loki_spectral` | FFT, power spectrum, Lomb-Scargle (unevenly sampled data) | planned |
| `loki_stationarity` | ADF, KPSS, unit root tests | planned |
| `loki_clustering` | k-means, DBSCAN, Hungarian algorithm | planned |
| `loki_qc` | Quality control, gap filling, flagging | planned |
| `loki_regression` | Linear, polynomial, robust regression | planned |

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
| `unit/homogeneity/test_seriesAdjuster.cpp` | SeriesAdjuster: empty CPs, single/multi shift, unordered input, edge indices, metadata suffix, timestamp/flag preservation, negative shift | 9 |

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

### Loader -- time_columns and space delimiter
When `delimiter` is `' '` or `'\t'`, `_splitLine` uses `operator>>` (word extraction)
to handle multiple consecutive spaces. For all other delimiters, exact splitting is used.
`time_columns` (0-based field indices) controls which fields are joined to form the
time token. Default empty = field 0 only. Example: `[0, 1]` for split UTC date/time.
`columns` (1-based from start of line) selects value columns. Example: `[3]` = third
column in file (0-based field index 2).

### plotHomogeneity -- fwdSlash helper
Gnuplot on Windows requires forward slashes. `fwdSlash()` static helper converts
backslashes in all path strings passed to gnuplot.

### SeriesAdjuster -- reference segment
Reference = **first segment** (index 0). All subsequent segments are pulled toward
the level of the first segment using prefix sums of shifts.
`prefixShift[0] = 0`, `prefixShift[j] = sum(shifts[0..j-1])`.
`adjusted[i] = original[i] - prefixShift[segmentIdx(i)]`.

### min_segment_duration -- human-readable format
`detection.min_segment_duration` in JSON (e.g. `"180d"`, `"1y"`, `"6h"`, `"30m"`, `"60s"`)
is parsed by `ConfigLoader::_parseDuration()` and stored in `minSegmentSeconds`.
Takes precedence over `min_segment_seconds` if non-empty.

---

## Implemented Modules

### loki::core -- complete
- `exceptions.hpp` -- full hierarchy
- `version.hpp` -- `0.1.0`
- `nanPolicy.hpp` -- `NanPolicy { THROW, SKIP, PROPAGATE }`
- `logger.hpp / .cpp` -- Meyers singleton, file + stdout/stderr, macros
- `config.hpp` -- all config structs including `InputConfig::timeColumns`,
                   `DetectionConfig::minSegmentDuration`, `PlotConfig::correctionCurve`
- `configLoader.hpp / .cpp` -- JSON loader, path resolution, `_parseDuration()`

### loki::timeseries -- complete
- `timeStamp.hpp / .cpp` -- MJD internal, GPS/UTC/Unix conversions
- `timeSeries.hpp / .cpp` -- Observation, SeriesMetadata, append, slice, indexOf
- `gapFiller.hpp / .cpp` -- LINEAR, FORWARD_FILL, MEAN, NONE strategies;
                             MEDIAN_YEAR blocked (ConfigException) until O3;
                             detects NaN and time-jump gaps;
                             bfill/ffill on leading/trailing edges; maxFillLength limit

### loki::stats -- complete
- `descriptive.hpp / .cpp` -- mean, median, variance, IQR, ACF, Hurst, summarize()
- `filter.hpp / .cpp` -- movingAverage (centered, O(n)), EMA, WMA

### loki::io -- complete
- `loader.hpp / .cpp` -- multi-field time parsing, space-delimiter word extraction,
                          1-based absolute column indexing
- `dataManager.hpp / .cpp`
- `gnuplot.hpp / .cpp`
- `plot.hpp / .cpp`

### apps/loki -- complete
Full pipeline: CLI -> ConfigLoader -> Logger -> DataManager -> stats -> Plot.

### loki_homogeneity -- complete (H1-H10)
- `changePointResult.hpp` -- ChangePointResult + ChangePoint structs
- `changePointDetector.hpp / .cpp` -- single segment SNHT, O(n) prefix sums, sigmaStar
- `multiChangePointDetector.hpp / .cpp` -- recursive binary splitting with
                                           minSegmentPoints + minSegmentSeconds guards
- `medianYearSeries.hpp / .cpp` -- median annual profile, auto-detected resolution
- `deseasonalizer.hpp / .cpp` -- MOVING_AVERAGE, MEDIAN_YEAR, NONE strategies
- `seriesAdjuster.hpp / .cpp` -- prefix sum correction, reference = first segment
- `homogenizer.hpp / .cpp` -- full pipeline orchestration
- `plotHomogeneity.hpp / .cpp` -- 8 plot types including correction curve step function

### apps/loki_homogeneity -- complete
Pipeline: CLI -> ConfigLoader -> Logger -> DataManager -> Homogenizer -> CSV + PlotHomogeneity.
Generic plots (histogram, ACF, QQ, boxplot) also called via loki::Plot.

### loki_outlier -- in progress
- `outlierResult.hpp` -- OutlierPoint + OutlierResult structs (two-level, O1 DONE)
- `libs/loki_outlier/CMakeLists.txt` -- library target (O1 DONE)
- Detectors, OutlierCleaner, app: O2-O5 planned

---

## Planned Thread Sequence for loki_homogeneity

| Thread | Scope | Status |
|---|---|---|
| H1 | ChangePointResult, ChangePointDetector | DONE |
| H2 | MultiChangePointDetector (binary splitting) | DONE |
| G  | GapFiller (loki_core/timeseries) | DONE |
| H3 | MedianYearSeries | DONE |
| H4 | HarmonicSeries | POSTPONED -> loki_spectral |
| H5 | MA filter + Deseasonalizer | DONE |
| H6 | SeriesAdjuster | DONE |
| H7 | Homogenizer + CMakeLists.txt | DONE |
| H8 | apps/loki_homogeneity/main.cpp + plots | DONE |
| H9 | Config review, MedianYearSeries wiring, plot fixes | DONE |
| H10 | Algorithm validation, loader fixes, SeriesAdjuster direction fix | DONE |
| H11 | data_domain / resolution-aware defaults | planned (low priority) |

---

## Planned Thread Sequence for loki_outlier

### Pipeline design
```
Input TimeSeries
      |
      v
[1. Deseasonalize]     -- uses Deseasonalizer from loki_homogeneity (or NONE)
      |                -- required for IQR, MAD, Z-score
      v                -- not required for hat matrix
[2. Detect outliers]   -- configurable method: IQR | MAD | Z-score | hat_matrix
      |
      v
[3. Replace outliers]  -- mark as NaN, then fill via GapFiller (LINEAR | MEDIAN_YEAR | ...)
      |
      v
[4. Reconstruct]       -- add seasonal component back (if deseasonalized in step 1)
      |
      v
Output TimeSeries (cleaned)
```

Step 4 is essential -- without it the output would be in residual units, not original units.
Replacement reuses `GapFiller` directly -- no code duplication.

### Key types (agreed design, O1 DONE)

Two-level result structure in `namespace loki::outlier`:

```cpp
// Per-outlier detail
struct OutlierPoint {
    std::size_t index;          // position in the residual / series vector
    double      originalValue;  // value before replacement
    double      replacedValue;  // NaN until OutlierCleaner fills it
    double      score;          // z-score, IQR multiple, Mahal. distance, etc.
    double      threshold;      // threshold that was exceeded
    int         flag{1};        // 1 = detected, 2 = replaced
};

// Summary for one detection run on one series
struct OutlierResult {
    std::vector<OutlierPoint> points;    // one per detected outlier
    std::string               method;   // "IQR", "MAD", "Z-score", "hat_matrix"
    double                    location; // robust location estimate (median / mean)
    double                    scale;    // robust scale estimate (IQR / MAD / std)
    std::size_t               n;        // number of input points
    std::size_t               nOutliers;// equals points.size()
};
```

### Thread plan

| Thread | Scope | Deliverables | Status |
|---|---|---|---|
| O1 | Architecture, OutlierResult, CMakeLists.txt | `outlierResult.hpp`, `CMakeLists.txt` for loki_outlier | DONE |
| O2 | IqrDetector, MadDetector, ZScoreDetector | `.hpp`, `.cpp`, `test_iqrDetector.cpp`, `test_madDetector.cpp`, `test_zScoreDetector.cpp` | planned |
| O3 | OutlierCleaner (pipeline orchestration) | `outlierCleaner.hpp/.cpp`, `test_outlierCleaner.cpp`, integration with GapFiller + Deseasonalizer | planned |
| O4 | HatMatrixDetector (leverage + Cook's D) | `hatMatrixDetector.hpp/.cpp`, `test_hatMatrixDetector.cpp` | planned |
| O5 | apps/loki_outlier, JSON config, wire into loki_homogeneity | `main.cpp`, `outlier.json`, enable preOutlier/postOutlier in HomogenizerConfig | planned |

### loki_outlier -- design rules
- All detectors operate on `std::vector<double>` residuals (deseasonalized values).
- Detectors return `OutlierResult` -- they do NOT modify the series.
- Replacement is always done by `OutlierCleaner` via `GapFiller`, not inside detectors.
- If `GapFiller` needs a new capability for outlier replacement, add a method to
  `GapFiller` -- do not create a separate replacer class.
- Detectors are stateless: construct with Config, call `detect(residuals)`.
- `OutlierCleaner` owns the full pipeline (deseasonalize -> detect -> replace -> reconstruct).
- `loki_homogeneity` will link against `loki_outlier` once O5 is complete.

### loki_outlier -- detector method summary

| Method | Input | Threshold | Notes |
|---|---|---|---|
| IQR | residuals | k * IQR (default k=1.5) | Requires deseasonalization |
| MAD | residuals | k * MAD / 0.6745 (default k=3.0) | More robust than IQR for heavy tails |
| Z-score | residuals | abs(z) > threshold (default 3.0) | Assumes normal distribution |
| Hat matrix | residuals + design matrix | leverage + Cook's D | Detects influential points |

### loki_outlier.json -- planned configuration schema

Keys to be confirmed and extended in O2-O5. Current placeholder:

```json
{
    "workspace": "...",
    "input": {
        "file": "...",
        "time_format": "gpst_seconds",
        "delimiter": ";",
        "comment_char": "%",
        "columns": [],
        "merge_strategy": "separate"
    },
    "output": {
        "log_level": "info"
    },
    "outlier": {
        "deseasonalization": {
            "strategy": "median_year | moving_average | none",
            "ma_window_size": 365
        },
        "detection": {
            "method": "iqr | mad | zscore | hat_matrix",
            "iqr_multiplier": 1.5,
            "mad_multiplier": 3.0,
            "zscore_threshold": 3.0
        },
        "replacement": {
            "strategy": "linear | forward_fill | nearest | median_year | none"
        }
    }
}
```

Parameters to be confirmed/added per thread:
- O2: `iqr_multiplier`, `mad_multiplier`, `zscore_threshold` (confirm defaults)
- O3: `replacement.strategy`, `median_year.min_years`
- O4: `hat_matrix.leverage_threshold` (default: auto = 2*(p+1)/n), `hat_matrix.cooks_d_threshold`
- O5: integration keys in `homogenization.json` for `pre_outlier` / `post_outlier`

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
5. Scale critical value by `sigmaStar` unconditionally.

### Binary splitting -- min segment constraint
Both conditions must be satisfied before recursing:
- `(end - begin) >= minSegmentPoints`
- If timestamps available and `minSegmentSeconds > 0`:
  time span in seconds >= `minSegmentSeconds`
`minSegmentDuration` in JSON (e.g. "180d") is parsed to seconds and stored in
`minSegmentSeconds` by ConfigLoader. Overrides explicit `min_segment_seconds`.

### Homogenization direction -- SeriesAdjuster
Reference segment = **first segment** (leftmost). All subsequent segments are
adjusted toward the level of the first segment using prefix sums of detected shifts.
Rationale: the first measurement period is assumed correct; replacements/reconfigurations
occur later and introduce inhomogeneities.

### GapFiller -- gap detection
Two sources of gaps detected simultaneously:
- NaN value in Observation (time present, value missing).
- Time jump > expectedStep * gapThresholdFactor (entire rows absent).
Expected step = median of consecutive MJD differences (robust to outlier jumps).

### GapFiller -- MEDIAN_YEAR integration
GapFiller::Strategy::MEDIAN_YEAR is defined but blocked with ConfigException.
Integration deferred to O3 (OutlierCleaner) or a future GapFiller extension thread.

### MedianYearSeries -- profile structure
Flat vector of 366 * slotsPerDay medians. Slot index:
  slot = (doy - 1) * slotsPerDay + slotOfDay
doy in [1, 366], slotOfDay in [0, slotsPerDay).
slotsPerDay = round(1 / stepDays), clamped to [1, 24].
Sub-hourly resolution (step < 1h) rejected -- median year loses statistical meaning.

### HarmonicSeries -- POSTPONED
Will be implemented after loki_spectral (FFT -> dominant periods -> harmonic fit).

### Loader -- column indexing
`columns` values are 1-based from the **start of the line** (field 0 = column 1).
Time fields (controlled by `time_columns`) are excluded automatically via the
`fieldIdx < firstValueField` guard. Example: for a file with columns
`[date] [time] [value]` and `time_columns: [0, 1]`, use `columns: [3]`.

### PlotHomogeneity -- correction curve
`plotCorrectionCurve()` computes the same prefix sums as SeriesAdjuster and
plots them as a step function using gnuplot `w steps`. This shows the cumulative
correction applied to each segment relative to the first (reference) segment.

### loki::stats ACF -- already implemented
`loki::stats::acf(x, maxLag)` exists in `descriptive.hpp`. Do NOT reimplement
in any other module. Import `loki_core` and call directly.

### loki_outlier -- OutlierResult is two-level
`OutlierPoint` holds per-outlier detail (index, score, threshold, flags).
`OutlierResult` holds the summary (method, location, scale, n, nOutliers, vector of points).
Detectors return `OutlierResult`. OutlierCleaner fills `replacedValue` in each point
after running GapFiller and sets `flag = 2`.

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
- Every new library module follows the same structure as `loki_core` and
  `loki_homogeneity`: own `include/loki/<module>/` and `src/` directories,
  own `CMakeLists.txt`, linked as a CMake target.
- Time series input may have no periodic component (GNSS, non-climatological data).
  Each detector must behave sensibly when deseasonalization strategy is NONE.
- Sampling rate varies from milliseconds to 6 hours. Detectors must not assume
  any fixed time step. Threshold defaults should be robust across resolutions.
