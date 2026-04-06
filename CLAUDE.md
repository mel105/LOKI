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
                `stationarity` | `arima` | `ssa` | `decomposition` | `spectral` | `kalman`
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
  cause race conditions with tmp files in subsequent runs. This change does not affect
  any existing modules (all LOKI plotters generate files, never interactive windows).

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

loki_kalman           (depends on loki_core only)                  <- NEXT
```

### Rules
- Every module is a separate CMake library with its own `include/` and `src/` tree.
- Include paths follow `#include <loki/<module>/<class>.hpp>`.
- No circular dependencies between modules.
- `loki_core` must not depend on any other loki module.
- Modules communicate via files and `loki_core` types -- never import each other
  except where explicitly noted in the dependency graph.

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
|   +-- loki_kalman/                <- NEXT
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
|   +-- loki_kalman/                <- NEXT
|       +-- CMakeLists.txt
|       +-- include/loki/kalman/
|       |   +-- kalmanModel.hpp
|       |   +-- kalmanModelBuilder.hpp
|       |   +-- kalmanFilter.hpp/.cpp
|       |   +-- rtsSmoother.hpp/.cpp
|       |   +-- emEstimator.hpp/.cpp
|       |   +-- kalmanResult.hpp
|       |   +-- kalmanAnalyzer.hpp/.cpp
|       |   +-- plotKalman.hpp/.cpp
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
|   +-- kalman.json                 <- NEXT
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

**No new external dependencies for `loki_kalman`.** Kalman filter matrices
use `Eigen::MatrixXd` / `Eigen::VectorXd`. All algorithms are self-contained.

---

## Build Instructions

### Platform: Windows MINGW64 shell, UCRT64 toolchain (GCC 13.2)

```bash
./scripts/loki.sh build kalman --copy-dlls
./scripts/loki.sh run kalman
./scripts/loki.sh test --rebuild
```

### CMake target name collision
Executable: `loki_kalman_app` with `OUTPUT_NAME "loki_kalman"`.
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
- `configLoader.hpp / .cpp` -- `_parseSpectral()` added
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
- `gnuplot.hpp / .cpp` -- `-persist` flag REMOVED (see plot rules above)
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

### loki_spectral -- COMPLETE

All files implemented and validated on 6h climatological data (n=36524, 25 years).

```
libs/loki_spectral/include/loki/spectral/
    spectralResult.hpp       -- SpectralPeak, SpectralResult, SpectrogramResult
                                SpectralResult now includes amplitudes and phases vectors
    fftAnalyzer.hpp/.cpp     -- Cooley-Tukey radix-2 FFT, Welch PSD, 5 window functions
                                FftFrame internal struct carries freq+power+amplitude+phase
    lombScargle.hpp/.cpp     -- Lomb-Scargle periodogram, Baluev (2008) FAP
                                amplitudes = sqrt(power); phases empty for L-S
    peakDetector.hpp/.cpp    -- local maxima, noise floor (median), FAP filter, top-N
    spectrogramAnalyzer.hpp/.cpp -- STFT sliding window, focus period zoom
    spectralAnalyzer.hpp/.cpp    -- orchestrator: gap fill, method select, peaks, plots, protocol
    plotSpectral.hpp/.cpp        -- PSD, amplitude, phase, spectrogram plots
```

Key facts about `loki_spectral`:
- Method auto-selection: < 5% steps exceed 1.1x median -> FFT, else Lomb-Scargle
- FFT: self-contained Cooley-Tukey radix-2, zero-padding to next pow2
- Amplitude spectrum: `|X[k]| / window_sum` (physical units, same as input signal)
- Phase spectrum: `atan2(Im, Re)`, only plotted where amplitude > 1% of max
- Welch: overlapping segments, averaged PSD; reduces variance, loses frequency resolution
- Lomb-Scargle FAP: Baluev (2008) analytic approximation
- Spectrogram: STFT with `nonuniform matrix` gnuplot format; focus period zoom
- **Plot flags** in `plots` section (NOT inside `"enabled"` sub-object):
  `spectral_psd`, `spectral_peaks`, `spectral_amplitude`, `spectral_phase`,
  `spectral_spectrogram`
- **Inline gnuplot data** used for PSD/amplitude/phase (no tmp files for these)
- **Tmp file** used only for spectrogram (nonuniform matrix format)

PlotConfig spectral fields (config.hpp):
```cpp
bool spectralPsd         {true};
bool spectralPeaks       {true};
bool spectralAmplitude   {false};
bool spectralPhase       {false};
bool spectralSpectrogram {false};
```

loki.sh registration:
```
["spectral"]="loki_spectral_app"
```

---

## NEXT THREAD: loki_kalman

### What loki_kalman does
Kalman filter pipeline for state estimation, smoothing, prediction, and
uncertainty quantification. Handles single-parameter time series from
climatological, GNSS, and train sensor data at any sampling rate.

