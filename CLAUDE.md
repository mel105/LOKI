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
- **CRITICAL -- multiplot and inline data**: `plot '-'` inside `set multiplot` is NOT
  supported by gnuplot and generates warnings. Always use datablocks (`$name << EOD`)
  defined BEFORE the `set multiplot` command. Pattern:
  ```
  gp("$data << EOD");
  gp(dataString + "EOD");
  gp("set multiplot ...");
  gp("plot $data using 1:2 ...");
  ```

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

loki_kriging          (depends on loki_core only)                   <- COMPLETE

loki_spline           (depends on loki_core only)                   <- NEXT
```

### Architecture principle -- math primitives in loki_core
Starting from loki_kriging, all reusable math primitives go into
`loki_core/math/` (flat, no sub-directories). Module libraries (loki_kriging,
loki_spline, etc.) are thin orchestrators: pipeline, protocol, CSV, plots.
This enables future spatial/space-time extensions to reuse the same math
without cross-module dependencies.

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
|   +-- loki_kriging/                <- COMPLETE
|   +-- loki_spline/                 <- NEXT
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
|   |                          lagMatrix, embedMatrix, randomizedSvd, spline, nelderMead,
|   |                          krigingTypes, krigingVariogram, krigingBase,
|   |                          simpleKriging, ordinaryKriging, universalKriging,
|   |                          krigingFactory)
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
|   +-- loki_kriging/                <- COMPLETE
|   +-- loki_spline/                 <- NEXT
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
./scripts/loki.sh build kriging --copy-dlls
./scripts/loki.sh run kriging
./scripts/loki.sh test --rebuild
```

### CMake target name collision
Executable: `loki_kriging_app` with `OUTPUT_NAME "loki_kriging"`.
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
["kriging"]="loki_kriging_app"      <- COMPLETE
["spline"]="loki_spline_app"        <- NEXT
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
                  `EvtBlockMaximaConfig`, `EvtDeseasonalizationConfig`,
                  `KrigingConfig` with `KrigingVariogramConfig`,
                  `KrigingPredictionConfig`, `KrigingSpatialStation`
- `configLoader.hpp / .cpp` -- all parse methods including `_parseKriging`
- `timeStamp.hpp / .cpp`
- `timeSeries.hpp / .cpp`
- `gapFiller.hpp / .cpp` -- Strategy::SPLINE added
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
- `spline.hpp / .cpp`              -- CubicSpline (natural/not-a-knot/clamped)
- `nelderMead.hpp / .cpp`
- `krigingTypes.hpp`               -- VariogramPoint, VariogramFitResult,
                                      KrigingPrediction, CrossValidationResult
- `krigingVariogram.hpp / .cpp`    -- empirical variogram + theoretical models + WLS fit
- `krigingBase.hpp / .cpp`         -- KrigingBase + predictGrid + O(n^2) LOO crossValidate
- `simpleKriging.hpp / .cpp`       -- SimpleKriging
- `ordinaryKriging.hpp / .cpp`     -- OrdinaryKriging
- `universalKriging.hpp / .cpp`    -- UniversalKriging
- `krigingFactory.hpp`             -- createKriging() header-only factory

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

### loki_kriging -- COMPLETE
**Architecture:** Math primitives in `loki_core/math/` (see above).
`loki_kriging` is a thin orchestrator wrapping core math.

**Files in `loki_kriging`:**
- `krigingResult.hpp`       -- KrigingResult (top-level pipeline struct) +
                               `using` aliases for loki::math types
- `krigingAnalyzer.hpp/.cpp` -- orchestrator: gap fill, variogram, fit, predict,
                                 LOO CV, protocol, CSV
- `plotKriging.hpp/.cpp`    -- variogram, predictions+CI, crossval plots

