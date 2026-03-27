# CLAUDE.md -- Project Instructions for LOKI

This file contains instructions for Claude when working on the LOKI project.
It should be placed in the root of the repository and kept up to date.

---

## Project Overview

LOKI is a professional C++ toolkit for statistical analysis of time series data.
The long-term goal is a modular ecosystem of analysis libraries with a GUI/IDE frontend.
Current focus: signal filtering (`loki_filter`), regression analysis (`loki_regression`).

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
- `loki::Plot` (loki_core) uses prefix `core_`.
- `PlotHomogeneity` (loki_homogeneity) uses prefix `homogeneity_`.
- `PlotOutlier` (loki_outlier) uses the `programName` constructor argument.
- `PlotFilter` (loki_filter) uses prefix `filter_`.
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
- `loki_filter` depends on `loki_core` only.
- `loki_regression` depends on `loki_core` only.

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
|   |   +-- CMakeLists.txt
|   |   +-- main.cpp
|   +-- loki_regression/             <- R7: to be created
|       +-- CMakeLists.txt
|       +-- main.cpp
+-- libs/
|   +-- loki_core/
|   |   +-- include/loki/
|   |       +-- core/         (exceptions, version, config, configLoader, logger, nanPolicy)
|   |       +-- timeseries/   (timeSeries, timeStamp, gapFiller, deseasonalizer, medianYearSeries)
|   |       +-- stats/        (descriptive, filter, distributions, hypothesis, metrics)
|   |       +-- io/           (loader, dataManager, gnuplot, plot)
|   |       +-- math/         (lsqResult, lsq, designMatrix)
|   +-- loki_outlier/
|   +-- loki_homogeneity/
|   +-- loki_filter/
|   +-- loki_regression/
|       +-- CMakeLists.txt
|       +-- include/loki/regression/
|       |   +-- regressionResult.hpp     <- R1 (contains PredictionPoint)
|       |   +-- regressor.hpp            <- R1 abstract base (fit, predict, name)
|       |   +-- regressorUtils.hpp       <- R1 shared internal helpers
|       |   +-- linearRegressor.hpp      <- R1
|       |   +-- polynomialRegressor.hpp  <- R2 (LOO-CV + k-fold CV)
|       |   +-- harmonicRegressor.hpp    <- R3
|       |   +-- trendEstimator.hpp       <- R3
|       |   +-- robustRegressor.hpp      <- R4
|       |   +-- calibrationRegressor.hpp <- R6 (TLS, planned)
|       |   +-- plotRegression.hpp       <- R7 (planned)
|       +-- src/
|           +-- regressorUtils.cpp
|           +-- linearRegressor.cpp
|           +-- polynomialRegressor.cpp
|           +-- harmonicRegressor.cpp
|           +-- trendEstimator.cpp
|           +-- robustRegressor.cpp
+-- tests/
|   +-- unit/
|   |   +-- core/
|   |   +-- homogeneity/
|   |   +-- filter/
|   |   +-- regression/
|   |       +-- test_linearRegressor.cpp     <- R1: 11 tests
|   |       +-- test_polynomialRegressor.cpp <- R2: 12 tests
|   |       +-- test_harmonicRegressor.cpp   <- R3: 10 tests (harmonic + trend)
|   |       +-- test_robustRegressor.cpp     <- R4: 9 tests
+-- config/
|   +-- homogenization.json
|   +-- outlier.json
|   +-- filter.json
|   +-- regression.json              <- R7: prototype below
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
./scripts/loki.sh build filter --copy-dlls
./scripts/loki.sh build all --copy-dlls

# Run
./scripts/loki.sh run filter
./scripts/loki.sh run regression

