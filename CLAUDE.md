# CLAUDE.md -- Project Instructions for LOKI

This file contains instructions for Claude when working on the LOKI project.
It should be placed in the root of the repository and kept up to date.

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

- **program** : `core` | `homogeneity` | `outlier` | `filter` | `regression` | `stationarity` | `arima`
- **dataset** : input filename stem without extension
- **parameter** : series `componentName` from metadata
- **plottype** : descriptive short name
- **format** : `png` | `eps` | `svg`

### Rules for plotters
- Temporary data files use `.tmp_` prefix and are deleted immediately after gnuplot runs.
- Gnuplot on Windows requires forward slashes -- use `fwdSlash()` helper on all paths.
- gnuplot terminal: `pngcairo noenhanced font 'Sans,12'`
- `qqPlotWithBands` and `cdfPlot` in `loki::Plot` expect `std::string` stem (not full path).
- All new plot types are added to `loki::Plot` (plot.hpp/cpp in loki_core) -- never use
  Gnuplot directly in app-layer code. The Gnuplot API is `gp("command")`, no `<<` operator.

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

loki_stationarity     (depends on loki_core only)

loki_arima            (depends on loki_core + loki_stationarity)  <- NEXT
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
|   +-- loki_stationarity/
|   +-- loki_arima/                   <- NEXT
|       +-- CMakeLists.txt
|       +-- main.cpp
+-- libs/
|   +-- loki_core/
|   |   +-- include/loki/
|   |       +-- core/         (exceptions, version, config, configLoader, logger, nanPolicy)
|   |       +-- timeseries/   (timeSeries, timeStamp, gapFiller, deseasonalizer, medianYearSeries)
|   |       +-- stats/        (descriptive, filter, distributions, hypothesis, metrics)
|   |       +-- io/           (loader, dataManager, gnuplot, plot)
|   |       +-- math/         (lsqResult, lsq, designMatrix, hatMatrix, svd, lm, lagMatrix)
|   +-- loki_outlier/
|   +-- loki_homogeneity/
|   +-- loki_filter/
|   +-- loki_regression/
|   +-- loki_stationarity/
|   +-- loki_arima/                   <- NEXT
|       +-- CMakeLists.txt
|       +-- include/loki/arima/
|       |   +-- arimaResult.hpp
|       |   +-- arimaFitter.hpp
|       |   +-- arimaOrderSelector.hpp
|       |   +-- arimaForecaster.hpp
|       |   +-- arimaAnalyzer.hpp
|       +-- src/
|           +-- arimaFitter.cpp
|           +-- arimaOrderSelector.cpp
|           +-- arimaForecaster.cpp
|           +-- arimaAnalyzer.cpp
+-- tests/
+-- config/
|   +-- outlier.json
|   +-- regression.json
|   +-- stationarity.json
|   +-- arima.json                    <- NEXT
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
./scripts/loki.sh build arima --copy-dlls
./scripts/loki.sh run arima
./scripts/loki.sh test --rebuild
```

### CMake target name collision
Executable: `loki_arima_app` with `OUTPUT_NAME "loki_arima"`.
Same pattern for all other apps.

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
- `version.hpp` -- `0.1.0`
- `nanPolicy.hpp` -- `NanPolicy { THROW, SKIP, PROPAGATE }`
- `logger.hpp / .cpp`
- `config.hpp` -- all config structs including `PlotConfig`, `NonlinearConfig`,
                  `OutlierConfig` with `HatMatrixSection`, `StationarityConfig`
                  with `AdfConfig`, `KpssConfig`, `PpConfig`
- `configLoader.hpp / .cpp`
- `timeStamp.hpp / .cpp`
- `timeSeries.hpp / .cpp`
- `gapFiller.hpp / .cpp`
- `deseasonalizer.hpp / .cpp`
- `medianYearSeries.hpp / .cpp`
- `descriptive.hpp / .cpp` -- including `acf()`, `pacf()` (Yule-Walker/Eigen LDLT),
                               `diff()`, `laggedDiff()`
- `filter.hpp / .cpp`
- `distributions.hpp / .cpp` -- normalCdf/Quantile, tCdf/Quantile,
                                 chi2Cdf/Quantile, fCdf,
                                 `adfCriticalValue()` (MacKinnon 1994),
                                 `kpssCriticalValue()` (Kwiatkowski et al. 1992)
- `hypothesis.hpp / .cpp` -- jarqueBera, shapiroWilk, kolmogorovSmirnov,
                              runsTest, durbinWatson, `ljungBox()`
- `metrics.hpp / .cpp`
- `loader.hpp / .cpp`, `dataManager.hpp / .cpp`
- `gnuplot.hpp / .cpp` -- RAII pipe wrapper, API: `gp("command")` only, no `<<`
- `plot.hpp / .cpp` -- including `cdfPlot`, `qqPlotWithBands`, `residualDiagnostics`,
                        `pacf()` (new, stationarity-specific)

### loki_core/math -- complete
- `lsqResult.hpp`
- `lsq.hpp / .cpp`
- `designMatrix.hpp / .cpp`
- `hatMatrix.hpp / .cpp`
- `svd.hpp` -- header-only (BDCSVD linking bug workaround)
- `lm.hpp / .cpp` -- Levenberg-Marquardt solver
- `lagMatrix.hpp / .cpp` -- AR(p) lag design matrix

### loki_outlier -- complete (O1-O4)
### loki_homogeneity -- complete
### loki_filter -- complete
### loki_regression -- complete (R1-R8, including nonlinear/LM)

### loki_stationarity -- complete
- `stationarityResult.hpp` -- `TestResult`, `StationarityResult`
- `stationarityUtils.hpp` -- header-only `detail::neweyWestVariance()` (Bartlett kernel,
                              auto bandwidth), shared between KPSS and PP
- `adfTest.hpp / .cpp` -- ADF test, OLS via Eigen LDLT, AIC/BIC/fixed lag selection,
                           Schwert (1989) auto-lag, MacKinnon (1994) critical values
- `kpssTest.hpp / .cpp` -- KPSS test, eta statistic, Newey-West, Kwiatkowski (1992) cv
- `ppTest.hpp / .cpp` -- PP Z(t) statistic, Newey-West correction on DF t-ratio,
                          uses `XtX` (not `XtXinv`) in correction term (critical!),
                          MacKinnon (1994) critical values
- `stationarityAnalyzer.hpp / .cpp` -- orchestrator, majority vote ADF+PP vs KPSS,
                                        `buildConclusion()` with newlines between tests

---

## NEXT THREAD: loki_arima

### Motivation
ARIMA modelling is the natural next step after stationarity testing.
We have all prerequisites: `lagMatrix`, `pacf`, `acf`, `ljungBox`, `diff`,
`StationarityAnalyzer` for auto-d, and `lm` for future MLE fitting.

### Primary use case
Climatological data (6h/1h resolution) with strong serial dependence after
deseasonalization. ARIMA characterises the residual autocorrelation structure
and enables forecasting. SARIMA extends this to data with a remaining seasonal
component.

### loki_core additions for ARIMA thread

No new core files needed -- all building blocks exist. The ARIMA thread
uses existing: `lagMatrix`, `lsq`, `lm`, `pacf`, `acf`, `ljungBox`, `diff`.

### ArimaResult (arimaResult.hpp)

```cpp
struct ArimaOrder {
    int p{0};   // AR order
    int d{0};   // differencing order
    int q{0};   // MA order
};

