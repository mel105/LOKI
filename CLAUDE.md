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

### CRITICAL -- namespace for free functions in .cpp files
- `using namespace loki::stats` (or any sub-namespace) is NOT sufficient for
  defining free functions -- they will be compiled in the global namespace and
  the linker will not find them.
- Free functions declared in `namespace loki::stats` MUST be defined inside an
  explicit `namespace loki::stats { }` block in the `.cpp` file.
- `using namespace loki;` may remain outside the block (for exception names).
- This applies to: `bootstrap.cpp`, `permutation.cpp`, `sampling.cpp`, and any
  future `.cpp` files defining free functions in a sub-namespace.
- Example correct pattern:
  ```cpp
  using namespace loki;  // for exceptions only

  namespace loki::stats {

  BootstrapResult percentileCI(...) { ... }
  BootstrapResult bcaCI(...) { ... }

  } // namespace loki::stats
  ```

---

## Plot Output Naming Convention

All plot output files follow this naming convention:
```
[program]_[dataset]_[parameter]_[plottype].[format]
```

- **program** : `core` | `homogeneity` | `outlier` | `filter` | `regression` |
                `stationarity` | `arima` | `ssa` | `decomposition` | `spectral` |
                `kalman` | `qc` | `clustering` | `simulate` | `evt` | `kriging` |
                `spline`
- **dataset** : input filename stem without extension
- **parameter** : series `componentName` from metadata
- **plottype** : descriptive short name
- **format** : `png` | `eps` | `svg`

### Rules for plotters
- Temporary data files use `.tmp_` prefix and are deleted immediately after gnuplot runs.
- Gnuplot on Windows requires forward slashes -- use `fwdSlash()` helper on all paths.
- gnuplot terminal: `pngcairo noenhanced font 'Sans,12'`
- All new plot types are added to `loki::Plot` (plot.hpp/cpp in loki_core) -- never use
  Gnuplot directly in app-layer code. The Gnuplot API is `gp("command")`, no `<<` operator.
- Module-specific plotters live in their own module,
  delegate generic plots (ACF, histogram, QQ) to `loki::Plot::residualDiagnostics()`.
- `Plot::residualDiagnostics()` is a **non-static member method** -- always instantiate
  `Plot corePlot(m_cfg)` and call `corePlot.residualDiagnostics(residuals, fittedValues, title)`.
- **CRITICAL -- gnuplot pipe and tmp files**: Do NOT write data to a tmp file and then
  immediately open a gnuplot pipe to read it. On Windows/MINGW64 the OS buffer may not
  be flushed before gnuplot opens the file. Use gnuplot inline data (`plot '-'`) for
  PSD, amplitude, phase, and other single-dataset plots. Only use tmp files for formats
  that cannot be sent inline (e.g. `nonuniform matrix` for spectrograms) -- in that case
  call `ofs.flush(); ofs.close();` explicitly before opening the Gnuplot object.
- **gnuplot `-persist` flag is REMOVED** from `gnuplot.cpp`. The constructor now calls
  `gnuplot` without `-persist`. This eliminates zombie gnuplot processes on Windows that
  cause race conditions with tmp files in subsequent runs.

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
loki_homogeneity      (depends on loki_core + loki_outlier)        <- COMPLETE

loki_filter           (depends on loki_core only)

loki_regression       (depends on loki_core only)

loki_stationarity     (depends on loki_core only)

loki_arima            (depends on loki_core + loki_stationarity)   <- COMPLETE

loki_ssa              (depends on loki_core only)                   <- COMPLETE

loki_decomposition    (depends on loki_core only)                   <- COMPLETE

loki_spectral         (depends on loki_core only)                   <- COMPLETE

loki_kalman           (depends on loki_core only)                   <- COMPLETE

loki_qc               (depends on loki_core + loki_outlier)        <- COMPLETE

loki_clustering       (depends on loki_core only)                   <- COMPLETE

loki_simulate         (depends on loki_core + loki_arima +
                        loki_kalman)                                <- COMPLETE

loki_evt              (depends on loki_core only)                   <- COMPLETE

