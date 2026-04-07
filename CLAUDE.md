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
                `kalman` | `qc`
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

loki_arima            (depends on loki_core + loki_stationarity)  <- COMPLETE

loki_ssa              (depends on loki_core only)                  <- COMPLETE

loki_decomposition    (depends on loki_core only)                  <- COMPLETE

loki_spectral         (depends on loki_core only)                  <- COMPLETE

loki_kalman           (depends on loki_core only)                  <- COMPLETE

loki_qc               (depends on loki_core + loki_outlier)        <- NEXT
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
|   +-- loki_qc/                    <- NEXT
|       +-- CMakeLists.txt
|       +-- main.cpp
+-- libs/
|   +-- loki_core/
|   |   +-- include/loki/
|   |       +-- core/         (exceptions, version, config, configLoader, logger, nanPolicy)
|   |       +-- timeseries/   (timeSeries, timeStamp, gapFiller, deseasonalizer,
|   |       |                  medianYearSeries)
|   |       +-- stats/        (descriptive, filter, distributions, hypothesis, metrics,
|   |       |                  wCorrelation)
|   |       +-- io/           (loader, dataManager, gnuplot, plot)
|   |       +-- math/         (lsqResult, lsq, designMatrix, hatMatrix, svd, lm,
|   |                          lagMatrix, embedMatrix, randomizedSvd)
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
|   +-- loki_qc/                    <- NEXT
|       +-- CMakeLists.txt
|       +-- include/loki/qc/
|       |   +-- qcResult.hpp
|       |   +-- qcAnalyzer.hpp/.cpp
|       |   +-- plotQc.hpp/.cpp
|       +-- src/
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
|   +-- qc.json                     <- NEXT
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
./scripts/loki.sh build kalman --copy-dlls
./scripts/loki.sh run kalman
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
                  `SpectralPeakConfig`
- **PENDING**: `KalmanConfig` (with `KalmanNoiseConfig`, `KalmanForecastConfig`)
               and kalman `PlotConfig` fields must still be added to `config.hpp`
               and `AppConfig`. See "loki_kalman -- COMPLETE" section below.
- `configLoader.hpp / .cpp` -- `_parseKalman()` added, kalman plot flags added
- `timeStamp.hpp / .cpp`
- `timeSeries.hpp / .cpp`
- `gapFiller.hpp / .cpp`
- `deseasonalizer.hpp / .cpp`
- `medianYearSeries.hpp / .cpp`
- `descriptive.hpp / .cpp`
- `filter.hpp / .cpp` (stats)
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
### loki_filter -- complete
### loki_regression -- complete (R1-R8)
### loki_arima -- complete
### loki_ssa -- complete (S1-S5)
### loki_decomposition -- complete
### loki_spectral -- complete

### loki_kalman -- COMPLETE

Validated on 6h climatological data (n=36524, 25 years).

```
libs/loki_kalman/include/loki/kalman/
    kalmanResult.hpp         -- KalmanResult struct (times, original, filteredState,
                                filteredStd, smoothedState, smoothedStd, predictedState,
                                innovations, innovationStd, gains, forecastTimes,
                                forecastState, forecastStd, logLikelihood, estimatedQ,
                                estimatedR, emIterations, emConverged, modelName,
                                noiseMethod, smoother)
    kalmanModel.hpp          -- KalmanModel struct (F, H, Q, R, x0, P0, name)
    kalmanModelBuilder.hpp/.cpp -- factory: localLevel(), localTrend(), constantVelocity()
    kalmanFilter.hpp/.cpp    -- forward pass, Joseph covariance update, logLikelihood(),
                                withNoise() for EM
    rtsSmoother.hpp/.cpp     -- RTS backward pass, SmootherStep struct,
                                Cholesky-based gain (LLT with pseudoinverse fallback)
    emEstimator.hpp/.cpp     -- EM for Q/R (scalar parameterisation Q=q*I, R=r),
                                lag-1 cross-covariance, EmResult struct
    kalmanAnalyzer.hpp/.cpp  -- orchestrator: gap fill, dt auto-detect, noise est,
                                model build, forward pass, RTS, forecast, protocol
    plotKalman.hpp/.cpp      -- 6 plot types, inline gnuplot data, steady-state sigma
                                via median of second-half innovationStd
```

Key facts about `loki_kalman`:
- dt auto-detected from median of consecutive MJD differences (seconds)
- Gap fill: "auto" uses MEDIAN_YEAR for 6h climate (dt < 0.3 days, span >= 10y),
  else LINEAR; remaining NaN -> predict-only filter step (no update)
- Three models: local_level (1-state), local_trend / constant_velocity (2-state, same F/H)
- EM: scalar Q and R (Q = q*I_n), M-step with lag-1 cross-covariance,
  convergence = relative logL change < emTol; for n=36524 each iter ~1.5s
