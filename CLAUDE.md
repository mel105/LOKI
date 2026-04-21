# CLAUDE.md -- Project Instructions for LOKI

This file contains instructions for Claude when working on the LOKI project.
It should be placed in the root of the repository and kept up to date.

---

## Project Overview

LOKI is a modular C++20 scientific data analysis framework.

The name originates from the Norse god of mischief -- the project began as a
toolkit for detecting false (inhomogeneous) events in climatological time series.
It has since grown into a general-purpose framework covering time series analysis,
spatial interpolation, multivariate statistics, and geodetic computation.

*LOKI: originally a detector of false events in observational data; now a
general-purpose toolkit for quantitative analysis of scientific datasets.*

Current domains:
- Time series analysis (1D): complete, see module inventory below.
- Spatial analysis (2D): in development -- loki_spatial is next.
- Multivariate analysis: planned -- loki_multivariate.
- Geodetic computations: planned -- loki_geodesy.

---

## Communication Rules

### No questionnaire widgets
- Claude must NEVER use the `ask_user_input` widget or any questionnaire/poll tool.
- All clarifying questions must be asked as plain text in the conversation.
- This rule is absolute and applies to every thread.

---

## Publication Workflow

### Two-document structure
LOKI documentation is published as two separate PDF documents written in LaTeX:
- **Part A (Theory)** -- Slovak. Mathematical foundations and algorithmic description
  of all modules. Target audience: researchers and developers.
- **Part B (Manual)** -- English. User guide, configuration reference, worked examples.
  Target audience: users of LOKI applications.

### LaTeX conventions
- One chapter per `.tex` file. Main document uses `\include{kapitola}`.
- Chapter structure starts at `\chapter{}` level.
- Equations numbered per chapter: `(3.1)`, `(3.2)`, ...
- Mathematical level: university technical textbook (definitions yes,
  formal proofs no). Rigorous but readable.
- Examples in Part A only where strictly necessary. Part B carries
  all practical examples.
- Bibliography: BibTeX, style to be confirmed per template.

### Part A -- chapter scope (Part I: Time Series)
Each chapter covers one module or one coherent group of methods.
Target length: ~10 pages per chapter. Not a hard limit -- let content
dictate length naturally.

### Thread startup for publication work
Start thread with: "Working on LOKI -- publikacia, see CLAUDE.md"
Attach: CLAUDE.md + LaTeX template + chapter(s) to write.
Claude reads the template first, confirms it is suitable, then begins writing.

### Key references (Part I, Chapter 3 -- Homogeneity)
- Csörgő, M. & Horváth, L. -- theoretical foundation (book)
- Jarušková, D. (1996) -- methodological framework
- Elias, M. & Jarušková, D. -- application/extension (joint paper)
Full BibTeX entries to be supplied at thread start.


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
- Free functions declared in `namespace loki::stats` (or any sub-namespace) MUST be
  defined inside an explicit `namespace loki::stats { }` block in the `.cpp` file.
- `using namespace loki::stats` is NOT sufficient -- functions end up in global namespace.
- Same principle applies to `namespace loki::math { }`, `namespace loki::spline { }`,
  `namespace loki::spatial { }`, and any future sub-namespaces.

---

## Plot Output Naming Convention

All plot output files follow this naming convention:
```
[program]_[dataset]_[parameter]_[plottype].[format]
```

- **program** : `core` | `homogeneity` | `outlier` | `filter` | `regression` |
                `stationarity` | `arima` | `ssa` | `decomposition` | `spectral` |
                `kalman` | `qc` | `clustering` | `simulate` | `evt` | `kriging` |
                `spline` | `spatial`
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
- **CRITICAL -- gnuplot pipe and tmp files**: Use gnuplot inline data (`plot '-'`) for
  single-dataset plots. Only use tmp files for formats that cannot be sent inline
  (e.g. `nonuniform matrix` for spectrograms) -- call `ofs.flush(); ofs.close()` explicitly.
- **gnuplot `-persist` flag is REMOVED** from `gnuplot.cpp`. Do not add it back.
- **CRITICAL -- multiplot and inline data**: `plot '-'` inside `set multiplot` is NOT
  supported. Always use datablocks (`$name << EOD`) defined before `set multiplot`.

