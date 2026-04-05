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
                `stationarity` | `arima` | `ssa` | `decomposition` | `spectral`
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

loki_spectral         (depends on loki_core only)                  <- NEXT
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
|   +-- loki_spectral/              <- NEXT
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
|   +-- loki_spectral/              <- NEXT
|       +-- CMakeLists.txt
|       +-- include/loki/spectral/
|       |   +-- spectralResult.hpp
|       |   +-- fftAnalyzer.hpp/.cpp
|       |   +-- lombScargle.hpp/.cpp
|       |   +-- peakDetector.hpp/.cpp
|       |   +-- spectrogramAnalyzer.hpp/.cpp
|       |   +-- spectralAnalyzer.hpp/.cpp
|       |   +-- plotSpectral.hpp/.cpp
|       +-- src/
+-- tests/
+-- config/
|   +-- outlier.json
|   +-- regression.json
|   +-- stationarity.json
|   +-- arima.json
|   +-- ssa.json
|   +-- decomposition.json
|   +-- spectral.json               <- NEXT
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

**No new external dependencies for `loki_spectral`.** FFT is implemented
as a self-contained Cooley-Tukey radix-2 algorithm in `fftAnalyzer.cpp`.
Lomb-Scargle is a direct O(n*f) implementation with optional O(n log n)
NFFT approximation, both self-contained.

---

## Build Instructions

### Platform: Windows MINGW64 shell, UCRT64 toolchain (GCC 13.2)

```bash
./scripts/loki.sh build spectral --copy-dlls
./scripts/loki.sh run spectral
./scripts/loki.sh test --rebuild
```

### CMake target name collision
Executable: `loki_spectral_app` with `OUTPUT_NAME "loki_spectral"`.
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
                  `StlDecompositionConfig`
- `configLoader.hpp / .cpp` -- `_parseDecomposition()` added
- `timeStamp.hpp / .cpp`
- `timeSeries.hpp / .cpp`
- `gapFiller.hpp / .cpp`
- `deseasonalizer.hpp / .cpp`
- `medianYearSeries.hpp / .cpp`
- `descriptive.hpp / .cpp`
- `filter.hpp / .cpp` (stats) -- `movingAverage`, `exponentialMovingAverage`,
                                  `weightedMovingAverage` (free functions, not filters)
- `distributions.hpp / .cpp`
- `hypothesis.hpp / .cpp`
- `metrics.hpp / .cpp`
- `wCorrelation.hpp / .cpp` -- w-correlation matrix for SSA
- `loader.hpp / .cpp`, `dataManager.hpp / .cpp`
- `gnuplot.hpp / .cpp`
- `plot.hpp / .cpp` -- `Plot::residualDiagnostics(residuals, fittedValues, title)` is
                        a non-static member method; always instantiate `Plot corePlot(cfg)`

### loki_core/math -- complete
- `lsqResult.hpp`
- `lsq.hpp / .cpp`
- `designMatrix.hpp / .cpp`
- `hatMatrix.hpp / .cpp`
- `svd.hpp` -- header-only (BDCSVD linking bug workaround)
- `lm.hpp / .cpp` -- LmSolver for Levenberg-Marquardt; `ModelFn = std::function<double(double, const Eigen::VectorXd&)>`
- `lagMatrix.hpp / .cpp`
- `embedMatrix.hpp / .cpp` -- Hankel trajectory matrix for SSA
- `randomizedSvd.hpp / .cpp` -- Halko et al. (2011) randomized truncated SVD

### loki_outlier -- complete (O1-O4)

**O1-O3: standard detectors** -- `IqrDetector`, `MadDetector`, `ZScoreDetector`,
all inherit from `OutlierDetector` base. Used via `OutlierCleaner`.

**O4: HatMatrixDetector** -- COMPLETE. Does NOT inherit from `OutlierDetector`
(fundamentally different interface). Implemented in `loki_outlier` but integrated
directly in `apps/loki_outlier/main.cpp` via a separate processing branch.