loki_kriging          (depends on loki_core only)                   <- PLANNED

loki_spline           (depends on loki_core only)                   <- PLANNED
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
|   +-- loki_arima/
|   +-- loki_ssa/
|   +-- loki_decomposition/
|   +-- loki_spectral/
|   +-- loki_kalman/
|   +-- loki_qc/
|   +-- loki_clustering/
|   +-- loki_simulate/
|   +-- loki_evt/                    <- COMPLETE
|   +-- loki_kriging/                <- PLANNED
|   +-- loki_spline/                 <- PLANNED
+-- libs/
|   +-- loki_core/
|   |   +-- include/loki/
|   |       +-- core/         (exceptions, version, config, configLoader, logger, nanPolicy)
|   |       +-- timeseries/   (timeSeries, timeStamp, gapFiller, deseasonalizer,
|   |       |                  medianYearSeries)
|   |       +-- stats/        (descriptive, filter, distributions, hypothesis, metrics,
|   |       |                  wCorrelation, sampling, bootstrap, permutation)
|   |       +-- io/           (loader, dataManager, gnuplot, plot)
|   |       +-- math/         (lsqResult, lsq, designMatrix, hatMatrix, svd, lm,
|   |                          lagMatrix, embedMatrix, randomizedSvd, spline, nelderMead)
|   +-- loki_outlier/
|   +-- loki_homogeneity/
|   +-- loki_filter/
|   +-- loki_regression/
|   +-- loki_stationarity/
|   +-- loki_arima/
|   +-- loki_ssa/
|   +-- loki_decomposition/
|   +-- loki_spectral/
|   +-- loki_kalman/
|   +-- loki_qc/
|   +-- loki_clustering/
|   +-- loki_simulate/
|   +-- loki_evt/                    <- COMPLETE
|   +-- loki_kriging/                <- PLANNED
|   +-- loki_spline/                 <- PLANNED
+-- tests/
|   +-- CMakeLists.txt
|   +-- demo/
|   |   +-- CMakeLists.txt
|   |   +-- demo_sampling.cpp
|   |   +-- demo_bootstrap.cpp
|   |   +-- demo_permutation.cpp
|   |   +-- input/
|   |   +-- png/
|   |   +-- protocol/
|   +-- unit/
+-- config/
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
./scripts/loki.sh build evt --copy-dlls
./scripts/loki.sh run evt
./scripts/loki.sh test --rebuild
```

### CMake target name collision
Executable: `loki_evt_app` with `OUTPUT_NAME "loki_evt"`.
Same pattern for all apps: library target != executable target name.

### Runtime DLLs (Windows)
`libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`
`--copy-dlls` flag in `loki.sh` handles this automatically.

### Eigen3 -- SYSTEM includes (critical)
```cmake
target_include_directories(loki_core SYSTEM PUBLIC
    $<TARGET_PROPERTY:Eigen3::Eigen,INTERFACE_INCLUDE_DIRECTORIES>)
```

### loki.sh registration (all apps)
```
["homogeneity"]="loki_homogeneity_app"
["outlier"]="loki_outlier_app"
["filter"]="loki_filter_app"
["regression"]="loki_regression_app"
["stationarity"]="loki_stationarity_app"
["arima"]="loki_arima_app"
["ssa"]="loki_ssa_app"
["decomposition"]="loki_decomposition_app"
["spectral"]="loki_spectral_app"
["kalman"]="loki_kalman_app"
["qc"]="loki_qc_app"
["clustering"]="loki_clustering_app"
["simulate"]="loki_simulate_app"
["evt"]="loki_evt_app"              <- COMPLETE
["kriging"]="loki_kriging_app"      <- PLANNED
["spline"]="loki_spline_app"        <- PLANNED
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
                  with `AdfConfig`, `KpssConfig`, `PpConfig`,
                  `ArimaConfig` with `ArimaFitterConfig`, `ArimaSarimaConfig`,
                  `SsaConfig` with `SsaWindowConfig`, `SsaGroupingConfig`,
                  `SsaReconstructionConfig`,
                  `DecompositionConfig` with `ClassicalDecompositionConfig`,
                  `StlDecompositionConfig`,
                  `SpectralConfig` with `SpectralFftConfig`,
                  `SpectralLombScargleConfig`, `SpectralSpectrogramConfig`,
                  `SpectralPeakConfig`,
                  `KalmanConfig` with `KalmanNoiseConfig`, `KalmanForecastConfig`,
                  `QcConfig` with `QcOutlierConfig`, `QcSeasonalConfig`,
                  `ClusteringConfig` with `ClusteringFeatureConfig`,
                  `KMeansClusteringConfig`, `DbscanClusteringConfig`,
                  `ClusteringOutlierConfig`,
                  `SplineFilterConfig`,
                  `EvtConfig` with `EvtThresholdConfig`, `EvtCiConfig`,
                  `EvtBlockMaximaConfig`, `EvtDeseasonalizationConfig`