# Test
./scripts/loki.sh test --rebuild
```

### CMake target name collision
`loki_outlier` is already the library target name. The executable target must be
named `loki_outlier_app` with `OUTPUT_NAME "loki_outlier"` set via `set_target_properties`.
Same pattern applies to `loki_filter` app (`loki_filter_app`) and `loki_regression` app
(`loki_regression_app`).

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
Do NOT use `PUBLIC Eigen3::Eigen` in `target_link_libraries` -- use SYSTEM include instead.

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
- `config.hpp` -- all config structs including `PlotOptionsConfig`, `FilterConfig`,
  `RegressionConfig` (added this thread)
- `configLoader.hpp / .cpp` -- JSON loader, `_parseFilter()`, `_parseRegression()` (added)
- `timeStamp.hpp / .cpp` -- MJD internal, GPS/UTC/Unix conversions
- `timeSeries.hpp / .cpp` -- Observation, SeriesMetadata, append, slice, indexOf
- `gapFiller.hpp / .cpp` -- LINEAR, FORWARD_FILL, MEAN, NONE, MEDIAN_YEAR strategies
- `deseasonalizer.hpp / .cpp` -- MOVING_AVERAGE, MEDIAN_YEAR, NONE
- `medianYearSeries.hpp / .cpp` -- median annual profile
- `descriptive.hpp / .cpp` -- mean, median, variance, IQR, MAD, ACF, Hurst, summarize()
- `filter.hpp / .cpp` -- movingAverage, EMA, WMA (free functions)
- `distributions.hpp / .cpp` -- normalCdf/Quantile, tCdf/Quantile, chi2Cdf/Quantile, fCdf
- `hypothesis.hpp / .cpp` -- jarqueBera, shapiroWilk, kolmogorovSmirnov, runsTest, durbinWatson
- `metrics.hpp / .cpp` -- computeMetrics, rmse, mae, bias (added this thread)
- `loader.hpp / .cpp`, `dataManager.hpp / .cpp`
- `gnuplot.hpp / .cpp` -- RAII gnuplot pipe wrapper
- `plot.hpp / .cpp` -- timeSeries, histogram, ACF, QQ, boxplot, comparison,
  cdfPlot, qqPlotWithBands, residualDiagnostics (added this thread)

### loki_core/math -- complete (C1)
- `lsqResult.hpp` -- LsqResult struct
- `lsq.hpp / .cpp` -- LsqSolver: static solve(), weighted + IRLS (HUBER, BISQUARE)
- `designMatrix.hpp / .cpp` -- DesignMatrix: polynomial(), harmonic(), identity()

### loki_core/stats -- complete (C2)
- `distributions.hpp / .cpp` -- t, F, chi2, normal CDF and quantile functions
- `hypothesis.hpp / .cpp` -- Shapiro-Wilk, Jarque-Bera, KS test, Runs test, Durbin-Watson
- `metrics.hpp / .cpp` -- RMSE, MAE, bias, MAPE with NanPolicy support

### loki_outlier -- complete
### loki_homogeneity -- complete

### loki_filter -- F1-F6 complete
(see previous CLAUDE.md for details)

### loki_regression -- R1-R4 complete

#### Key design decisions
- **X-axis convention**: `x = mjd - tRef` (days relative to first valid observation).
  Ensures numerical stability for both ms-resolution and 6h data. `tRef` stored in
  `RegressionResult` for use in `predict()` and protocols.
- **`PredictionPoint`** defined in `regressionResult.hpp` (shared by all regressors).
- **`predict()`** is pure virtual in `Regressor` base -- all regressors must implement it.
- **`regressorUtils`** contains shared `computeGoodnessOfFit()` and `computeIntervals()`.
  Lives in `include/loki/regression/` (not hidden in src) for cross-regressor access.
- **Goodness of fit**: R^2, adjusted R^2, AIC/BIC (MLE sigma^2 for information criteria).
- **CV**: analytical LOO-CV via hat matrix diagonal for OLS (O(n), exact);
  k-fold CV for robust fits. `cvFolds` in `RegressionConfig` [2, 100], default 10.
- **IRLS tuning constants**: Huber k=1.345, Bisquare c=4.685 (95% Gaussian efficiency).
- **`TrendEstimator`**: joint fit of linear trend + K harmonics in single LSQ.
  Returns `DecompositionResult` with `trend`, `seasonal`, `residuals` as `TimeSeries`.
  Invariant: `trend + seasonal + residuals == original` (verified in tests).
- **`RobustRegressor`**: forces `robust=true` even if config says false (with warning).
  Exposes `weightedObservations()` for outlier diagnostics.
- **Output directory**: `OUTPUT/PROTOCOLS/` for protocol text files (not `REPORTS`).
  `protocolsDir` added to `AppConfig`. Created alongside LOG, CSV, IMG in `main.cpp`.

#### RegressionResult fields
```cpp
struct PredictionPoint {
    double x;           // mjd - tRef (days)
    double predicted;
    double confLow, confHigh;   // confidence interval
    double predLow, predHigh;   // prediction interval
};

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
    double           tRef;      // MJD of first observation
};
```

#### Coefficient layouts
- `LinearRegressor`:    `[a0, a1]`
- `PolynomialRegressor`: `[a0, a1, ..., ad]`
- `HarmonicRegressor`:  `[a0, s1, c1, s2, c2, ..., sK, cK]`
- `TrendEstimator`:     `[a0, a1, s1, c1, s2, c2, ..., sK, cK]`
- `RobustRegressor`:    same as polynomial of chosen degree

---

## Config Structs (config.hpp) -- RegressionConfig

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
    double                     period{365.25};       // days
    bool                       robust{false};
    int                        robustIterations{10};
    std::string                robustWeightFn{"bisquare"};  // huber | bisquare
    bool                       computePrediction{false};
    double                     predictionHorizon{0.0};  // days ahead
    double                     confidenceLevel{0.95};
    double                     significanceLevel{0.05};
    int                        cvFolds{10};              // [2, 100]
};
```