---

## Library Architecture

### Project identity and domain scope
LOKI is no longer limited to time series. The framework covers:
- 1D time series analysis (complete)
- 2D spatial analysis (loki_spatial -- in development)
- Multivariate analysis (loki_multivariate -- planned)
- Geodetic computations (GeoKit -- separate project, uses loki_core)

### Architecture principle -- math primitives in loki_core
All reusable math primitives go into `loki_core/math/` (flat, no sub-directories).
Module libraries are thin orchestrators: pipeline, protocol, CSV, plots.
This enables all future modules (spatial, multivariate) to reuse the same math
without cross-module dependencies.

### Dependency graph
```
loki_core
    ^
    |
loki_outlier          (depends on loki_core only)
    ^
    |
loki_homogeneity      (depends on loki_core + loki_outlier)        <- COMPLETE

loki_filter           (depends on loki_core only)                   <- COMPLETE

loki_regression       (depends on loki_core only)                   <- COMPLETE

loki_stationarity     (depends on loki_core only)                   <- COMPLETE

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

loki_spline           (depends on loki_core only)                   <- COMPLETE

loki_spatial          (depends on loki_core only)                   <- NEXT

loki_multivariate     (depends on loki_core only)                   <- PLANNED

loki_wavelet          (depends on loki_core only)                   <- PLANNED

loki_geodesy          (depends on loki_core only)                   <- PLANNED
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
|   +-- loki_kriging/                <- COMPLETE
|   +-- loki_spline/                 <- COMPLETE
|   +-- loki_spatial/                <- NEXT
|   +-- loki_geodesy/                <- PLANNED
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
|   |                          krigingFactory, bspline, bsplineFit)
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
|   +-- loki_spline/                 <- COMPLETE
|   +-- loki_spatial/                <- NEXT
|   +-- loki_geodesy/                <- PLANNED
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
./scripts/loki.sh build spline --copy-dlls
./scripts/loki.sh run spline
./scripts/loki.sh test --rebuild
```

### CMake target name collision
Executable: `loki_spline_app` with `OUTPUT_NAME "loki_spline"`.
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
["spline"]="loki_spline_app"        <- COMPLETE
["spatial"]="loki_spatial_app"      <- NEXT
["geodesy"]="loki_geodesy_app"      <- PLANNED
```

---

## Implemented Modules

### loki_core -- complete
- `exceptions.hpp` -- full hierarchy
- `version.hpp` -- `0.1.0`
- `nanPolicy.hpp` -- `NanPolicy { THROW, SKIP, PROPAGATE }`
- `logger.hpp / .cpp`
- `config.hpp` -- all config structs including `PlotConfig`, `NonlinearConfig`,
                  `OutlierConfig`, `StationarityConfig`, `ArimaConfig`, `SsaConfig`,
                  `DecompositionConfig`, `SpectralConfig`, `KalmanConfig`, `QcConfig`,
                  `ClusteringConfig`, `SplineFilterConfig`, `EvtConfig`,
                  `KrigingConfig`, `SplineConfig` (with `BSplineConfig`, `NurbsConfig`)
- `configLoader.hpp / .cpp` -- all parse methods including `_parseKriging`, `_parseSpline`
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
- `bspline.hpp / .cpp`             -- B-spline basis (de Boor), knot vectors,
                                      normaliseParams, evalBSpline
- `bsplineFit.hpp / .cpp`          -- BSplineFitResult, CvPoint, fitBSpline,
                                      crossValidateBSpline, selectOptimalNCtrl

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
### loki_kriging -- complete (temporal only; spatial/space_time = placeholder)

### loki_spline -- COMPLETE
**Architecture:** Math primitives in `loki_core/math/` (bspline, bsplineFit).
`loki_spline` is a thin orchestrator.

**Files in `loki_spline`:**
- `splineResult.hpp`         -- SplineResult + using aliases for loki::math types
- `splineAnalyzer.hpp/.cpp`  -- orchestrator: gap fill, knot placement detection,
                                 CV or manual nCtrl, fit, CI, protocol, CSV
- `plotSpline.hpp/.cpp`      -- overlay, residuals, basis, knots, CV curve plots

**Key implementation notes:**
- B-spline degree 1-5, default cubic (degree=3).
- fitMode: "approximation" (LSQ, nCtrl < nObs) or "exact_interpolation" (nCtrl == nObs,
  guarded by exactInterpolationMaxN=2000).
- knotPlacement: "uniform" or "chord_length" (Hartley-Judd averaging).
  Auto-detection: if CV of timestep diffs > 0.1, switches to chord_length.
- CV: k-fold (default 5), sweeps nCtrl in [nControlMin, min(n/5, 200)].
  One-SE elbow rule for optimal nCtrl selection.
- CI band: residual-based, fitted +/- z * residualStd (homoscedastic).
- NURBS: placeholder only -- throws AlgorithmException if requested.
- namespace: loki::spline

**PlotConfig flags added:**
```cpp
bool splineOverlay     {true};
bool splineResiduals   {true};
bool splineBasis       {false};
bool splineKnots       {true};
bool splineCv          {true};
bool splineDiagnostics {false};
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
| `loki_spline` | N/A (spline is the output, not preprocessing) |

