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
  such as --, |, +, etc. Use plain - or = for decorative separators.
  Unicode in source files causes GCC on Windows to misparse tokens.
- Every class must have a Doxygen-style comment block explaining its purpose.
- Every public method must have a brief Doxygen comment.
- Inline comments should explain *why*, not *what*.

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
- All exception classes live in `namespace loki`.
- In `.cpp` files, add `using namespace loki;` after the last `#include`.
- In `apps/` (main), use fully qualified `loki::LOKIException` in catch blocks.
- `exceptions.hpp` must be included early -- it is included by `logger.hpp`.

### Where to catch
- In library code (`libs/`), throw -- do not catch and swallow.
- In `apps/` (main), catch and report cleanly to the user.

---

## Code Delivery

### Always use artifacts
- All code (`.cpp`, `.hpp`, `CMakeLists.txt`) must be delivered in **artifacts**, never in plain chat text.
- Each artifact contains exactly **one file**.
- The artifact title must match the file name.

### License header
- Do NOT add any license/copyright header block at the top of `.cpp` or `.hpp` files.

### No code duplication
- Before implementing any functionality, check if it already exists in `loki_core` or
  another module. Reuse and extend existing classes rather than duplicating logic.

### Robustness checklist before delivering code
- [ ] No raw owning pointers -- use `std::unique_ptr` / `std::shared_ptr` / value types.
- [ ] No `using namespace std;` in headers.
- [ ] All inputs validated, exceptions thrown where appropriate.
- [ ] No magic numbers -- use named constants.
- [ ] No compiler warnings with `-Wall -Wextra -Wpedantic`.
- [ ] Doxygen comments on all public interfaces.
- [ ] Comments in English, ASCII characters only.
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
- Every module is a separate CMake library with its own `include/` and `src/` tree.
- Include paths follow `#include <loki/<module>/<class>.hpp>`.
- No circular dependencies between modules.
- `loki_core` must not depend on any other loki module.

---

## Project Structure Reference

```
loki/
+-- CLAUDE.md
+-- CMakeLists.txt
+-- CMakePresets.json
+-- cmake/
+-- apps/
|   +-- loki/
|   +-- loki_homogeneity/
|   +-- loki_outlier/
|       +-- CMakeLists.txt
|       +-- main.cpp
+-- libs/
|   +-- loki_core/
|   |   +-- include/loki/
|   |       +-- core/         (exceptions, version, config, configLoader, logger, nanPolicy)
|   |       +-- timeseries/   (timeSeries, timeStamp, gapFiller)
|   |       +-- stats/        (descriptive, filter)
|   |       +-- io/           (loader, dataManager, gnuplot, plot)
|   +-- loki_outlier/
|   |   +-- CMakeLists.txt
|   |   +-- include/loki/outlier/
|   |   |   +-- outlierResult.hpp
|   |   |   +-- outlierDetector.hpp
|   |   |   +-- iqrDetector.hpp
|   |   |   +-- madDetector.hpp
|   |   |   +-- zScoreDetector.hpp
|   |   |   +-- outlierCleaner.hpp
|   |   |   +-- hatMatrixDetector.hpp   <- O4, planned
|   |   +-- src/
|   |       +-- outlierDetector.cpp
|   |       +-- iqrDetector.cpp
|   |       +-- madDetector.cpp
|   |       +-- zScoreDetector.cpp
|   |       +-- outlierCleaner.cpp
|   |       +-- hatMatrixDetector.cpp   <- O4, planned
|   +-- loki_homogeneity/
|       (see homogeneity section below)
+-- tests/
+-- config/
|   +-- homogenization.json
|   +-- outlier.json             <- O5, planned
+-- scripts/
+-- docs/
```

---

## Dependencies