#### AppConfig output directories
```cpp
struct AppConfig {
    // ...
    std::filesystem::path logDir;        // OUTPUT/LOG
    std::filesystem::path csvDir;        // OUTPUT/CSV
    std::filesystem::path imgDir;        // OUTPUT/IMG
    std::filesystem::path protocolsDir;  // OUTPUT/PROTOCOLS
};
```

---

## regression.json -- Prototype Configuration

```json
{
    "workspace": "C:/Users/eliasmichal/Documents/Osobne",

    "input": {
        "file": "CLIM_DATA_EX1.txt",
        "time_format": "mjd",
        "time_columns": [0],
        "delimiter": ";",
        "comment_char": "%",
        "columns": [2],
        "merge_strategy": "separate"
    },

    "output": {
        "log_level": "info"
    },

    "plots": {
        "output_format": "png",
        "time_format": "utc",
        "enabled": {
            "time_series":              true,
            "histogram":                true,
            "acf":                      false,
            "qq_plot":                  false,
            "boxplot":                  false,
            "regression_overlay":       true,
            "regression_residuals":     true,
            "regression_cdf_plot":      false,
            "regression_qq_bands":      true
        }
    },

    "stats": {
        "enabled": true,
        "nan_policy": "skip",
        "hurst": false
    },

    "regression": {
        "gap_filling": {
            "strategy": "linear",
            "max_fill_length": 0
        },

        "method": "linear",

        "polynomial_degree": 1,

        "harmonic_terms": 2,
        "period": 365.25,

        "robust": false,
        "robust_iterations": 10,
        "robust_weight_fn": "bisquare",

        "compute_prediction": false,
        "prediction_horizon": 365.25,
        "confidence_level": 0.95,
        "significance_level": 0.05,

        "cv_folds": 10
    }
}
```

#### JSON key reference for `regression` block