- `configLoader.hpp / .cpp` -- vsetky parse metody vratane `_parseEvt`
- `timeStamp.hpp / .cpp`
- `timeSeries.hpp / .cpp`
- `gapFiller.hpp / .cpp` -- Strategy::SPLINE pridana
- `deseasonalizer.hpp / .cpp`
- `medianYearSeries.hpp / .cpp`
- `descriptive.hpp / .cpp`
- `filter.hpp / .cpp`
- `distributions.hpp / .cpp`
- `hypothesis.hpp / .cpp`
- `metrics.hpp / .cpp`
- `wCorrelation.hpp / .cpp`
- `sampling.hpp / .cpp`
- `bootstrap.hpp / .cpp`
- `permutation.hpp / .cpp`
- `loader.hpp / .cpp`, `dataManager.hpp / .cpp`
- `gnuplot.hpp / .cpp` -- `-persist` flag REMOVED
- `plot.hpp / .cpp`

### loki_core/math -- complete
- `lsqResult.hpp`
- `lsq.hpp / .cpp`
- `designMatrix.hpp / .cpp`
- `hatMatrix.hpp / .cpp`
- `svd.hpp` -- header-only (BDCSVD linking bug workaround)
- `lm.hpp / .cpp`
- `lagMatrix.hpp / .cpp`
- `embedMatrix.hpp / .cpp`
- `randomizedSvd.hpp / .cpp`
- `spline.hpp / .cpp`
- `nelderMead.hpp / .cpp`         <- added for loki_evt (Nelder-Mead minimiser)

### loki_outlier -- complete (O1-O4)
### loki_homogeneity -- complete
### loki_stationarity -- complete
### loki_filter -- complete (vrátane SplineFilter + FilterAnalyzer)
### loki_regression -- complete (R1-R8)
### loki_arima -- complete
### loki_ssa -- complete (S1-S5)
### loki_decomposition -- complete
### loki_spectral -- complete
### loki_kalman -- complete
### loki_qc -- complete
### loki_clustering -- complete
### loki_simulate -- complete

### loki_evt -- complete
- `evtResult.hpp`               -- GpdFitResult, GevFitResult, ReturnLevelCI, GoFResult, EvtResult
- `gpd.hpp / .cpp`              -- GPD: cdf, quantile, logLik, fit (MLE+PWM fallback n<50)
- `gev.hpp / .cpp`              -- GEV: cdf, quantile, logLik, fit (MLE)
- `thresholdSelector.hpp / .cpp` -- mean excess elbow detection, manual threshold
- `evtAnalyzer.hpp / .cpp`      -- orchestrator: run() pattern, gap fill, deseasonalization,
                                    POT/block maxima dispatch, profile likelihood / bootstrap /
                                    delta CI, AD+KS GoF, protocol, CSV
- `plotEvt.hpp / .cpp`          -- mean_excess, stability, return_levels, exceedances, gpd_qq

**Key implementation notes:**
- Nelder-Mead (loki_core/math/nelderMead.hpp) used for GPD MLE and GEV MLE.
  Do NOT use self-contained NM -- always use loki::math::nelderMead.
- GPD MLE uses log-transform for sigma (sigma = exp(p[0])) and barrier penalty
  for xi <= -0.5 constraint.
