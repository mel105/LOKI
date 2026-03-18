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
  such as `─`, `│`, `└`, etc. Use plain `-` or `=` for decorative separators.
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
+-- CLAUDE.md                        <- this file
+-- CMakeLists.txt
+-- CMakePresets.json
+-- cmake/
|   +-- CompilerOptions.cmake
|   +-- Dependencies.cmake
|   +-- Version.cmake
+-- apps/
|   +-- loki/
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
|   |   |       +-- stats/
|   |   |       |   +-- descriptive.hpp
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
|   |       +-- stats/
|   |       |   +-- descriptive.cpp
|   |       +-- io/
|   |           +-- loader.cpp
|   |           +-- dataManager.cpp
|   |           +-- gnuplot.cpp
|   |           +-- plot.cpp
|   |
|   +-- loki_homogeneity/
|       +-- CMakeLists.txt
|       +-- include/
|       |   +-- loki/
|       |       +-- homogeneity/
|       |           +-- changePointDetector.hpp
|       |           +-- snhtDetector.hpp
|       |           +-- seriesAdjuster.hpp
|       |           +-- homogenizer.hpp
|       +-- src/
|           +-- changePointDetector.cpp
|           +-- snhtDetector.cpp
|           +-- seriesAdjuster.cpp
|           +-- homogenizer.cpp
|
+-- tests/
|   +-- CMakeLists.txt               <- single file, owns all test targets
|   +-- unit/
|   |   +-- core/
|   |   |   +-- test_exceptions.cpp
|   |   +-- timeseries/
|   |   |   +-- test_timeStamp.cpp
|   |   |   +-- test_timeSeries.cpp
|   |   +-- stats/
|   |       +-- test_descriptive.cpp
|   |       +-- test_summarize.cpp
|   +-- integration/
|       +-- CMakeLists.txt           <- commented out, no tests yet
|
+-- config/
|   +-- loki_homogeneity.json
|
+-- scripts/
|   +-- build.sh
|   +-- clean.sh
|   +-- run.sh
|   +-- test.sh
|
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
| `loki_homogeneity` | Change point detection, SNHT, Pettitt, Buishand -- **current focus** |
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

Prefer using the scripts in `scripts/` -- see Scripts section below.
Manual cmake invocation for reference only:

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
After first build, copy UCRT64 runtime DLLs next to executables:

```bash
cp /c/msys64/ucrt64/bin/libgcc_s_seh-1.dll   build/debug/apps/loki/
cp /c/msys64/ucrt64/bin/libstdc++-6.dll       build/debug/apps/loki/
cp /c/msys64/ucrt64/bin/libwinpthread-1.dll   build/debug/apps/loki/
# Also required next to test executables:
cp /c/msys64/ucrt64/bin/libgcc_s_seh-1.dll   build/debug/tests/
cp /c/msys64/ucrt64/bin/libstdc++-6.dll       build/debug/tests/
cp /c/msys64/ucrt64/bin/libwinpthread-1.dll   build/debug/tests/
```

`build.sh --copy-dlls` handles this automatically for both locations.

### CMakePresets (future use with Ninja)
Presets in `CMakePresets.json` support both `MinGW Makefiles` and `Ninja` generators.
Ninja is not currently installed. To install, place `ninja.exe` in
`C:/msys64/ucrt64/bin/` and use:

```bash
cmake --preset debug
cmake --build --preset debug
```

---

## Scripts

All scripts run from the **repository root**. See `scripts/` directory.

### build.sh -- configure and compile
```
./scripts/build.sh [debug|release] [--tests] [--copy-dlls]
```
Runs cmake configure + mingw32-make. Always full cmake configure (slow but reliable).
Use when: changing CMakeLists, adding a library, first build, switching presets.

| Command | When to use |
|---|---|
| `./scripts/build.sh` | Standard debug build without tests |
| `./scripts/build.sh --tests --copy-dlls` | First build with tests |
| `./scripts/build.sh release` | Release build |
| `./scripts/build.sh debug --tests` | Rebuild with tests, DLLs already present |