**Key implementation notes:**
- Math namespace: `loki::math` (krigingVariogram, krigingBase etc.)
- Kriging types namespace: `loki::math` (VariogramPoint, KrigingPrediction etc.)
- `krigingResult.hpp` re-exports types via `using loki::math::XxxType`
- Variance floor = nugget (not 0) -- measurement noise always present at obs points
- LOO shortcut: O(n^2), uses cached K^{-1}; e_i = alpha_i / K^{-1}_{ii}
- Predictions plot x-axis = sample index (not MJD) to avoid gnuplot float precision issues
- Crossval top panel x-axis = sample index for the same reason
- CI band: `filledcurves using 1:2:3` (index, ci_lower, ci_upper), pink `#ffb3c1`
- Spatial/space-time mode: PLACEHOLDER, throws AlgorithmException if requested
- Forecast beyond variogram range converges to mean -- physically correct, visually
  surprising. Config docs note this explicitly.

**KrigingConfig in config.hpp:**
```cpp
struct KrigingSpatialStation { std::string file; double x; double y; };
struct KrigingVariogramConfig { model, nLagBins, maxLag, nugget, sill, range };
struct KrigingPredictionConfig { enabled, targetMjd, horizonDays, nSteps };
struct KrigingConfig {
    mode, method, gapFillStrategy, gapFillMaxLength, knownMean, trendDegree,
    crossValidate, confidenceLevel, significanceLevel,
    KrigingVariogramConfig variogram,
    KrigingPredictionConfig prediction,
    vector<KrigingSpatialStation> stations  // spatial placeholder
};
```

**PlotConfig flags added:**
```cpp
bool krigingVariogram   {true};
bool krigingPredictions {true};
bool krigingCrossval    {true};
```

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
| `loki_kriging` | DONE |

---

## Planned New Applications

### loki_spline -- NEXT

**Purpose:** B-spline fitting and approximation for time series data.
**Dependencies:** loki_core only.

**Architecture:** same pattern as loki_kriging.
- Math primitives go into `loki_core/math/` (flat).
- `loki_spline` is a thin orchestrator (analyzer + plots).

**Existing base:** `loki_core/math/spline.hpp/.cpp` -- CubicSpline (natural /
not-a-knot / clamped boundary conditions). This stays unchanged and is used
by GapFiller and SplineFilter. `loki_spline` builds ON TOP of it with
B-spline approximation.

**Functionality:**
- **B-spline approximation** (`fitMode = "approximation"`): LSQ fit with
  p control points < n data points. The primary use case -- smoothing with
  physical interpretation. Number of control points controls smoothing level.
- **Exact interpolation** (`fitMode = "exact_interpolation"`): p = n,
  passes exactly through all data points. Kept for completeness but not
  the primary use case (CubicSpline already covers this).
- **Degree:** configurable 1-5, default 3 (cubic).
- **Knot placement:** `"uniform"` or `"chord_length"`. Chord-length places
  knots denser where the signal changes rapidly.
- **Automatic control point selection:** k-fold cross-validation over a
  range [nControlMin, nControlMax]. Finds optimal smoothing level.
- **Manual control point selection:** explicit `nControlPoints` value.
- **NURBS:** PLACEHOLDER. Structure prepared, not implemented. Rational
  weights have limited utility for 1D scalar time series; NURBS is reserved
  for future 2D/3D spatial use cases (trajectory fitting, surface fitting).

**New math primitives in loki_core/math/ (to be added):**
```
bspline.hpp/.cpp       -- B-spline basis functions (de Boor algorithm),
                          knot vector construction, basis evaluation,
                          derivative evaluation. namespace loki::math.
bsplineFit.hpp/.cpp    -- LSQ B-spline approximation, chord-length knot
                          placement, k-fold CV for automatic knot selection.
                          namespace loki::math.
```

**Files in loki_spline:**
```
libs/loki_spline/
  include/loki/spline/
    splineResult.hpp        -- BSplineFitResult, SplineResult
    splineAnalyzer.hpp/.cpp -- orchestrator: run() pattern
    plotSpline.hpp/.cpp     -- overlay, residuals, basis, knots, CV curve

apps/loki_spline/
  main.cpp
  CMakeLists.txt
libs/loki_spline/CMakeLists.txt
```