---

## Planned Modules

### loki_spatial -- NEXT

**Purpose:** 2D spatial interpolation and analysis. Input: irregular scatter
points (x, y, variable). Output: interpolated regular grid, variogram analysis,
spatial statistics, visualisation (contour/heatmap).

**This is not a time series module.** LOKI has grown beyond its original 1D
scope -- loki_spatial is the first purely spatial module.

**Architecture:** same pattern as loki_kriging.
- Math primitives in `loki_core/math/` (spatial variogram, grid builder,
  interpolation kernels).
- `loki_spatial` is a thin orchestrator (analyzer + plots).

**Interpolation methods planned:**
- Spatial Kriging (Ordinary/Universal) -- reuse KrigingBase, change lag to
  Euclidean distance sqrt((xi-xj)^2 + (yi-yj)^2). This is the primary
  upgrade of the existing kriging placeholder.
- IDW (Inverse Distance Weighting) -- simple baseline, fast.
- Natural Neighbor (Sibson) -- Voronoi/Delaunay-based, good for irregular networks.
- RBF (Radial Basis Functions):
  - Multiquadric: phi(r) = sqrt(r^2 + epsilon^2)
  - Gaussian:     phi(r) = exp(-epsilon^2 * r^2)
  - Thin plate spline: phi(r) = r^2 * log(r)  -- natural 2D extension of
    cubic spline, minimises surface bending energy, recommended for scatter data.
- Tensor product B-spline surface -- reuses bspline.hpp from loki_core.
  Works well for quasi-regular input grids. For truly irregular scatter,
  thin plate spline is preferred.
- Bilinear interpolation -- only for regular input grids (e.g. NWM output).
  Implemented in spatialInterp alongside IDW and polynomial surface.
- NURBS surface -- PLACEHOLDER, same pattern as loki_spline: config parsed,
  AlgorithmException thrown if requested. Reserved for future implementation.

**New math primitives in loki_core/math/:**
```
spatialTypes.hpp          -- SpatialPoint {x,y,z}, SpatialGrid, GridExtent
spatialInterp.hpp/.cpp    -- IDW, bilinear (regular grid), polynomial surface (LSQ)
rbf.hpp/.cpp              -- RBF kernels: multiquadric, Gaussian, thin plate spline
naturalNeighbor.hpp/.cpp  -- Sibson interpolation (Delaunay triangulation)
```
Kriging math stays in existing files -- only lag computation changes
(Euclidean distance instead of |t_i - t_j|).

**Additional spatial analysis features:**
- 2D variogram (isotropic and anisotropic)
- LOO cross-validation on the spatial network
- Spatial autocorrelation: Moran's I, Geary's C
- Voronoi diagram / Thiessen polygons
- 2D Kernel Density Estimation (KDE)
- Spatial outlier detection

**Visualisation (gnuplot):**
- Contour map (interpolated grid)
- Heatmap (pm3d)
- Scatter map (input points coloured by value)
- Variogram plot (2D, optionally directional)
- Cross-validation scatter (predicted vs observed)

**Input format:** CSV with columns (x, y, variable). Multiple variable columns
supported. Coordinate system is user-defined (degrees, metres, etc.) --
loki_spatial does not perform coordinate transformations.