| Key | Type | Default | Description |
|---|---|---|---|
| `method` | string | `"linear"` | `linear` \| `polynomial` \| `harmonic` \| `trend` \| `robust` \| `calibration` |
| `polynomial_degree` | int | `1` | Degree for polynomial/robust regression (>= 1) |
| `harmonic_terms` | int | `2` | Number of sin/cos pairs K for harmonic/trend |
| `period` | double | `365.25` | Fundamental period in days |
| `robust` | bool | `false` | Enable IRLS robust estimation |
| `robust_iterations` | int | `10` | Max IRLS iterations |
| `robust_weight_fn` | string | `"bisquare"` | `huber` \| `bisquare` |
| `compute_prediction` | bool | `false` | Compute and save prediction beyond data range |
| `prediction_horizon` | double | `0.0` | Days beyond last observation to predict |
| `confidence_level` | double | `0.95` | Confidence level for intervals |
| `significance_level` | double | `0.05` | Alpha for hypothesis tests in protocol |
| `cv_folds` | int | `10` | K-fold CV folds [2, 100]; LOO-CV used for OLS automatically |
| `gap_filling.strategy` | string | `"linear"` | `linear` \| `forward_fill` \| `mean` \| `none` |
| `gap_filling.max_fill_length` | int | `0` | Max gap to fill in samples (0 = unlimited) |

---

## loki_filter -- Key Details
(unchanged from previous CLAUDE.md -- see filter thread for full detail)

---

## Planned Future Work

### R5 -- ANOVA + hypothesis testing + residual diagnostics (NEXT)
- ANOVA table (SSR, SSE, SST, F-statistic, p-value)
- VIF (variance inflation factor) for multicollinearity detection
- Cook's distance for influence diagnostics
- Breusch-Pagan test for heteroscedasticity
- Durbin-Watson already in `loki_core/stats/hypothesis`
- These will consume `distributions.hpp` and `hypothesis.hpp` from C2

### R6 -- Calibration + Total Least Squares
- `CalibrationRegressor` -- orthogonal regression (errors in both x and y)
- TLS via SVD (Eigen)
- `multicollinearity` -- condition number, VIF

### R7 -- `apps/loki_regression` + `PlotRegression` + protocol + CMakeLists
Files to create:
- `libs/loki_regression/include/loki/regression/plotRegression.hpp`
- `libs/loki_regression/src/plotRegression.cpp`
- `apps/loki_regression/CMakeLists.txt`
- `apps/loki_regression/main.cpp`
- `libs/loki_regression/CMakeLists.txt`
- `config/regression.json`

`main.cpp` pipeline:
1. Load config (`ConfigLoader::load`)
2. Init logger, create output dirs (LOG, CSV, IMG, PROTOCOLS)
3. Load data (`DataManager`)
4. Gap fill if configured
5. Instantiate regressor based on `method` (factory pattern or switch)
6. `fit(ts)` -> `RegressionResult`
7. Log: modelName, sigma0, R^2, adj-R^2, AIC, BIC, dof
8. If `computePrediction`: call `predict()`, save CSV
9. Plots: overlay, residual diagnostics, QQ with bands
10. Protocol: write `OUTPUT/PROTOCOLS/regression_[dataset]_[param]_protocol.txt`
11. LOO-CV or k-fold CV (polynomial/robust), log CV RMSE and bias

Protocol format (see CLAUDE.md section below).

### R8 -- Levenberg-Marquardt nonlinear LSQ (separate thread, later)

---

## Protocol Format (OUTPUT/PROTOCOLS/)

Filename: `regression_[dataset]_[componentName]_protocol.txt`

```
============================================================
 REGRESSION PROTOCOL -- LinearRegressor
============================================================
 Dataset:      CLIM_DATA_EX1    Series: col_3
 Observations: 1276    Parameters: 2    DOF: 1274
 Method:       linear    Robust: no

 COEFFICIENTS
 -------------------------------------------------------
 Parameter    Estimate     Std.Err     t-stat    p-value
 a0           5.7421       0.0312      184.1     <0.001
 a1           0.0023       0.0004        5.8     <0.001

 MODEL FIT
 -------------------------------------------------------
 sigma0:           0.0318
 R^2:              0.9823    Adjusted R^2: 0.9822
 AIC:              -4821.3   BIC: -4810.1
 F-statistic:      33.7      p-value: <0.001

 CROSS-VALIDATION
 -------------------------------------------------------
 Method:           LOO-CV (analytical)
 CV RMSE:          0.0341
 CV MAE:           0.0251
 CV Bias:          0.0001

 RESIDUAL DIAGNOSTICS
 -------------------------------------------------------
 Mean:             0.0000    Std dev: 0.0318
 Normality (J-B):  p = 0.312   [PASS]
 Autocorr. (D-W):  1.94        [PASS]
============================================================
```