Three filter variants: standard linear Kalman filter, RTS backward smoother
(Rauch-Tung-Striebel), and EM-based noise covariance estimation.
Extended KF and UKF are planned for a later thread.

### Primary use cases
- **TROPO** (IWV, tropospheric delay): slow-varying signal + fast noise.
  Local level or local trend model. 6h resolution. Prediction: hours to days.
- **TRAIN** (velocity from pulse encoder or Doppler radar via RS485):
  state = [velocity, acceleration]. ms resolution.
  Prediction: 0.1 -- 1 second.

### State space models (built-in)

| Model | State vector | F matrix | H matrix | Best for |
|---|---|---|---|---|
| `local_level` | `[x]` | `[1]` | `[1]` | IWV, troposfera, slowly varying signals |
| `local_trend` | `[x, x_dot]` | `[[1,dt],[0,1]]` | `[1,0]` | GNSS coordinates with drift |
| `constant_velocity` | `[v, a]` | `[[1,dt],[0,1]]` | `[1,0]` | Train velocity (measured directly) |

Note: `local_trend` and `constant_velocity` have identical F/H structure but
different physical interpretation and default Q/R values.

### Noise estimation methods

| Method | Description |
|---|---|
| `manual` | User specifies Q and R directly in JSON |
| `heuristic` | R = var(measurements), Q = R / smoothing_factor |
| `em` | EM algorithm: iterates Kalman forward + RTS backward to maximize log-likelihood |

EM M-step update equations:
```
Q_new = (1/T) * sum( P[t|T] + (x[t|T] - F*x[t-1|T]) * (...)^T )
R_new = (1/T) * sum( (y[t] - H*x[t|T])^2 + H*P[t|T]*H^T )
```

### Planned module files
```
libs/loki_kalman/include/loki/kalman/
    kalmanModel.hpp          -- KalmanModel struct: F, H, Q, R, x0, P0 (all Eigen matrices)
    kalmanModelBuilder.hpp   -- factory methods: localLevel(), localTrend(),
                                constantVelocity(dt); builds KalmanModel from scalars
    kalmanFilter.hpp/.cpp    -- forward pass: predict() + update() + run()
    rtsSmoother.hpp/.cpp     -- RTS backward smoother: smooth(KalmanResult)
    emEstimator.hpp/.cpp     -- EM for Q/R: iterate forward+backward until convergence
    kalmanResult.hpp         -- KalmanResult struct (see below)
    kalmanAnalyzer.hpp/.cpp  -- orchestrator: gap fill, build model, run, smooth, plot, protocol
    plotKalman.hpp/.cpp      -- all plots (inline gnuplot data, no tmp files)
```

### KalmanResult struct
```cpp
struct KalmanResult {
    std::vector<double> times;             // MJD
    std::vector<double> original;          // raw measurements (with NaN for gaps)
    std::vector<double> filteredState;     // x_hat[t|t]  (first component of state)
    std::vector<double> filteredStd;       // sqrt(P[t|t][0,0])
    std::vector<double> smoothedState;     // x_hat[t|T]  (RTS, empty if smoother off)
    std::vector<double> smoothedStd;       // sqrt(P[t|T][0,0])
    std::vector<double> predictedState;    // x_hat[t|t-1]
    std::vector<double> innovations;       // y[t] - H * x_hat[t|t-1]
    std::vector<double> innovationStd;     // sqrt(S[t])
    std::vector<double> gains;             // K[t][0] (first row of Kalman gain)
    std::vector<double> forecastState;     // predict_steps steps beyond last obs
    std::vector<double> forecastStd;       // growing uncertainty
    double              logLikelihood;     // sum of log N(innovation; 0, S)
    double              estimatedQ;        // from EM or manual
    double              estimatedR;        // from EM or manual
    std::string         modelName;         // "local_level" | "local_trend" | "constant_velocity"
    std::string         noiseMethod;       // "manual" | "heuristic" | "em"
};
```

### KalmanConfig design (to add to config.hpp)
```cpp
struct KalmanNoiseConfig {
    std::string estimation {"manual"};  // "manual" | "heuristic" | "em"
    double      Q          {1.0};       // process noise variance (manual)
    double      R          {1.0};       // measurement noise variance (manual)
    double      smoothingFactor {10.0}; // R/Q ratio for heuristic: Q = R/factor
    double      QInit      {1.0};       // initial Q for EM
    double      RInit      {1.0};       // initial R for EM
    int         emMaxIter  {100};       // EM max iterations
    double      emTol      {1.0e-6};    // EM convergence tolerance (rel. log-likelihood)
};

struct KalmanForecastConfig {
    int    steps     {0};    // prediction steps beyond last observation (0 = no forecast)
    double confidenceLevel {0.95};
};

// KalmanNoiseConfig and KalmanForecastConfig defined outside KalmanConfig (GCC 13 pattern)

struct KalmanConfig {
    std::string         gapFillStrategy  {"linear"};
    int                 gapFillMaxLength {0};
    std::string         model            {"local_level"};  // "local_level"|"local_trend"|"constant_velocity"
    KalmanNoiseConfig   noise            {};
    std::string         smoother         {"rts"};   // "none" | "rts"
    KalmanForecastConfig forecast        {};
    double              significanceLevel{0.05};
};
```