- Joseph-form covariance update for numerical stability
- RTS smoother: Cholesky inversion of PPred with pseudoinverse fallback
- Forecast: repeated F*x predict steps, uncertainty grows as sqrt(P[t|t] + k*Q)
- Steady-state Kalman gain K = Q/(Q+R) for local_level
- Validated result: EM gives Q=1.28e-4, R=4e-6, K=0.97 for GPS IWV
  (near-interpolating, physically correct -- GPS IWV precision ~2mm, real variability large)
  Manual Q=1e-4, R=4e-6 gives K=0.40 -- better for smoothing applications

**PENDING config.hpp changes** (must be added before loki_qc thread):
```cpp
// Add before AppConfig -- outside due to GCC 13 pattern:
struct KalmanNoiseConfig {
    std::string estimation    {"manual"};
    double      Q             {1.0};
    double      R             {1.0};
    double      smoothingFactor{10.0};
    double      QInit         {1.0};
    double      RInit         {1.0};
    int         emMaxIter     {100};
    double      emTol         {1.0e-6};
};
struct KalmanForecastConfig {
    int    steps          {0};
    double confidenceLevel{0.95};
};
struct KalmanConfig {
    std::string          gapFillStrategy  {"auto"};
    int                  gapFillMaxLength {0};
    std::string          model            {"local_level"};
    KalmanNoiseConfig    noise            {};
    std::string          smoother         {"rts"};
    KalmanForecastConfig forecast         {};
    double               significanceLevel{0.05};
};

// Add to PlotConfig (after spectralSpectrogram):
bool kalmanOverlay     {true};
bool kalmanInnovations {true};
bool kalmanGain        {false};
bool kalmanUncertainty {false};
bool kalmanForecast    {true};
bool kalmanDiagnostics {false};

// Add to AppConfig (after SpectralConfig spectral):
KalmanConfig  kalman;
```

loki.sh registration:
```
["kalman"]="loki_kalman_app"
```

---

## NEXT THREAD: loki_qc

### What loki_qc does
Diagnostic and reporting module -- the entry gate of the LOKI pipeline.
Runs before any other analysis module. Does NOT modify data. Produces:
- A detailed quality control protocol (text)
- A flagging CSV (one row per epoch, bitfield flag column)
- Coverage and outlier plots

### Design philosophy
- Pure diagnostics: no data correction, no modelling
- Wraps existing loki_core infrastructure (GapFiller, IqrDetector, MadDetector,
  ZScoreDetector, MedianYearSeries, descriptive stats, hypothesis tests)
- Each analysis section is independently configurable via JSON `enabled` flag
- Adapts to data type: seasonal analysis auto-disabled for sub-hourly data
- Provides explicit recommendations in protocol for downstream modules

### QC sections (each independently enabled/disabled)

**Section 1: Temporal coverage**
- Start/end in MJD, UTC string, GPS total seconds (all three formats)
- Expected epoch count vs actual -> completeness %
- Gap statistics: count, min/max/median length, longest gap location
- Sampling uniformity: % of steps exceeding 1.1x median step
- Detected sampling rate changes (e.g. transition from 6h to 3h)

**Section 2: Descriptive statistics**
- N valid, N NaN, N flagged outlier
- Mean, median, std, IQR, skewness, kurtosis, min, max, percentiles
- Hurst exponent (configurable, slow for large n)
- Jarque-Bera normality test

**Section 3: Outlier detection**
- IQR, MAD, ZScore detectors (HatMatrix excluded -- that is loki_outlier standalone)
- Count and % flagged per method
- List of flagged epochs with value and flag reason
- Configurable per-method enable flag and threshold

**Section 4: Sampling rate analysis**
- Median step, min/max step
- Non-uniform step count and %
- This drives the FFT vs Lomb-Scargle recommendation

**Section 5: Seasonal consistency** (auto-disabled if step > 1h)
- Years with < threshold % coverage per month
- Minimum years per MedianYearSeries slot
- Whether MEDIAN_YEAR gap filling is feasible

### Flagging CSV output
Bitfield flag per epoch:
```
0  = valid
1  = gap (missing epoch)
2  = outlier_iqr
4  = outlier_mad
8  = outlier_zscore
```
Flags are OR-combined (e.g. 6 = outlier_iqr + outlier_mad).
File: `OUTPUT/CSV/qc_[dataset]_[component]_flags.csv`
Format: `mjd ; utc ; value ; flag`