struct SarimaOrder {
    int P{0};   // seasonal AR order
    int D{0};   // seasonal differencing order
    int Q{0};   // seasonal MA order
    int s{0};   // seasonal period in samples (0 = no SARIMA)
};

struct ArimaResult {
    ArimaOrder              order;
    SarimaOrder             seasonal;
    std::vector<double>     arCoeffs;     // phi_1 .. phi_p
    std::vector<double>     maCoeffs;     // theta_1 .. theta_q
    double                  intercept{0.0};
    double                  sigma2{0.0};  // residual variance
    double                  logLik{0.0};
    double                  aic{0.0};
    double                  bic{0.0};
    std::vector<double>     residuals;
    std::vector<double>     fitted;
    std::size_t             n{0};
    std::string             method;       // "css" | "mle"
};

struct ForecastResult {
    std::vector<double> forecast;
    std::vector<double> lower95;
    std::vector<double> upper95;
    int                 horizon{0};
};
```

### ArimaFitter (arimaFitter.hpp / .cpp)

```cpp
class ArimaFitter {
public:
    struct Config {
        ArimaOrder  order     {};
        SarimaOrder seasonal  {};
        std::string method    {"css"};   // "css" | "mle"
        int         maxIter   {200};
        double      tol       {1.0e-8};
    };
    using Config = ArimaFitterConfig;  // outside class, GCC 13 rule

