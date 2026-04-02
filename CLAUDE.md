# CLAUDE.md -- Project Instructions for LOKI

This file contains instructions for Claude when working on the LOKI project.
It should be placed in the root of the repository and kept up to date.

---

## Project Overview

LOKI is a professional C++ toolkit for statistical analysis of time series data.
The long-term goal is a modular ecosystem of analysis libraries with a GUI/IDE frontend.
Current focus: hat matrix leverage-based outlier detection (`loki_outlier` O4).

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
- **dataset** : input filename stem without extension
- **parameter** : series `componentName` from metadata
- **plottype** : descriptive short name
- **format** : `png` | `eps` | `svg`

### Rules for plotters
- Temporary data files use `.tmp_` prefix and are deleted immediately after gnuplot runs.
- Gnuplot on Windows requires forward slashes -- use `fwdSlash()` helper on all paths.
- gnuplot terminal: `pngcairo noenhanced font 'Sans,12'`
- `qqPlotWithBands` and `cdfPlot` in `loki::Plot` expect `std::string` stem (not full path).

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
|   |       +-- math/         (lsqResult, lsq, designMatrix, hatMatrix, svd, lm)
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
|       |   +-- nonlinearRegressor.hpp
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
|           +-- nonlinearRegressor.cpp
|           +-- regressionDiagnostics.cpp
|           +-- plotRegression.cpp
+-- tests/
+-- config/
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
./scripts/loki.sh build regression --copy-dlls
./scripts/loki.sh run regression
./scripts/loki.sh test --rebuild
```

### CMake target name collision
Executable: `loki_regression_app` with `OUTPUT_NAME "loki_regression"`.
Same pattern for `loki_outlier_app` with `OUTPUT_NAME "loki_outlier"`.

### Runtime DLLs (Windows)
`libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`
`--copy-dlls` flag in `loki.sh` handles this automatically.

### Eigen3 -- SYSTEM includes (critical)
```cmake
target_include_directories(loki_core SYSTEM PUBLIC
    $<TARGET_PROPERTY:Eigen3::Eigen,INTERFACE_INCLUDE_DIRECTORIES>)
```

---

## Implemented Modules

### loki_core -- complete
- `exceptions.hpp` -- full hierarchy
- `version.hpp` -- `0.3.0`
- `nanPolicy.hpp` -- `NanPolicy { THROW, SKIP, PROPAGATE }`
- `logger.hpp / .cpp`
- `config.hpp` -- all config structs including `PlotConfig`, `NonlinearConfig`
- `configLoader.hpp / .cpp`
- `timeStamp.hpp / .cpp`
- `timeSeries.hpp / .cpp`
- `gapFiller.hpp / .cpp`
- `deseasonalizer.hpp / .cpp`
- `medianYearSeries.hpp / .cpp`
- `descriptive.hpp / .cpp`
- `filter.hpp / .cpp`
- `distributions.hpp / .cpp`
- `hypothesis.hpp / .cpp`
- `metrics.hpp / .cpp`
- `loader.hpp / .cpp`, `dataManager.hpp / .cpp`
- `gnuplot.hpp / .cpp`
- `plot.hpp / .cpp` -- including `cdfPlot`, `qqPlotWithBands`, `residualDiagnostics`

### loki_core/math -- complete
- `lsqResult.hpp`
- `lsq.hpp / .cpp`
- `designMatrix.hpp / .cpp`
- `hatMatrix.hpp / .cpp`
- `svd.hpp` -- header-only (BDCSVD linking bug workaround, see Known Issues)
- `lm.hpp / .cpp` -- Levenberg-Marquardt solver

### loki_outlier -- complete (O1-O3)
### loki_homogeneity -- complete
### loki_filter -- complete

### loki_regression -- R1-R8 complete
- `LinearRegressor`, `PolynomialRegressor`, `HarmonicRegressor`
- `TrendEstimator` (does NOT inherit Regressor, handled separately in main)
- `RobustRegressor` (IRLS, Huber/Bisquare)
- `CalibrationRegressor` (TLS via JacobiSVD)
- `NonlinearRegressor` (LM, built-in: EXPONENTIAL/LOGISTIC/GAUSSIAN, CUSTOM via ModelFn)
- `RegressionDiagnostics`, `PlotRegression`

#### Key design decisions (regression)
- X-axis: `x = mjd - tRef`; `tRef` stored in `RegressionResult`
- `designMatrix` in `RegressionResult` after every `fit()`
- `NonlinearRegressor`: standalone (not inheriting Regressor), `designMatrix` = Jacobian at solution
- `LmResult` -> `RegressionResult` mapping: params->coefficients, covariance->cofactorX, jacobian->designMatrix
- `ModelFn = std::function<double(double x, const Eigen::VectorXd& params)>` defined in `lm.hpp`
- Prediction intervals for nonlinear: delta method (gradient-based, approximate)
- `computeIntervals` in `regressorUtils::detail` -- takes explicit `xNew` vector
- Prediction guard: `MAX_PREDICTION_POINTS = 100'000`
- Protocol: `OUTPUT/PROTOCOLS/regression_[dataset]_[param]_protocol.txt`
- CSV fitted: `mjd;original;fitted;residual` (+ `trend;seasonal` for TrendEstimator)
- CSV prediction: `mjd;predicted;conf_low;conf_high;pred_low;pred_high`