| Library | Version | Purpose |
|---|---|---|
| `Eigen3` | 3.4.0 | Linear algebra, LSQ, SVD, Kalman |
| `nlohmann_json` | 3.11.3 | Configuration files |
| `Catch2` | 3.5.2 | Unit and integration tests |

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
cp /c/msys64/ucrt64/bin/libgcc_s_seh-1.dll   build/debug/apps/loki_outlier/
cp /c/msys64/ucrt64/bin/libstdc++-6.dll       build/debug/apps/loki_outlier/
cp /c/msys64/ucrt64/bin/libwinpthread-1.dll   build/debug/apps/loki_outlier/
```

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
Catch2 v3.5.2 via FetchContent.

### Adding a new test file
1. Create `tests/unit/<module>/test_<name>.cpp`.
2. Add entry in `tests/CMakeLists.txt`:
```cmake
loki_add_test_exe(
    NAME    test_<name>
    SOURCES unit/<module>/test_<name>.cpp
    LIBS    loki_core loki_outlier
)
```

### Current test coverage

| File | Covers | Tests |
|---|---|---|
| `unit/core/test_exceptions.cpp` | Full exception hierarchy | 14 |
| `unit/timeseries/test_timeStamp.cpp` | Calendar, MJD, Unix, GPS conversions | 12 |
| `unit/timeseries/test_timeSeries.cpp` | append, access, slice, metadata | 21 |
| `unit/stats/test_descriptive.cpp` | mean, median, variance, IQR, ACF, Hurst | 27 |
| `unit/stats/test_summarize.cpp` | summarize(), NanPolicy, formatSummary() | 13 |
| `unit/timeseries/test_gapFiller.cpp` | GapFiller: detectGaps, fill, edges, maxFillLength | 25 |
| `unit/homogeneity/test_medianYearSeries.cpp` | MedianYearSeries: profile, slots, NaN | 14 |
| `unit/homogeneity/test_seriesAdjuster.cpp` | SeriesAdjuster: shifts, edge cases | 9 |
| `unit/outlier/test_iqrDetector.cpp` | IqrDetector: construction, detection, edges | 9 |
| `unit/outlier/test_madDetector.cpp` | MadDetector: construction, detection, edges | 9 |
| `unit/outlier/test_zScoreDetector.cpp` | ZScoreDetector: construction, detection, edges | 10 |
| `unit/outlier/test_outlierCleaner.cpp` | OutlierCleaner: pipeline, seasonal, metadata | 12 |

Total: **175 tests** (target -- not all confirmed passing yet, build pending).

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

### TimeStamp construction from MJD
Use `TimeStamp::fromMjd(double)` -- there is no `setMjd()` method.

### ::TimeStamp in loki namespace code
TimeStamp is not in namespace loki. Always qualify as `::TimeStamp`.

### Config struct with enum member -- GCC 13 aggregate init
Use explicit constructor with member-initializer list.
Use `Config cfg = Config{}` as default argument.

### Loader -- time_columns and space delimiter
When delimiter is space/tab, `_splitLine` uses `operator>>` for word extraction.
`time_columns` (0-based) controls which fields form the time token.
`columns` (1-based from start of line) selects value columns.

### plotHomogeneity -- fwdSlash helper
Gnuplot on Windows requires forward slashes. Use `fwdSlash()` static helper.

### SeriesAdjuster -- reference segment
Reference = **last (rightmost) segment**. All previous segments adjusted
left-to-right using cumulative shift sums.

### OutlierCleaner -- MEDIAN_YEAR and NONE not permitted as fillStrategy
Both throw ConfigException on construction. Use LINEAR, FORWARD_FILL, or MEAN.

---

## Implemented Modules

### loki::core -- complete
- `exceptions.hpp` -- full hierarchy
- `version.hpp` -- `0.1.0`
- `nanPolicy.hpp` -- `NanPolicy { THROW, SKIP, PROPAGATE }`
- `logger.hpp / .cpp` -- Meyers singleton, file + stdout/stderr, macros
- `config.hpp` -- all config structs
- `configLoader.hpp / .cpp` -- JSON loader, path resolution, `_parseDuration()`

### loki::timeseries -- complete
- `timeStamp.hpp / .cpp` -- MJD internal, GPS/UTC/Unix conversions
  - Construction from MJD: `TimeStamp::fromMjd(double)` (factory method, not setMjd)
- `timeSeries.hpp / .cpp` -- Observation, SeriesMetadata, append, slice, indexOf
- `gapFiller.hpp / .cpp` -- LINEAR, FORWARD_FILL, MEAN, NONE strategies

### loki::stats -- complete
- `descriptive.hpp / .cpp` -- mean, median, variance, IQR, MAD, ACF, Hurst, summarize()
- `filter.hpp / .cpp` -- movingAverage, EMA, WMA

### loki::io -- complete
- `loader.hpp / .cpp`, `dataManager.hpp / .cpp`, `gnuplot.hpp / .cpp`, `plot.hpp / .cpp`

### loki_homogeneity -- complete (H1-H10)
- Full pipeline: ChangePointDetector, MultiChangePointDetector, MedianYearSeries,
  Deseasonalizer, SeriesAdjuster, Homogenizer, PlotHomogeneity
- `apps/loki_homogeneity/main.cpp` -- full pipeline app

### loki_outlier -- O1-O3 complete, O4-O5 planned

#### O1-O3 complete
- `outlierResult.hpp` -- OutlierPoint + OutlierResult structs
- `outlierDetector.hpp / .cpp` -- abstract base class (Template Method pattern)
- `iqrDetector.hpp / .cpp` -- IQR-based detection (multiplier default 1.5)
- `madDetector.hpp / .cpp` -- MAD-based detection (multiplier default 3.0, normalised by 0.6745)
- `zScoreDetector.hpp / .cpp` -- Z-score detection (threshold default 3.0)
- `outlierCleaner.hpp / .cpp` -- full pipeline orchestration
  - `clean(series)` -- no seasonal component
  - `clean(series, seasonal)` -- with precomputed seasonal vector from Deseasonalizer
  - Returns `CleanResult { cleaned, residuals, detection }`
  - Internally: subtract seasonal -> detect -> mark NaN -> GapFiller -> reconstruct
- `libs/loki_outlier/CMakeLists.txt` -- library target

#### Tests written (build pending)
- `test_iqrDetector.cpp` -- 9 tests
- `test_madDetector.cpp` -- 9 tests
- `test_zScoreDetector.cpp` -- 10 tests
- `test_outlierCleaner.cpp` -- 12 tests

---

## Planned Thread Sequence for loki_outlier

### Pipeline design
```
Input TimeSeries
      |
      v