- PWM fallback triggered when n_exceedances < 50. GpdFitResult::converged = false.
- Profile likelihood CI: chi2(1, level)/2 drop threshold, Brent root finding.
  If Brent search fails (flat logLik), CI bounds fall back to estimate with LOKI_WARNING.
- Deseasonalization: uses loki::Deseasonalizer + loki::MedianYearSeries (same API
  as loki_homogeneity). MedianYearSeries constructor: `MedianYearSeries(series, cfg)`
  where cfg.minYears (NOT minYearsPerSlot). No separate build() call.
- Return levels in residual units when deseasonalization enabled -- noted in protocol.
- evtAnalyzer.hpp requires `<functional>` and `<loki/math/nelderMead.hpp>` includes.

---

## GapFiller::Strategy::SPLINE -- Deployment Status

| App | Status |
|---|---|
| `loki_filter` | DONE |
| `loki_homogeneity` | DONE |
| `loki_regression` | DONE |
| `loki_arima` | DONE |
| `loki_ssa` | DONE |
| `loki_decomposition` | DONE |
| `loki_spectral` | DONE |
| `loki_kalman` | DONE |
| `loki_stationarity` | DONE |
| `loki_qc` | N/A |
| `loki_clustering` | DONE |
| `loki_simulate` | DONE |
| `loki_evt` | DONE |

---

## Planned New Applications

### loki_kriging -- NEXT

**Purpose:** Spatial/temporal interpolation using Kriging methods.
**Dependencies:** loki_core only

**Functionality:**
- **Simple Kriging (SK):** known mean mu. Suitable when stationarity is guaranteed.
- **Ordinary Kriging (OK):** unknown local mean -- most commonly used.
  Constraint: sum of weights = 1.
- **Universal Kriging (UK):** mean is a polynomial trend. Suitable when a
  deterministic trend is present (e.g. altitude dependence of temperature).
- **Variogram modelling:**
  - Empirical variogram: gamma(h) = 0.5 * mean[(Z(x+h) - Z(x))^2]
  - Theoretical models: spherical, exponential, Gaussian, nugget, power
  - Fitting: WLS (weighted least squares) on empirical variogram
  - Nugget effect: discontinuity at h=0 (measurement noise + micro-variability)
- **Cross-validation:** leave-one-out -- estimate each point from its neighbours,
  compare to observed value. RMSE and standardized residuals as diagnostics.
- **Kriging variance:** automatic output -- uncertainty estimate at each point.

**Domain notes:**
- For GNSS networks: spatial interpolation of tropospheric delay between stations.
- For climatological data: spatial interpolation between measurement sites.
- Isotropy assumed in basic implementation (no directional anisotropy).
- Variogram fitting is the most sensitive step -- bad variogram = bad predictions.

**Planned files:**
```
libs/loki_kriging/
  include/loki/kriging/
    krigingResult.hpp          -- VariogramModel, VariogramFitResult, KrigingResult
    variogram.hpp / .cpp       -- empirical variogram + theoretical models + WLS fit
    kriging.hpp / .cpp         -- SK / OK / UK solver (Kriging system via Eigen)
    krigingAnalyzer.hpp / .cpp -- orchestrator: run() pattern
    plotKriging.hpp / .cpp     -- variogram plot, kriging map, cross-validation plot
  src/ ...
apps/loki_kriging/
  main.cpp
  CMakeLists.txt
libs/loki_kriging/CMakeLists.txt
```

**KrigingConfig structure (planned):**
```cpp
struct KrigingVariogramConfig {
    std::string model    = "spherical"; // "spherical"|"exponential"|"gaussian"|"power"
    double      nugget   = 0.0;         // 0 = fit automatically
    double      sill     = 0.0;         // 0 = fit automatically
    double      range    = 0.0;         // 0 = fit automatically
    int         nLagBins = 20;          // number of lag bins for empirical variogram
    double      maxLag   = 0.0;         // 0 = auto: half max pairwise distance
};

struct KrigingConfig {
    std::string            method            = "ordinary";
    // "simple" | "ordinary" | "universal"
    std::string            gapFillStrategy   = "linear";
    int                    gapFillMaxLength  = 0;
    double                 knownMean         = 0.0;   // simple kriging only
    int                    trendDegree       = 1;     // universal kriging
    bool                   crossValidate     = true;
    double                 significanceLevel = 0.05;
    KrigingVariogramConfig variogram         {};
};
```

