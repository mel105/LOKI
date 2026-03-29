# CLAUDE.md -- Project Instructions for LOKI

This file contains instructions for Claude when working on the LOKI project.
It should be placed in the root of the repository and kept up to date.

---

## Project Overview

LOKI is a professional C++ toolkit for statistical analysis of time series data.
The long-term goal is a modular ecosystem of analysis libraries with a GUI/IDE frontend.
Current focus: regression analysis (`loki_regression`) -- build, debug, and test.

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
- In `apps/` (main), `using namespace loki;` after includes covers most types.
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
- [ ] Unused parameters marked with `/*paramName*/` syntax.
- [ ] No `M_PI` -- use `std::numbers::pi` (C++20).

---

## Plot Output Naming Convention

All plot output files follow this naming convention:
```
[program]_[dataset]_[parameter]_[plottype].[format]
```

- **program** : `core` | `homogeneity` | `outlier` | `filter` | `regression`
- **dataset** : input filename stem without extension (e.g. `CLIM_DATA_EX1`)
- **parameter** : series `componentName` from metadata (e.g. `col_3`, `dN`, `temperature`)
- **plottype** : descriptive short name (e.g. `original`, `filtered`, `residuals`, `overlay`)
- **format** : `png` | `eps` | `svg`

### Rules for plotters
- `PlotRegression` (loki_regression) uses prefix `regression_`.
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

loki_filter           (depends on loki_core only)

loki_regression       (depends on loki_core only)
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
+-- CONFIG_REFERENCE.md
+-- CMakeLists.txt
+-- CMakePresets.json
+-- cmake/
+-- apps/
|   +-- loki_homogeneity/
|   +-- loki_outlier/
|   +-- loki_filter/
|   +-- loki_regression/
|       +-- CMakeLists.txt
|       +-- main.cpp
+-- libs/
|   +-- loki_core/
|   |   +-- include/loki/
|   |       +-- core/         (exceptions, version, config, configLoader, logger, nanPolicy)
|   |       +-- timeseries/   (timeSeries, timeStamp, gapFiller, deseasonalizer, medianYearSeries)
|   |       +-- stats/        (descriptive, filter, distributions, hypothesis, metrics)
|   |       +-- io/           (loader, dataManager, gnuplot, plot)
|   |       +-- math/         (lsqResult, lsq, designMatrix, hatMatrix, svd)
|   +-- loki_outlier/
|   +-- loki_homogeneity/
|   +-- loki_filter/
|   +-- loki_regression/
|       +-- CMakeLists.txt
|       +-- include/loki/regression/
|       |   +-- regressionResult.hpp
|       |   +-- regressor.hpp
|       |   +-- regressorUtils.hpp
|       |   +-- linearRegressor.hpp
|       |   +-- polynomialRegressor.hpp
|       |   +-- harmonicRegressor.hpp
|       |   +-- trendEstimator.hpp
|       |   +-- robustRegressor.hpp
|       |   +-- calibrationRegressor.hpp
|       |   +-- regressionDiagnostics.hpp
|       |   +-- plotRegression.hpp
|       +-- src/
|           +-- regressorUtils.cpp
|           +-- linearRegressor.cpp
|           +-- polynomialRegressor.cpp
|           +-- harmonicRegressor.cpp
|           +-- trendEstimator.cpp
|           +-- robustRegressor.cpp
|           +-- calibrationRegressor.cpp
|           +-- regressionDiagnostics.cpp
|           +-- plotRegression.cpp
+-- tests/
|   +-- unit/
|   |   +-- core/
|   |   +-- homogeneity/
|   |   +-- filter/
|   |   +-- regression/
+-- config/
|   +-- homogenization.json
|   +-- outlier.json
|   +-- filter.json
|   +-- regression.json
+-- scripts/
|   +-- loki.sh
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
# Build specific app
./scripts/loki.sh build regression --copy-dlls
./scripts/loki.sh build all --copy-dlls

# Run
./scripts/loki.sh run regression

