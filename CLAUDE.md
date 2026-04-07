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
loki_homogeneity      (depends on loki_core + loki_outlier)

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
                        loki_kalman)                                <- PLANNED

loki_evt              (depends on loki_core only)                   <- PLANNED

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
|   +-- loki_simulate/               <- PLANNED
|   +-- loki_evt/                    <- PLANNED
|   +-- loki_kriging/                <- PLANNED
|   +-- loki_spline/                 <- PLANNED
+-- libs/
|   +-- loki_core/
|   |   +-- include/loki/
|   |       +-- core/         (exceptions, version, config, configLoader, logger, nanPolicy)
|   |       +-- timeseries/   (timeSeries, timeStamp, gapFiller, deseasonalizer,
|   |       |                  medianYearSeries)
|   |       +-- stats/        (descriptive, filter, distributions, hypothesis, metrics,
|   |       |                  wCorrelation, sampling*, bootstrap*, permutation*)
|   |       |                  -- * PLANNED additions
|   |       +-- io/           (loader, dataManager, gnuplot, plot)
|   |       +-- math/         (lsqResult, lsq, designMatrix, hatMatrix, svd, lm,
|   |                          lagMatrix, embedMatrix, randomizedSvd, spline*)
|   |                          -- * PLANNED addition
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
|   +-- loki_simulate/               <- PLANNED
|   +-- loki_evt/                    <- PLANNED
|   +-- loki_kriging/                <- PLANNED
|   +-- loki_spline/                 <- PLANNED
+-- tests/
+-- config/
|   +-- outlier.json
|   +-- regression.json
|   +-- stationarity.json
|   +-- arima.json
|   +-- ssa.json
|   +-- decomposition.json
|   +-- spectral.json
|   +-- kalman.json
|   +-- qc.json
|   +-- clustering.json
|   +-- simulate.json                <- PLANNED
|   +-- evt.json                     <- PLANNED
|   +-- kriging.json                 <- PLANNED
|   +-- spline.json                  <- PLANNED
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
./scripts/loki.sh build clustering --copy-dlls
./scripts/loki.sh run clustering
./scripts/loki.sh test --rebuild
```

### CMake target name collision
Executable: `loki_qc_app` with `OUTPUT_NAME "loki_qc"`.
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
["simulate"]="loki_simulate_app"    <- PLANNED
["evt"]="loki_evt_app"              <- PLANNED
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
                  `ClusteringOutlierConfig`
- `configLoader.hpp / .cpp` -- `_parseKalman()`, `_parseQc()`, `_parseClustering()` added
- `timeStamp.hpp / .cpp`
- `timeSeries.hpp / .cpp`
- `gapFiller.hpp / .cpp`
- `deseasonalizer.hpp / .cpp`
- `medianYearSeries.hpp / .cpp`
- `descriptive.hpp / .cpp` -- includes `hurstExponent()` and `summarize()`
- `filter.hpp / .cpp` (stats -- movingAverage, EMA, WMA)
- `distributions.hpp / .cpp`
- `hypothesis.hpp / .cpp`
- `metrics.hpp / .cpp`
- `wCorrelation.hpp / .cpp`
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

### loki_outlier -- complete (O1-O4)
### loki_homogeneity -- complete
### loki_stationarity -- complete
### loki_filter -- complete (SplineFilter NOT YET implemented)
### loki_regression -- complete (R1-R8)
### loki_arima -- complete
### loki_ssa -- complete (S1-S5)
### loki_decomposition -- complete
### loki_spectral -- complete
### loki_kalman -- complete
### loki_qc -- complete
### loki_clustering -- complete

---

## Planned Extensions to loki_core

### stats/sampling.hpp / .cpp
Random number generators for statistical distributions via `std::mt19937`.
Distributions: Normal, Student-t, Chi2, F, Uniform, Exponential, Poisson,
Binomial, Beta, Gamma, Laplace.
Foundation for bootstrap, Monte Carlo simulation, and loki_simulate.
No external dependencies beyond `<random>`.

### stats/bootstrap.hpp / .cpp
- Percentile bootstrap -- simplest, for large samples.
- BCa (bias-corrected accelerated) -- standard for small samples.
- **Block bootstrap** -- for dependent (autocorrelated) time series.
  Block length selection: fixed, optimal (Politis & White 2004), or manual.
  Block bootstrap is REQUIRED for climatological/GNSS data. iid bootstrap
  is invalid for autocorrelated series.
Use cases: CI for Hurst exponent, SNHT critical values, regression coefficients.

### stats/permutation.hpp / .cpp
Non-parametric permutation tests as alternative to parametric hypothesis tests.
Complements `hypothesis.hpp`. Useful when normality assumption is violated.

### math/spline.hpp / .cpp
Cubic spline interpolation with boundary condition options:
- Natural (second derivative = 0 at endpoints)
- Clamped (specified first derivative at endpoints)
- Not-a-knot (third derivative continuous at second and second-to-last knots)

This is **shared infrastructure** used by:
- `GapFiller` (new SPLINE strategy) -- smoother gap filling than linear
- `SplineFilter` in loki_filter (currently NOT YET implemented)
- `loki_spline` app (advanced fitting -- uses this as foundation)

---

## Planned Extensions to Existing Modules

### GapFiller -- new SPLINE strategy
Requires `loki_core/math/spline.hpp` first.
New `Strategy::SPLINE` using cubic spline interpolation.
Particularly useful for sub-daily sensor data where linear interpolation
introduces artificial kinks at gap boundaries.

### SplineFilter in loki_filter
Currently placeholder. Implementable once `math/spline.hpp` exists.
Smoothing spline with roughness penalty -- trade-off between data fidelity
and smoothness controlled by lambda parameter.

### loki_homogeneity -- new change point detectors
The existing `changePointDetector.cpp` uses **Yao & Davis (1986)**
likelihood-ratio test with Gumbel asymptotic approximation and
Bartlett-window long-run variance correction. This is NOT Alexandersson SNHT.

New detectors added alongside existing one, selectable via `method` config key:

**SNHT** (`snhtDetector.hpp / .cpp`) -- Alexandersson (1986):
Standard Normal Homogeneity Test. T-statistic compares the ratio of segment
means to overall mean. Widely used in climatological homogenisation literature.
Config: `"method": "snht"`

**PELT** (`peltDetector.hpp / .cpp`) -- Killick et al. (2012):
Pruned Exact Linear Time algorithm. Finds the globally optimal set of change
points (minimises penalised cost function) in O(n) amortised time.
Handles multiple change points efficiently without binary segmentation bias.
Config: `"method": "pelt"`

**BOCPD** (`bocpdDetector.hpp / .cpp`) -- Adams & MacKay (2007):
Bayesian Online Change Point Detection. Computes posterior probability of a
change point at each time step. Suitable for streaming/real-time sensor data.
Outputs a probability series rather than a single detected index.
Config: `"method": "bocpd"`

The `MultiChangePointDetector` dispatcher updated to route to all four methods.

---

## Planned New Applications

### loki_simulate
Monte Carlo simulation and synthetic time series generation.

Pipeline:
- Generate synthetic AR(p)/MA(q)/ARIMA(p,d,q) series from fitted parameters
  (reuses loki_arima coefficient output).
- Generate Kalman state-space model realisations from estimated Q and R
  (reuses loki_kalman EM output).
- Monte Carlo integration for probability estimation.
- Parametric bootstrap: refit model on each simulated realisation, build
  distribution of parameter estimates and CI.
- Validation use case: generate series with known change point position and
  magnitude, verify loki_homogeneity detection rate.

Depends on: loki_core (sampling), loki_arima, loki_kalman.

### loki_evt
Extreme Value Theory for tail probability modelling.

Methods:
- **GEV** (Generalized Extreme Value): Gumbel (xi=0), Frechet (xi>0),
  Weibull (xi<0) as special cases. Block maxima method.
- **GPD** (Generalized Pareto Distribution): Peaks over Threshold (POT) method.
  Preferred over GEV for small datasets as it uses all threshold exceedances.
- Return level estimation: T-year return level with confidence intervals
  via profile likelihood or bootstrap.
- Threshold selection for POT: mean excess plot, stability plots.
- Goodness-of-fit: Anderson-Darling test adapted for GEV/GPD.
- Probability-probability and quantile-quantile plots for EVT distributions.

Primary use case: **SIL 4 safety analysis** (IEC 61508) where error intensity
must be < 1e-8 per hour. EVT provides statistically principled extrapolation
to very small probabilities from limited observed failure data without
assuming a specific parametric distribution for the bulk of the data.

Depends on: loki_core only.

### loki_kriging
Gaussian process-based spatial and temporal interpolation.

Components:
- **Empirical variogram**: compute semi-variance as function of lag distance.
  Supports isotropic and directional variograms.
- **Variogram model fitting**: spherical, exponential, Gaussian, nugget+sill.
  Weighted least squares fit. Visual inspection essential -- automated fitting
  is a starting point only.
- **Simple Kriging**: known stationary mean. Best linear unbiased predictor.
- **Ordinary Kriging**: unknown mean, estimated from data. Most commonly used.
  Unbiasedness constraint via Lagrange multiplier.
- **Universal Kriging**: non-stationary mean modelled as linear combination
  of known functions (trend surface). Generalises ordinary Kriging.
- **Cross-validation**: leave-one-out error for variogram model selection.
  Standardised residuals should be approximately N(0,1) for correct model.

Use cases: spatial interpolation of GNSS station networks (dN, dE, dU fields),
temporal interpolation of irregularly sampled climate records, filling spatial
gaps in measurement networks.

Depends on: loki_core only (Eigen3 for kriging system solving).

### loki_spline
Advanced spline fitting, B-spline, and NURBS.

Uses `loki_core/math/spline.hpp` as foundation (shared with GapFiller).
This app handles the analytical/fitting use cases beyond basic interpolation.

Components:
- **B-spline**: basis functions with configurable degree (1=linear, 2=quadratic,
  3=cubic, ...) and knot vector. Knot selection: uniform, chord-length,
  centripetal parametrisation.
- **NURBS** (Non-Uniform Rational B-Splines): weighted control points.
  Exact representation of conic sections (circles, ellipses, parabolas).
  Useful for smooth reconstruction of vehicle trajectories and sensor paths
  with geometric constraints.
- **Smoothing splines**: minimise sum of squared residuals + lambda * integral
  of squared second derivative. Lambda controls smoothness vs data fidelity.
  Generalised cross-validation (GCV) for automatic lambda selection.
- **Knot insertion/removal**: de Boor algorithm for evaluation.
  Adaptive knot placement at high-curvature regions.
- Output: fitted curve sampled at user-defined resolution, CSV export,
  control points, knot vector, residuals.

Depends on: loki_core only.

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
- Always set explicit `set xrange [tMin:tMax]` before plot commands
- For horizontal reference lines use `set arrow N from x1,y to x2,y nohead`
- PlotClustering: x-axis uses relative time in seconds when series span < 1 day

### loki_spectral -- plot flags location
Spectral plot flags are at top-level of `"plots"` JSON object (not in `"enabled"`).
Same convention adopted for kalman, qc, and clustering plot flags.

### DataManager loading pattern (all apps)
```cpp
std::vector<loki::LoadResult> loadResults;
loki::DataManager dm(cfg);
loadResults = dm.load();
for (const auto& r : loadResults) {
    const std::string datasetName = r.filePath.stem().string();
    for (const loki::TimeSeries& ts : r.series) {
        analyzer.run(ts, datasetName);
    }
}
```

### loki_clustering -- key learnings
- k-means edge points (first rows with NaN derivative/slope) handled by
  `_handleEdgePoints()` -- assigns nearest centroid using only non-NaN features.
  Never pass NaN rows directly to k-means fit (causes segfault via empty cluster cascade).
- `abs_derivative` and `derivative` are linearly dependent -- do not combine.
- For vehicle phase segmentation, `slope` (OLS over `slope_window` samples)
  strongly preferred over `derivative` -- averages out sensor noise.
- `slope_window` in samples (at 1 Hz: window=15 covers 15 seconds).
- Label assignment is by ascending first-feature (value) centroid.
- DBSCAN auto eps via k-NN elbow: use `eps = 0.0` for automatic estimation.
- Silhouette global mean line clipped to [-1, 1] for gnuplot xrange.

### loki_homogeneity -- detector identity
The existing `changePointDetector.cpp` uses **Yao & Davis (1986)** -- NOT SNHT.
SNHT (Alexandersson 1986) will be a separate `snhtDetector.hpp / .cpp`.

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
- ACF of Kalman innovations lag-1 negative for atmospheric data -- expected.
- EVT/SIL 4: GPD/POT preferred over GEV/block-maxima for small datasets.
  Block maxima wastes data; POT uses all exceedances above threshold.
  For error intensity < 1e-8/h, threshold selection and GPD shape parameter
  estimation are the critical steps.
- Bootstrap: standard iid bootstrap INVALID for autocorrelated time series.
  Block bootstrap required. Block length >= effective decorrelation length
  (lag where ACF < 1.96/sqrt(n)).
- Kriging: variogram fitting is the most sensitive step. Visual inspection
  of empirical variogram essential before automated fitting. Nugget effect
  = measurement noise + micro-scale variability below sampling resolution.
- NURBS vs B-spline: NURBS allows exact conic sections via rational weights.
  For general curve fitting without geometric constraints, B-spline sufficient.
- Spline in GapFiller: cubic spline preferred over linear for smooth signals
  (IWV, GNSS coordinates). Linear interpolation introduces kinks at gap edges.

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
| 1 | `loki_core/math/spline` | core extension | PLANNED |
| 1 | `loki_core/stats/sampling` | core extension | PLANNED |
| 1 | `loki_core/stats/bootstrap` | core extension | PLANNED |
| 1 | `loki_core/stats/permutation` | core extension | PLANNED |
| 2 | `GapFiller` SPLINE strategy | core extension | PLANNED (needs spline first) |
| 2 | `SplineFilter` in loki_filter | module extension | PLANNED (needs spline first) |
| 2 | SNHT in loki_homogeneity | module extension | PLANNED |
| 2 | PELT in loki_homogeneity | module extension | PLANNED |
| 2 | BOCPD in loki_homogeneity | module extension | PLANNED |
| 3 | `loki_simulate` | new app | PLANNED |
| 3 | `loki_evt` | new app | PLANNED |
| 4 | `loki_kriging` | new app | PLANNED |
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
  `randomizedSvd.hpp` are in `loki_core/math/`.
- `wCorrelation.hpp` is in `loki_core/stats/`.
- `svd.cpp` does NOT exist -- `SvdDecomposition` is header-only in `svd.hpp`.
- `window` parameters in filters and SSA are in samples, not time units.
- `HatMatrixDetector` does NOT inherit from `OutlierDetector`.
- `NonlinearRegressor` does NOT inherit from `Regressor`.
- `SplineFilter` is NOT YET IMPLEMENTED -- requires `math/spline.hpp` first.
- gnuplot.cpp: `-persist` flag is REMOVED. Do not restore it.
- TimeStamp: `.mjd()` | `.utcString()` | `.gpsTotalSeconds()`
- loki_qc seasonal section: auto-disable when median step > 3600s.
- loki_qc flags: bitfield OR-combined in CSV output.
- loki_clustering: NaN edge points -> `_handleEdgePoints()`, never raw to fit.
- loki_clustering: `slope` preferred over `derivative` for noisy sensor data.
- loki_homogeneity existing detector: Yao & Davis (1986), NOT SNHT.
- loki_spline app: B-spline + NURBS. Cubic spline interpolation in loki_core/math.
- loki_evt: SIL 4 use case (< 1e-8/h). GPD/POT preferred for small datasets.
- loki_kriging: Simple / Ordinary / Universal variants. Variogram fitting critical.
- Block bootstrap required for autocorrelated series; iid bootstrap invalid.