**Files to request at thread start:**
- `config.hpp`, `configLoader.hpp`, `configLoader.cpp`
- `timeSeries.hpp`
- `apps/loki_evt/main.cpp` (pipeline pattern -- simplest standalone app)
- `apps/loki_clustering/main.cpp` (alternative pattern)

**Note on input data format for kriging:**
Kriging requires spatial coordinates (x, y) + observed values z for each
station/point. The standard LOKI TimeSeries has no spatial coordinate field.
Two options to resolve:
1. Load multiple series (one per station) and use station metadata for coordinates.
2. Add an optional spatial coordinate field to `SeriesMetadata` (preferred long-term).
This design question must be resolved at thread start before any code is written.

### loki_spline -- PLANNED
(design unchanged from previous CLAUDE.md)

---

## Known Issues and Workarounds

### GCC 13 aggregate-init bug -- CRITICAL
Any `Config` struct with enum member + default value CANNOT be a nested struct inside
a class. Define outside with descriptive name, add `using` alias inside class.
All `XxxConfig` structs in `config.hpp` follow this pattern.

### Eigen BDCSVD linking bug -- CRITICAL
`Eigen::BDCSVD` causes `undefined reference` errors when used in `.cpp` files compiled
into static libraries on Windows/GCC. Workarounds in place:
1. `SvdDecomposition` (`svd.hpp`) is **header-only**.
2. `CalibrationRegressor` uses `Eigen::JacobiSVD` directly.
3. `SsaAnalyzer` uses randomized SVD (small internal matrices only).
4. `loki_kalman` uses `Eigen::LLT` for covariance updates -- safe in static libs.
Do NOT use `BDCSVD` in any `.cpp` compiled into a static library.

### Free functions in sub-namespaces -- CRITICAL
`using namespace loki::stats` v `.cpp` NESTACI pre definicie volnych funkcii.
Funkcie su potom v globalnom namespace a linker ich nenajde.
Riesenie: obalit vsetky definicie do explicitneho `namespace loki::stats { }` bloku.

### MedianYearSeries API (critical -- caused build error in loki_evt)
Constructor: `MedianYearSeries(const TimeSeries& series, Config cfg = Config{})`
Config field: `cfg.minYears` (NOT `minYearsPerSlot`, NOT separate `build()` call).
The series is passed directly to the constructor -- no two-step construction.

### nelderMead -- must be in loki_core CMakeLists
`nelderMead.cpp` must be listed in `add_library(loki_core STATIC ...)`.
Forgetting this causes `undefined reference` at link time for loki_evt and
any future module that uses `loki::math::nelderMead`.

### evtAnalyzer.hpp -- required includes
Must include `<functional>` (for `std::function`) and
`<loki/math/nelderMead.hpp>` (for `_brentRoot` and profile likelihood).
Missing these caused build errors:
- `std::function has not been declared`
- `loki::math has not been declared`

### `TimeSeries` API
- No `observations()` method -- use direct indexing: `ts[i].value`, `ts[i].time`
- `TimeSeries::append(const TimeStamp& time, double value, uint8_t flag = 0)`
- `::TimeStamp` is NOT in `namespace loki` -- always qualify as `::TimeStamp`
- MJD getter on TimeStamp: `ts[i].time.mjd()` (not `toMjd()`)
- UTC string: `ts[i].time.utcString()`
- GPS total seconds: `ts[i].time.gpsTotalSeconds()`

### `std::numbers::pi` instead of `M_PI`
Use `std::numbers::pi` (C++20 `<numbers>`).

### Logger macros
- `LOKI_WARNING` (not `LOKI_WARN`)
- `LOKI_INFO` accepts only a single string argument -- use concatenation