[1. Deseasonalize]     -- uses Deseasonalizer from loki_homogeneity (or NONE)
      |                -- MEDIAN_YEAR for hourly+ climate data
      |                -- MOVING_AVERAGE for ms/s sensor data
      v                -- NONE for GNSS / aperiodic
[2. Detect outliers]   -- IQR | MAD | Z-score | hat_matrix (O4)
      |
      v
[3. Replace outliers]  -- mark as NaN -> GapFiller (LINEAR | FORWARD_FILL | MEAN)
      |
      v
[4. Reconstruct]       -- add seasonal component back
      |
      v
Output TimeSeries (cleaned)
```

Deseasonalization strategy is chosen via config JSON -- OutlierCleaner does NOT
choose automatically. The caller (app or Homogenizer) runs Deseasonalizer and
passes `seasonal` vector to `OutlierCleaner::clean(series, seasonal)`.

### Thread plan

| Thread | Scope | Status |
|---|---|---|
| O1 | Architecture, OutlierResult, CMakeLists.txt | DONE |
| O2 | IqrDetector, MadDetector, ZScoreDetector | DONE |
| O3 | OutlierCleaner (pipeline orchestration) | DONE |
| O4 | HatMatrixDetector (leverage, AR model, DEH) | planned |
| O5 | apps/loki_outlier, outlier.json, integrate into loki_homogeneity | next |

---

## O4 -- HatMatrixDetector design notes

### Reference
- Hau & Tong (1989): "A practical method for outlier detection in autoregressive
  time series modelling", Stochastic Hydrology and Hydraulics 3, 241-260.
- Author's PhD dissertation, section 5.3.1: applied to tropospheric delay time series.

### Algorithm (to be implemented)
1. Build design matrix Gamma (n x p) from residuals:
   - Row t: z_t = [y_{t-1}, y_{t-2}, ..., y_{t-p}]
   - First p rows have incomplete lagged values -- handle by padding with zeros
     or skipping (configurable, default: skip first p rows).
2. Compute diagonal elements of hat matrix (DEH):
   h_t = z_t^T (Gamma^T Gamma)^{-1} z_t
   Use Eigen ColPivHouseholderQR for numerical stability.
3. Compute n*h_t as Mahalanobis distance approximation.
4. Flag point t as outlier if n*h_t > chi2_threshold.

### Outlier identification rule (from paper)
- If h_{t-1} is small AND h_t is large -> Y_{t-1} is the outlier.
- Consecutive large h_t values suggest innovation outlier (batch).
- Score stored in OutlierPoint::score = n * h_t.
- OutlierPoint::threshold = chi2_threshold.

### Parameters
```cpp
struct Config {
    int    arOrder{2};             // AR model order p (default 2, practical choice)
    double chi2Threshold{0.0};     // 0.0 = auto: chi2_p(alpha=0.01)
    double alpha{0.01};            // significance level for chi2 critical value
    bool   skipFirstP{true};       // skip first p rows (incomplete lag vectors)
};
```

### Chi-squared critical value
- Under H0 (no outlier, Gaussian AR): n*h_t ~ chi2_p asymptotically.
- Critical value: chi2_p at alpha=0.01 (e.g. p=2 -> 9.21, p=4 -> 13.28).
- Computed via Eigen or a simple lookup table for p in [1..10].
- Override via explicit `chi2Threshold` in config.

### Difference from other detectors
- HatMatrixDetector does NOT inherit from OutlierDetector base class.
  Reason: it does not follow the location/scale/score pattern.
  It computes leverage directly from the design matrix.
  It has its own `detect(residuals)` returning `OutlierResult`.
- Score = n * h_t (Mahalanobis distance approximation).
- Requires minimum n > p + 2, throws SeriesTooShortException otherwise.

### Deseasonalization for hat matrix
- Hat matrix operates on residuals (same as other detectors).
- For GNSS/sensor data: use NONE or MOVING_AVERAGE deseasonalization.
- For climate data: use MEDIAN_YEAR.
- AR order p should reflect temporal autocorrelation structure of the residuals.

### Eigen usage
- `Eigen::MatrixXd` for Gamma and (Gamma^T Gamma).
- `Eigen::ColPivHouseholderQR` for solving / inverting (Gamma^T Gamma).
- Alternative: `Eigen::LDLT` for symmetric positive definite matrices.
- For large n (>10000): use the recursive QR formula from Section 7 of Hau & Tong.

---

## O5 -- apps/loki_outlier requirements

### What Claude needs from the user at thread start
1. **`apps/loki_homogeneity/main.cpp`** -- reference for app structure, CLI pattern,
   ConfigLoader usage, DataManager usage, Logger init, CSV export pattern.
2. **`config/homogenization.json`** -- reference for JSON schema style.
3. **`libs/loki_core/include/loki/core/config.hpp`** -- to see AppConfig structure
   and understand what new config keys need to be added for outlier app.
4. **`libs/loki_core/src/core/configLoader.cpp`** -- to see how JSON keys are parsed,
   so outlier.json keys can be added consistently.
5. **`libs/loki_homogeneity/homogenizer.hpp` + `homogenizer.cpp`** -- to wire
   OutlierCleaner into the existing preOutlier/postOutlier placeholder steps.

### O5 deliverables
1. `apps/loki_outlier/main.cpp` -- CLI outlier pipeline app
2. `apps/loki_outlier/CMakeLists.txt`
3. `config/outlier.json` -- config schema for outlier app
4. Updates to `libs/loki_core/include/loki/core/config.hpp` -- new OutlierAppConfig
5. Updates to `libs/loki_core/src/core/configLoader.cpp` -- parse outlier.json keys
6. Updated `libs/loki_homogeneity/homogenizer.hpp` -- replace OutlierConfig placeholder
   with real OutlierCleaner integration
7. Updated `libs/loki_homogeneity/homogenizer.cpp` -- run OutlierCleaner in steps 2 and 4
8. Updated `libs/loki_homogeneity/CMakeLists.txt` -- add `loki_outlier` as dependency

### O5 pipeline (apps/loki_outlier/main.cpp)
```
1. Parse CLI args (<config.json> [--help] [--version])
2. ConfigLoader::load()
3. Logger::initDefault()
4. DataManager::load()
5. For each series:
   a. (optional) Deseasonalizer::deseasonalize() -- if strategy != NONE
   b. Choose detector: IqrDetector | MadDetector | ZScoreDetector
   c. OutlierCleaner::clean(series) or clean(series, seasonal)
   d. writeCsv() -- original, residuals, cleaned, outlier flags
   e. (future) plotOutliers()