### run.sh -- incremental build and run
```
./scripts/run.sh [config]
```
Incremental build (changed files only) + runs `loki.exe`. cmake not re-invoked -- fast.
Copies missing DLLs automatically.

### clean.sh -- remove build directories
```
./scripts/clean.sh [debug|release|all|rebuild] [--tests]
```

| Command | When to use |
|---|---|
| `./scripts/clean.sh` | Remove both builds (before commit, on weird errors) |
| `./scripts/clean.sh debug` | Remove debug only |
| `./scripts/clean.sh rebuild` | Clean rebuild without tests |
| `./scripts/clean.sh rebuild --tests` | Clean rebuild with tests |

### test.sh -- run test suite
```
./scripts/test.sh [--rebuild] [--filter <pat>] [--verbose] [--list]
```
Incremental build + DLL check + ctest. If `build/debug/tests/` does not exist,
automatically calls `build.sh --tests --copy-dlls`.

| Command | When to use |
|---|---|
| `./scripts/test.sh` | Run all tests (incremental build) |
| `./scripts/test.sh --rebuild` | Clean rebuild + all tests |
| `./scripts/test.sh --filter exceptions` | Tests matching "exceptions" in name |
| `./scripts/test.sh --filter stats --verbose` | Full assertion output for stats tests |
| `./scripts/test.sh --list` | List all tests without running |

---

## Test Infrastructure

### Framework
Catch2 v3.5.2 via FetchContent. Test executables built in `build/debug/tests/`.

### CMakeLists structure
`tests/CMakeLists.txt` is the **single** file that owns all test targets.
It defines the `loki_add_test_exe` macro and registers every test exe via
`catch_discover_tests(...  DISCOVERY_MODE PRE_TEST)`.
`PRE_TEST` is required on Windows -- without it cmake tries to run the exe
during configure phase before DLLs are present, causing exit code 0xc0000139.

Do NOT add sub-CMakeLists in `tests/unit/` subdirectories -- they are not needed
and caused "find_package Catch2 not found" errors in the past.
`tests/integration/CMakeLists.txt` exists but is fully commented out.

### Adding a new test file
1. Create `tests/unit/<module>/test_<name>.cpp`.
2. Add a `loki_add_test_exe()` entry in `tests/CMakeLists.txt`:
```cmake
loki_add_test_exe(
    NAME    test_<name>
    SOURCES unit/<module>/test_<name>.cpp
    LIBS    loki_core          # or loki_homogeneity etc.
)
```
3. Rebuild with `./scripts/test.sh --rebuild`.

### Current test files and coverage

| File | Covers | Tests |
|---|---|---|
| `unit/core/test_exceptions.cpp` | Full exception hierarchy, message round-trip | 14 |
| `unit/timeseries/test_timeStamp.cpp` | Calendar, MJD, Unix, GPS conversions, comparisons | 12 |
| `unit/timeseries/test_timeSeries.cpp` | append, access, sorted flag, indexOf, slice, metadata | 21 |
| `unit/stats/test_descriptive.cpp` | mean, median, variance, IQR, skewness, pearsonR, acf, Hurst | 27 |
| `unit/stats/test_summarize.cpp` | summarize(), NanPolicy, Hurst flag, formatSummary() | 13 |

Total: **87 tests, 87 passing**.

### Known test design decisions

**IQR quantile variant** -- LOKI uses `h = p * (n-1)` (interpolate between
`s[floor(h)]` and `s[ceil(h)]`). For `{1,2,3,4,5}` this gives IQR = 2.0.
NumPy default also claims type-7 but uses a slightly different index formula
and returns IQR = 3.0 for the same input. LOKI's formula is self-consistent;
do not change the expected value in the test to match NumPy.

**GPS epoch test** -- `fromGpsTotalSeconds(0.0)` returns UTC 1980-01-05 23:59:50,
NOT 1980-01-06 00:00:00. This is correct: at the GPS epoch, GPS -- UTC = 10 s
(from the 1980-01-01 leap second table entry). The implementation subtracts
10 seconds, landing on the previous day. The test reflects this behaviour.