    explicit ArimaFitter(Config cfg = {});
    ArimaResult fit(const std::vector<double>& y) const;

private:
    Config m_cfg;
    ArimaResult fitCss(const std::vector<double>& y) const;  // CSS via OLS on AR part
    ArimaResult fitMle(const std::vector<double>& y) const;  // MLE via L-M (future)
};
```

### ArimaOrderSelector (arimaOrderSelector.hpp / .cpp)

```cpp
// Selects optimal (p, q) by AIC/BIC grid search over [0..maxP] x [0..maxQ].
// d is determined externally from StationarityAnalyzer or config.
// Uses CSS fitting for speed during grid search.
class ArimaOrderSelector {
public:
    struct Config {
        int         maxP      {5};
        int         maxQ      {5};
        std::string criterion {"aic"};  // "aic" | "bic"
        std::string method    {"css"};
    };
    explicit ArimaOrderSelector(Config cfg = {});
    ArimaOrder select(const std::vector<double>& y, int d) const;
};
```

### ArimaForecaster (arimaForecaster.hpp / .cpp)

```cpp
class ArimaForecaster {
public:
    explicit ArimaForecaster(const ArimaResult& result);
    ForecastResult forecast(int horizon) const;
};
```

### ArimaAnalyzer (arimaAnalyzer.hpp / .cpp)

```cpp
// Top-level orchestrator. Calls stationarity -> order selection -> fit -> diagnose.
class ArimaAnalyzer {
public:
    using Config = ArimaConfig;   // from config.hpp
    explicit ArimaAnalyzer(Config cfg = {});
    ArimaResult analyze(const std::vector<double>& y) const;
};
```

### ArimaConfig (to add to config.hpp)

```cpp
struct ArimaFitterConfig {
    std::string method         {"css"};   // "css" | "mle"
    int         maxIterations  {200};
    double      tol            {1.0e-8};
};

struct ArimaSarimaConfig {
    int P{0}, D{0}, Q{0}, s{0};          // 0 = no seasonal component
};

struct ArimaConfig {
    // Gap filling
    std::string  gapFillStrategy  {"linear"};
    int          gapFillMaxLength {0};

    // Deseasonalization (same options as stationarity)
    DeseasonalizationConfig deseasonalization {};

    // Order
    bool         autoOrder  {true};    // auto-select p,q via AIC/BIC
    int          p          {1};       // AR order (used if autoOrder=false)
    int          d          {-1};      // differencing (-1 = auto from stationarity)
    int          q          {0};       // MA order (used if autoOrder=false)
    int          maxP       {5};       // grid search upper bound
    int          maxQ       {5};
    std::string  criterion  {"aic"};   // "aic" | "bic"

    // Seasonal (SARIMA -- s=0 disables)
    ArimaSarimaConfig seasonal {};

    // Fitting
    ArimaFitterConfig fitter {};

    // Forecast
    bool         computeForecast   {false};
    double       forecastHorizon   {0.0};    // days
    double       confidenceLevel   {0.95};
    double       significanceLevel {0.05};
};
```

### Pipeline in apps/loki_arima/main.cpp

```
1. Load series (DataManager)
2. GapFiller
3. Deseasonalizer (median_year / moving_average / none)
4. If d == -1: run StationarityAnalyzer -> use recommendedDiff as d
5. Apply differencing (diff() x d times)
6. ArimaOrderSelector::select() -> best (p, q) if autoOrder=true
7. ArimaFitter::fit() -> ArimaResult
8. LjungBox on residuals (diagnostic, log result)
9. ArimaForecaster::forecast() if computeForecast=true
10. Log results + AIC/BIC/sigma2/coefficients
11. CSV: mjd; original; residual; fitted; forecast (NaN where not available)
12. Protocol: ORDER/COEFFICIENTS/DIAGNOSTICS/FORECAST
13. Plots: timeSeries of residuals, ACF/PACF of model residuals,
           forecast plot (if enabled)