### Gnuplot on Windows -- CRITICAL
- API: `gp("command")` -- no `<<` operator
- All module-specific plotters use local `fwdSlash()` static private helper
- Font: `'Sans,12'` (not `'Helvetica,12'`)
- Terminal: `noenhanced`
- `-persist` flag REMOVED from gnuplot.cpp -- do not add it back
- Use `plot '-'` (inline data) instead of tmp files wherever possible
- For tmp files (spectrogram matrix format): call `ofs.flush(); ofs.close()`
  explicitly before constructing the `Gnuplot` object
- `Plot::residualDiagnostics()` is non-static -- instantiate `Plot corePlot(cfg)`

### loki_evt -- domain notes
- POT/GPD preferred over GEV/block_maxima for sensor data (train velocity etc.)
  Blocks are physically arbitrary for short measurement campaigns.
- SIL 4: return_periods = [1e8] with time_unit = "hours". Profile likelihood CI
  is mandatory -- delta method severely underestimates upper CI for large T.
- xi > 0.5 with small n: often a sign of PWM fallback or too-high threshold.
  Inspect stability plot before interpreting return levels.
- Auto threshold (elbow on mean excess): may be suboptimal for short series (n < 500).
  Always visually inspect mean_excess and stability plots.
- Profile likelihood Brent search: falls back to point estimate with LOKI_WARNING
  if the logLik curve is too flat (common for xi near 0, large T).
- Deseasonalization with median_year: return levels are in residual units --
  explicitly noted in protocol.

---

## Statistical / Domain Key Learnings

- SSA window L should be a multiple of the dominant period for clean separation.
- Classical decomposition: residual variance > 40% is normal for sub-daily
  climatological data (synoptic variability dominates).
- STL outer loop (n_outer >= 1) needed when seasonal amplitude changes over time.
- Period in decomposition is always in samples -- 1461 for 6h data (1 year).
- Draconitic period for GNSS: 351.4 days (not 365.25).
- Lomb-Scargle preferred over FFT for GNSS and sensor data with frequent gaps.
- Kalman local_level K = Q/(Q+R); GPS IWV: Q~1e-4, R~4e-6 gives K~0.97.
- EM on GPS IWV finds near-interpolating filter (K~0.97) -- physically correct.
- EVT/SIL 4: GPD/POT preferred over GEV/block-maxima for small datasets and
  sensor data. Block maxima wastes data; POT uses all exceedances above threshold.
- EVT profile likelihood CI: logLik drops chi2(0.95)/2 = 1.92 for 95% CI.
  Delta method unreliable for large T (SIL 4) -- profile likelihood mandatory.
- EVT exceedance rate lambda: estimated from median dt of series in time_unit.
- GPD xi near 0 for deseasonalized climatological residuals (exponential tail).
  GPD xi < 0 for bounded physical quantities (velocity with hard maximum).
- Bootstrap: standard iid bootstrap INVALID for autocorrelated time series.
  Block bootstrap required. Block length >= effective decorrelation length.
- loki_simulate: ARIMA bootstrap diverges when Q/R>>1 for Kalman or when model
  does not have correct ACF structure. Diagnose via ACF comparison plot.
- Kriging: variogram fitting is the most sensitive step.
- NURBS vs B-spline: NURBS allows exact conic sections via rational weights.

---

## Module Roadmap (full picture)