**IntelliSense false positive** -- VS Code / IntelliSense may show red squiggles
on `REQUIRE(t1 < t2)` for `TimeStamp` comparisons, complaining that
`operator<` is not found. This is a known IntelliSense quirk with C++20
`operator<=> = default` on types in the global namespace. GCC compiles and
links correctly; the tests pass. Will resolve itself when `TimeStamp` is
moved to `namespace loki` during the planned refactoring.

---

## Workspace Layout

The workspace root is an **absolute path** set in `config/loki_homogeneity.json`
under the `"workspace"` key. All other paths are relative to it:

```
<workspace>/
+-- INPUT/          <- input time series files
+-- OUTPUT/
    +-- LOG/        <- log files written by Logger
    +-- CSV/        <- exported results (future)
    +-- IMG/        <- gnuplot images (future)
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
        "method": "snht",
        "significance_level": 0.05
    }
}
```

The `"file"` path is resolved relative to `<workspace>/INPUT/` automatically.
Do **not** include `INPUT/` in the workspace path itself -- the loader appends it.

---

## Known Issues and Workarounds

### Unicode in source files (Windows GCC)
GCC on Windows can misparse tokens when source files contain UTF-8 box-drawing
characters (e.g. `─`, `│`) in comments. Symptoms: `expected unqualified-id before '&'`,
even on syntactically correct catch blocks.
**Rule: all comments must use ASCII characters only.**

### exceptions.hpp must be included early
`LOKIException` and its subclasses must be visible before any catch block.
`logger.hpp` includes `exceptions.hpp`, so including `logger.hpp` first is sufficient.
If a translation unit does not include `logger.hpp`, add:
```cpp
#include "loki/core/exceptions.hpp"
```

### namespace loki in .cpp files
All LOKI types live in `namespace loki`. Library `.cpp` files add:
```cpp
using namespace loki;
```
after the last `#include` so exception names and other types can be used unqualified.

### DLL mismatch on Windows (exit code 0xc0000139)
The MINGW64 shell PATH includes `/mingw64/bin/` but the compiler and runtime are
in `/c/msys64/ucrt64/bin/`. Three DLLs must be present next to **every** executable
that is run directly -- including test executables in `build/debug/tests/`.
`build.sh --copy-dlls` copies them to both `apps/loki/` and `tests/`.

### catch_discover_tests and DLLs (DISCOVERY_MODE PRE_TEST)
`catch_discover_tests()` without `DISCOVERY_MODE PRE_TEST` tries to run the test
executable during cmake generate phase to enumerate TEST_CASEs. On Windows this
fails with exit code 0xc0000139 because DLLs have not been copied yet.
Always use `DISCOVERY_MODE PRE_TEST` in `tests/CMakeLists.txt`.

### find_package(Catch2) in tests/
Do NOT call `find_package(Catch2 3 REQUIRED)` in `tests/CMakeLists.txt`.
Catch2 is declared via FetchContent in `cmake/Dependencies.cmake` and its
targets are available automatically. An explicit `find_package` call will
fail because `Catch2Config.cmake` is not on `CMAKE_PREFIX_PATH`.

### TimeStamp not in namespace loki
`TimeStamp` is currently in the global namespace -- known inconsistency left
over from the original codebase. Planned fix: move to `namespace loki` during
full `libs/` layout refactoring. Until then, IntelliSense may show false
positives for `operator<` on TimeStamp (GCC compiles correctly).

---

## Notes and Reminders

- Data files (`.csv`, `.dat`, etc.) and plot outputs (`.eps`, `.png`) are **not** committed.
- Third-party dependencies are managed via CMake `FetchContent` -- no vendored source code.
- The build directory is always `build/` and is gitignored.
- `newmat` is replaced by `Eigen3` throughout the codebase.
- This file should be updated whenever architecture decisions change.
- Do NOT add any license/copyright block at the top of source files.
- gnuplot font: use `'Sans,12'` not `'Helvetica,12'` (Helvetica not available on Windows/UCRT64).
- gnuplot terminal: use `'noenhanced'` to prevent underscore subscript rendering in titles.
- SeriesMetadata must be populated by Loader after loading (componentName, unit, stationId).
- Column names from file header may contain unit in brackets e.g. `"WIG_SPEED[m/s]"` --
  split into componentName + unit in Loader, sanitize filenames in `Plot::outputPath()`.