```

### CSS fitting approach (first implementation)

AR part fitted via `lagMatrix` + OLS (existing `lsq`).
MA part approximated via innovation recursion (Hannan-Rissanen two-step):
1. Fit high-order AR to get innovations (residuals)
2. Use lagged innovations as regressors for MA part
3. Joint OLS on AR+MA regressors

This avoids nonlinear optimisation for the initial implementation.
MLE via Levenberg-Marquardt (`lm.hpp`) can be added as `method: "mle"` later.

### Files to request at thread start
- `libs/loki_core/include/loki/math/lagMatrix.hpp`
- `libs/loki_core/src/math/lagMatrix.cpp`
- `libs/loki_core/include/loki/math/lsq.hpp`
- `libs/loki_core/include/loki/math/lsqResult.hpp`
- `libs/loki_core/include/loki/math/lm.hpp`
- `libs/loki_core/include/loki/stats/descriptive.hpp`
- `libs/loki_core/include/loki/stats/hypothesis.hpp`
- `libs/loki_core/include/loki/core/config.hpp`
- `libs/loki_core/src/core/configLoader.cpp`
- `libs/loki_core/CMakeLists.txt`
- `libs/loki_stationarity/include/loki/stationarity/stationarityAnalyzer.hpp`
- `libs/loki_stationarity/include/loki/stationarity/stationarityResult.hpp`
- `apps/loki_stationarity/main.cpp` (pipeline pattern reference)
- `libs/loki_stationarity/CMakeLists.txt` (CMake pattern reference)

---

## Domain Knowledge -- Use Cases

LOKI is used across three domains with different characteristics.
This informs module priority and parameter choices.

### Climatological data (6h / 1h resolution)
- Strong annual and sub-annual seasonal cycle
- Long-range memory (Hurst > 0.5 typical)
- Change points from instrument changes, station relocations
- Recommended deseasonalization: `median_year`
- Typical `ma_window_size`: 1461 (6h, 1 year) or 8760 (1h, 1 year)
- Typical `min_segment_points`: 600+ for change point detection
- `significance_level`: 0.01 recommended for dense data
- ARIMA: SARIMA may be needed if seasonal residuals remain after deseasonalization

### GNSS data (coordinates / velocities, seconds resolution)
- Draconitic period: 351.4 days (use `period: 351.4` in regression)
- Multipath, antenna phase centre variations
- Step-like change points from equipment changes
- Kalman filter is the natural processing tool (future `loki_kalman`)
- Lomb-Scargle for uneven sampling (future `loki_spectral`)

### Train sensor data (radar / odometric velocities, ms resolution)
- No seasonal component (`deseasonalization: none`)
- Distinct journey phases: acceleration -> cruise -> braking
- Logistic regression for velocity profile modelling (already in `loki_regression`)
- DBSCAN / k-means for phase segmentation (future `loki_clustering`)
- Kalman filter for sensor fusion (radar + odometry, future `loki_kalman`)
- No ARIMA needed; focus on `loki_kalman` and `loki_clustering`

---

## Module Roadmap (priority order)

| Priority | Module | Primary use case | Key dependency |
|---|---|---|---|
| 1 | `loki_arima` | Climatological residual modelling, forecasting | stationarity, lagMatrix, lsq |
| 2 | `loki_kalman` | GNSS processing, train sensor fusion | loki_core only |
| 3 | `loki_svd` | SSA/PCA for climatological trend extraction | svd.hpp (already in core) |
| 4 | `loki_spectral` | FFT + Lomb-Scargle for GNSS draconitic, climate periods | loki_core only |
| 5 | `loki_clustering` | Train phase segmentation (DBSCAN, k-means) | loki_core only |
| 6 | `loki_qc` | Quality control, gap flagging, automated reporting | loki_outlier |
| 7 | `loki_decomposition` | Classical STL-like trend/seasonal/residual decomposition | loki_core only |

### Notes on loki_kalman
- Standalone module, no dependency on other loki modules
- State vector: position/velocity for GNSS; velocity/acceleration for train
- Extended Kalman Filter (EKF) for nonlinear motion models
- Sensor fusion: multiple measurement sources with different noise covariances
- High priority because it serves both GNSS and train domains

### Notes on loki_svd
- `SvdDecomposition` in `loki_core/math/svd.hpp` already prepared (header-only)
- SSA (Singular Spectrum Analysis): embed -> SVD -> reconstruct
- Separate application from `loki_decomposition` (different mathematical approach)
- Key for extracting trend + quasi-periodic components without parametric model

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
- API: `gp("command")` -- no `<<` operator, no `flush()`, no `fwdSlash()`
- All new plot types go into `loki::Plot` (plot.hpp/cpp) -- never use Gnuplot
  directly in app-layer code
- Font: `'Sans,12'` (not `'Helvetica,12'`)
- Terminal: `noenhanced` (prevents underscore subscript interpretation)
- `plot.cpp` needs `#include "loki/stats/descriptive.hpp"` for `pacf()` access

