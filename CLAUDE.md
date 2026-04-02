# CLAUDE.md -- Project Instructions for LOKI

This file contains instructions for Claude when working on the LOKI project.
It should be placed in the root of the repository and kept up to date.

---

## Project Overview

LOKI is a professional C++ toolkit for statistical analysis of time series data.
The long-term goal is a modular ecosystem of analysis libraries with a GUI/IDE frontend.
Current focus: nonlinear regression with Levenberg-Marquardt (`loki_regression` R8).

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
- `PlotRegression` (loki_regression) uses prefix `regression_`.
- Temporary data files use `.tmp_` prefix and are deleted immediately after gnuplot runs.
- Gnuplot on Windows requires forward slashes -- use `fwdSlash()` helper on all paths.
- gnuplot terminal: `pngcairo noenhanced font 'Sans,12'`
- gnuplot title for Cook's distance: use `'Cook s distance'` (no apostrophe) to avoid
  gnuplot string concatenation errors.
- `qqPlotWithBands` and `cdfPlot` in `loki::Plot` expect `std::string` stem (not full path)
  -- `Plot` builds the full output path internally via `outputPath(stem)`.

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
|   |       +-- math/         (lsqResult, lsq, designMatrix, hatMatrix, svd, lm [R8])
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
|       |   +-- nonlinearRegressor.hpp  [R8]
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
|           +-- nonlinearRegressor.cpp  [R8]
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
- `config.hpp` -- all config structs including `PlotConfig` with 8 regression plot flags
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
- `svd.hpp` -- header-only (see SVD note below)

### loki_outlier -- complete
### loki_homogeneity -- complete
### loki_filter -- complete

### loki_regression -- R1-R7 complete, R8 next

#### Key design decisions
- **X-axis convention**: `x = mjd - tRef` (days relative to first valid observation).
- **`designMatrix`** stored in `RegressionResult` after every `fit()`.
- **`HatMatrix`** and **`SvdDecomposition`** in `loki_core/math/` for reuse.
- **`TrendEstimator`** does NOT inherit from `Regressor` -- returns `DecompositionResult`.
  Handled separately in `main.cpp` with explicit branch on `RegressionMethodEnum::TREND`.
  Prediction is NOT supported for `TrendEstimator`.
- **`CalibrationRegressor`** (TLS): uses `Eigen::JacobiSVD` directly (not `SvdDecomposition`).
- **`computeIntervals`** in `regressorUtils` takes explicit `xNew` vector for correct
  `PredictionPoint::x` assignment -- critical for non-polynomial models (harmonic etc.)
  where design matrix columns are not simply [1, x, x^2, ...].
- **Prediction guard**: `MAX_PREDICTION_POINTS = 100'000` -- skips prediction with warning
  if estimated point count exceeds limit (protects against ms-rate data + long horizon).
- **Re-fit before predict()**: prediction in `main.cpp` creates a new regressor, re-fits,
  then calls `predict()` -- necessary because `buildRegressor` returns a fresh object.
- **Protocol**: `OUTPUT/PROTOCOLS/regression_[dataset]_[param]_protocol.txt`
- **CSV fitted**: `mjd;original;fitted;residual` (+ `trend;seasonal` for `TrendEstimator`)
- **CSV prediction**: `mjd;predicted;conf_low;conf_high;pred_low;pred_high`
  conf_* = confidence interval for mean response (narrow for large n)
  pred_* = prediction interval for future observations (includes sigma0, wider)

#### RegressionResult fields
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
    Eigen::MatrixXd  designMatrix;
};
```

#### PredictionPoint fields
```cpp
struct PredictionPoint {
    double x;         // mjd - tRef
    double predicted;
    double confLow;
    double confHigh;
    double predLow;
    double predHigh;
};
```

#### PlotConfig -- regression flags
```cpp
bool regressionOverlay       {true};
bool regressionResiduals     {true};
bool regressionCdfPlot       {false};
bool regressionQqBands       {true};
bool regressionResidualAcf   {false};
bool regressionResidualHist  {false};
bool regressionInfluence     {false};
bool regressionLeverage      {false};
// prediction plot reuses regressionOverlay flag -- shown when prediction is non-empty
```

---

## NEXT THREAD: R8 -- Levenberg-Marquardt Nonlinear Regression

### Architecture decisions (agreed)
- **`LmSolver`** lives in `loki_core/math/` (analogous to `LsqSolver`)
- **`NonlinearRegressor`** in `loki_regression/` -- standalone, does NOT inherit `Regressor`
  (different interface: requires model function + initial parameter guess)
- **Jacobian**: numerical by default (finite differences), analytical optional for built-in models
- **Built-in models** (selected via config enum):
  - `EXPONENTIAL`: `y = a * exp(b*x)` -- nonlinear trend, climatological data
  - `LOGISTIC`: `y = L / (1 + exp(-k*(x - x0)))` -- S-curve, velocity profiles (train data)
  - `GAUSSIAN`: `y = A * exp(-((x-mu)^2) / (2*sigma^2))` -- isolated peaks
  - `CUSTOM`: user-supplied `std::function<double(double, const Eigen::VectorXd&)>`
- **Model function type**:
  ```cpp
  using ModelFn = std::function<double(double x, const Eigen::VectorXd& params)>;
  ```
- **Initial parameter estimates**: required from config or user -- LM is sensitive to
  starting point; bad initial values -> non-convergence

### Config additions needed (config.hpp + configLoader.cpp)
```cpp
enum class NonlinearModelEnum { EXPONENTIAL, LOGISTIC, GAUSSIAN, CUSTOM };