- Plot output goes to `OUTPUT/IMG/`, temp files use `.tmp_` prefix and are deleted after render.

---

## Implemented Modules

### loki::stats (loki_core)
- Free functions in namespace `loki::stats`
- Header: `libs/loki_core/include/loki/stats/descriptive.hpp`
- Source:  `libs/loki_core/src/stats/descriptive.cpp`
- NaN handling via `loki::NanPolicy` (defined in `loki/core/nanPolicy.hpp`)
- Key types: `SummaryStats`, `NanPolicy`
- Key functions: `summarize()`, `formatSummary()`, `acf()`, `hurstExponent()`
- `summarize()` signature:
  `SummaryStats summarize(const std::vector<double>& x,
                          loki::NanPolicy policy = loki::NanPolicy::SKIP,
                          bool computeHurst = true);`
- Controlled via `StatsConfig` in AppConfig (JSON key: `"stats"`)

---

## Design Decisions

### NanPolicy location
`loki::NanPolicy` is defined in `loki/core/nanPolicy.hpp` (not in
`descriptive.hpp`) to avoid a circular dependency between `loki_core`
and `loki::stats`. Any module needing NanPolicy includes only this
lightweight header.

### stats API style
Descriptive statistics are implemented as free functions in
`namespace loki::stats`, not as a class. This follows modern C++20
style and makes individual functions independently testable.

### IQR quantile formula
LOKI uses `h = p * (n-1)` (type-7 variant, interpolate between
`s[floor(h)]` and `s[ceil(h)]`). This is self-consistent throughout the
codebase. NumPy also calls its default "type-7" but uses a slightly different
index formula -- do not align LOKI's expected values to NumPy's output.

### TimeSeries -> std::vector<double> extraction
TimeSeries has no `values()` method. Extract raw values before calling
stats functions:
```cpp
std::vector<double> vals;
vals.reserve(ts.size());
for (const auto& obs : ts) { vals.push_back(obs.value); }
```

### summarize() and NaN
`summarize()` always counts NaN into `nMissing` regardless of policy.
`NanPolicy::THROW` in `summarize()` throws before computation if any NaN
is present. `SKIP` removes them silently. `PROPAGATE` is not meaningful
for `summarize()` -- use individual functions for that case.

---

## Implemented Code: loki_core (complete)

This section documents everything that is **already implemented and working**
in `loki_core`. Do not rewrite, duplicate, or replace any of this unless
explicitly asked. When adding new functionality, check here first.

---

### loki::core

**`exceptions.hpp`** -- full exception hierarchy, all classes implemented:
- `LOKIException` (base, inherits `std::exception`, holds `std::string m_message`)
- `DataException` -> `SeriesTooShortException`, `MissingValueException`
- `IoException` -> `FileNotFoundException`, `ParseException`
- `ConfigException`
- `AlgorithmException` -> `ConvergenceException`, `SingularMatrixException`

**`version.hpp`** -- `VERSION_MAJOR`, `VERSION_MINOR`, `VERSION_PATCH`,
`VERSION_STRING` as `constexpr` in `namespace loki`. Current: `0.1.0`.

**`nanPolicy.hpp`** -- `enum class NanPolicy { THROW, SKIP, PROPAGATE }`
in `namespace loki`. Intentionally in its own header to avoid circular
dependencies between `loki_core` and `loki::stats`.

**`logger.hpp` / `logger.cpp`** -- fully implemented:
- `enum class LogLevel : int { DEBUG=0, INFO=1, WARNING=2, ERROR=3 }`
- `class ILogger` -- abstract interface with pure virtual `log()`, virtual
  destructor; enables mock injection in tests.