Key facts about `HatMatrixDetector`:
- Class: `loki::outlier::HatMatrixDetector` in `hatMatrixDetector.hpp`
- Config: `HatMatrixDetectorConfig` (outside class, GCC 13 pattern)
  - `arOrder` (int, default 5) -- AR lag order p
  - `significanceLevel` (double, default 0.05)
- Result: `HatMatrixResult` struct:
  - `outlierIndices` -- indices into original series (offset by arOrder)
  - `leverages` -- Eigen::VectorXd, length = n - arOrder
  - `threshold` -- chi2Quantile(1-alpha, p) / n
  - `arOrder`, `n`, `nOutliers`
- Input: `std::vector<double>` residuals (NaN-free)
- Algorithm: DEH method (Hau & Tong 1989) -- AR(p) lag matrix -> hat matrix
  diagonal -> chi-squared threshold
- Integration in `main.cpp`: separate `if (method == "hat_matrix")` branch,
  converts result to `OutlierResult` via `toOutlierResult()` helper for
  reuse of existing plot methods

### loki_homogeneity -- complete
### loki_stationarity -- complete

### loki_filter -- complete
All filters inherit from `loki::Filter` base class (pure virtual `apply()` + `name()`).
Located in `libs/loki_filter/include/loki/filter/`.

Complete filter set:
- `MovingAverageFilter` -- centered MA, bfill/ffill edges
- `EmaFilter` -- exponential MA (causal, no edge NaN)
- `WeightedMovingAverageFilter` -- user-supplied kernel weights
- `KernelSmoother` -- Epanechnikov / Gaussian / Uniform / Triangular kernels
- `LoessFilter` -- locally weighted polynomial (degree 1 or 2), optional IRLS robustness
- `SavitzkyGolayFilter` -- polynomial convolution, precomputed coefficients, asymmetric edge handling
- `FilterWindowAdvisor` -- auto bandwidth estimation (Silverman MAD, Silverman, ACF peak)
- `PlotFilter` -- diagnostic plots for filter pipeline
- `FilterResult` struct: `{filtered, residuals, filterName, effectiveWindow, effectiveBandwidth}`

**SplineFilter** -- planned future addition (cubic spline smoothing). Not yet implemented.
Will live in `loki_filter` alongside existing filters.

### loki_regression -- complete (R1-R8)

Complete regressor set:
- `LinearRegressor`, `PolynomialRegressor`, `HarmonicRegressor`, `TrendEstimator`
- `RobustRegressor` (IRLS), `CalibrationRegressor` (TLS via SVD)
- `NonlinearRegressor` (R8) -- Levenberg-Marquardt, COMPLETE

Key facts about `NonlinearRegressor`:
- Does NOT inherit from `Regressor` base (incompatible interface)
- Built-in models (via `NonlinearModelEnum`):
  - `EXPONENTIAL`: f(x, [a,b]) = a * exp(b*x)
  - `LOGISTIC`: f(x, [L,k,x0]) = L / (1 + exp(-k*(x-x0)))
  - `GAUSSIAN`: f(x, [A,mu,sigma]) = A * exp(-((x-mu)^2)/(2*sigma^2))
- Custom model via constructor: `NonlinearRegressor(cfg, modelFn, p0)`
  where `ModelFn = std::function<double(double, const Eigen::VectorXd&)>`
- `fit(ts)` returns `RegressionResult` (pipeline consistent)
- `predict(xNew)` returns `vector<PredictionPoint>` with delta-method intervals
- x-axis convention: `x = mjd - tRef` (same as all other regressors)
- Jacobian: numerical via central differences
- `result.converged` flag -- check and log warning if false

### loki_arima -- complete
### loki_ssa -- complete (S1-S5)
### loki_decomposition -- complete

Key facts about `loki_decomposition`:
- `ClassicalDecomposer`: MA trend (bfill/ffill edges via GapFiller) + modulo-period slot median/mean
- `StlDecomposer`: private weighted LOESS (no dependency on loki_filter),
  bisquare outer robustness weights, inner/outer loop structure per Cleveland (1990)