---

## NEXT THREAD: O4 -- HatMatrixDetector in loki_outlier

### Background and motivation
The hat matrix (projection matrix) `H = A(A^T A)^{-1} A^T` maps observations
to fitted values. Its diagonal elements `h_ii` (leverages) measure how much
influence each observation has on its own fit. High-leverage points are
geometrically remote from the bulk of the data -- they are potential outliers
even if their residuals appear small ("hidden outliers" or "masked outliers").

This is the key advantage over residual-based methods (MAD, IQR, Z-score):
a high-leverage outlier can pull the fit toward itself, resulting in a small
residual despite being anomalous. The hat matrix detects these geometrically.

### Pipeline context
The `HatMatrixDetector` operates on **residuals** (deseasonalized series),
not the raw signal. Deseasonalization is done upstream in the outlier pipeline
using either:
- `moving_average` strategy (sensor/signal data)
- `median_year` strategy (climatological/GNSS data, step >= 1 hour)

This is consistent with the existing `loki_outlier` pipeline (O1-O3 detectors
all operate on residuals after the deseasonalization step).

### Design decisions (agreed)
- `HatMatrixDetector` lives in `loki_outlier`
- Does NOT inherit from `OutlierDetector` base class -- different interface
  (requires AR order parameter; output includes leverage values, not just flags)
- Input: deseasonalized `TimeSeries` (residuals)
- Internally builds a design matrix `A` from AR(p) structure:
  row i = [e_{i-1}, e_{i-2}, ..., e_{i-p}]  (lagged residuals as predictors)
  This captures temporal dependence structure -- key for time series outliers
- Computes `H = A(A^T A)^{-1} A^T` via `HatMatrix` from `loki_core/math/`
- Detection threshold: chi-squared with `p` degrees of freedom at `alpha` level
  `h_ii > chi2_quantile(1 - alpha, p) / n`  (scaled leverage threshold)
- Output struct `HatMatrixResult`:
  - `std::vector<std::size_t> outlierIndices` -- indices in original series
  - `Eigen::VectorXd leverages` -- full `h_ii` vector (for plotting)
  - `double threshold` -- the chi-squared derived threshold used
  - `int arOrder` -- AR order actually used

### Config additions needed
```cpp
struct HatMatrixConfig {
    int    arOrder          {5};     // AR lag order p; higher = more context
    double significanceLevel{0.05};  // alpha for chi-squared threshold
    bool   enabled          {true};
};
```
Add `HatMatrixConfig hatMatrix{}` to `OutlierConfig`.

### Files to request at thread start
- `libs/loki_outlier/include/loki/outlier/outlierDetector.hpp` -- base class
- `libs/loki_outlier/include/loki/outlier/` -- directory listing (all existing detectors)
- `libs/loki_outlier/CMakeLists.txt`
- `apps/loki_outlier/main.cpp`
- `libs/loki_core/include/loki/math/hatMatrix.hpp` -- existing HatMatrix API
- `libs/loki_core/include/loki/core/config.hpp` -- OutlierConfig section
- `libs/loki_core/src/core/configLoader.cpp` -- _parseOutlier section
- The academic paper on the method (user will attach PDF)

---

## Known Issues and Workarounds

### GCC 13 aggregate-init bug -- CRITICAL
Any `Config` struct with enum member + default value CANNOT be a nested struct inside
a class. Define outside with descriptive name, add `using` alias inside class.