- `class Logger : public ILogger` -- Meyers singleton, writes to file +
  stdout/stderr simultaneously, thread-safe via `std::mutex`.
- `static ILogger& instance()` -- returns custom instance if set, else
  default singleton.
- `static void setInstance(std::unique_ptr<ILogger>)` -- for test injection.
- `static void initDefault(logDir, prefix, minLevel)` -- convenience init.
- `void init(logDir, prefix, minLevel)` -- opens timestamped log file,
  creates directory if needed.
- Macros (outside namespace): `LOKI_DEBUG`, `LOKI_INFO`, `LOKI_WARNING`,
  `LOKI_ERROR` -- use `__func__` for caller name automatically.
- Log format: `[YYYY-MM-DD hh:mm:ss.mmm] [LEVEL  ] [caller_name          ] message`
- WARNING/ERROR go to `stderr`; DEBUG/INFO go to `stdout`.

**`config.hpp`** -- all config structs fully defined:
- `enum class TimeFormat { INDEX, GPS_TOTAL_SECONDS, GPS_WEEK_SOW, UTC, MJD, UNIX }`
- `enum class InputMode { SINGLE_FILE, FILE_LIST, SCAN_DIRECTORY }`
- `enum class MergeStrategy { SEPARATE, MERGE }`
- `struct InputConfig` -- mode, file, files, scanDir, timeFormat, delimiter,
  commentChar, columns (1-based), mergeStrategy.
- `struct OutputConfig` -- logLevel.
- `struct HomogeneityConfig` -- method (string), significanceLevel (double).
- `struct PlotConfig` -- outputFormat, timeFormat, flags: timeSeries,
  comparison, histogram, acf, qqPlot, boxplot.
- `struct StatsConfig` -- enabled, nanPolicy, hurst.
- `struct AppConfig` -- workspace, input, output, plots, homogeneity, stats,
  derived paths: logDir, csvDir, imgDir (all `std::filesystem::path`).

**`configLoader.hpp` / `configLoader.cpp`** -- fully implemented:
- `class ConfigLoader` -- static methods only (no instance state).
- `static AppConfig load(jsonPath)` -- validates existence, parses JSON,
  resolves all paths against workspace, populates all sub-configs.
- Required JSON key: `"workspace"`. All other keys optional with defaults
  and `LOKI_WARNING` when missing.

**`directoryScanner.hpp` / `directoryScanner.cpp`** -- implemented:
- `class DirectoryScanner` -- scans a directory for files with recognised
  extensions (`.txt`, `.csv`, `.dat`).
- `static std::vector<std::filesystem::path> scan(dir)` -- returns sorted list.

---

### loki::timeseries

**`timeStamp.hpp` / `timeStamp.cpp`** -- fully implemented:
- Internal representation: `double m_mjd` (Modified Julian Date, UTC).
- Constructors: default (MJD 0.0), calendar `(Y,M,D,h,m,s)`, UTC string.
- Factory methods: `fromMjd()`, `fromUnix()`, `fromGpsTotalSeconds()`,
  `fromGpsWeekSow()`.
- Getters: `year()`, `month()`, `day()`, `hour()`, `minute()`, `second()`,
  `mjd()`, `unixTime()`, `gpsTotalSeconds()`, `gpsWeek()`,
  `gpsSecondsOfWeek()`, `utcString()`.
- C++20 spaceship operator `<=>` = default (enables all 6 comparisons).
- Static `constexpr`: `MJD_UNIX_EPOCH=40587.0`, `MJD_GPS_EPOCH=44244.0`,
  `SECONDS_PER_DAY`, `SECONDS_PER_GPS_WEEK`, `MJD_MIN`, `MJD_MAX`.
- Built-in leap second table (last entry: 2017-01-01, +18 s, 28 entries total).
- GPS conversion note: `fromGpsTotalSeconds(0)` returns UTC 1980-01-05 23:59:50,
  not 1980-01-06, because the 10 s leap offset valid at that MJD is subtracted.
- Note: `TimeStamp` is in global namespace (not `namespace loki`) -- known
  inconsistency, to be fixed in full `libs/` refactoring.