**Config structure (to be designed in implementation thread):**
```
SpatialConfig {
    method          -- "kriging" | "idw" | "rbf" | "natural_neighbor" | "bilinear"
    gridResolution  -- output grid spacing (same units as input coords)
    gridExtent      -- auto or explicit [xmin, xmax, ymin, ymax]
    kriging { ... } -- reuse KrigingVariogramConfig
    idw { power }
    rbf { kernel, epsilon }
    crossValidate
    confidenceLevel
}
```

**Files to request at thread start:**
- `libs/loki_core/include/loki/core/config.hpp`
- `libs/loki_core/include/loki/core/configLoader.hpp`
- `libs/loki_core/src/core/configLoader.cpp` (last 200 lines -- _parseSpline as pattern)
- `libs/loki_core/include/loki/math/krigingBase.hpp`
- `libs/loki_core/include/loki/math/krigingVariogram.hpp`
- `libs/loki_core/include/loki/math/krigingTypes.hpp`
- `libs/loki_core/include/loki/math/bspline.hpp`
- `libs/loki_core/include/loki/math/bsplineFit.hpp`
- `libs/loki_core/include/loki/math/spline.hpp`
- `apps/loki_kriging/main.cpp` (pipeline pattern)
- `libs/loki_kriging/src/krigingAnalyzer.cpp` (orchestrator pattern)
- `libs/loki_kriging/include/loki/kriging/krigingResult.hpp`

---

### loki_multivariate -- PLANNED

**Purpose:** Analysis of multiple simultaneous time series or multivariate
observation vectors.

**Planned functionality:**
- PCA (Principal Component Analysis) on multivariate time series
- ICA (Independent Component Analysis)
- MSSA (Multivariate SSA) -- extension of existing loki_ssa
- VAR model (Vector Autoregression)
- Cross-correlation matrix and lag analysis
- Granger causality testing
- Hungarian algorithm (optimal assignment between station sets / segments)
- Mahalanobis distance (extension of loki_outlier)
- Covariance/correlation matrix visualisation

**Input:** multiple time series loaded simultaneously (multi-column CSV or
multiple files merged). Uses existing DataManager merge functionality.

**Notes:**
- Hungarian algorithm is relevant for matching clustered phases across
  multiple sensor channels, or for station-to-station assignment problems.
- Granger causality connects to the multi-station GNSS use case (does IWV
  at station A predict IWV at station B?).
- PCA/ICA can serve as a pre-processing step before loki_spatial (reduce
  multivariate field to dominant spatial modes).

---

### loki_geodesy -- PLANNED

**Purpose:** Geodetic coordinate transformations with full covariance
propagation. Part of LOKI -- same module pattern as all other modules
(JSON config, CSV input/output, protocol, gnuplot visualisation).

Rationale for inclusion in LOKI: geodetic transformations with covariance
propagation are quantitative scientific analysis -- the same domain as
loki_spatial and loki_multivariate. The "not a time series" argument applies
equally to loki_spatial, which is already in LOKI. Consistent decision:
all quantitative scientific analysis tools belong in LOKI.

**Planned functionality:**
- Coordinate transformations:
  - ECEF (X, Y, Z) <-> geodetic (phi, lambda, h)
  - ECEF <-> local topocentric (ENU / NEU)
  - Helmert 7-parameter transformation (datum shift)
- Full covariance matrix propagation through each transformation
  (law of error propagation: Cx_out = J * Cx_in * J^T)
- Visualisation of error ellipses (2D) and ellipsoids (3D) via gnuplot
- Transformation parameter estimation from control points (LSQ)
- Protocol output: transformation parameters, residuals, quality metrics

**Input:** CSV with coordinate columns + optional variance/covariance columns.
**Config:** transformation type, source/target system, ellipsoid parameters.

**Math primitives needed in loki_core/math/:**
- `helmert.hpp/.cpp` -- 7-parameter Helmert transformation + LSQ estimation
- `ellipsoid.hpp`    -- ellipsoid parameters (GRS80, WGS84, Bessel, etc.)
- `covariance.hpp`   -- covariance propagation J * C * J^T helper

---

### loki_wavelet -- PLANNED