### PlotConfig additions for kalman (to add to config.hpp)
```cpp
bool kalmanOverlay      {true};   // original + filtered + smoothed + confidence band
bool kalmanInnovations  {true};   // innovations (residuals) vs time
bool kalmanGain         {false};  // Kalman gain K[t] vs time
bool kalmanUncertainty  {false};  // sqrt(P[t]) -- filter uncertainty vs time
bool kalmanForecast     {true};   // forecast with prediction intervals (if steps > 0)
bool kalmanDiagnostics  {false};  // ACF of innovations + histogram (white noise check)
```

### Example JSON config (kalman.json)
```json
{
    "workspace": "C:/LOKI_DATA",
    "input": {
        "file": "CLIM_DATA_EX1.txt",
        "time_format": "utc",
        "time_columns": [0, 1],
        "delimiter": " ",
        "comment_char": "%",
        "columns": [3]
    },
    "output": { "log_level": "info" },
    "plots": {
        "output_format": "png",
        "kalman_overlay":     true,
        "kalman_innovations": true,
        "kalman_gain":        false,
        "kalman_uncertainty": false,
        "kalman_forecast":    true,
        "kalman_diagnostics": false
    },
    "kalman": {
        "gap_fill_strategy": "linear",
        "gap_fill_max_length": 0,
        "model": "local_level",
        "noise": {
            "estimation": "em",
            "Q_init": 1.0,
            "R_init": 1.0,
            "em_max_iter": 100,
            "em_tol": 1e-6
        },
        "smoother": "rts",
        "forecast": {
            "steps": 24,
            "confidence_level": 0.95
        },
        "significance_level": 0.05
    }
}
```

### Files to request at loki_kalman thread start
```
libs/loki_core/include/loki/core/config.hpp          -- current state (post-spectral)
libs/loki_core/src/core/configLoader.cpp             -- last 60 lines (_parseSpectral pattern)
libs/loki_spectral/CMakeLists.txt                    -- lib CMake pattern
apps/loki_spectral/CMakeLists.txt                    -- app CMake pattern
apps/loki_spectral/main.cpp                          -- DataManager + analyzer pattern
```

### Domain notes for loki_kalman
- Train velocity measured directly (not integrated from position)
  -> state = `[velocity, acceleration]`, H = `[1, 0]`
- Sources: pulse encoder (impulzy) or Doppler radar (RS485 line)
- For ms data: `dt` in seconds (e.g. 0.001 for 1 kHz)
- For 6h climatological data: `dt = 0.25` (days) or convert to seconds
- Prediction horizon: train = 0.1--1 s; troposfera = hours to days
- EM convergence criterion: relative change in log-likelihood < `em_tol`
- Extended KF and UKF: planned for a later thread, not in this implementation

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
Do NOT use `BDCSVD` in any `.cpp` compiled into a static library.
For `loki_kalman`: use `Eigen::LLT` or `Eigen::LDLT` for covariance updates
(positive definite matrices) -- these are safe in static libs.

### `TimeSeries` API
- No `observations()` method -- use direct indexing: `ts[i].value`, `ts[i].time`
- `TimeSeries::append(const TimeStamp& time, double value, uint8_t flag = 0)`
- `::TimeStamp` is NOT in `namespace loki` -- always qualify as `::TimeStamp`
- MJD getter on TimeStamp: `ts[i].time.mjd()` (not `toMjd()`)

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

### loki_spectral -- plot flags location
Spectral plot flags (`spectral_psd`, `spectral_peaks`, `spectral_amplitude`,
`spectral_phase`, `spectral_spectrogram`) are read directly from the top-level
`"plots"` JSON object, NOT from a nested `"enabled"` sub-object.
This is because most other modules use `"enabled"` but spectral was added later.
The `_parsePlots()` function handles both locations.

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

### SSA performance -- key learnings
- For 6h climatological data (n=36524): L=365, k=40 -> ~2.5 min total
- Use randomized SVD (`randomizedSvd.hpp/.cpp`), not JacobiSVD or BDCSVD

### `distributions.hpp` namespace
Functions are in `loki::stats::` -- e.g. `loki::stats::chi2Quantile(p, df)`.

---

## Config Structs (current state post-spectral)

