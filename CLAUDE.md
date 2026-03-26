# CLAUDE.md -- Project Instructions for LOKI

This file contains instructions for Claude when working on the LOKI project.
It should be placed in the root of the repository and kept up to date.

---

## Project Overview

LOKI is a professional C++ toolkit for statistical analysis of time series data.
The long-term goal is a modular ecosystem of analysis libraries with a GUI/IDE frontend.
Current focus: outlier detection (`loki_outlier`) and homogeneity testing (`loki_homogeneity`).

---

## Communication Rules

### No questionnaire widgets
- Claude must NEVER use the `ask_user_input` widget or any questionnaire/poll tool.
- All clarifying questions must be asked as plain text in the conversation.
- This rule is absolute and applies to every thread.

---

## Workflow Rules

### One task per conversation thread
- Each new feature, refactoring task, or module gets its **own conversation thread**.
- Start each task thread with a reference to this file: *"Working on LOKI -- see CLAUDE.md"*.

### Before writing any code
- Discuss the approach first. Do not jump into implementation without agreeing on design.
- If the task is ambiguous, ask clarifying questions (as plain text) before starting.
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
- **All comments must use ASCII characters only** -- no Unicode box-drawing characters.
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

## Plot Output Naming Convention

All plot output files follow this naming convention:
```
[program]_[dataset]_[parameter]_[plottype].[format]
```

- **program** : `core` | `homogeneity` | `outlier`
- **dataset** : input filename stem without extension (e.g. `CLIM_DATA_EX1`)
- **parameter** : series `componentName` from metadata (e.g. `col_3`, `dN`, `temperature`)
- **plottype** : descriptive short name (e.g. `original`, `deseas`, `cp`, `boxplot`)
- **format** : `png` | `eps` | `svg`

Examples:
```
core_CLIM_DATA_EX1_col_3_timeseries.png
core_CLIM_DATA_EX1_col_3_boxplot.png
homogeneity_CLIM_DATA_EX1_col_3_original.png
homogeneity_CLIM_DATA_EX1_col_3_deseas.png
homogeneity_CLIM_DATA_EX1_col_3_cp.png
homogeneity_CLIM_DATA_EX1_col_3_outlier_overlay.png
homogeneity_CLIM_DATA_EX1_col_3_residuals_with_bounds.png
outlier_CLIM_DATA_EX1_col_3_original_with_outliers.png
outlier_CLIM_DATA_EX1_col_3_cleaned.png
outlier_CLIM_DATA_EX1_col_3_residuals_with_bounds.png
```

### Rules for plotters
- `loki::Plot` (loki_core) uses prefix `core_`.
- `PlotHomogeneity` (loki_homogeneity) uses prefix `homogeneity_`.
- `PlotOutlier` (loki_outlier) uses the `programName` constructor argument:
  - `"outlier"` when called from standalone `apps/loki_outlier`
  - `"homogeneity"` when called from `PlotHomogeneity::plotAll()` inside the homogeneity pipeline
- The `_datasetName()` helper in `PlotOutlier` extracts the stem from `cfg.input.file`.
- The `_baseName()` helper in `PlotHomogeneity` extracts the stem from `cfg.input.file`.
- Temporary data files use `.tmp_` prefix and are deleted immediately after gnuplot runs.
- Gnuplot on Windows requires forward slashes -- use `fwdSlash()` helper on all paths.
- gnuplot terminal: `pngcairo noenhanced font 'Sans,12'`

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
```

### Rules
- Every module is a separate CMake library with its own `include/` and `src/` tree.
- Include paths follow `#include <loki/<module>/<class>.hpp>`.
- No circular dependencies between modules.
- `loki_core` must not depend on any other loki module.
- `loki_outlier` must not depend on `loki_homogeneity`.

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
|   |   +-- CMakeLists.txt          (target: homogenization)
|   |   +-- main.cpp
|   +-- loki_outlier/
|       +-- CMakeLists.txt          (target: loki_outlier_app, OUTPUT_NAME: loki_outlier)
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
|   |   |   +-- plotOutlier.hpp
|   |   +-- src/
|   |       +-- outlierDetector.cpp
|   |       +-- iqrDetector.cpp
|   |       +-- madDetector.cpp
|   |       +-- zScoreDetector.cpp
|   |       +-- outlierCleaner.cpp
|   |       +-- plotOutlier.cpp
|   +-- loki_homogeneity/
|       +-- CMakeLists.txt          (links loki_core + loki_outlier)
|       +-- include/loki/homogeneity/
|       |   +-- changePointResult.hpp
|       |   +-- changePointDetector.hpp
|       |   +-- multiChangePointDetector.hpp
|       |   +-- medianYearSeries.hpp
|       |   +-- deseasonalizer.hpp
|       |   +-- seriesAdjuster.hpp
|       |   +-- homogenizer.hpp
|       |   +-- plotHomogeneity.hpp
|       +-- src/
+-- tests/
+-- config/
|   +-- homogenization.json
|   +-- outlier.json
+-- scripts/
|   +-- loki.sh                     (build/clean/run/test unified script)
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
# Build all apps (debug)
./scripts/loki.sh build all --copy-dlls