---

## Known Issues and Workarounds

### GCC 13 aggregate-init bug -- CRITICAL
Any `Config` struct that contains an enum member with a default value CANNOT
be defined as a nested struct inside a class. Fix: define outside with descriptive
names, add `using` aliases inside class.
Applies to: `LsqSolver`, `FilterWindowAdvisor`, `FilterMethodEnum`,
`RegressionMethodEnum`, and ALL future Config structs with enum members.

### `TimeSeries` API
- No `observations()` method -- use direct indexing: `ts[i].value`, `ts[i].time`
- `TimeSeries::append(const TimeStamp& time, double value, uint8_t flag = 0)`
- `::TimeStamp` is NOT in `namespace loki` -- always qualify as `::TimeStamp`

### `std::numbers::pi` instead of `M_PI`
Use `std::numbers::pi` (C++20 `<numbers>`) throughout. `M_PI` is not standard
and may be missing on Windows/MinGW without `_USE_MATH_DEFINES`.

### `regressorUtils.hpp` location
Lives in `include/loki/regression/` (not hidden in `src/`) so all regressors
can include it as `<loki/regression/regressorUtils.hpp>`.

### `computeIntervals()` x field
`detail::computeIntervals()` sets `PredictionPoint::x = aRow[1]` (second column).
This is correct for linear and polynomial (column 1 = x).
For harmonic, `x` in `PredictionPoint` is the t value passed in `xNew` -- correct
because `DesignMatrix::harmonic` puts t implicitly in the sin/cos terms, not as
a raw column. If needed, caller can overwrite `pt.x` after `computeIntervals()`.

### `loki_regression` CMake target name
Library: `loki_regression`, Executable: `loki_regression_app` with
`OUTPUT_NAME "loki_regression"`.

---

## What Claude Needs at Start of Next Thread

To continue seamlessly on **R5 (ANOVA + diagnostics)**:
- No new files needed -- R5 uses existing `lsq.hpp`, `distributions.hpp`,
  `hypothesis.hpp`, and `RegressionResult`.
- Claude should propose `AnovaTable` struct and `RegressionDiagnostics` class
  signatures before implementing.

To continue on **R7 (app + CMakeLists + main.cpp)**:
- `libs/loki_filter/CMakeLists.txt` (as reference for structure)
- `apps/loki_filter/main.cpp` (as reference for pipeline pattern)
- `libs/loki_regression/CMakeLists.txt` (if already started)

---

## Notes and Reminders

- Data files and plot outputs are **not** committed to the repository.
- Third-party dependencies via CMake `FetchContent` only -- no vendored source.
- Build directory `build/` is gitignored.
- `newmat` replaced by `Eigen3` throughout.
- Do NOT add license/copyright blocks at the top of source files.
- gnuplot font: `'Sans,12'` (not `'Helvetica,12'`).
- gnuplot terminal: `'noenhanced'` (except boxplot stats panel which uses `enhanced`).
- `loader.hpp` is in `loki_core/io/`, NOT in `timeseries/`.
- `deseasonalizer.hpp` and `medianYearSeries.hpp` are in `loki_core/timeseries/`.
- Plot output -> `OUTPUT/IMG/`, protocols -> `OUTPUT/PROTOCOLS/`, temp files use `.tmp_` prefix.
- Time series input may have no periodic component (GNSS, non-climatological data).
- Sampling rate varies from milliseconds to 6 hours. Detectors must not assume fixed time step.
- For ms data: MedianYearSeries throws ConfigException (resolution < 1h).
- `metrics.hpp` is in `loki_core/stats/` -- usable from all modules.
- `plot.hpp` now includes `cdfPlot`, `qqPlotWithBands`, `residualDiagnostics`.
  Requires `#include <loki/stats/distributions.hpp>` in `plot.cpp`.