**SplineConfig structure (to be added to config.hpp):**
```cpp
// NURBS placeholder -- not yet implemented
struct NurbsConfig {
    int degree = 3;
    // weights, rational basis -- FUTURE (spatial use cases)
};

struct BSplineConfig {
    int         degree         = 3;               // 1-5, default cubic
    std::string fitMode        = "approximation"; // "approximation"|"exact_interpolation"
    int         nControlPoints = 0;               // 0 = auto via CV
    int         nControlMin    = 5;               // CV search lower bound
    int         nControlMax    = 0;               // 0 = auto: n/5
    std::string knotPlacement  = "uniform";       // "uniform"|"chord_length"
    int         cvFolds        = 5;               // k-fold CV folds
};

struct SplineConfig {
    std::string   method            = "bspline";  // "bspline"|"nurbs" (placeholder)
    std::string   gapFillStrategy   = "linear";
    int           gapFillMaxLength  = 0;
    double        confidenceLevel   = 0.95;
    double        significanceLevel = 0.05;
    BSplineConfig bspline           {};
    NurbsConfig   nurbs             {};           // placeholder, not yet implemented
};
```

**PlotConfig flags to be added:**
```cpp
bool splineOverlay     {true};   // Original + B-spline fit + CI band.
bool splineResiduals   {true};   // Residuals vs sample index + RMSE lines.
bool splineBasis       {false};  // B-spline basis functions N_{i,p}(t).
bool splineKnots       {true};   // Knot positions overlaid on original series.
bool splineCv          {true};   // CV curve: RMSE vs n_control_points.
bool splineDiagnostics {false};  // 4-panel residual diagnostics via Plot::residualDiagnostics().
```

**Design decisions (agreed):**
- No standalone `interpolation` mode -- B-spline interpolation is just
  approximation with p = n (exact_interpolation covers this edge case).
- NURBS not implemented for 1D time series -- rational weights have no
  physical motivation for scalar signals. Reserved for spatial use.
- `splineDiagnostics` delegates to `Plot::residualDiagnostics()` from core
  (ACF, histogram, QQ, fitted vs residuals). Do NOT reimplement.
- `spline_basis` plot: default false -- visually cluttered for large n.
- B-spline de Boor algorithm preferred over explicit basis matrix for
  numerical stability.

**Files to request at thread start:**
- `libs/loki_core/include/loki/core/config.hpp`
- `libs/loki_core/include/loki/core/configLoader.hpp`
- `libs/loki_core/src/core/configLoader.cpp` (last 200 lines -- _parseKriging as pattern)
- `libs/loki_core/include/loki/math/spline.hpp` (existing CubicSpline API)
- `libs/loki_core/include/loki/math/krigingBase.hpp` (pattern for math primitive)
- `apps/loki_kriging/main.cpp` (pipeline pattern)
- `libs/loki_kriging/src/krigingAnalyzer.cpp` (orchestrator pattern)

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
Rovnaky princip plati pre `namespace loki::math { }` (krigingVariogram.cpp atd.).

### MedianYearSeries API (critical -- caused build error in loki_evt)
Constructor: `MedianYearSeries(const TimeSeries& series, Config cfg = Config{})`
Config field: `cfg.minYears` (NOT `minYearsPerSlot`, NOT separate `build()` call).
The series is passed directly to the constructor -- no two-step construction.

### nelderMead -- must be in loki_core CMakeLists
`nelderMead.cpp` must be listed in `add_library(loki_core STATIC ...)`.
Forgetting this causes `undefined reference` at link time.
Same applies to all new kriging .cpp files added to loki_core/math/.

### evtAnalyzer.hpp -- required includes
Must include `<functional>` (for `std::function`) and
`<loki/math/nelderMead.hpp>` (for `_brentRoot` and profile likelihood).

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
- `plot '-'` inside `set multiplot` NOT supported -- use datablocks (`$name << EOD`)
  defined before `set multiplot`. This was the root cause of crossval plot warnings
  in loki_kriging.

### loki_kriging -- gnuplot precision issue with short time series
MJD values for short series (< 1 day) have insufficient float precision for
gnuplot x-axis display. Solution: use sequential sample index (0..n-1) as
x-axis in predictions and crossval plots. MJD preserved in CSV output.

