# CLAUDE.md — Project Instructions for LOKI

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
- Start each task thread with a reference to this file: *"Working on LOKI — see CLAUDE.md"*.

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
- **All comments must use ASCII characters only** — no Unicode box-drawing characters
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
|   |       +-- io/
|   |           +-- loader.cpp
|   |           +-- dataManager.cpp
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
|   +-- CMakeLists.txt
|   +-- unit/
|   |   +-- core/
|   |   +-- homogeneity/
|   +-- integration/
|
+-- config/
|   +-- loki_homogeneity.json
|
+-- scripts/
|   +-- build.sh
|   +-- clean.sh
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

```bash
# From the LOKI repository root:

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
After first build, copy UCRT64 runtime DLLs next to the executable:

```bash
cp /c/msys64/ucrt64/bin/libgcc_s_seh-1.dll   build/debug/apps/loki/
cp /c/msys64/ucrt64/bin/libstdc++-6.dll       build/debug/apps/loki/
cp /c/msys64/ucrt64/bin/libwinpthread-1.dll   build/debug/apps/loki/
```

These DLLs are required because the shell PATH points to MINGW64 but the compiler
is from UCRT64. Without them the executable exits with code 127 (DLL not found).

### Running
```bash
./build/debug/apps/loki/loki.exe config/loki_homogeneity.json
```

### CMakePresets (future use with Ninja)
Presets in `CMakePresets.json` support both `MinGW Makefiles` and `Ninja` generators.
Ninja is not currently installed. To install it, download `ninja-win.zip` from
https://github.com/ninja-build/ninja/releases and place `ninja.exe` in
`C:/msys64/ucrt64/bin/`. Then use:

```bash
cmake --preset debug
cmake --build --preset debug
```

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

### DLL mismatch on Windows
The MINGW64 shell PATH includes `/mingw64/bin/` but the compiler and runtime are
in `/c/msys64/ucrt64/bin/`. Always copy the three UCRT64 DLLs next to the executable
after first build (see Build Instructions above).

---

## Notes and Reminders

- Data files (`.csv`, `.dat`, etc.) and plot outputs (`.eps`, `.png`) are **not** committed.
- Third-party dependencies are managed via CMake `FetchContent` -- no vendored source code.
- The build directory is always `build/` and is gitignored.
- `newmat` is replaced by `Eigen3` throughout the codebase.
- This file should be updated whenever architecture decisions change.
- Do NOT add any license/copyright block at the top of source files.
- gnuplot font: use 'Sans,12' not 'Helvetica,12' (Helvetica not available on Windows/UCRT64).
- gnuplot terminal: use 'noenhanced' to prevent underscore subscript rendering in titles.
- SeriesMetadata must be populated by Loader after loading (componentName, unit, stationId).
- Column names from file header may contain unit in brackets e.g. "WIG_SPEED[m/s]" --
  split into componentName + unit in Loader, sanitize filenames in Plot::outputPath().
- Plot output goes to OUTPUT/IMG/, temp files use .tmp_ prefix and are deleted after render.

## Implemented Modules

### loki::stats (loki_core)
- Free functions in namespace `loki::stats`
- Header: `libs/loki_core/include/loki/stats/descriptive.hpp`
- Source:  `libs/loki_core/src/stats/descriptive.cpp`
- NaN handling via `loki::NanPolicy` (defined in `loki/core/nanPolicy.hpp`)
- Key types: `SummaryStats`, `NanPolicy`
- Key functions: `summarize()`, `formatSummary()`, `acf()`, `hurstExponent()`
- `summarize()` signature:
  SummaryStats summarize(const std::vector<double>& x,
                         loki::NanPolicy policy = loki::NanPolicy::SKIP,
                         bool computeHurst = true);
- Controlled via `StatsConfig` in AppConfig (JSON key: "stats")

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

### TimeSeries -> std::vector<double> extraction
TimeSeries has no values() method. Extract raw values before calling
stats functions:
    std::vector<double> vals;
    vals.reserve(ts.size());
    for (const auto& obs : ts) { vals.push_back(obs.value); }

### summarize() and NaN
summarize() always counts NaN into nMissing regardless of policy.
NanPolicy::THROW in summarize() throws before computation if any NaN
is present. SKIP removes them silently. PROPAGATE is not meaningful
for summarize() -- use individual functions for that case.

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
- gnuplot font note: use `'Sans,12'` not `'Helvetica,12'` (not available on Windows/UCRT64).

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
- Private helpers: `_parseInput`, `_parseOutput`, `_parseHomogeneity`,
  `_parsePlots`, `_parseStats`, `_parseTimeFormat`, `_parseMergeStrategy`,
  `_resolvePath`.
- Internal `getOrDefault<T>()` template in anonymous namespace for safe
  JSON key access with fallback.
- JSON key names: `workspace`, `input.file`, `input.time_format`,
  `input.delimiter`, `input.comment_char`, `input.columns`,
  `input.merge_strategy`, `output.log_level`, `stats.enabled`,
  `stats.nan_policy`, `stats.hurst`, `plots.output_format`,
  `plots.time_format`, `plots.time_series`, `plots.comparison`,
  `plots.histogram`, `plots.acf`, `plots.qq_plot`, `plots.boxplot`,
  `homogeneity.method`, `homogeneity.significance_level`.

**`directoryScanner.hpp` / `directoryScanner.cpp`** -- implemented:
- `class DirectoryScanner` -- scans a directory for files with recognised
  extensions (`.txt`, `.csv`, `.dat`).
- `static std::vector<std::filesystem::path> scan(dir)` -- returns sorted
  list of matching files.

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
- Static `constexpr`: `MJD_UNIX_EPOCH`, `MJD_GPS_EPOCH`, `SECONDS_PER_DAY`,
  `SECONDS_PER_GPS_WEEK`, `MJD_MIN`, `MJD_MAX`.
- Built-in leap second table (last entry: 2017-01-01, +18 s).
- Private helpers: `_parseUtcString()`, `_calendarToMjd()`,
  `_mjdToCalendar()`, `_leapSecondsAt()`.
- Note: `TimeStamp` is in global namespace (not `namespace loki`) -- this
  is a known inconsistency to be cleaned up when refactoring to full
  `libs/` layout.

**`timeSeries.hpp` / `timeSeries.cpp`** -- fully implemented:
- `struct SeriesMetadata { stationId, componentName, unit, description }`.
- `struct Observation { TimeStamp time; double value{0.0}; uint8_t flag{0} }`.
- Free function `bool isValid(const Observation&) noexcept` -- NaN check
  via `value == value`.
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
- Internal: `m_isSorted` flag (lazy tracking), `_requireSorted()` helper.
- Note: `TimeSeries` has **no** `values()` method. Extract raw doubles
  manually before passing to stats functions:
  `for (const auto& obs : ts) { vals.push_back(obs.value); }`

---

### loki::stats

**`descriptive.hpp` / `descriptive.cpp`** -- fully implemented as free
functions in `namespace loki::stats`:

Central tendency: `mean()`, `median()`, `mode()`, `trimmedMean(fraction)`.

Dispersion: `variance(population=false)`, `stddev(population=false)`,
`mad()`, `iqr()`, `range()`, `cv()`.

Shape: `skewness()`, `kurtosis()` (excess, Fisher definition, normal=0).

Quantiles: `quantile(p)` (type 7, same as R/NumPy),
`fiveNumberSummary()` -> `std::array<double,5>`.

Bivariate: `covariance()`, `pearsonR()`, `spearmanR()`.

Time series specific: `autocorrelation(lag)`, `acf(maxLag)`,
`hurstExponent()` (R/S analysis, requires n >= 20).

Summary facade: `summarize(x, policy=SKIP, computeHurst=true)`
-> `SummaryStats`. `formatSummary(SummaryStats, label="")` -> `std::string`
(for logging/console, not serialisation).

`struct SummaryStats` fields: `n`, `nMissing`, `min`, `max`, `range`,
`mean`, `median`, `q1`, `q3`, `iqr`, `variance`, `stddev`, `mad`, `cv`,
`skewness`, `kurtosis`, `hurstExp` (NaN if n<20 or disabled).

All functions accept `NanPolicy policy` parameter (default `THROW` for
individual functions, `SKIP` for `summarize()`). Internal helper
`applyNanPolicy()` in anonymous namespace handles all three cases.

---

### loki::io

**`loader.hpp` / `loader.cpp`** -- fully implemented:
- `struct LoadResult { series, columnNames, filePath, linesRead, linesSkipped }`.
- `class Loader` -- parses a single delimited text file.
- `explicit Loader(const InputConfig&)`.
- `LoadResult load(filePath)` -- throws `FileNotFoundException`,
  `IoException`, `ParseException`.
- Detects column names from `"% Columns: NAME1[unit], NAME2"` comment line.
- Column names split into `componentName` + `unit` (bracket notation).
- Malformed data lines: skipped with `LOKI_WARNING`, parsing continues.
- Private: `_parseColumnHeader()`, `_parseTime()`, `_splitLine()`.
- Supports all `TimeFormat` values including `GPS_WEEK_SOW` (two-token).

**`dataManager.hpp` / `dataManager.cpp`** -- fully implemented:
- `class DataManager` -- orchestrates file discovery + loading.
- `explicit DataManager(const AppConfig&)`.
- `std::vector<LoadResult> load()` -- respects `InputMode` and
  `MergeStrategy`. On partial failure: logs error, continues, throws
  `DataException` only if **no** file loaded successfully.
- Private: `_collectFiles()`, `_loadFiles()`, `static _merge()`.

**`gnuplot.hpp` / `gnuplot.cpp`** -- fully implemented:
- `class Gnuplot` -- RAII wrapper around `FILE*` pipe to gnuplot process.
- Constructor opens `popen("gnuplot -persist", "w")`, throws `IoException`
  on failure.
- Destructor sends `"exit\n"` and calls `pclose()` -- always `noexcept`.
- Non-copyable, movable (move constructor + move `operator=` implemented).
- `void send(const std::string& cmd)` -- writes cmd + `\n` + `fflush`.
- `void operator()(const std::string& cmd)` -- syntactic sugar for `send()`.
- `bool isOpen()` -- checks `m_pipe != nullptr`.
- Platform: uses `_popen`/`_pclose` on Windows, `popen`/`pclose` on Linux.

**`plot.hpp` / `plot.cpp`** -- fully implemented:
- `class Plot` -- high-level plotting interface over `Gnuplot`.
- `explicit Plot(const AppConfig&)` -- creates `imgDir` if needed.
- Gnuplot opened **per call** (not held open between calls).
- Two overload families for every plot type:
  `TimeSeries` overloads (use metadata for labels/filenames) and
  `std::vector<double>` overloads (sequential index x-axis).
- Implemented plot types: `timeSeries()`, `comparison()` (two-panel),
  `acf()` (impulse + 95% confidence band), `histogram()` (with normal
  overlay), `qqPlot()`, `boxplot()` (with stats annotation).
- Output: `imgDir/<type>_<stationId>_<componentName>.<fmt>`.
- Temp files: `.tmp_<stem>` prefix, deleted after render (even on exception).
- Output formats: `png` (pngcairo, default), `eps` (postscript), `svg`.
- Font: `'Sans,12'`, flag `noenhanced` (prevents `_` subscript rendering).
- Private helpers: `effectiveTimeFormat()`, `outputPath()`,
  `writeTempData()`, `writeTempDataMulti()`, `removeTempFile()`,
  `toXY()`, `terminalCmd()`, `gnuplotTimeFmt()`,
  `computeAcf()`, `computeQQ()`, `validValues()`.
- Static constants: `DEFAULT_WIDTH_PX=1200`, `DEFAULT_HEIGHT_PX=600`,
  `COMPARE_HEIGHT_PX=900`, `CONF_95_COEFF=1.96`.

---

### apps/loki

**`main.cpp`** -- fully implemented pipeline:
1. Parse CLI args (`--help`, `--version`, `<config.json>`).
2. `ConfigLoader::load()` -- throws on failure, exits with `EXIT_FAILURE`.
3. `Logger::initDefault()` -- initialises logging to file + stdout.
4. `DataManager::load()` -- loads all configured input files.
5. `loki::stats::summarize()` -- per series if `cfg.stats.enabled`.
6. `Plot` -- per series, gated by `cfg.plots.*` flags.
7. All errors caught as `loki::LOKIException` or `std::exception`,
   logged and exit with `EXIT_FAILURE`.

---

### Not yet implemented

- `loki_homogeneity` -- library skeleton exists, no algorithm code yet.
- Unit tests (`tests/`) -- CMake scaffold ready (`LOKI_BUILD_TESTS` option),
  Catch2 via FetchContent, `ILogger` ready for mock injection. No test
  files written yet.
- `GnuplotWriter` / CSV export -- planned for `loki::io`.
- All other planned modules: `loki_outlier`, `loki_decomposition`,
  `loki_svd`, `loki_filter`, `loki_kalman`, `loki_arima`,
  `loki_spectral`, `loki_stationarity`, `loki_clustering`,
  `loki_qc`, `loki_regression`.