### `distributions.hpp` namespace
Functions are in `loki::stats::` -- e.g. `loki::stats::chi2Quantile(p, df)`.
`tQuantile` takes `double df` (not int).

### PP test -- critical implementation note
The correction term uses `XtX(levelCol, levelCol)` -- the normal equations matrix,
NOT `XtXinv(levelCol, levelCol)`. Using the inverse causes the statistic to explode
to large positive values (e.g. +1051 instead of -38). This was a confirmed bug
fixed in the stationarity thread.

### lagMatrix.cpp -- must be in loki_core CMakeLists.txt
`lagMatrix.cpp` must be listed in `libs/loki_core/CMakeLists.txt` sources.

### loki.sh -- unknown app guard
The argument parser in `loki.sh` raises an error for unknown app names:
```bash
*) if [[ "${arg}" == *.json ]]; then CONFIG_OVERRIDE="${arg}"
   elif [[ "${arg}" != --* ]]; then
       echo "[LOKI] ERROR: Unknown argument '${arg}'." >&2
       echo "       Known apps: ${!APP_EXE[*]}" >&2
       exit 1
   fi ;;
```
This prevents silently running the wrong app when the name is mistyped.

---

## Config Structs

### StationarityConfig (config.hpp) -- complete
```cpp
struct AdfConfig { std::string trendType{"constant"}; int maxLags{-1};
                   std::string lagSelection{"aic"}; double significanceLevel{0.05}; };
struct KpssConfig { std::string trendType{"level"}; int lags{-1};
                    double significanceLevel{0.05}; };
struct PpConfig   { std::string trendType{"constant"}; int lags{-1};
                    double significanceLevel{0.05}; };
struct StationarityDifferencingConfig { bool apply{false}; int order{1}; };
struct StationarityTestsConfig { bool adfEnabled{true}; bool kpssEnabled{true};
    bool ppEnabled{true}; bool runsTestEnabled{true};
    AdfConfig adf{}; KpssConfig kpss{}; PpConfig pp{}; };
struct StationarityConfig {
    DeseasonalizationConfig        deseasonalization{};
    StationarityDifferencingConfig differencing{};
    StationarityTestsConfig        tests{};
    double                         significanceLevel{0.05};
};
```

### ArimaConfig (config.hpp) -- NEXT THREAD
See design above in "NEXT THREAD: loki_arima" section.

### HatMatrixSection (config.hpp)
```cpp
struct HatMatrixSection { int arOrder{5}; double significanceLevel{0.05}; bool enabled{true}; };
```