- `DecompositionAnalyzer`: orchestrator, uses `DataManager` for loading
- `PlotDecomposition`: `plotOverlay`, `plotPanels`, `plotDiagnostics`
  (diagnostics delegates to `Plot corePlot(cfg); corePlot.residualDiagnostics(...)`)
- Seasonal normalization: slot values normalized so sum over one period = 0
- Period is always in **samples** (not time units)

---

## NEXT THREAD: loki_spectral

### What loki_spectral does
Spectral analysis of time series: identifies dominant frequencies/periods,
estimates power spectral density, and detects statistically significant peaks.
Handles both uniformly and non-uniformly sampled series.

### Method selection
```json
"spectral": {
    "method": "auto"
}
```

| Value | Behaviour |
|---|---|
| `"auto"` | Inspects the series: if uniformly sampled (< 5% of samples exceed 1.1x median step) -> FFT, otherwise -> Lomb-Scargle |
| `"fft"` | Always FFT. Requires gap-free input (GapFiller runs first). Warning logged if gaps remain. |
| `"lomb_scargle"` | Always Lomb-Scargle. Works on non-uniform and gappy series directly. |

### Planned module files
```
libs/loki_spectral/include/loki/spectral/
    spectralResult.hpp          -- SpectralResult, SpectralPeak, SpectrogramResult
    fftAnalyzer.hpp/.cpp        -- Cooley-Tukey radix-2 FFT, Welch PSD, windowing
    lombScargle.hpp/.cpp        -- Lomb-Scargle periodogram + FAP
    peakDetector.hpp/.cpp       -- peak finding, significance ranking, top-N output
    spectrogramAnalyzer.hpp/.cpp -- STFT sliding-window spectrogram
    spectralAnalyzer.hpp/.cpp   -- orchestrator: method selection, gap fill, output
    plotSpectral.hpp/.cpp       -- PSD plot, periodogram, spectrogram heatmap
```

### Key data structures (to design in thread)
```cpp
struct SpectralPeak {
    double periodDays;      // period in days (primary output for protocols)
    double freqCpd;         // frequency in cycles per day
    double power;           // normalised power [0, 1]
    double fap;             // false alarm probability (Lomb-Scargle only)
    int    rank;            // 1 = strongest peak
};

struct SpectralResult {
    std::vector<double>      frequencies;  // cycles per day
    std::vector<double>      power;        // PSD or normalised periodogram
    std::vector<SpectralPeak> peaks;       // sorted by power descending
    std::string              method;       // "fft" | "lomb_scargle"
    double                   samplingStepDays;
};

struct SpectrogramResult {
    std::vector<double> times;        // MJD of each window centre
    std::vector<double> frequencies;  // cycles per day
    std::vector<std::vector<double>> power;  // power[time][freq]
};
```

### FFT implementation plan
- Self-contained Cooley-Tukey radix-2 in `fftAnalyzer.cpp` (no external library)
- Zero-padding to next power of 2 for efficiency
- Windowing functions: Hann (default), Hamming, Blackman, Flat-top, Rectangular
  Applied before FFT to reduce spectral leakage
- Output: single-sided PSD in units of (amplitude^2 / frequency)
- Welch method: overlapping segments, averaged PSD, reduces variance

### Lomb-Scargle implementation plan
- Direct O(n * nFreq) implementation for n < 100k
- NFFT approximation (Press & Rybicki 1989) for n >= 100k -- toggleable via
  `"fast_ls": true` in JSON config
- FAP (False Alarm Probability) via analytic formula (Baluev 2008) --
  more accurate than bootstrap for large n
- Frequency grid: from f_min = 1/(T_total) to f_max = 1/(2 * median_step),
  oversampling factor configurable (default 4)

### Spectrogram plan
- STFT: sliding window FFT with configurable overlap
- `window_length` and `overlap` in JSON (in samples)
- `focus_period_min` / `focus_period_max` for frequency zoom (in days)
- Output: 2D power matrix -> gnuplot heatmap (pm3d)
- Use case examples:
  - Detect amplitude modulation of annual cycle (climate)
  - Detect frequency shifts / phase slips in sensor data
  - Identify when a periodic signal appears or disappears

