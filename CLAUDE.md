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
  Use `loki::filter::PlotFilter` etc. for sub-namespace types.
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
|   |       +-- stats/        (descriptive, filter, distributions, hypothesis)   <- C2: distributions+hypothesis NEW
|   |       +-- io/           (loader, dataManager, gnuplot, plot)
|   |       +-- math/         (lsqResult, lsq, designMatrix)
|   +-- loki_outlier/
|   +-- loki_homogeneity/
|   +-- loki_filter/
|   +-- loki_regression/             <- R1-R6: to be created
|       +-- CMakeLists.txt
|       +-- include/loki/regression/
|       |   +-- regressionResult.hpp
|       |   +-- regressor.hpp              <- abstract base
|       |   +-- linearRegressor.hpp        <- R1
|       |   +-- polynomialRegressor.hpp    <- R2
|       |   +-- harmonicRegressor.hpp      <- R3
|       |   +-- trendEstimator.hpp         <- R3
|       |   +-- robustRegressor.hpp        <- R4
|       |   +-- calibrationRegressor.hpp   <- R6 (TLS)
|       |   +-- regressionReport.hpp       <- R1+ (protocol generator)
|       |   +-- plotRegression.hpp         <- R7
|       +-- src/
+-- tests/
|   +-- unit/
|   |   +-- core/
|   |   +-- homogeneity/
|   |   +-- filter/
|   |   |   +-- test_filters.cpp     <- F2+F3a tests written, F3b+F4+F5 pending
|   |   +-- regression/              <- R7: to be created
+-- config/
|   +-- homogenization.json
|   +-- outlier.json
|   +-- filter.json
|   +-- regression.json              <- R7: to be created
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
Same pattern applies to `loki_filter` app (`loki_filter_app`) and `loki_regression` app.

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

---

## Implemented Modules

### loki_core -- complete
- `exceptions.hpp` -- full hierarchy
- `version.hpp` -- `0.3.0`
- `nanPolicy.hpp` -- `NanPolicy { THROW, SKIP, PROPAGATE }`
- `logger.hpp / .cpp` -- Meyers singleton, file + stdout/stderr, macros
- `config.hpp` -- all config structs including `PlotOptionsConfig`, `FilterConfig`
- `configLoader.hpp / .cpp` -- JSON loader, path resolution, `_parseFilter()`
- `timeStamp.hpp / .cpp` -- MJD internal, GPS/UTC/Unix conversions
- `timeSeries.hpp / .cpp` -- Observation, SeriesMetadata, append, slice, indexOf
- `gapFiller.hpp / .cpp` -- LINEAR, FORWARD_FILL, MEAN, NONE, MEDIAN_YEAR strategies
- `deseasonalizer.hpp / .cpp` -- MOVING_AVERAGE, MEDIAN_YEAR, NONE
- `medianYearSeries.hpp / .cpp` -- median annual profile
- `descriptive.hpp / .cpp` -- mean, median, variance, IQR, MAD, ACF, Hurst, summarize()
- `filter.hpp / .cpp` -- movingAverage, EMA, WMA (free functions, used by Deseasonalizer)
- `loader.hpp / .cpp`, `dataManager.hpp / .cpp`
- `gnuplot.hpp / .cpp` -- RAII gnuplot pipe wrapper
- `plot.hpp / .cpp` -- timeSeries, histogram, ACF, QQ, boxplot, comparison

### loki_core/math -- complete (C1)
- `lsqResult.hpp` -- LsqResult struct: coefficients, residuals, sigma0, vTPv, cofactorX, dof, converged
- `lsq.hpp / .cpp` -- LsqSolver: static solve(), weighted + IRLS (HUBER, BISQUARE)
- `designMatrix.hpp / .cpp` -- DesignMatrix: polynomial(), harmonic(), identity()

### loki_core/stats -- C2 pending
- `distributions.hpp / .cpp` -- t, F, chi2, normal CDF and quantile functions  <- NEW
- `hypothesis.hpp / .cpp` -- Shapiro-Wilk, Jarque-Bera, KS test, Runs test, Durbin-Watson  <- NEW

### loki_outlier -- complete
### loki_homogeneity -- complete

### loki_filter -- F1-F6 complete
- `filter.hpp` -- abstract base: `apply(TimeSeries) -> FilterResult`, `name() -> string`
- `filterResult.hpp` -- `FilterResult { filtered, residuals, filterName, effectiveWindow, effectiveBandwidth }`
- `movingAverageFilter.hpp / .cpp`
- `emaFilter.hpp / .cpp`
- `weightedMovingAverageFilter.hpp / .cpp`
- `kernelSmoother.hpp / .cpp`
- `loessFilter.hpp / .cpp`
- `savitzkyGolayFilter.hpp / .cpp`
- `filterWindowAdvisor.hpp / .cpp`
- `plotFilter.hpp / .cpp`
- `apps/loki_filter/main.cpp`
- `config/filter.json`