6. Log summary: n outliers detected/replaced per series
```

### O5 outlier.json schema (planned)
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
            "strategy": "linear | forward_fill | mean",
            "max_fill_length": 0
        }
    }
}
```

### Homogenizer integration (O5)
- `loki_homogeneity` CMakeLists.txt: add `loki_outlier` to `target_link_libraries`.
- `HomogenizerConfig::OutlierConfig` -- extend with `method` string and per-method params.
- `homogenizer.cpp` steps 2 and 4: if `enabled=true`, construct detector by method string,
  run `OutlierCleaner::clean(working, deseasResult.seasonal)`, replace `working` series.
- Deseasonalizer runs in step 3 and also provides `seasonal` for OutlierCleaner.
- Pre-outlier (step 2): runs on raw series with NONE deseasonalization (no seasonal yet).
- Post-outlier (step 4): runs on deseasonalized residuals, reconstructs before step 5.

---

## Design Decisions

### T-statistics computation -- O(n) via prefix sums
Old code used O(n^2) nested loops. New implementation uses prefix sums.
Critical for series with n > 10000 (NWM data had n > 36000).

### sigmaStar -- Antoch et al. noise correction
After detecting the candidate change point at index m:
1. Center each segment around its own mean -> VALSIGMA.
2. Compute ACF of VALSIGMA using `loki::stats::acf()`.
3. Estimate f0 using Bartlett window of width L = ceil(n^(1/3)).
4. sigmaStar = sqrt(|f0|).
5. Scale critical value by sigmaStar unconditionally.