**Purpose:** Wavelet transform and wavelet-based signal processing as a
complement to loki_spectral (FFT/Lomb-Scargle) and loki_decomposition (STL).

**Motivation:** FFT gives global frequency content; wavelets give localised
time-frequency content. For non-stationary signals (train sensors, GNSS with
earthquake transients, climate with changing seasonal amplitude) wavelets are
more informative than global spectral analysis.

**Planned functionality:**
- DWT (Discrete Wavelet Transform) -- Mallat algorithm, O(n).
  Families: Haar, Daubechies (db2-db10), Symlets.
- CWT (Continuous Wavelet Transform) -- Morlet, Mexican hat, Paul wavelets.
- Wavelet power spectrum + significance test (Torrence & Compo 1998 --
  standard reference in climatology).
- Wavelet denoising: soft/hard thresholding (VisuShrink / SureShrink).
- Multi-resolution analysis (MRA): decomposition and reconstruction.
- Optional: wavelet-ARIMA hybrid forecast (decompose -> model each level
  with ARIMA -> reconstruct).

**Implementation note:** no external wavelet library needed -- DWT via
filter bank (Mallat) is straightforward with Eigen. CWT via FFT convolution.

---

### loki_realtime -- FUTURE (long-term)

**Purpose:** Online/streaming change point and anomaly detection for
real-time sensor data. Complement to the offline batch processing in
existing modules.

**Note:** LOKI is primarily a post-processing toolkit. loki_realtime is a
long-term goal, not near-term.

---

### loki_ml -- FUTURE (long-term)

**Purpose:** Machine learning based anomaly detection and pattern recognition.

**Planned:** Isolation Forest, Local Outlier Factor (LOF), autoencoder-based
anomaly detection. Separate from statistical methods in loki_outlier.

**Note:** May require an external ML dependency (e.g. a header-only neural
network library). Design deferred until concrete use case is identified.

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
`using namespace loki::stats` in `.cpp` is NOT sufficient for defining free functions.
Functions end up in global namespace and linker cannot find them.
Solution: wrap all definitions in explicit `namespace loki::stats { }` block.
Same applies to `namespace loki::math { }`, `namespace loki::spline { }`, etc.

### MedianYearSeries API
Constructor: `MedianYearSeries(const TimeSeries& series, Config cfg = Config{})`
Config field: `cfg.minYears` (NOT `minYearsPerSlot`).

### nelderMead -- must be in loki_core CMakeLists
`nelderMead.cpp` must be listed in `add_library(loki_core STATIC ...)`.
Same applies to all new math .cpp files: `bspline.cpp`, `bsplineFit.cpp`,
and future spatial math files.

### `TimeSeries` API
- No `observations()` method -- use direct indexing: `ts[i].value`, `ts[i].time`
- `TimeSeries::append(const TimeStamp& time, double value, uint8_t flag = 0)`
- `::TimeStamp` is NOT in `namespace loki` -- always qualify as `::TimeStamp`
- MJD getter: `ts[i].time.mjd()`
- UTC string: `ts[i].time.utcString()`
- GPS total seconds: `ts[i].time.gpsTotalSeconds()`

### Gnuplot on Windows -- CRITICAL
- API: `gp("command")` -- no `<<` operator
- Font: `'Sans,12'` (not `'Helvetica,12'`)
- Terminal: `noenhanced`
- `-persist` flag REMOVED from gnuplot.cpp -- do not add it back
- Use `plot '-'` (inline data) instead of tmp files wherever possible
- `plot '-'` inside `set multiplot` NOT supported -- use datablocks (`$name << EOD`)

### loki_spline -- implementation notes
- `_isNonUniform()` uses `gpsTotalSeconds()` for timestep CV detection.
  For MJD data, use `ts[i].time.mjd()` instead -- adapt when implementing
  spatial module which may use MJD or GPS timestamps.
- CV in `bsplineFit.cpp`: nCtrlMax is clamped to `nObs / folds` to ensure
  enough training data per fold. For very small series this may reduce the
  effective search range -- logged as WARNING.
- `bsplineBasisRow()`: t=1.0 edge case handled explicitly (last basis function = 1).

---

## Statistical / Domain Key Learnings

- SSA window L should be a multiple of the dominant period for clean separation.
- Classical decomposition: residual variance > 40% is normal for sub-daily
  climatological data.