### Peak detection plan
- Local maxima in PSD/periodogram above noise floor
- Noise floor: median of PSD (robust to peaks)
- Configurable: `top_n_peaks` (default 10), `min_period_days`, `max_period_days`
- FAP threshold for Lomb-Scargle: `fap_threshold` (default 0.01)
- Protocol output: ranked table of top N peaks with period, frequency, power, FAP

### SpectralConfig design (to add to config.hpp)
```cpp
struct SpectralFftConfig {
    std::string windowFunction {"hann"};   // "hann"|"hamming"|"blackman"|"flattop"|"rectangular"
    bool        welch          {false};    // enable Welch averaged PSD
    int         welchSegments  {8};        // number of overlapping segments for Welch
    double      welchOverlap   {0.5};      // segment overlap fraction
};

struct SpectralLombScargleConfig {
    double oversampling  {4.0};    // frequency grid oversampling factor
    bool   fastNfft      {false};  // use NFFT approximation for n >= 100k
    double fapThreshold  {0.01};   // FAP significance threshold for peak reporting
};

struct SpectralSpectrogramConfig {
    bool   enabled         {false};
    int    windowLength    {1461};  // STFT window in samples
    double overlap         {0.5};   // window overlap fraction [0, 1)
    double focusPeriodMin  {0.0};   // zoom lower bound in days (0 = no zoom)
    double focusPeriodMax  {0.0};   // zoom upper bound in days (0 = no zoom)
};

struct SpectralPeakConfig {
    int    topN           {10};    // number of top peaks to report
    double minPeriodDays  {0.0};   // ignore peaks below this period (0 = no limit)
    double maxPeriodDays  {0.0};   // ignore peaks above this period (0 = no limit)
};

struct SpectralConfig {
    std::string                gapFillStrategy   {"linear"};
    int                        gapFillMaxLength  {0};
    std::string                method            {"auto"};  // "auto"|"fft"|"lomb_scargle"
    SpectralFftConfig          fft               {};
    SpectralLombScargleConfig  lombScargle       {};
    SpectralSpectrogramConfig  spectrogram       {};
    SpectralPeakConfig         peaks             {};
    double                     significanceLevel {0.05};
};
```

### PlotConfig additions for spectral
```cpp
bool spectralPsd          {true};   // PSD / periodogram plot (log-log or log-linear)
bool spectralPeaks        {true};   // annotated peaks on PSD plot
bool spectralSpectrogram  {false};  // 2D time-frequency heatmap
```

### Output units convention
- **Frequencies**: cycles per day (cpd) -- natural for climatological data
- **Periods**: days -- used in protocols and peak tables
- **For ms sensor data**: period in seconds derived from sampling step automatically
- Protocols list top N peaks in descending power order

### Protocol format (sketch)
```
==========================================================
 LOKI Spectral Analysis Protocol
==========================================================
 Dataset   : CLIM_DATA_EX1
 Component : col_3
 Method    : Lomb-Scargle
 N         : 36524 observations
 Span      : 25.12 years
 Median step: 0.25 days (6h)
----------------------------------------------------------
 Top 10 dominant periods:
  Rank  Period (days)  Freq (cpd)   Power    FAP
     1      365.25      0.002738    0.847    < 0.001
     2      182.62      0.005476    0.341    < 0.001
     3      351.40      0.002846    0.128      0.003
     ...
----------------------------------------------------------
 Spectrogram: disabled
==========================================================
```