---

## loki_filter -- Key Details

### FilterResult fields
```cpp
struct FilterResult {
    TimeSeries  filtered;
    TimeSeries  residuals;
    std::string filterName;
    int         effectiveWindow{0};      // samples actually used (0 = not applicable e.g. EMA)
    double      effectiveBandwidth{0.0}; // window/n fraction (0 = not applicable)
};
```
Every filter populates `effectiveWindow` and `effectiveBandwidth` in `apply()`.
`main.cpp` logs these after every filter run with a tuning hint.

### Window vs bandwidth
- `MovingAverageFilter`, `WeightedMovingAverageFilter`, `SavitzkyGolayFilter`:
  configured by `window` in **samples** (not days/seconds).
  For 6h data: 1 year = 1461 samples. For daily: 365. For 1Hz: 86400.
- `KernelSmoother`, `LoessFilter`: configured by `bandwidth` as **fraction of n**.
  Effective window = `ceil(bandwidth * n)` samples.
- `EmaFilter`: no fixed window, controlled by `alpha` in (0, 1].

### Auto-window
If `window == 0` or `bandwidth == 0.0` in JSON, `FilterWindowAdvisor` is called
automatically. Method selected by `auto_window_method`: `silverman_mad` (default),
`silverman`, `acf_peak`. Result is logged at INFO level.

### Performance
- LOESS is O(n * k * p^2) -- avoid for n > 10000. Use KernelSmoother instead.
- KernelSmoother, MA, WMA, SG: O(n * w) -- suitable for any n.
- EMA: O(n) -- fastest, causal only.

### FilterWindowAdvisor is static
`FilterWindowAdvisor::advise(series, cfg)` -- static method, do NOT instantiate.

---

## loki_regression -- Planned (threads R1-R8)

### Architecture
```
loki_core/math/lsq         <- reused directly
loki_core/stats/distributions  <- C2: needed for p-values
loki_core/stats/hypothesis     <- C2: needed for residual diagnostics
loki_regression/               <- R1-R6: regressors + report + plots
apps/loki_regression/          <- R7: pipeline app
```

### RegressionResult (R1)
```cpp
struct RegressionResult {
    Eigen::VectorXd  coefficients;     // estimated parameters
    Eigen::VectorXd  residuals;        // observation residuals
    Eigen::MatrixXd  cofactorX;        // cofactor matrix of unknowns
    double           sigma0;           // unit weight standard deviation
    double           rSquared;         // coefficient of determination
    double           rSquaredAdj;      // adjusted R^2
    double           aic;              // Akaike Information Criterion
    double           bic;              // Bayesian Information Criterion
    int              dof;              // degrees of freedom (n - p)
    bool             converged;        // IRLS convergence flag
    std::string      modelName;        // for logging and report header
    // Prediction support:
    TimeSeries       fitted;           // fitted values at observation times
};
```

### Prediction + confidence intervals (R1)
Given new design matrix `A_new`, prediction at new points:
```
y_pred = A_new * x
var_pred = sigma0^2 * (A_new * cofactorX * A_new^T + I)  // prediction interval
var_conf = sigma0^2 * (A_new * cofactorX * A_new^T)       // confidence interval
```
Requires t-distribution quantile from `loki_core/stats/distributions`.