### loki_evt -- domain notes
- POT/GPD preferred over GEV/block_maxima for sensor data (train velocity etc.)
- SIL 4: return_periods = [1e8] with time_unit = "hours". Profile likelihood CI mandatory.
- xi > 0.5 with small n: often PWM fallback or too-high threshold.
- Auto threshold (elbow on mean excess): may be suboptimal for short series (n < 500).
- Profile likelihood Brent search: falls back to point estimate with LOKI_WARNING
  if logLik curve is too flat.
- Deseasonalization with median_year: return levels are in residual units.

### loki_kriging -- domain notes
- Temporal Kriging range = maximum useful forecast horizon. Beyond range,
  prediction converges to mean (OK) or drift (UK). Physically correct but
  visually surprising -- documented in CONFIG_REFERENCE.md ch.19.
- For sensor data at 1 Hz: use Gaussian variogram model (smooth signal).
- For GNSS IWV/topo delay: use Exponential model (rapid correlation decay).
- For GNSS coordinates with velocity: use Universal Kriging, trend_degree=1.
- Nugget = measurement noise floor. High nugget-to-sill ratio (> 0.5) means
  mostly unstructured noise; Kriging still optimal but heavily smoothed.
- CV meanSSE ideal = 1. > 1 = variance underestimated; < 1 = overestimated.

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
- Kriging: variogram fitting is the most sensitive step. Bad variogram = bad CI.
- Kriging LOO O(n^2) shortcut: e_i = alpha_i / K^{-1}_{ii}, alpha = K^{-1}*z.
  Full inverse cached in fit() -- avoids n re-factorisations.
- NURBS vs B-spline: NURBS allows exact conic sections via rational weights.
  For 1D scalar time series, rational weights have no physical motivation.
  B-spline approximation is sufficient and simpler.
- B-spline approximation: number of control points controls smoothing.
  Too few = underfitting (systematic residual patterns). Too many = overfitting.
  CV selects optimal number automatically.

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
| done | `loki_kriging` | new app | COMPLETE |
| done | Kriging math to loki_core/math/ | refactoring | COMPLETE |
| next | `loki_spline` | new app | NEXT |

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
- Kriging math files also in `loki_core/math/`: `krigingTypes.hpp`,
  `krigingVariogram.hpp/.cpp`, `krigingBase.hpp/.cpp`, `simpleKriging.hpp/.cpp`,
  `ordinaryKriging.hpp/.cpp`, `universalKriging.hpp/.cpp`, `krigingFactory.hpp`.
- `wCorrelation.hpp`, `sampling.hpp`, `bootstrap.hpp`, `permutation.hpp`
  are in `loki_core/stats/`.
- `svd.cpp` does NOT exist -- `SvdDecomposition` is header-only in `svd.hpp`.
- `krigingFactory.hpp` is header-only (inline factory function).
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
- loki_kriging: temporal mode only in v1. Spatial/space-time = placeholder.
  Forecast beyond variogram range converges to series mean (correct behaviour).
  Sample index on x-axis in plots (not MJD) for short series precision.
- Block bootstrap required for autocorrelated series; iid bootstrap invalid.
- `SplineFilterConfig` definovana TYLKO v `config.hpp`, nie v `splineFilter.hpp`.
- `bootstrap.cpp`, `permutation.cpp`: funkcie musia byt v `namespace loki::stats { }`
  bloku, nie len cez `using namespace`.
- Kriging math `.cpp` files: funkcie musia byt v `namespace loki::math { }` bloku.
- SNHT v rekurzivnom mode: min_segment_points >= 3-4 roky (4383-5844 pri 6h).
- PELT mbic je najkonzervativnejsi penalty typ -- odporucany default pre klimaticke data.
- BOCPD vyzaduje kalibраciu prior_beta blizko skutocnemu sigma^2 serie.
- `tests/demo/` gitignore: `png/`, `protocol/`, `input/` adresare ignorovat.
- CRITICAL: snhtDetector.hpp a snhtDetector.cpp existuju v repozitari ako PRAZDNE
  subory -- ide o systemovu vec. NIKDY sa nepytaj ci su prilozene alebo nie.
  Jednoducho ich naplnit podla dizajnu. TOTO PRAVIDLO NEMAZES.