struct NonlinearConfig {
    NonlinearModelEnum   model{NonlinearModelEnum::EXPONENTIAL};
    Eigen::VectorXd      initialParams;   // starting point for LM
    int                  maxIterations{100};
    double               tolerance{1e-6};
    double               lambdaInit{1e-3};
    double               lambdaFactor{10.0};
    double               confidenceLevel{0.95};
};
```

### LmSolver design
- Input: `ModelFn f`, `Eigen::VectorXd x`, `Eigen::VectorXd y`,
         `Eigen::VectorXd p0` (initial params), `LmConfig`
- Output: `LmResult` with converged params, residuals, Jacobian at solution,
          sigma0, covariance matrix (for intervals)
- Jacobian: numerical via central differences `(f(x, p+h) - f(x, p-h)) / (2h)`
- Damping: standard Marquardt -- increase lambda on step rejection, decrease on acceptance

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

### `computeIntervals` -- x assignment
`PredictionPoint::x` must be set from the explicit `xNew` vector, NOT extracted from
the design matrix row. For harmonic models `aRow[1] = sin(2*pi*t/T)`, not `t`.
The `computeIntervals` signature requires `xNew` as a separate parameter.

### Polynomial regression -- multicollinearity warning
For high-degree polynomials (>= 4) on short series, VIF values in millions are expected
due to natural correlation of power basis columns. This is NOT a bug -- it is an inherent
property of the monomial basis. For high-degree fits, orthogonal polynomials (Legendre)
would be needed. Current recommendation: use degree <= 3 for stable results.
For sensor/train velocity data, polynomial regression is a poor physical model --
consider logistic curve (NonlinearRegressor) or Kalman filter instead.

### `TimeSeries` API
- No `observations()` method -- use direct indexing: `ts[i].value`, `ts[i].time`
- `TimeSeries::append(const TimeStamp& time, double value, uint8_t flag = 0)`
- `::TimeStamp` is NOT in `namespace loki` -- always qualify as `::TimeStamp`

### `std::numbers::pi` instead of `M_PI`
Use `std::numbers::pi` (C++20 `<numbers>`). `M_PI` is not standard on Windows/MinGW.
IntelliSense may show false errors for `std::numbers::pi` -- set `"cppStandard": "c++20"`
in `.vscode/c_cpp_properties.json` and reset IntelliSense database.

### Logger macros
- `LOKI_WARNING` (not `LOKI_WARN`)
- `LOKI_INFO` accepts only a single string argument -- use concatenation

### Gnuplot on Windows
- Use `fwdSlash()` helper on all paths
- Font: `'Sans,12'` (not `'Helvetica,12'`)
- Terminal: `noenhanced` (prevents underscore subscript interpretation)
- String concatenation in gnuplot commands does NOT work -- avoid `'Cook' + "'" + 's'`

### `hypothesis.cpp` -- sign conversion
Vector indexing with `int` requires `static_cast<std::size_t>()` throughout
`shapiroWilkWZ()` to avoid `-Werror=sign-conversion`.

### `distributions.cpp` -- `std::numbers::pi`
Requires `#include <numbers>` at top of file.

### `plot.cpp` -- shadowed variable
Lambda inside `qqPlotWithBands` must use `oss` not `ss` (shadows outer `double ss`).

---

## Config Structs

### RegressionConfig (config.hpp)
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

---

## Planned Future Work

- **R8** -- Levenberg-Marquardt nonlinear LSQ (next thread -- see design above)
- **`loki_kalman`** -- standalone Kalman filter; ideal for sensor/train velocity data
- **`SplineFilter`** -- future addition to `loki_filter`
- **`HatMatrixDetector`** (O4) -- leverage-based outlier detection in `loki_outlier`
- **`loki_spectral`** -- FFT, prerequisite for HarmonicSeries
- **`loki_arima`** -- AR, ARMA, ARIMA
- **`loki_svd`** -- SSA, PCA; uses `SvdDecomposition` from `loki_core/math/`
- **`loki_clustering`** -- k-means, DBSCAN; useful for segmenting train velocity profiles
  (identify acceleration/cruise/braking phases) and climatological regime detection
- Additional change point methods: SNHT Alexandersson, Pettitt, Buishand
  NOTE: `snhtDetector` references in memory are from empty file attachments -- ignore them,
  these detectors are not yet implemented.

---

## Notes and Reminders

- Data files and plot outputs are **not** committed to the repository.
- Third-party dependencies via CMake `FetchContent` only -- no vendored source.
- Build directory `build/` is gitignored.
- Do NOT add license/copyright blocks at the top of source files.
- `loader.hpp` is in `loki_core/io/`, NOT in `timeseries/`.
- `hatMatrix.hpp` and `svd.hpp` are in `loki_core/math/`.
- `svd.cpp` does NOT exist -- `SvdDecomposition` is header-only in `svd.hpp`.
- Plot output -> `OUTPUT/IMG/`, protocols -> `OUTPUT/PROTOCOLS/`, CSV -> `OUTPUT/CSV/`.
- Temp gnuplot files use `.tmp_` prefix and are deleted after gnuplot runs.
- `regressorUtils.hpp` is in `include/loki/regression/` (not hidden in `src/`).
- `loki_regression` CMake: library `loki_regression`, executable `loki_regression_app`
  with `OUTPUT_NAME "loki_regression"`.