### SpectralConfig (config.hpp) -- complete
```cpp
struct SpectralFftConfig {
    std::string windowFunction {"hann"};
    bool        welch          {false};
    int         welchSegments  {8};
    double      welchOverlap   {0.5};
};
struct SpectralLombScargleConfig {
    double oversampling {4.0};
    bool   fastNfft     {false};
    double fapThreshold {0.01};
};
struct SpectralSpectrogramConfig {
    bool   enabled        {false};
    int    windowLength   {1461};
    double overlap        {0.5};
    double focusPeriodMin {0.0};
    double focusPeriodMax {0.0};
};
struct SpectralPeakConfig {
    int    topN          {10};
    double minPeriodDays {0.0};
    double maxPeriodDays {0.0};
};
struct SpectralConfig {
    std::string                gapFillStrategy  {"linear"};
    int                        gapFillMaxLength {0};
    std::string                method           {"auto"};
    SpectralFftConfig          fft              {};
    SpectralLombScargleConfig  lombScargle      {};
    SpectralSpectrogramConfig  spectrogram      {};
    SpectralPeakConfig         peaks            {};
    double                     significanceLevel{0.05};
};
```

### PlotConfig spectral fields (config.hpp) -- complete
```cpp
bool spectralPsd         {true};
bool spectralPeaks       {true};
bool spectralAmplitude   {false};
bool spectralPhase       {false};
bool spectralSpectrogram {false};
```

### KalmanConfig (config.hpp) -- TO ADD in loki_kalman thread
See "NEXT THREAD: loki_kalman" section above for full design.

---

## Statistical / Domain Key Learnings

- SSA window L should be a multiple of the dominant period for clean separation
- Classical decomposition: residual variance > 40% is normal for sub-daily
  climatological data (synoptic variability dominates)
- STL outer loop (n_outer >= 1) needed when seasonal amplitude changes over time
- Period in decomposition is always in samples -- 1461 for 6h data (1 year)
- Draconitic period for GNSS: 351.4 days (not 365.25)
- For 6h climatological data: L=365 (quarter-year) is a good SSA starting point
- Variance fractions from randomized SVD are relative to captured variance
- `min_segment_points` for change point detection should be high (e.g. 600)
  for 6h climatological data to avoid false detections
- Lomb-Scargle preferred over FFT for GNSS and sensor data with frequent gaps
- For ms-resolution sensor data: FFT peak frequency resolution = fs/nfft;
  for 1 kHz data and n=1000: resolution = 1 Hz (1 cpms)
- Spectral `max_period_days` should always be set for FFT to avoid
  long-period trend artefacts dominating rank-1 peak
- Kalman `local_level` Q/R ratio determines smoothness: small Q/R = very smooth,
  large Q/R = follows measurements closely

---

## Module Roadmap (priority order)

| Priority | Module | Primary use case | Status |
|---|---|---|---|
| 1 | `loki_arima` | Climatological residual modelling, forecasting | COMPLETE |
| 2 | `loki_ssa` | SSA decomposition: trend + oscillations + noise | COMPLETE |
| 3 | `loki_decomposition` | Classical + STL trend/seasonal/residual decomposition | COMPLETE |
| 4 | `loki_spectral` | FFT + Lomb-Scargle, period identification, spectrogram | COMPLETE |
| 5 | `loki_kalman` | State estimation, smoothing, prediction, uncertainty | **NEXT** |
| 6 | `loki_clustering` | Train phase segmentation (DBSCAN, k-means) | planned |
| 7 | `loki_qc` | Quality control, gap flagging, automated reporting | planned |

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
                  `stationarity` | `arima` | `ssa` | `decomposition` | `spectral` | `kalman`
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
- `embedMatrix.cpp`, `wCorrelation.cpp`, `randomizedSvd.cpp`, `lagMatrix.cpp`
  MUST be listed in `libs/loki_core/CMakeLists.txt` sources.
- `window` parameters in filters and SSA are in samples, not time units.
- `HatMatrixDetector` does NOT inherit from `OutlierDetector`.
- `NonlinearRegressor` does NOT inherit from `Regressor`.
- `SplineFilter` is planned but NOT yet implemented.
- Classical decomposition and SSA are separate applications -- do not merge.
- `loki_spectral` depends only on `loki_core`.
- `loki_kalman` depends only on `loki_core`.
- gnuplot.cpp: `-persist` flag is REMOVED. Do not restore it.
- loki.sh -- register new apps in APP_EXE map:
  `["spectral"]="loki_spectral_app"`
  `["kalman"]="loki_kalman_app"`
- TimeStamp MJD getter: `.mjd()` (confirmed from timeStamp.hpp)
- SNHT detector (`snhtDetector.hpp/.cpp`) -- not yet implemented,
  files are empty placeholders. Will be added to `loki_homogeneity` later.