### Files to request at loki_spectral thread start
```
libs/loki_core/include/loki/core/config.hpp          -- current state (post-decomposition)
libs/loki_core/src/core/configLoader.cpp             -- last 50 lines (_parseDecomposition pattern)
libs/loki_core/include/loki/io/plot.hpp              -- Plot API (residualDiagnostics signature)
libs/loki_decomposition/CMakeLists.txt               -- CMake pattern
apps/loki_decomposition/CMakeLists.txt               -- app CMake pattern
apps/loki_decomposition/main.cpp                     -- DataManager loading pattern
```

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
- API: `gp("command")` -- no `<<` operator
- All module-specific plotters use local `fwdSlash()` static private helper
- Font: `'Sans,12'` (not `'Helvetica,12'`)
- Terminal: `noenhanced`
- `Plot::residualDiagnostics()` is non-static -- instantiate `Plot corePlot(cfg)`

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
Do NOT use `Loader::load()` directly -- it requires a file path argument
and is a lower-level API. `DataManager` handles all input modes
(SINGLE_FILE, FILE_LIST, SCAN_DIRECTORY).

### SSA performance -- key learnings
- JacobiSVD on K x L trajectory matrix (K=35000, L=1461) takes hours
- SelfAdjointEigenSolver on L x L: also too slow for L=1461
- Solution: randomized SVD (Halko et al. 2011) in `randomizedSvd.hpp/.cpp`
- For 6h climatological data (n=36524): L=365, k=40 -> ~2.5 min total

### loki_decomposition -- key learnings
- Classical seasonal: modulo-period slot median (no MedianYearSeries dependency)
- STL: private weighted LOESS in `stlDecomposer.cpp` (no loki_filter dependency)
- MA trend edge NaN filled via `GapFiller(LINEAR).fill()` -- bfill/ffill
- Period is always in samples -- document clearly in config

### `distributions.hpp` namespace
Functions are in `loki::stats::` -- e.g. `loki::stats::chi2Quantile(p, df)`.

---

## Config Structs (current state post-decomposition)

### DecompositionConfig (config.hpp) -- complete
```cpp
enum class DecompositionMethodEnum { CLASSICAL, STL };

struct ClassicalDecompositionConfig {
    std::string trendFilter  {"moving_average"};
    std::string seasonalType {"median"};
};
struct StlDecompositionConfig {
    int nInner{2}; int nOuter{1}; int sDegree{1}; int tDegree{1};
    double sBandwidth{0.0}; double tBandwidth{0.0};
};
struct DecompositionConfig {
    std::string gapFillStrategy{"linear"}; int gapFillMaxLength{0};
    DecompositionMethodEnum method{DecompositionMethodEnum::CLASSICAL};
    int period{1461};
    ClassicalDecompositionConfig classical{};
    StlDecompositionConfig stl{};
    double significanceLevel{0.05};
};
```

### PlotConfig decomposition additions (config.hpp) -- complete
```cpp
bool decompOverlay     {true};
bool decompPanels      {true};
bool decompDiagnostics {false};
```

### SpectralConfig (config.hpp) -- TO ADD in loki_spectral thread
See "NEXT THREAD: loki_spectral" section above for full design.

### PlotConfig spectral additions (config.hpp) -- TO ADD
```cpp
bool spectralPsd         {true};
bool spectralPeaks       {true};
bool spectralSpectrogram {false};
```

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
- For ms-resolution sensor data with n > 100k: use NFFT approximation in L-S

---

## Module Roadmap (priority order)

| Priority | Module | Primary use case | Status |
|---|---|---|---|
| 1 | `loki_arima` | Climatological residual modelling, forecasting | COMPLETE |
| 2 | `loki_ssa` | SSA decomposition: trend + oscillations + noise | COMPLETE |
| 3 | `loki_decomposition` | Classical + STL trend/seasonal/residual decomposition | COMPLETE |
| 4 | `loki_spectral` | FFT + Lomb-Scargle, period identification, spectrogram | **NEXT** |
| 5 | `loki_kalman` | GNSS processing, train sensor fusion | planned |
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
                  `stationarity` | `arima` | `ssa` | `decomposition` | `spectral`
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
- `loki_spectral` depends only on `loki_core` -- no dependency on
  `loki_decomposition`, `loki_ssa`, or `loki_filter`.
- loki.sh -- register new apps in APP_EXE map:
  `["spectral"]="loki_spectral_app"`