| Priority | Module / Extension | Type | Status |
|---|---|---|---|
| done | `loki_arima` | app | COMPLETE |
| done | `loki_ssa` | app | COMPLETE |
| done | `loki_decomposition` | app | COMPLETE |
| done | `loki_spectral` | app | COMPLETE |
| done | `loki_kalman` | app | COMPLETE |
| done | `loki_qc` | app | COMPLETE |
| done | `loki_clustering` | app | COMPLETE |
| done | `loki_core/math/spline` | core extension | COMPLETE |
| done | `loki_core/math/nelderMead` | core extension | COMPLETE |
| done | `loki_core/stats/sampling` | core extension | COMPLETE |
| done | `loki_core/stats/bootstrap` | core extension | COMPLETE |
| done | `loki_core/stats/permutation` | core extension | COMPLETE |
| done | `GapFiller` SPLINE strategy | core extension | COMPLETE |
| done | `SplineFilter` in loki_filter | module extension | COMPLETE |
| done | `FilterAnalyzer` in loki_filter | module extension | COMPLETE |
| done | SNHT in loki_homogeneity | module extension | COMPLETE |
| done | PELT in loki_homogeneity | module extension | COMPLETE |
| done | BOCPD in loki_homogeneity | module extension | COMPLETE |
| done | HomogeneityAnalyzer + protocol | module extension | COMPLETE |
| done | `loki_simulate` | new app | COMPLETE |
| done | `loki_evt` | new app | COMPLETE |
| next | `loki_kriging` | new app | PLANNED |
| 4 | `loki_spline` | new app | PLANNED |

---

## Output Conventions
- Protocols/reports: `OUTPUT/PROTOCOLS/`
- Images: `OUTPUT/IMG/`
- CSV: `OUTPUT/CSV/`
- Logs: `OUTPUT/LOG/`
- CSV delimiter: semicolon (`;`)

---

## Notes and Reminders
- Data files and plot outputs are **not** committed to the repository.
- Third-party dependencies via CMake `FetchContent` only -- no vendored source.
- Build directory `build/` is gitignored.
- Do NOT add license/copyright blocks at the top of source files.
- `loader.hpp` is in `loki_core/io/`, NOT in `timeseries/`.
- `hatMatrix.hpp`, `svd.hpp`, `lm.hpp`, `lagMatrix.hpp`, `embedMatrix.hpp`,
  `randomizedSvd.hpp`, `spline.hpp`, `nelderMead.hpp` are in `loki_core/math/`.
- `wCorrelation.hpp`, `sampling.hpp`, `bootstrap.hpp`, `permutation.hpp`
  are in `loki_core/stats/`.
- `svd.cpp` does NOT exist -- `SvdDecomposition` is header-only in `svd.hpp`.
- `HatMatrixDetector` does NOT inherit from `OutlierDetector`.
- `NonlinearRegressor` does NOT inherit from `Regressor`.
- gnuplot.cpp: `-persist` flag is REMOVED. Do not restore it.
- TimeStamp: `.mjd()` | `.utcString()` | `.gpsTotalSeconds()`
- loki_qc seasonal section: auto-disable when median step > 3600s.
- loki_clustering: NaN edge points -> `_handleEdgePoints()`, never raw to fit.
- loki_clustering: `slope` preferred over `derivative` for noisy sensor data.
- loki_homogeneity existing detector: Yao & Davis (1986), NOT SNHT.
- loki_evt: SIL 4 use case (< 1e-8/h). GPD/POT for all data types.
  GEV/block_maxima only for climatological data (natural annual blocks).
  Profile likelihood CI mandatory for SIL 4 return periods (1e8 hours).
- loki_kriging: spatial coordinates needed -- design question to resolve at
  thread start (SeriesMetadata extension vs multi-series loading pattern).
- Block bootstrap required for autocorrelated series; iid bootstrap invalid.
- `SplineFilterConfig` definovana TYLKO v `config.hpp`, nie v `splineFilter.hpp`.
- `bootstrap.cpp`, `permutation.cpp`: funkcie musia byt v `namespace loki::stats { }`
  bloku, nie len cez `using namespace`.
- SNHT v rekurzivnom mode: min_segment_points >= 3-4 roky (4383-5844 pri 6h).
- PELT mbic je najkonzervativnejsi penalty typ -- odporucany default pre klimaticke data.
- BOCPD vyzaduje kalibраciu prior_beta blizko skutocnemu sigma^2 serie.
- `tests/demo/` gitignore: `png/`, `protocol/`, `input/` adresare ignorovat.
- CRITICAL: snhtDetector.hpp a snhtDetector.cpp existuju v repozitari ako PRAZDNE
  subory -- ide o systemovu vec. NIKDY sa nepytaj ci su prilozene alebo nie.
  Jednoducho ich naplnit podla dizajnu. TOTO PRAVIDLO NEMAZES.