# Test
./scripts/loki.sh test --rebuild
```

### CMake target name collision
The executable target must be named `loki_regression_app` with
`OUTPUT_NAME "loki_regression"` set via `set_target_properties`.

### Runtime DLLs (Windows)
Three DLLs must be present next to every executable:
`libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`
`--copy-dlls` flag in `loki.sh` handles this automatically.

### Eigen3 -- SYSTEM includes (critical)
Eigen3 must be included as SYSTEM to suppress -Werror on its internal headers.
In `libs/loki_core/CMakeLists.txt`:
```cmake
target_link_libraries(loki_core PUBLIC nlohmann_json::nlohmann_json PRIVATE loki_compiler_options)
target_include_directories(loki_core SYSTEM PUBLIC
    $<TARGET_PROPERTY:Eigen3::Eigen,INTERFACE_INCLUDE_DIRECTORIES>)
```

### loki.sh app registration
Every new app must be added to BOTH associative arrays in `loki.sh`:
```bash
APP_EXE=([regression]="apps/loki_regression/loki_regression.exe" ...)
APP_CONFIG=([regression]="config/regression.json" ...)
```
AND to the `case` statement in argument parsing:
```bash
loki|homogenization|outlier|filter|regression|all)
    APP="${arg}" ;;
```

---

## Implemented Modules

### loki_core -- complete
- `exceptions.hpp` -- full hierarchy
- `version.hpp` -- `0.3.0`
- `nanPolicy.hpp` -- `NanPolicy { THROW, SKIP, PROPAGATE }`
- `logger.hpp / .cpp` -- Meyers singleton, file + stdout/stderr, macros
- `config.hpp` -- all config structs including `PlotConfig` (updated this thread:
  added `regressionResidualAcf`, `regressionResidualHist`, `regressionInfluence`,
  `regressionLeverage` flags, all default `false`)
- `configLoader.hpp / .cpp` -- JSON loader, `_parseRegression()`, `_parsePlots()`
  (updated this thread: parses 4 new regression plot flags)
- `timeStamp.hpp / .cpp`
- `timeSeries.hpp / .cpp`
- `gapFiller.hpp / .cpp`
- `deseasonalizer.hpp / .cpp`
- `medianYearSeries.hpp / .cpp`
- `descriptive.hpp / .cpp`
- `filter.hpp / .cpp` -- movingAverage, EMA, WMA
- `distributions.hpp / .cpp`
- `hypothesis.hpp / .cpp`
- `metrics.hpp / .cpp`
- `loader.hpp / .cpp`, `dataManager.hpp / .cpp`
- `gnuplot.hpp / .cpp`
- `plot.hpp / .cpp` -- including `cdfPlot`, `qqPlotWithBands`, `residualDiagnostics`

### loki_core/math -- complete
- `lsqResult.hpp`
- `lsq.hpp / .cpp` -- LsqSolver: static solve(), weighted + IRLS (HUBER, BISQUARE)
- `designMatrix.hpp / .cpp`
- `hatMatrix.hpp / .cpp` -- NEW (this thread): thin QR, leverages eager, full H lazy
- `svd.hpp / .cpp` -- NEW (this thread): BDCSVD wrapper, truncate(), explainedVarianceRatio(),
  pseudoinverse(), condition(), rank()

### loki_outlier -- complete
### loki_homogeneity -- complete
### loki_filter -- complete

### loki_regression -- R1-R7 complete

#### Key design decisions
- **X-axis convention**: `x = mjd - tRef` (days relative to first valid observation).
- **`designMatrix`** stored in `RegressionResult` after every `fit()` -- required by
  `RegressionDiagnostics::computeInfluence()` and `computeVif()`.
- **`HatMatrix`** lives in `loki_core/math/` for reuse by future `HatMatrixDetector`.
- **`SvdDecomposition`** lives in `loki_core/math/` for reuse by future `loki_svd` module.
- **`CalibrationRegressor`** (TLS): residuals are orthogonal distances; `cofactorX` is empty;
  prediction intervals are approximate.
- **`RegressionDiagnostics`**: `computeAnova()`, `computeInfluence()`, `computeVif()`,
  `computeBreuschPagan()`. All called in `main.cpp`; results logged and written to protocol.
- **VIF threshold**: 10.0 (hardcoded); flagged indices reported in protocol.
- **Cook's threshold**: `4/n`; leverage threshold: `2p/n`.
- **Protocol**: `OUTPUT/PROTOCOLS/regression_[dataset]_[param]_protocol.txt`
- **CSV**: `mjd;original;fitted;residual` (+ `trend;seasonal` for `TrendEstimator`)

#### RegressionResult fields (complete)
```cpp
struct RegressionResult {
    Eigen::VectorXd  coefficients;
    Eigen::VectorXd  residuals;
    Eigen::MatrixXd  cofactorX;
    double           sigma0;
    double           rSquared;
    double           rSquaredAdj;
    double           aic;
    double           bic;
    int              dof;
    bool             converged;
    std::string      modelName;
    TimeSeries       fitted;
    double           tRef;
    Eigen::MatrixXd  designMatrix;   // <- added this thread
};
```

---

## NEXT THREAD: Build and Debug loki_regression

### What needs to happen at start of next thread
1. Michal runs `./scripts/loki.sh build regression --copy-dlls`
2. Pastes build errors into the thread
3. Claude fixes systematically

### Known issues to watch for (identified before build)

**1. `robustRegressor.cpp` and `trendEstimator.cpp` not updated**
Both still missing `result.designMatrix = A` before `m_lastResult = result`.
Same one-line fix as the other regressors. Michal needs to paste these files
or Claude will ask for them.

**2. `computeIntervals()` with empty `cofactorX` (CalibrationRegressor)**
`regressorUtils.cpp::computeIntervals()` likely uses `cofactorX` without
checking if it's empty. For TLS, `cofactorX` is an empty matrix.
Fix: guard `if (result.cofactorX.rows() > 0)` before interval computation,
fall back to sigma0-only interval (`+/- t * sigma0`) if empty.

**3. `plotRegression.cpp` -- `qqPlotWithBands` and `cdfPlot` signatures**
`plotQqBands()` and `plotCdf()` call `plot.qqPlotWithBands(result.fitted, outPath)`
and `plot.cdfPlot(result.fitted, outPath)`. The actual signatures of these methods
in `plot.hpp` need to be verified -- they may take different arguments.

**4. `plotResiduals()` -- residuals are `Eigen::VectorXd`, not `TimeSeries`**
In `plotRegression.cpp::plotResiduals()`, residuals are written from
`result.residuals[i]` (Eigen vector) but timestamps come from `result.fitted[i].time.mjd()`.
This is correct in principle but verify the index alignment is right when
`original` has NaN gaps (fitted is shorter than original).

**5. `loki.sh` registration**
`loki_regression` must be added to `loki.sh` before `./scripts/loki.sh run regression` works.

**6. Root `CMakeLists.txt`**
Must include `add_subdirectory(apps/loki_regression)` and
`add_subdirectory(libs/loki_regression)` if not already present.

**7. `loki_regression/CMakeLists.txt` -- `hatMatrix` and `svd` linkage**
`regressionDiagnostics.cpp` uses `HatMatrix` from `loki_core/math/`.
`calibrationRegressor.cpp` uses `SvdDecomposition` from `loki_core/math/`.
Both link transitively via `loki_core` -- this should work, but verify that
`hatMatrix.cpp` and `svd.cpp` are listed in `loki_core/CMakeLists.txt` sources.

**8. `writeCsv` -- TrendEstimator decomposition**
`trend` and `seasonal` columns are written as `NaN` placeholders. This is intentional
for now -- full `DecompositionResult` CSV integration is deferred.

---

## Config Structs -- RegressionConfig (config.hpp)

```cpp
enum class RegressionMethodEnum {
    LINEAR, POLYNOMIAL, HARMONIC, TREND, ROBUST, CALIBRATION
};