### Recommendations in protocol
The protocol ends with an explicit recommendation block:
- Gap filling: recommended strategy based on step and span
- Spectral method: FFT or Lomb-Scargle based on uniformity %
- MedianYearSeries: feasible yes/no based on span and coverage
- Outlier cleaning: recommended yes/no and method
- Homogeneity: consider if series is long and continuous

### Plots
```
qc_[dataset]_[component]_coverage.[fmt]   -- time axis: valid/gap/outlier color bands
qc_[dataset]_[component]_histogram.[fmt]  -- value histogram with normal fit
qc_[dataset]_[component]_acf.[fmt]        -- ACF (optional)
```

### QcConfig design (to add to config.hpp)

```cpp
struct QcOutlierConfig {
    bool   iqrEnabled       {true};
    double iqrMultiplier    {1.5};
    bool   madEnabled       {true};
    double madMultiplier    {3.0};
    bool   zscoreEnabled    {true};
    double zscoreThreshold  {3.0};
};

struct QcSeasonalConfig {
    bool   enabled              {true};   // auto-disabled for step > 1h
    int    minYearsPerSlot      {5};
    double minMonthCoverage     {0.5};    // fraction [0,1]
};

// QcOutlierConfig and QcSeasonalConfig defined outside QcConfig (GCC 13 pattern)

struct QcConfig {
    std::string      gapFillStrategy  {"none"};  // QC does not fill -- detect only
    bool             temporalEnabled  {true};
    bool             statsEnabled     {true};
    bool             outlierEnabled   {true};
    bool             samplingEnabled  {true};
    bool             seasonalEnabled  {true};
    bool             hurstEnabled     {true};
    QcOutlierConfig  outlier          {};
    QcSeasonalConfig seasonal         {};
    double           significanceLevel{0.05};
    double           uniformityThreshold{0.05}; // fraction of non-uniform steps -> Lomb-Scargle
    double           minSpanYears     {10.0};   // min span for MEDIAN_YEAR recommendation
};
```

### PlotConfig additions for qc
```cpp
bool qcCoverage   {true};   // time-axis coverage plot (valid/gap/outlier bands)
bool qcHistogram  {true};   // value histogram with normal fit
bool qcAcf        {false};  // ACF of valid observations
```

### Files to request at loki_qc thread start
```
libs/loki_core/include/loki/core/config.hpp        -- current state (post-kalman additions)
libs/loki_core/src/core/configLoader.cpp           -- last 80 lines (_parseKalman pattern)
libs/loki_core/include/loki/core/configLoader.hpp  -- private section
libs/loki_kalman/CMakeLists.txt                    -- lib CMake pattern
apps/loki_kalman/CMakeLists.txt                    -- app CMake pattern
apps/loki_kalman/main.cpp                          -- DataManager + analyzer pattern
libs/loki_core/include/loki/timeseries/gapFiller.hpp
libs/loki_core/include/loki/stats/descriptive.hpp
libs/loki_core/include/loki/stats/hypothesis.hpp
libs/loki_outlier/include/loki/outlier/iqrDetector.hpp    -- outlier detector API
libs/loki_outlier/include/loki/outlier/madDetector.hpp
libs/loki_outlier/include/loki/outlier/zscoreDetector.hpp
```

**IMPORTANT**: Before starting loki_qc thread, first apply the pending
config.hpp changes for loki_kalman (KalmanConfig, KalmanForecastConfig,
KalmanNoiseConfig, kalman PlotConfig fields, KalmanConfig in AppConfig).

### Domain notes for loki_qc
- Sampling rate auto-detection: median of consecutive MJD diffs * 86400 = seconds
- "Sub-hourly" threshold for disabling seasonal analysis: median step < 3600s
- Completeness = actual_epochs / expected_epochs where
  expected = round(total_span / median_step) + 1
- For ms data: gap threshold factor should be generous (1.5x median step)
  to avoid false gap detection from jitter
- Coverage plot: one colored bar per day (aggregated) for long series,
  per epoch for short series (< 1000 epochs)
- loki_qc depends on loki_outlier for IQR/MAD/ZScore detectors
  (these are already in loki_outlier, not duplicated)

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
- Always set explicit `set xrange [tMin:tMax]` before plot commands to prevent
  gnuplot from inferring range only from the first dataset (filledcurves issue)
- For horizontal reference lines use `set arrow N from x1,y to x2,y nohead` --
  NOT inline datasets (they only appear at data points, not continuously)

### loki_spectral -- plot flags location
Spectral plot flags are at top-level of `"plots"` JSON object (not in `"enabled"`).
Same convention adopted for kalman and qc plot flags.

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

### loki_kalman -- key learnings
- EM with n=36524 and emTol=1e-6 does not converge in 100 iterations (logL
  change ~1e-5 at iter 100). Q and R stabilise after ~20-30 iterations.
  Use emTol=1e-4 or emTol=1e-3 for practical convergence.