### Eigen BDCSVD linking bug -- CRITICAL
`Eigen::BDCSVD` causes `undefined reference` errors when used in `.cpp` files compiled
into static libraries on Windows/GCC. Two fixes applied:
1. `SvdDecomposition` (`svd.hpp`) is **header-only** -- all methods inlined, no `svd.cpp`.
2. `CalibrationRegressor` uses `Eigen::JacobiSVD` directly (not `SvdDecomposition`).
Do NOT move `SvdDecomposition` back to a `.cpp` file. Do NOT use `BDCSVD` in any `.cpp`
that gets compiled into a static library.

### `TimeSeries` API
- No `observations()` method -- use direct indexing: `ts[i].value`, `ts[i].time`
- `TimeSeries::append(const TimeStamp& time, double value, uint8_t flag = 0)`
- `::TimeStamp` is NOT in `namespace loki` -- always qualify as `::TimeStamp`

### `std::numbers::pi` instead of `M_PI`
Use `std::numbers::pi` (C++20 `<numbers>`).

### Logger macros
- `LOKI_WARNING` (not `LOKI_WARN`)
- `LOKI_INFO` accepts only a single string argument -- use concatenation

### Gnuplot on Windows
- Use `fwdSlash()` helper on all paths
- Font: `'Sans,12'` (not `'Helvetica,12'`)
- Terminal: `noenhanced` (prevents underscore subscript interpretation)

### `distributions.hpp` namespace
Functions are in `loki::stats::` -- e.g. `loki::stats::tQuantile(p, df)`.
`tQuantile` takes `double df` (not int).

### `regressorUtils` namespace
`computeIntervals` and `computeGoodnessOfFit` are in `loki::regression::detail`.

---

## Config Structs

### NonlinearConfig (config.hpp) -- added in R8
```cpp
enum class NonlinearModelEnum {
    EXPONENTIAL,  // a * exp(b * x)
    LOGISTIC,     // L / (1 + exp(-k * (x - x0)))
    GAUSSIAN      // A * exp(-((x - mu)^2) / (2 * sigma^2))
};

struct NonlinearConfig {
    NonlinearModelEnum  model          {NonlinearModelEnum::EXPONENTIAL};
    std::vector<double> initialParams  {};
    int                 maxIterations  {100};
    double              gradTol        {1.0e-8};
    double              stepTol        {1.0e-8};
    double              lambdaInit     {1.0e-3};
    double              lambdaFactor   {10.0};
    double              confidenceLevel{0.95};
};
```
Member of `RegressionConfig`: `NonlinearConfig nonlinear{}`.

### RegressionMethodEnum (config.hpp)
```cpp
enum class RegressionMethodEnum {
    LINEAR, POLYNOMIAL, HARMONIC, TREND, ROBUST, CALIBRATION, NONLINEAR
};
```

---

## Planned Future Work

- **O4** -- `HatMatrixDetector` in `loki_outlier` (next thread -- see design above)
- **`loki_kalman`** -- standalone Kalman filter
- **`SplineFilter`** -- future addition to `loki_filter`
- **`loki_spectral`** -- FFT, prerequisite for HarmonicSeries
- **`loki_arima`** -- AR, ARMA, ARIMA
- **`loki_svd`** -- SSA, PCA; uses `SvdDecomposition` from `loki_core/math/`
- **`loki_clustering`** -- k-means, DBSCAN
- Additional change point methods: Pettitt, Buishand

---

## Notes and Reminders

- Data files and plot outputs are **not** committed to the repository.
- Third-party dependencies via CMake `FetchContent` only -- no vendored source.
- Build directory `build/` is gitignored.
- Do NOT add license/copyright blocks at the top of source files.
- `loader.hpp` is in `loki_core/io/`, NOT in `timeseries/`.
- `hatMatrix.hpp`, `svd.hpp`, `lm.hpp` are in `loki_core/math/`.
- `svd.cpp` does NOT exist -- `SvdDecomposition` is header-only in `svd.hpp`.
- `lm.hpp / lm.cpp` -- `LmSolver` and `ModelFn` type alias live here.
- Plot output -> `OUTPUT/IMG/`, protocols -> `OUTPUT/PROTOCOLS/`, CSV -> `OUTPUT/CSV/`.
- `regressorUtils.hpp` is in `include/loki/regression/` (not hidden in `src/`).
- `loki_regression` CMake: library `loki_regression`, executable `loki_regression_app`
  with `OUTPUT_NAME "loki_regression"`.
- `loki_outlier` CMake: library `loki_outlier`, executable `loki_outlier_app`
  with `OUTPUT_NAME "loki_outlier"`.