- STL outer loop (n_outer >= 1) needed when seasonal amplitude changes over time.
- Period in decomposition is always in samples -- 1461 for 6h data (1 year).
- Draconitic period for GNSS: 351.4 days (not 365.25).
- Lomb-Scargle preferred over FFT for GNSS and sensor data with frequent gaps.
- Kalman local_level K = Q/(Q+R); GPS IWV: Q~1e-4, R~4e-6 gives K~0.97.
- EVT/SIL 4: GPD/POT preferred over GEV/block-maxima for small datasets.
  Profile likelihood CI mandatory for SIL 4 return periods (1e8 hours).
- Bootstrap: iid bootstrap INVALID for autocorrelated time series.
  Block bootstrap required.
- Kriging: variogram fitting is the most sensitive step.
  LOO O(n^2) shortcut: e_i = alpha_i / K^{-1}_{ii}.
- B-spline approximation: nCtrl controls smoothing. CV (one-SE elbow) selects
  optimal nCtrl automatically. Chord-length knots preferred for non-uniform sampling.
- Spatial Kriging: lag = Euclidean distance, not time difference.
  Variogram fitting same procedure as temporal, but isotropy assumption
  should be verified (directional variograms for anisotropic fields).

---

## Module Roadmap (full picture)

| Priority | Module / Extension | Type | Status |
|---|---|---|---|
| done | all time series modules | apps | COMPLETE |
| done | `loki_core/math/bspline` | core extension | COMPLETE |
| done | `loki_core/math/bsplineFit` | core extension | COMPLETE |
| done | `loki_spline` | new app | COMPLETE |
| next | `loki_spatial` | new app | NEXT |
| planned | `loki_multivariate` | new app | PLANNED |
| planned | `loki_wavelet` | new app | PLANNED |
| planned | `loki_geodesy` | new app | PLANNED |
| future | `loki_realtime` | new app | FUTURE |
| future | `loki_ml` | new app | FUTURE |
| separate | `GeoKit` | separate project | PLANNED |
| in progress | Publication Part A (Theory, SK) | docs | IN PROGRESS |
| in progress | Publication Part B (Manual, EN) | docs | IN PROGRESS |

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
  `randomizedSvd.hpp`, `spline.hpp`, `nelderMead.hpp`, `bspline.hpp`,
  `bsplineFit.hpp` are in `loki_core/math/`.
- Kriging math files also in `loki_core/math/`.
- `svd.cpp` does NOT exist -- `SvdDecomposition` is header-only in `svd.hpp`.
- `krigingFactory.hpp` is header-only (inline factory function).
- gnuplot.cpp: `-persist` flag is REMOVED. Do not restore it.
- TimeStamp: `.mjd()` | `.utcString()` | `.gpsTotalSeconds()`
- SNHT v rekurzivnom mode: min_segment_points >= 3-4 roky.
- PELT mbic je najkonzervativnejsi penalty typ -- odporucany default.
- BOCPD vyzaduje kalibраciu prior_beta blizko skutocnemu sigma^2 serie.
- loki_spline: NURBS = placeholder, hodi AlgorithmException ak je requested.
  Spatial/trajectory NURBS rezervovane pre loki_spatial / loki_multivariate.
- loki_geodesy: sucast LOKI (nie samostatny projekt). Rovnaky argument ako
  loki_spatial -- kvantitativna analyza vedeckych dat patri do LOKI.
- CRITICAL: snhtDetector.hpp a snhtDetector.cpp existuju v repozitari ako PRAZDNE
  subory -- ide o systemovu vec. NIKDY sa nepytaj ci su prilozene alebo nie.
  Jednoducho ich naplnit podla dizajnu. TOTO PRAVIDLO NEMAZES.
- Publikacia Part A: slovensky, LaTeX, teoria, ~10 stran/kapitola.
- Publikacia Part B: anglicky, LaTeX, manual a prakticke priklady.
- Obsah Part I (Time Series): kapitoly 1-18 podla hrubej osnovy v docs/.
- Kapitola 3 obsahuje: nas algoritmus (t-statistika, Csorgo-Horvath,
  Jaruskova 1996, Elias-Jaruskova), SNHT, PELT, BOCPD.