### Binary splitting -- min segment constraint
Both conditions must be satisfied before recursing:
- `(end - begin) >= minSegmentPoints`
- If timestamps available and minSegmentSeconds > 0:
  time span in seconds >= minSegmentSeconds

### Homogenization direction -- SeriesAdjuster
Reference segment = **last (rightmost)**. All previous segments adjusted
left-to-right using cumulative shift sums.

### GapFiller -- gap detection
Two sources of gaps: NaN value, or time jump > expectedStep * gapThresholdFactor.
Expected step = median of consecutive MJD differences.

### OutlierDetector -- Template Method pattern
Base class `OutlierDetector` implements `detect()`. Derived classes supply:
`computeLocation()`, `computeScale()`, `computeScore()`, `threshold()`, `methodName()`.
Scale == 0 (all identical values) -> empty result, no exception.

### OutlierCleaner -- seasonal vector passed by caller
OutlierCleaner does NOT run Deseasonalizer itself. Caller (app or Homogenizer)
runs Deseasonalizer and passes `seasonal` vector. This keeps loki_outlier free
of any dependency on loki_homogeneity.

### Deseasonalization strategy selection
Strategy chosen via config JSON. No automatic detection of data domain.
Future: `time_domain` setting will provide defaults per data type (ms, s, hourly, daily).

### loki::stats functions reused in loki_outlier
All detectors call `loki::stats::median()`, `loki::stats::iqr()`,
`loki::stats::mad()`, `loki::stats::mean()`, `loki::stats::stddev()`
with `NanPolicy::SKIP`. No reimplementation in loki_outlier.

### HatMatrixDetector -- standalone (not inheriting OutlierDetector)
Hat matrix detection is based on leverage (design matrix geometry), not on
location/scale/score. Therefore HatMatrixDetector does not inherit from
OutlierDetector. It has its own `detect(residuals)` returning `OutlierResult`.

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
- Every new library module follows the same structure as `loki_core`.
- Time series input may have no periodic component (GNSS, non-climatological data).
- Sampling rate varies from milliseconds to 6 hours. Detectors must not assume
  any fixed time step.