### NonlinearConfig (config.hpp)
```cpp
enum class NonlinearModelEnum { EXPONENTIAL, LOGISTIC, GAUSSIAN };
struct NonlinearConfig { NonlinearModelEnum model{NonlinearModelEnum::EXPONENTIAL};
    std::vector<double> initialParams{}; int maxIterations{100};
    double gradTol{1.0e-8}; double stepTol{1.0e-8};
    double lambdaInit{1.0e-3}; double lambdaFactor{10.0}; double confidenceLevel{0.95}; };
```

---

## Statistical / Domain Key Learnings

- x-axis convention for regression: `x = mjd - tRef` for numerical stability
- AR order selection: PACF cuts off at lag p for AR(p) -- use PACF, not ACF
- ACF cuts off at lag q for MA(q) -- use ACF for MA order selection
- For 6h climatological data: `min_segment_points >= 600` for change point detection
- `significance_level = 0.01` recommended for hat_matrix with dense data
- KPSS is very sensitive to change points and trends -- a single undetected shift
  can cause eta >> critical value (e.g. 15.7 vs 0.46). This is expected behaviour.
- PP test: `Z(t) = tAlpha * (s/lambda) - correction` where correction uses
  `XtX_{gamma,gamma}` (normal equations), NOT `XtXinv`. Confirmed bug source.
- ADF and PP should give consistent signs; large discrepancy indicates numerical issue
- For climatological residuals after deseasonalization: ADF/PP typically reject unit
  root, but KPSS may still reject stationarity due to change points or long memory.
  Correct workflow: homogenize first, then test stationarity.
- ARIMA for train data: not recommended (no stationarity, etapy jazdy dominate).
  Use Kalman filter + clustering instead.
- Draconitic period for GNSS: 351.4 days (not 365.25)

---

## Approach & Patterns

- **Thread-based workflow:** Each conversation handles a specific milestone.
  Always begins with "Working on LOKI -- see CLAUDE.md" + attached CLAUDE.md + relevant source files.
- **Design-first:** Discuss architecture and approve signatures before any implementation.
- **Iterative debugging:** Build errors shared in chunks; systematic resolution.
- **Documentation artifacts:** Each thread concludes with updated CLAUDE.md and
  relevant reference docs (CONFIG_REFERENCE.md section).
- **Config philosophy:** JSON configs kept clean; all documentation in CONFIG_REFERENCE.md.
- **Output conventions:** Protocols -> `OUTPUT/PROTOCOLS/`; plots -> `OUTPUT/IMG/`;
  CSV -> `OUTPUT/CSV/`; logs -> `OUTPUT/LOG/`.

---

## Output Conventions
- Protocols/reports: `OUTPUT/PROTOCOLS/`
- Images: `OUTPUT/IMG/`
- CSV: `OUTPUT/CSV/`
- Logs: `OUTPUT/LOG/`
- Plot naming: `[program]_[dataset]_[parameter]_[plottype].[format]`
- Program prefix: `core` | `homogeneity` | `outlier` | `filter` | `regression` | `stationarity` | `arima`
- CSV delimiter: semicolon (`;`)

---

## Notes and Reminders
- Data files and plot outputs are **not** committed to the repository.
- Third-party dependencies via CMake `FetchContent` only -- no vendored source.
- Build directory `build/` is gitignored.
- Do NOT add license/copyright blocks at the top of source files.
- `loader.hpp` is in `loki_core/io/`, NOT in `timeseries/`.
- `hatMatrix.hpp`, `svd.hpp`, `lm.hpp`, `lagMatrix.hpp` are in `loki_core/math/`.
- `svd.cpp` does NOT exist -- `SvdDecomposition` is header-only in `svd.hpp`.
- `lagMatrix.cpp` MUST be listed in `libs/loki_core/CMakeLists.txt` sources.
- `loki_stationarity` CMake: library `loki_stationarity`, executable
  `loki_stationarity_app` with `OUTPUT_NAME "loki_stationarity"`.
- `loki_arima` CMake: library `loki_arima`, executable
  `loki_arima_app` with `OUTPUT_NAME "loki_arima"`.
- Classical decomposition (`loki_decomposition`) and SVD-based SSA (`loki_svd`)
  are separate applications serving different mathematical purposes.
- `window` parameters in filters are in samples, not time units.