struct RegressionGapFillingConfig {
    std::string strategy{"linear"};
    int         maxFillLength{0};
};

struct RegressionConfig {
    RegressionGapFillingConfig gapFilling{};
    RegressionMethodEnum       method{RegressionMethodEnum::LINEAR};
    int                        polynomialDegree{1};
    int                        harmonicTerms{2};
    double                     period{365.25};
    bool                       robust{false};
    int                        robustIterations{10};
    std::string                robustWeightFn{"bisquare"};
    bool                       computePrediction{false};
    double                     predictionHorizon{0.0};
    double                     confidenceLevel{0.95};
    double                     significanceLevel{0.05};
    int                        cvFolds{10};
};
```

### PlotConfig -- regression flags (config.hpp)
```cpp
bool regressionOverlay       {true};
bool regressionResiduals     {true};
bool regressionCdfPlot       {false};
bool regressionQqBands       {true};
bool regressionResidualAcf   {false};  // new
bool regressionResidualHist  {false};  // new
bool regressionInfluence     {false};  // new
bool regressionLeverage      {false};  // new
```

---

## Known Issues and Workarounds

### GCC 13 aggregate-init bug -- CRITICAL
Any `Config` struct that contains an enum member with a default value CANNOT
be defined as a nested struct inside a class. Fix: define outside with descriptive
names, add `using` aliases inside class.

### `TimeSeries` API
- No `observations()` method -- use direct indexing: `ts[i].value`, `ts[i].time`
- `TimeSeries::append(const TimeStamp& time, double value, uint8_t flag = 0)`
- `::TimeStamp` is NOT in `namespace loki` -- always qualify as `::TimeStamp`

### `std::numbers::pi` instead of `M_PI`
Use `std::numbers::pi` (C++20 `<numbers>`). `M_PI` is not standard on Windows/MinGW.

### Logger macros
- `LOKI_WARNING` (not `LOKI_WARN`)
- `LOKI_INFO` accepts only a single string argument -- use concatenation

### `regressorUtils.hpp` location
Lives in `include/loki/regression/` (not hidden in `src/`).

### `loki_regression` CMake target name
Library: `loki_regression`, Executable: `loki_regression_app` with
`OUTPUT_NAME "loki_regression"`.

### Gnuplot on Windows
- Use `fwdSlash()` helper on all paths
- Font: `'Sans,12'` (not `'Helvetica,12'`)
- Terminal: `noenhanced` (prevents underscore subscript interpretation)

---

## Planned Future Work

- **R8** -- Levenberg-Marquardt nonlinear LSQ (separate thread, later)
- **`loki_kalman`** -- standalone Kalman filter module
- **`SplineFilter`** -- future addition to `loki_filter`
- **`HatMatrixDetector`** (O4) -- leverage-based outlier detection in `loki_outlier`;
  uses `HatMatrix` from `loki_core/math/`; does NOT inherit from `OutlierDetector` base
- **`loki_spectral`** -- FFT, prerequisite for HarmonicSeries
- **`loki_arima`** -- AR, ARMA, ARIMA
- **`loki_svd`** -- SSA, PCA decomposition; uses `SvdDecomposition` from `loki_core/math/`
- Additional change point methods: SNHT Alexandersson, Pettitt, Buishand

---

## Notes and Reminders

- Data files and plot outputs are **not** committed to the repository.
- Third-party dependencies via CMake `FetchContent` only -- no vendored source.
- Build directory `build/` is gitignored.
- Do NOT add license/copyright blocks at the top of source files.
- `loader.hpp` is in `loki_core/io/`, NOT in `timeseries/`.
- `deseasonalizer.hpp` and `medianYearSeries.hpp` are in `loki_core/timeseries/`.
- `metrics.hpp` is in `loki_core/stats/`.
- `hatMatrix.hpp` and `svd.hpp` are in `loki_core/math/`.
- Plot output -> `OUTPUT/IMG/`, protocols -> `OUTPUT/PROTOCOLS/`, CSV -> `OUTPUT/CSV/`.
- Temp files use `.tmp_` prefix and are deleted after gnuplot runs.
- Time series input may have no periodic component (GNSS, non-climatological data).
- Sampling rate varies -- detectors must not assume fixed time step.