- For GPS IWV: EM result Q=1.28e-4, R=4e-6 is physically correct (K~0.97,
  near-interpolating). If smoothing desired, use manual Q=1e-4, R=4e-4 (K~0.20)
  or manual Q=1e-6, R=1e-3 (K~0.001) for trend extraction.
- ACF of innovations shows lag-1 negative correlation (~-0.2) for atmospheric
  data -- this is a data property (short-term atmospheric persistence), not
  a filter defect. Requires AR-augmented model (future feature) to eliminate.
- RTS smoother std is always <= filter std; gap between them indicates how
  much future information helps -- for GPS IWV the gap is small (P is stable).
- plotKalman.cpp: set xrange explicitly; use gnuplot arrows for sigma bands.

### loki_qc -- design decisions (pre-agreed)
- HatMatrix detector NOT included -- that is standalone in loki_outlier
- Seasonal analysis auto-disabled when median step > 3600s (sub-hourly data)
- Each section independently enabled in JSON
- Three time formats in protocol: MJD, UTC, GPS total seconds
- Bitfield flags in CSV (OR-combined): 0=valid, 1=gap, 2=iqr, 4=mad, 8=zscore
- loki_qc depends on loki_outlier (for IQR/MAD/ZScore); do not duplicate detectors
- Coverage plot: aggregated per day for long series

---

## Config Structs (current state)

### KalmanConfig -- PENDING addition to config.hpp
See "PENDING config.hpp changes" in loki_kalman section above.

### QcConfig -- TO ADD in loki_qc thread
See "NEXT THREAD: loki_qc" section above for full design.

### SpectralConfig -- complete (see previous CLAUDE.md)

---

## Statistical / Domain Key Learnings

- SSA window L should be a multiple of the dominant period for clean separation
- Classical decomposition: residual variance > 40% is normal for sub-daily
  climatological data (synoptic variability dominates)
- STL outer loop (n_outer >= 1) needed when seasonal amplitude changes over time
- Period in decomposition is always in samples -- 1461 for 6h data (1 year)
- Draconitic period for GNSS: 351.4 days (not 365.25)
- For 6h climatological data: L=365 (quarter-year) is a good SSA starting point
- Lomb-Scargle preferred over FFT for GNSS and sensor data with frequent gaps
- Kalman local_level Q/R ratio determines smoothness: small Q/R = smooth,
  large Q/R = tracks measurements; K = Q/(Q+R) for local_level steady-state
- GPS IWV precision ~2mm -> R = 4e-6 m^2; synoptic variability -> Q ~ 1e-4 m^2
- EM on GPS IWV finds near-interpolating filter (K~0.97) -- physically correct
- ACF of Kalman innovations lag-1 negative for atmospheric data -- expected

---

## Module Roadmap (priority order)

| Priority | Module | Status |
|---|---|---|
| 1 | `loki_arima` | COMPLETE |
| 2 | `loki_ssa` | COMPLETE |
| 3 | `loki_decomposition` | COMPLETE |
| 4 | `loki_spectral` | COMPLETE |
| 5 | `loki_kalman` | COMPLETE |
| 6 | `loki_qc` | **NEXT** |
| 7 | `loki_clustering` | planned |

---

## Approach & Patterns

- **Thread-based workflow:** Each conversation handles a specific milestone.
  Always begins with "Working on LOKI -- see CLAUDE.md" + attached CLAUDE.md +
  relevant source files listed in "Files to request" section.
- **Design-first:** Discuss architecture and approve signatures before any implementation.
- **Iterative debugging:** Build errors shared in chunks; systematic resolution.
- **Documentation artifacts:** Each thread concludes with updated CLAUDE.md and
  CONFIG_REFERENCE.md section if needed.
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
- Program prefix: `core` | `homogeneity` | `outlier` | `filter` | `regression` |
                  `stationarity` | `arima` | `ssa` | `decomposition` | `spectral` |
                  `kalman` | `qc`
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
- `SplineFilter` is planned but NOT yet implemented.
- Classical decomposition and SSA are separate applications -- do not merge.
- gnuplot.cpp: `-persist` flag is REMOVED. Do not restore it.
- loki.sh -- register new apps in APP_EXE map:
  `["kalman"]="loki_kalman_app"`
  `["qc"]="loki_qc_app"`
- TimeStamp MJD getter: `.mjd()` -- UTC string: `.utcString()` -- GPS: `.gpsTotalSeconds()`
- loki_qc depends on loki_outlier (IQR/MAD/ZScore detectors already implemented there)
- loki_qc seasonal section: auto-disable when median step > 3600s
- loki_qc flags are bitfield OR-combined in CSV output