# Build specific app
./scripts/loki.sh build homogenization --copy-dlls
./scripts/loki.sh build outlier --copy-dlls

# Run
./scripts/loki.sh run homogenization
./scripts/loki.sh run outlier

# Test
./scripts/loki.sh test --rebuild
```

### App registry in loki.sh
| Key | Executable | Default config |
|---|---|---|
| `loki` | `apps/loki/loki.exe` | `config/loki_homogeneity.json` |
| `homogenization` | `apps/loki_homogeneity/homogenization.exe` | `config/homogenization.json` |
| `outlier` | `apps/loki_outlier/loki_outlier.exe` | `config/outlier.json` |

### CMake target name collision
`loki_outlier` is already the library target name. The executable target must be
named `loki_outlier_app` with `OUTPUT_NAME "loki_outlier"` set via `set_target_properties`.

### Runtime DLLs (Windows)
Three DLLs must be present next to every executable:
`libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`
`--copy-dlls` flag in `loki.sh` handles this automatically.

---

## Implemented Modules

### loki_core -- complete
- `exceptions.hpp` -- full hierarchy
- `version.hpp` -- `0.1.0`
- `nanPolicy.hpp` -- `NanPolicy { THROW, SKIP, PROPAGATE }`
- `logger.hpp / .cpp` -- Meyers singleton, file + stdout/stderr, macros
- `config.hpp` -- all config structs (see Config Structs section)
- `configLoader.hpp / .cpp` -- JSON loader, path resolution
- `timeStamp.hpp / .cpp` -- MJD internal, GPS/UTC/Unix conversions
- `timeSeries.hpp / .cpp` -- Observation, SeriesMetadata, append, slice, indexOf
- `gapFiller.hpp / .cpp` -- LINEAR, FORWARD_FILL, MEAN, NONE, MEDIAN_YEAR strategies
- `descriptive.hpp / .cpp` -- mean, median, variance, IQR, MAD, ACF, Hurst, summarize()
- `filter.hpp / .cpp` -- movingAverage, EMA, WMA
- `loader.hpp / .cpp`, `dataManager.hpp / .cpp`
- `gnuplot.hpp / .cpp` -- RAII gnuplot pipe wrapper
- `plot.hpp / .cpp` -- timeSeries, histogram, ACF, QQ, boxplot, comparison

### loki_outlier -- complete (O1-O5)
- `outlierResult.hpp` -- `OutlierPoint` + `OutlierResult` structs
- `outlierDetector.hpp / .cpp` -- abstract base (Template Method pattern)
- `iqrDetector.hpp / .cpp` -- IQR detection (multiplier default 1.5)
- `madDetector.hpp / .cpp` -- MAD detection (multiplier default 3.0, normalised by 0.6745)
- `zScoreDetector.hpp / .cpp` -- Z-score detection (threshold default 3.0)
- `outlierCleaner.hpp / .cpp` -- pipeline: subtract seasonal -> detect -> NaN -> GapFiller -> reconstruct
- `plotOutlier.hpp / .cpp` -- diagnostic plots for outlier pipeline

### loki_homogeneity -- complete (H1-H8, O5 integration)
- Full pipeline: GapFiller -> pre-outlier -> Deseasonalizer -> post-outlier ->
  MultiChangePointDetector -> SeriesAdjuster
- `homogenizer.hpp / .cpp` -- orchestrates full pipeline
- `plotHomogeneity.hpp / .cpp` -- all diagnostic plots including outlier overlay

---

## Config Structs (config.hpp)

### AppConfig structure
```cpp
struct AppConfig {
    workspace, input, output, plots,
    HomogeneityConfig homogeneity,   // for apps/loki_homogeneity
    StatsConfig stats,
    OutlierConfig outlier,           // for apps/loki_outlier
    logDir, csvDir, imgDir           // derived paths
};
```

### OutlierFilterConfig (used inside HomogeneityConfig)
Fields: `enabled`, `method` (mad_bounds|iqr|mad|zscore), `madMultiplier`,
`iqrMultiplier`, `zscoreThreshold`, `replacementStrategy`, `maxFillLength`.

### OutlierConfig (used by apps/loki_outlier)
Nested sections: `DeseasonalizationSection`, `DetectionSection`, `ReplacementSection`.

### homogenization.json -- outlier sections
```json
"pre_outlier": {
    "enabled": false,
    "method": "mad_bounds",
    "mad_multiplier": 5.0,
    "iqr_multiplier": 1.5,
    "zscore_threshold": 3.0,
    "replacement_strategy": "linear",
    "max_fill_length": 0
},
"post_outlier": {
    "enabled": false,
    "method": "mad",
    "mad_multiplier": 3.0,
    ...
}
```

---

## HomogenizerConfig -- OutlierConfig mapping

In `apps/loki_homogeneity/main.cpp`, `buildHomogenizerConfig()` maps
`loki::OutlierFilterConfig` (from `AppConfig`) to `loki::homogeneity::OutlierConfig`
via a local `mapOutlier()` lambda. Default MAD multipliers: pre=5.0 (coarse), post=3.0 (fine).

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
`TimeStamp` is not in namespace `loki`. Always qualify as `::TimeStamp`.

### Config struct with enum member -- GCC 13 aggregate init
Use explicit constructor with member-initializer list.

### CMake target name collision
`loki_outlier` is already the static library. The app executable target must be
named `loki_outlier_app` with `OUTPUT_NAME "loki_outlier"`.

### Deseasonalizer -- MEDIAN_YEAR requires profileLookup
`Deseasonalizer::Config` has no `medianYearMinYears` field. For MEDIAN_YEAR strategy,
build `MedianYearSeries` separately and pass it as a `profileLookup` lambda:
```cpp
MedianYearSeries mys{series, mysCfg};
auto lookup = [&mys](const ::TimeStamp& t) { return mys.valueAt(t); };
dsResult = deseasonalizer.deseasonalize(series, lookup);
```

### OutlierCleaner constructor order
`OutlierCleaner(Config cfg, const OutlierDetector& detector)` -- Config first, detector second.
Detector is held by non-owning pointer and must outlive the cleaner.

### OutlierResult field names
`result.detection.points` (not `.outliers`), `result.detection.nOutliers` (not `.outliers.size()`).

### PlotOutlier -- homogeneity pipeline programName
When `PlotOutlier` is instantiated from `PlotHomogeneity::plotAll()`, use `"homogeneity"`
as `programName` so output files carry the `homogeneity_` prefix.
When instantiated from `apps/loki_outlier/main.cpp`, use `"outlier"`.

### plot.cpp -- makeStem helper
`makeStem()` must be defined BEFORE the first `Plot::` method that uses it.
It is a `static` free function in the anonymous namespace of `plot.cpp`.

---

## Planned Modules

| Module | Description | Status |
|---|---|---|
| `loki_homogeneity` | Change point detection, SNHT, homogenization | complete |
| `loki_outlier` | IQR, MAD, Z-score, hat matrix, OutlierCleaner | complete (O1-O5) |
| `loki_decomposition` | Trend, seasonality, residuals (STL, classical) | planned |
| `loki_svd` | SVD/PCA decomposition | planned |
| `loki_filter` | Butterworth, Savitzky-Golay | planned |
| `loki_kalman` | Kalman filter, smoother, EKF | planned |
| `loki_arima` | AR, ARMA, ARIMA, SARIMA | planned |
| `loki_spectral` | FFT, power spectrum, Lomb-Scargle | planned |
| `loki_stationarity` | ADF, KPSS, unit root tests | planned |
| `loki_clustering` | k-means, DBSCAN, Hungarian algorithm | planned |
| `loki_qc` | Quality control, gap filling, flagging | planned |
| `loki_regression` | Linear, polynomial, robust regression | planned |

---

## Next Steps

- **Next thread:** Ladit vysledky homogenizacie -- pipeline dava nespravne vysledky.
  Skontrolovat desezonalizaciu, detekciu change pointov a adjustaciu.
- **O4 (planned):** `HatMatrixDetector` -- leverage-based outlier detection (Hau & Tong 1989).
- **O5 tests (planned):** Unit testy pre `loki_outlier` (build pending).

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
- Sampling rate varies from milliseconds to 6 hours. Detectors must not assume any fixed time step.