**`timeSeries.hpp` / `timeSeries.cpp`** -- fully implemented:
- `struct SeriesMetadata { stationId, componentName, unit, description }`.
- `struct Observation { TimeStamp time; double value{0.0}; uint8_t flag{0} }`.
- Free function `bool isValid(const Observation&) noexcept` -- NaN check.
- `class TimeSeries` -- ordered sequence of Observations in `std::vector`.
- Construction: `TimeSeries()` default, `explicit TimeSeries(SeriesMetadata)`.
- Population: `append(time, value, flag=0)`, `reserve(n)`.
- Element access: `operator[](index)` (no bounds check),
  `at(index)` (throws `DataException`), `size()`, `empty()`.
- Time-based: `indexOf(t, toleranceMjd=1e-8)` -> `std::optional<size_t>`,
  `atTime(t, toleranceMjd)` -> `const Observation&`. Both require sorted
  series, throw `AlgorithmException` otherwise.
- Slicing: `slice(TimeStamp from, TimeStamp to)` (sorted required),
  `slice(size_t from, size_t to)` (half-open, no sort required).
- Iteration: `begin()` / `end()` return `cbegin()` / `cend()` -- read-only.
- Sorting: `sortByTime()` (stable sort), `isSorted()`.
- Metadata: `metadata()`, `setMetadata()`.
- No `values()` method -- extract manually (see Design Decisions above).

---

### loki::stats

**`descriptive.hpp` / `descriptive.cpp`** -- fully implemented as free
functions in `namespace loki::stats`:

Central tendency: `mean()`, `median()`, `mode()`, `trimmedMean(fraction)`.

Dispersion: `variance(population=false)`, `stddev(population=false)`,
`mad()`, `iqr()`, `range()`, `cv()`.

Shape: `skewness()`, `kurtosis()` (excess, Fisher definition, normal=0).

Quantiles: `quantile(p)` (type-7 variant, `h = p*(n-1)`),
`fiveNumberSummary()` -> `std::array<double,5>`.

Bivariate: `covariance()`, `pearsonR()`, `spearmanR()`.

Time series specific: `autocorrelation(lag)`, `acf(maxLag)`,
`hurstExponent()` (R/S analysis, requires n >= 20).

Summary facade: `summarize(x, policy=SKIP, computeHurst=true)`
-> `SummaryStats`. `formatSummary(SummaryStats, label="")` -> `std::string`.

`struct SummaryStats` fields: `n`, `nMissing`, `min`, `max`, `range`,
`mean`, `median`, `q1`, `q3`, `iqr`, `variance`, `stddev`, `mad`, `cv`,
`skewness`, `kurtosis`, `hurstExp` (NaN if n<20 or disabled).

---

### loki::io

**`loader.hpp` / `loader.cpp`** -- fully implemented.
**`dataManager.hpp` / `dataManager.cpp`** -- fully implemented.
**`gnuplot.hpp` / `gnuplot.cpp`** -- fully implemented.
**`plot.hpp` / `plot.cpp`** -- fully implemented.

(Full details unchanged -- see previous CLAUDE.md version if needed.)

---

### apps/loki

**`main.cpp`** -- fully implemented pipeline:
1. Parse CLI args (`--help`, `--version`, `<config.json>`).
2. `ConfigLoader::load()`.
3. `Logger::initDefault()`.
4. `DataManager::load()`.
5. `loki::stats::summarize()` per series if `cfg.stats.enabled`.
6. `Plot` per series gated by `cfg.plots.*` flags.
7. All errors caught as `loki::LOKIException` or `std::exception`.

---

### Not yet implemented

- `loki_homogeneity` -- library skeleton exists, no algorithm code yet.
- `GnuplotWriter` / CSV export -- planned for `loki::io`.
- All other planned modules: `loki_outlier`, `loki_decomposition`,
  `loki_svd`, `loki_filter`, `loki_kalman`, `loki_arima`,
  `loki_spectral`, `loki_stationarity`, `loki_clustering`,
  `loki_qc`, `loki_regression`.