### Thread plan
| Thread | Content |
|---|---|
| C2 | `distributions.hpp/.cpp` + `hypothesis.hpp/.cpp` in loki_core/stats |
| R1 | `RegressionResult` + `LinearRegressor` + prediction + confidence intervals + `RegressionReport` (TXT) |
| R2 | `PolynomialRegressor` + AIC/BIC + cross-validation (leave-one-out) |
| R3 | `HarmonicRegressor` + `TrendEstimator` (trend + seasonal + residuals) |
| R4 | `RobustRegressor` (IRLS interface) + weighted regression |
| R5 | ANOVA table + hypothesis testing + residual diagnostics (VIF, Cook's distance, Breusch-Pagan, Durbin-Watson) |
| R6 | Calibration + Total Least Squares (orthogonal regression) + multicollinearity |
| R7 | `apps/loki_regression` + `PlotRegression` + CSV + protocol + unit tests |
| R8 | Levenberg-Marquardt nonlinear LSQ (separate thread, later) |

### RegressionReport format (R1+)
Plain text `.txt` file written to `OUTPUT/REPORTS/`:
```
============================================================
 REGRESSION REPORT -- LinearRegressor
============================================================
 Model:        y = a0 + a1 * x
 Observations: 1276    Parameters: 2    DOF: 1274

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

 RESIDUAL DIAGNOSTICS
 -------------------------------------------------------
 Mean:             0.0000    Std dev: 0.0318
 Normality (J-B):  p = 0.312   [PASS]
 Autocorr. (D-W):  1.94        [PASS]
============================================================
```

### FilterReport format (F7)
Requires C2 (Jarque-Bera, Durbin-Watson). Written to `OUTPUT/REPORTS/`:
```
============================================================
 FILTER REPORT -- KernelSmoother(Epanechnikov)
============================================================
 Dataset:      CLIM_DATA_EX1    Series: col_3
 Observations: 36524
 Filter:       KernelSmoother(Epanechnikov)
 Window:       3652 samples     Bandwidth: 0.100000
 Auto-window:  yes (SILVERMAN_MAD)

 RESIDUAL STATISTICS
 -------------------------------------------------------
 Mean:              0.0001    Std dev: 0.0318
 MAD:               0.0214    RMSE:    0.0318
 Min:              -0.1123    Max:     0.1456

 RESIDUAL DIAGNOSTICS
 -------------------------------------------------------
 Normality (J-B):   p = 0.041   [FAIL -- non-normal residuals]
 Autocorr. (D-W):   0.43        [FAIL -- strong autocorrelation]
 ACF lag-1:         0.821

 TUNING HINTS
 -------------------------------------------------------
 High ACF in residuals suggests bandwidth is too large.
 Current: bandwidth=0.100 -> window=3652 samples (~913 days at 6h resolution).
 Suggested: try bandwidth 0.01-0.03 for better frequency resolution.
============================================================
```
TUNING HINTS section is generated automatically based on ACF lag-1 and D-W thresholds:
- ACF lag-1 > 0.5 -> bandwidth too large, suggest reducing
- ACF lag-1 < 0.1 and D-W near 2.0 -> good fit
- D-W < 1.5 -> positive autocorrelation remaining

### OutlierReport format (to be added in loki_outlier update thread)
Written to `OUTPUT/REPORTS/`:
```
============================================================
 OUTLIER REPORT -- MAD detector
============================================================
 Dataset:      CLIM_DATA_EX1    Series: col_3
 Observations: 36524
 Deseasonalization: MEDIAN_YEAR

 DETECTION RESULTS
 -------------------------------------------------------
 Method:            MAD    Multiplier: 3.0
 Outliers detected: 47     (0.13% of series)
 Replacement:       linear interpolation   Max fill: unlimited

 SERIES STATISTICS
 -------------------------------------------------------
             Before cleaning    After cleaning
 Mean:            0.0923             0.0921
 Std dev:         0.0341             0.0318
 MAD:             0.0241             0.0198
 Min:            -0.1456            -0.0987
 Max:             0.2341             0.1823

 DIAGNOSTICS
 -------------------------------------------------------
 Normality (J-B):   p = 0.124   [PASS]
============================================================
```

### HomogeneityReport format (to be added in loki_homogeneity update thread)
Written to `OUTPUT/REPORTS/`:
```
============================================================
 HOMOGENEITY REPORT -- SNHT (Alexandersson)
============================================================
 Dataset:      CLIM_DATA_EX1    Series: col_3
 Observations: 36524
 Significance level: 0.05    Min segment: 180d

 PRE-PROCESSING
 -------------------------------------------------------
 Gap filling:         linear    Filled: 23 gaps
 Pre-outliers removed:  23      (method: mad_bounds, k=5.0)
 Deseasonalization:   MEDIAN_YEAR
 Post-outliers removed: 12      (method: mad, k=3.0)
 ACF dependence correction: yes (lag-1 ACF = 0.34)

 CHANGE POINTS DETECTED: 3
 -------------------------------------------------------
 #   Index    MJD          Date         Shift       p-value
 1   8640     51840.000    2001-01-15   -0.0312      0.001
 2   14400    53400.000    2005-03-22    0.0187      0.023
 3   21600    55200.000    2010-02-08   -0.0095      0.041

 SEGMENT STATISTICS
 -------------------------------------------------------
 Seg  Date range                n       Mean     Std dev
 1    1987-01-01 -- 2001-01-15  8640    0.0923   0.0318
 2    2001-01-15 -- 2005-03-22  5760    0.0611   0.0301
 3    2005-03-22 -- 2010-02-08  7200    0.0798   0.0325
 4    2010-02-08 -- 2016-12-31  7924    0.0703   0.0312

 ADJUSTMENTS APPLIED
 -------------------------------------------------------
 Reference segment: 1 (oldest / leftmost)
 Total cumulative correction: +0.0220
 Segment 2 corrected by: +0.0312
 Segment 3 corrected by: +0.0125
 Segment 4 corrected by: +0.0220
============================================================
```

### Report infrastructure -- shared across all modules
- Report dir: `OUTPUT/REPORTS/` -- new subdir alongside LOG, CSV, IMG.
- Must be created in `main.cpp` alongside other output dirs.
- Filename: `[program]_[dataset]_[parameter]_report.txt`
  e.g. `filter_CLIM_DATA_EX1_col_3_report.txt`
- A `ReportWriter` helper class in each module handles formatting.
  Alternatively a shared `loki_core/io/reportWriter.hpp` if patterns converge.
- Dates shown in human-readable form (UTC) when time_format allows,
  otherwise MJD. Use `TimeStamp::toUtcString()` if available.
- All float values formatted with consistent precision (4 decimal places default).
- `[PASS]` / `[FAIL]` / `[WARN]` tags for diagnostic results.
- Reports are always generated (not gated by a config flag) when the pipeline runs.

---

## Config Structs (config.hpp)

### FilterConfig -- complete
Enum `FilterMethodEnum` and all sub-configs defined OUTSIDE `FilterConfig`
to avoid GCC 13 aggregate-init bug. `FilterConfig` uses `using` aliases.

### RegressionConfig (to be added in R7)
```cpp
enum class RegressionMethodEnum {
    LINEAR, POLYNOMIAL, HARMONIC, TREND, ROBUST, CALIBRATION
};

struct RegressionConfig {
    RegressionMethodEnum method{RegressionMethodEnum::LINEAR};
    int    polynomialDegree{1};
    int    harmonicTerms{2};
    double period{365.25};          // days, for harmonic regression
    bool   robust{false};
    int    robustIterations{10};
    std::string robustWeightFn{"bisquare"};  // huber | bisquare
    bool   computePrediction{false};
    double predictionHorizon{0.0};  // days ahead to predict
    double confidenceLevel{0.95};   // for intervals
};
```

### AppConfig output directories
```cpp
struct AppConfig {
    std::filesystem::path workspace;
    // ...
    std::filesystem::path logDir;      // OUTPUT/LOG
    std::filesystem::path csvDir;      // OUTPUT/CSV
    std::filesystem::path imgDir;      // OUTPUT/IMG
    std::filesystem::path reportsDir;  // OUTPUT/REPORTS  <- ADD in C2/R1 thread
};
```
`reportsDir` must be added to `AppConfig` and initialized in `ConfigLoader::load()`
alongside `logDir`, `csvDir`, `imgDir`. All `main.cpp` files must create it with
`std::filesystem::create_directories(cfg.reportsDir)`.

---

## Known Issues and Workarounds

### GCC 13 aggregate-init bug -- CRITICAL, applies to every new module
Any `Config` struct that contains an enum member with a default value CANNOT
be defined as a nested struct inside a class. GCC 13 fails with:
  `could not convert '<brace-enclosed initializer list>' to 'const Config&'`
**Fix:** Define the enum and Config struct OUTSIDE the class with descriptive names
(e.g. `LsqWeightFunction`, `LsqSolverConfig`), then add `using` aliases inside:
```cpp
// WRONG -- causes GCC 13 error:
class MyClass {
    struct Config { MyEnum val{MyEnum::DEFAULT}; };  // FAILS
};
// CORRECT:
enum class MyClassEnum { DEFAULT };
struct MyClassConfig { MyClassEnum val{MyClassEnum::DEFAULT}; };
class MyClass {
    using Config = MyClassConfig;
    using MyEnum = MyClassEnum;
};
```
This applies to: `LsqSolver`, `FilterWindowAdvisor`, `FilterMethodEnum`,
and ALL future Config structs with enum members.

### `TimeSeries` API -- no `observations()` method
`TimeSeries` does NOT have an `observations()` method. Always use direct indexing:
```cpp
// WRONG:
const auto& obs = series.observations();  // compile error
obs[i].timestamp                          // compile error, field is .time

// CORRECT:
series[i].value
series[i].time   // not .timestamp
series.size()
```

### `TimeSeries::append` signature
```cpp
void append(const TimeStamp& time, double value, uint8_t flag = 0);
// NOT: append({time, value}) -- brace-init does not work
```

### `FilterWindowAdvisor` is a static-only class
Do NOT instantiate `FilterWindowAdvisor`. Call directly:
```cpp
// WRONG:
FilterWindowAdvisor advisor{cfg};
advisor.advise(series);
// CORRECT:
FilterWindowAdvisor::advise(series, cfg);
```

### `::TimeStamp` not in `namespace loki`
Always qualify as `::TimeStamp` in loki namespace code.

### `using namespace loki;` in app `main.cpp`
Add after includes. Sub-namespace types still need qualification:
`loki::filter::PlotFilter`, `loki::filter::FilterResult` -- wait, FilterResult
is in `namespace loki` directly, not `loki::filter`. Check the header.

### Eigen3 SYSTEM includes
See Build Instructions section. Never use `PUBLIC Eigen3::Eigen` in
`target_link_libraries` -- causes -Werror failures on Eigen internal headers.

### `loki_filter` app CMake target name
Library: `loki_filter`, Executable target: `loki_filter_app` with
`OUTPUT_NAME "loki_filter"`. Same pattern for `loki_regression_app`.

### `loki.sh` app registration
Every new app must be added to BOTH associative arrays in `loki.sh`:
```bash
APP_EXE=([filter]="apps/loki_filter/loki_filter.exe" ...)
APP_CONFIG=([filter]="config/filter.json" ...)
```
AND to the `case` statement in argument parsing:
```bash
loki|homogenization|outlier|filter|regression|all)
    APP="${arg}" ;;
```
Forgetting the case statement means the app argument is silently ignored
and the default `APP="loki"` is used -- hard to diagnose.

### Window parameter is in SAMPLES not days/seconds
`MovingAverageFilter.window`, `SavitzkyGolayFilter.window` are in samples:
- Daily data: 1 year = 365 samples
- 6-hourly data: 1 year = 1461 samples
- 1Hz data: 1 minute = 60 samples
Future improvement: parse human-readable duration like `min_segment_duration`.

---

## Planned Future Work

### C2 -- loki_core/stats extensions (NEXT)
- `distributions.hpp / .cpp`: t-CDF, F-CDF, chi2-CDF, normal CDF/quantile
- `hypothesis.hpp / .cpp`: Jarque-Bera, Shapiro-Wilk, KS test, Runs test, Durbin-Watson

### loki_regression (R1-R8, after C2)
See thread plan above.

### loki_filter -- Pending (F7)
- `PlotFilter` additional diagnostics
- Unit tests for F3b (LOESS), F4 (SG), F5 (Advisor) -- pending
- Integration tests on 6h climatological and ms-resolution data

### SplineFilter (after loki_filter stable)
Cubic smoothing spline, also usable as GapFiller strategy.

### loki_kalman
Standalone module, NOT a wrapper in loki_filter.
LSQ from loki_core/math used for state initialisation.

### Alternative homogeneity detectors
SNHT Alexandersson, Pettitt, Buishand -- after loki_regression stable.

---

## Planned Modules

| Module | Description | Status |
|---|---|---|
| `loki_homogeneity` | Change point detection, SNHT, homogenization | complete |
| `loki_outlier` | IQR, MAD, Z-score, OutlierCleaner | complete |
| `loki_filter` | MA/EMA/WMA/Kernel/LOESS/SavitzkyGolay, FilterWindowAdvisor | F1-F6 done, F7 pending |
| `loki_regression` | Linear, polynomial, harmonic, robust, calibration, NLS | planned R1-R8 |
| `loki_decomposition` | Trend, seasonality, residuals (STL, classical) | planned |
| `loki_svd` | SVD/PCA decomposition | planned |
| `loki_kalman` | Kalman filter, smoother, EKF | planned |
| `loki_arima` | AR, ARMA, ARIMA, SARIMA | planned |
| `loki_spectral` | FFT, power spectrum, Lomb-Scargle | planned |
| `loki_stationarity` | ADF, KPSS, unit root tests | planned |
| `loki_clustering` | k-means, DBSCAN, Hungarian algorithm | planned |
| `loki_qc` | Quality control, gap filling, flagging | planned |

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
- `SeriesMetadata` must be populated by Loader after loading.
- Plot output -> `OUTPUT/IMG/`, reports -> `OUTPUT/REPORTS/`, temp files use `.tmp_` prefix.
- Time series input may have no periodic component (GNSS, non-climatological data).
- Sampling rate varies from milliseconds to 6 hours. Detectors must not assume any fixed time step.
- For ms data: MedianYearSeries throws ConfigException (resolution < 1h).