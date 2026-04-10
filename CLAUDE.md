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
|   |       |                  wCorrelation, sampling, bootstrap, permutation)
|   |       +-- io/           (loader, dataManager, gnuplot, plot)
|   |       +-- math/         (lsqResult, lsq, designMatrix, hatMatrix, svd, lm,
|   |                          lagMatrix, embedMatrix, randomizedSvd, spline)
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
|   +-- CMakeLists.txt               <- Catch2 unit tests only, nemeneny
|   +-- demo/
|   |   +-- CMakeLists.txt           <- demo executables (LOKI_BUILD_DEMOS=ON)
|   |   +-- demo_sampling.cpp        <- Sampler demo
|   |   +-- demo_bootstrap.cpp       <- Bootstrap CI demo
|   |   +-- demo_permutation.cpp     <- Permutation tests demo
|   |   +-- input/                   <- gitignored, CLIM_DATA_EX1.txt sem
|   |   +-- png/                     <- gitignored
|   |   +-- protocol/                <- gitignored
|   +-- unit/
+-- config/
+-- scripts/
|   +-- loki.sh                      <- supports demo_sampling/bootstrap/permutation
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
./scripts/loki.sh run demo_sampling
./scripts/loki.sh run demo_bootstrap
./scripts/loki.sh run demo_permutation
./scripts/loki.sh test --rebuild
```

### Demo executables
- Spravované cez `LOKI_BUILD_DEMOS=ON` (automaticky v debug preset).
- Spúšťajú sa bez config argumentu -- `cmd_run` to ošetruje prázdnym `APP_CONFIG`.
- Input dáta: `tests/demo/input/CLIM_DATA_EX1.txt` (gitignored).
- Output: `tests/demo/png/`, `tests/demo/protocol/` (gitignored).
- `.gitignore` ignoruje: `tests/demo/png/`, `tests/demo/protocol/`, `tests/demo/input/`.

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
["demo_sampling"]=""        <- no config, runs directly
["demo_bootstrap"]=""       <- no config, runs directly
["demo_permutation"]=""     <- no config, runs directly
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
                  `ClusteringOutlierConfig`,
                  `SplineFilterConfig` (subsampleStep, bc) -- pre loki_filter
- `configLoader.hpp / .cpp` -- vsetky parse metody vratane `_parseFilter` so spline sekciou
- `timeStamp.hpp / .cpp`
- `timeSeries.hpp / .cpp`
- `gapFiller.hpp / .cpp` -- Strategy::SPLINE pridana, fillSpline() implementovana
- `deseasonalizer.hpp / .cpp`
- `medianYearSeries.hpp / .cpp`
- `descriptive.hpp / .cpp` -- includes `hurstExponent()` and `summarize()`
- `filter.hpp / .cpp` (stats -- movingAverage, EMA, WMA)
- `distributions.hpp / .cpp`
- `hypothesis.hpp / .cpp`
- `metrics.hpp / .cpp`
- `wCorrelation.hpp / .cpp`
- `sampling.hpp / .cpp` -- `Sampler` class, vsetky distribucie + bootstrap indices
- `bootstrap.hpp / .cpp` -- `percentileCI`, `bcaCI`, `blockCI`
- `permutation.hpp / .cpp` -- `oneSampleTest`, `twoSampleTest`, `correlationTest`
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
- `spline.hpp / .cpp` -- `CubicSpline`, `BoundaryCondition`, `CubicSplineConfig`

### loki_outlier -- complete (O1-O4)
### loki_homogeneity -- complete
- `changePointDetector.hpp / .cpp`   -- Yao & Davis (1986), recursive binary splitting
- `snhtDetector.hpp / .cpp`          -- SNHT (Alexandersson 1986), Monte Carlo p-value
- `peltDetector.hpp / .cpp`          -- PELT (Killick et al. 2012), O(n) global optimisation
- `bocpdDetector.hpp / .cpp`         -- BOCPD (Adams & MacKay 2007), NIX conjugate model
- `multiChangePointDetector.hpp / .cpp` -- dispatcher: yao_davis/snht use recursive split(),
                                           pelt/bocpd call detectAll() directly (single pass)
- `seriesAdjuster.hpp / .cpp`
- `homogenizer.hpp / .cpp`
- `homogeneityAnalyzer.hpp / .cpp`   -- full pipeline orchestrator, protocol, CSV, plots
- `plotHomogeneity.hpp / .cpp`
### loki_stationarity -- complete
### loki_filter -- complete (vrátane SplineFilter)
### loki_regression -- complete (R1-R8)
### loki_arima -- complete
### loki_ssa -- complete (S1-S5)
### loki_decomposition -- complete
### loki_spectral -- complete
### loki_kalman -- complete
### loki_qc -- complete
### loki_clustering -- complete

### loki_filter -- FilterAnalyzer
- `filterAnalyzer.hpp / .cpp` -- orchestrates full pipeline (gap fill, filter, protocol, CSV, plots)
- Protokol obsahuje: filter params, residual stats, J-B test, Durbin-Watson, ACF lag-1, tuning hints
- Vzor rovnaky ako `DecompositionAnalyzer` -- `run(ts, datasetName)` pattern

---

## GapFiller::Strategy::SPLINE -- Deployment Status

`GapFiller::Strategy::SPLINE` je implementovana v `gapFiller.cpp`. String `"spline"` sa
konvertuje na enum v kazdom `apps/loki_*/main.cpp` samostatne. Status patchov:

| App | Status |
|---|---|
| `loki_filter` | DONE |
| `loki_homogeneity` | DONE -- `homogeneityAnalyzer.cpp`, `homogenizer.cpp`, `outlierCleaner.hpp` |
| `loki_regression` | DONE -- `buildGapFillerConfig()` v `main.cpp` |
| `loki_arima` | DONE -- `main.cpp` Step 1 |
| `loki_ssa` | DONE -- `main.cpp` Step 1 |
| `loki_decomposition` | DONE -- `decompositionAnalyzer.cpp` Step 1 |
| `loki_spectral` | DONE -- `spectralAnalyzer.cpp` gap fill blok |
| `loki_kalman` | DONE -- `fillGaps()` refactored + spline pridany |
| `loki_stationarity` | DONE -- `main.cpp` + `StationarityConfig` v `config.hpp` |
| `loki_qc` | N/A -- GapFiller sa nepouiva priamo |
| `loki_clustering` | DONE |

Vzor patchu (rovnaky pre vsetky apps) -- najdi blok kde sa konvertuje `gapFillStrategy`
string a pridaj vetvu:
```cpp
else if (s == "spline") gfc.strategy = loki::GapFiller::Strategy::SPLINE;
```

JSON konfig pre spline gap filling (platne pre vsetky moduly):
```json
"gap_filling": {
    "strategy": "spline",
    "max_fill_length": 0,
    "gap_threshold_factor": 1.5,
    "min_series_years": 10
}
```
Poznamka: `min_series_years` sa pre SPLINE ignoruje (relevantne len pre MEDIAN_YEAR).

---

## Planned Extensions to loki_core

### stats/sampling.hpp / .cpp -- COMPLETE
### stats/bootstrap.hpp / .cpp -- COMPLETE
### stats/permutation.hpp / .cpp -- COMPLETE
### math/spline.hpp / .cpp -- COMPLETE

### GapFiller -- SPLINE strategy -- COMPLETE
Cubic spline interpolation cez vsetky platne body.
Poziadavky: aspon 3 platne body, inak fallback na LINEAR s LOKI_WARNING.

### SplineFilter in loki_filter -- COMPLETE
- `subsampleStep`: kazdy k-ty bod ako uzol (0 = auto: n/200, min 1)
- `bc`: "natural" | "not_a_knot" | "clamped"
- `SplineFilterConfig` je definovana v `config.hpp` (NIE v `splineFilter.hpp`)
  -- redefinition error ak by bola na oboch miestach

---

## Planned New Applications

### loki_simulate -- PLANNED

### loki_simulate -- Implementation Notes

**Architecture decision: Analyzer pattern**
Rovnaký vzor ako ostatné moduly -- `SimulationAnalyzer` trieda s metódou
`run(const std::string& datasetName)`. Nie priamo v `main()`.
Dôvod: konzistencia, testovateľnosť, čistý main().

**Dependencies (files to request at thread start):**
Pri otvorení nového threadu požiadaj o tieto súbory:

From loki_core:
- `config.hpp` -- pre pridanie SimulateConfig
- `configLoader.hpp` + `configLoader.cpp` -- pre parsovanie nového JSON bloku
- `timeSeries.hpp` -- API pripomienka
- `gapFiller.hpp` -- ak budeme injektovať gaps

From loki_core/stats:
- `bootstrap.hpp` -- percentileCI, bcaCI, blockCI (StatFn, BootstrapConfig, BootstrapResult)
- `sampling.hpp`  -- Sampler trieda (seed, mt19937_64, bootstrap indices)

From loki_arima:
- `arimaAnalyzer.hpp` -- API pre fit + residuals
- `arimaForecaster.hpp` -- API pre simuláciu dopredných krokov

From loki_kalman:
- `kalmanAnalyzer.hpp` -- API
- `kalmanModel.hpp` + `kalmanModelBuilder.hpp` -- pre state-space simuláciu
- `kalmanFilter.hpp` -- pre forward simulation

From existing apps (for main() pipeline inspiration):
- `apps/loki_arima/main.cpp` -- vzor gap fill + deseasonalize + analyze pipeline
- `apps/loki_kalman/main.cpp` -- vzor fillGaps() + estimateNoise() pattern

**Output files:**
- `OUTPUT/LOG/` -- logger (automaticky)
- `OUTPUT/PROTOCOLS/simulate_[dataset]_[param]_protocol.txt` -- pekne formátované výsledky
- `OUTPUT/IMG/simulate_[dataset]_[param]_[plottype].png` -- grafy
- `OUTPUT/CSV/simulate_[dataset]_[param]_simulations.csv` -- ak bootstrap/MC

**Plot types (new -- plotSimulation.hpp/cpp):**
- `simulate_overlay` -- originálna séria + simulované repliky (polopriesvitné)
- `simulate_envelope` -- percentilový obal simulácií (5/25/50/75/95)
- `simulate_bootstrap_dist` -- histogramy bootstrap distribúcií odhadov
- `simulate_acf_comparison` -- ACF originál vs ACF simulovaných radov
Základné grafy (histogram, ACF, QQ) delegovať na `loki::Plot`.

**SimulateConfig structure (draft):**
```cpp
struct SimulateInjectOutliersConfig {
    bool   enabled   {false};
    double fraction  {0.01};
    double magnitude {5.0};
};

struct SimulateInjectGapsConfig {
    bool        enabled   {false};
    int         nGaps     {5};
    int         maxLength {10};
};

struct SimulateInjectShiftsConfig {
    bool   enabled   {false};
    int    nShifts   {2};
    double magnitude {1.0};
};

struct SimulateArimaConfig {
    int    p     {1};
    int    d     {0};
    int    q     {0};
    double sigma {1.0};
};

struct SimulateKalmanConfig {
    std::string model {"local_level"};
    double      Q     {0.001};
    double      R     {0.01};
};

struct SimulateConfig {
    std::string                  model           {"arima"};
    int                          n               {1000};
    uint64_t                     seed            {42};
    int                          nSimulations    {100};
    std::string                  gapFillStrategy {"linear"};
    int                          gapFillMaxLength{0};
    SimulateArimaConfig          arima           {};
    SimulateKalmanConfig         kalman          {};
    SimulateInjectOutliersConfig injectOutliers  {};
    SimulateInjectGapsConfig     injectGaps      {};
    SimulateInjectShiftsConfig   injectShifts    {};
    double                       confidenceLevel {0.95};
    double                       significanceLevel{0.05};
};
```

**Key implementation notes:**
- Simulácia ARIMA: generuj white noise epsilon ~ N(0, sigma^2), aplikuj
  AR a MA polynómy rekurzívne. Pre d>0 kumulatívne sumuj.
- Simulácia Kalman: x_{t+1} = F*x_t + w_t kde w_t ~ N(0,Q),
  y_t = H*x_t + v_t kde v_t ~ N(0,R). Použiť KalmanModelBuilder.
- Random number generation: `std::mt19937_64` so seed z configu.
  seed=0 znamená náhodný seed (std::random_device).
- Bootstrap workflow: (1) fit model na vstupných dátach,
  (2) generuj B simulácií z fitted modelu,
  (3) pre každú simuláciu refit model,
  (4) zbieraj distribúcie odhadovaných parametrov.
- Injektovanie posunov: vyber náhodné indexy, pridaj konštantu
  od daného indexu do konca segmentu (step change).
- CSV output: jeden riadok = jedna simulácia, stĺpce = časové kroky.
  Pre veľké n*B môže byť CSV obrovský -- zvážiť len summary statistiky.
- Bootstrap CI pre parametre: pre autocorrelované simulované rady pouzi
  blockCI() s automatickym blockLength (cfg.blockLength=0).
  Pre iid statistiky (napr. mean syntetickej serie) postaci percentileCI().
- StatFn vzor pre ARIMA parameter:
  [&analyzer](const std::vector<double>& v) {
      return analyzer.analyze(v).arCoeffs[0];
  }
- Sampler inicializacia: Sampler sampler(cfg.seed == 0
      ? std::random_device{}()
      : cfg.seed);

### loki_evt -- PLANNED
### loki_kriging -- PLANNED
### loki_spline -- PLANNED

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
Postihuje: `bootstrap.cpp`, `permutation.cpp` a vsetky buducte `.cpp` so
slobodnymi funkciami v sub-namespacoch.

### bcaCI -- degeneratny jackknife
Pre statistiky s malym rozptylom (napr. mean na velkych seriach) je jackknife
distribucia takmer konstantna -> `den < 1e-15` -> fallback na percentile CI
s `LOKI_WARNING`. NIE vynimka.

### SplineFilterConfig redefinition
`SplineFilterConfig` je definovana **iba** v `config.hpp`.
`splineFilter.hpp` pouziva `using Config = SplineFilterConfig` (z config.hpp).
Nesmie byt definovana v oboch suboroch -- redefinition error.

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
SNHT (Alexandersson 1986) bude separatny `snhtDetector.hpp / .cpp`.
Subory uz existuju v repozitari ako PRAZDNE -- naplnit, nie vytvorit.

### GapFiller SPLINE -- outlier replacement warning
SplineFilter/SPLINE strategy je nevhodna pre outlier replacement
(replacement_strategy v OutlierConfig). Spline overshootuje (Runge fenomen)
okolo jednotlivych outlier bodov. Pre replacement_strategy pouzivat LINEAR
alebo FORWARD_FILL. SPLINE ma zmysel len pre gap_fill_strategy (dlhe useky).

### StationarityConfig -- gap fill fieldy
StationarityConfig v config.hpp neobsahovala gapFillStrategy / gapFillMaxLength
-- boli doplnene v tomto threade. Vzor rovnaky ako SsaConfig, ArimaConfig.

### GapFiller parsovanie -- best practice vzor
Referencny vzor pre parsovanie gapFillStrategy stringu je filterAnalyzer.cpp
a regression/main.cpp (buildGapFillerConfig() helper). Kalman fillGaps()
je referencny vzor pre dlhsie funkcie s MEDIAN_YEAR + auto logikou:
parsovanie oddelene od konstrukcie GapFiller, jedna GapFiller instancia,
MEDIAN_YEAR rieseny na jednom mieste.

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
- Bootstrap: standard iid bootstrap INVALID for autocorrelated time series.
  Block bootstrap required. Block length >= effective decorrelation length
  (lag where ACF < 1.96/sqrt(n)).
- bcaCI jackknife degeneruje pre statistiky s malym rozptylom (napr. mean)
  na velkych seriach -- fallback na percentile CI je spravne chovanie.
- SplineFilter subsampleStep=0 (auto) = n/200, vhodne pre vacsinu klimatologickych
  dat. Pre kratke serie (n<200) = 1 (presna interpolacia).
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
| done | `loki_core/stats/sampling` | core extension | COMPLETE |
| done | `loki_core/stats/bootstrap` | core extension | COMPLETE |
| done | `loki_core/stats/permutation` | core extension | COMPLETE |
| done | `GapFiller` SPLINE strategy | core extension | COMPLETE |
| done | `SplineFilter` in loki_filter | module extension | COMPLETE |
| done | `FilterAnalyzer` in loki_filter | module extension | COMPLETE |
| done | SNHT in loki_homogeneity          | module extension | COMPLETE |
| done | PELT in loki_homogeneity          | module extension | COMPLETE |
| done | BOCPD in loki_homogeneity         | module extension | COMPLETE |
| done | HomogeneityAnalyzer + protocol    | module extension | COMPLETE |
| done | GapFiller SPLINE patche (apps) | patche | COMPLETE |
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
  `randomizedSvd.hpp`, `spline.hpp` are in `loki_core/math/`.
- `wCorrelation.hpp`, `sampling.hpp`, `bootstrap.hpp`, `permutation.hpp`
  are in `loki_core/stats/`.
- `svd.cpp` does NOT exist -- `SvdDecomposition` is header-only in `svd.hpp`.
- `window` parameters in filters and SSA are in samples, not time units.
- `HatMatrixDetector` does NOT inherit from `OutlierDetector`.
- `NonlinearRegressor` does NOT inherit from `Regressor`.
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
- `SplineFilterConfig` definovana TYLKO v `config.hpp`, nie v `splineFilter.hpp`.
- `bootstrap.cpp`, `permutation.cpp`: funkcie musia byt v `namespace loki::stats { }`
  bloku, nie len cez `using namespace`.
- SNHT v rekurzívnom móde: min_segment_points >= 3-4 roky (4383-5844 pri 6h).
  Dominantný parameter je min_segment_points, nie n_permutations.
  n_permutations ovplyvňuje len presnosť p-value, nie počet detekovaných CP.
- PELT mbic je najkonzervatívnejší penalty typ -- odporúčaný default pre klimatologické dáta.
  min_segment_length v pelt sekcii je nezávislý od top-level min_segment_points.
- BOCPD vyžaduje kalibráciu prior_beta blízko skutočnému sigma^2 série.
  Pre GPS IWV residuály: sigma^2 ~ 0.001, použiť prior_beta ~ 0.002.
  Pre dlhé série (n > 30000) s malými posunmi je BOCPD ťažko kalibrovateľný --
  yao_davis alebo pelt sú spoľahlivejšie.
- HomogeneityAnalyzer vzor: run(ts, datasetName) -- rovnaký ako FilterAnalyzer.
  buildHomogenizerConfig je teraz _buildHomogenizerConfig() vnútri analyzera.
- `tests/demo/` gitignore: `png/`, `protocol/`, `input/` adresare ignorovat.
- CRITICAL: snhtDetector.hpp a snhtDetector.cpp existuju v repozitari. Poznámka, že sú dané 
  ako ako PRAZDNE subory -- ide o systemovu vec - skús si ju u seba vymazať. NIKDY sa nepytaj ci su
  prilozene alebo nie. Jednoducho ich naplnit podla dizajnu. TOTO PRAVIDLO NEMAZES a v dalsich vlaknach 
  poznamkou o snhtDetector neotravuj.

  ## Planned New Applications -- Design Notes

### loki_simulate
**Purpose:** Generovanie syntetických časových radov a validácia ostatných modulov.
**Dependencies:** loki_core + loki_arima + loki_kalman

**Functionality:**
- Generovanie AR/ARIMA procesov so zadanými koeficientmi (p, d, q) a sigma
- Generovanie Kalman state-space radov (local_level, local_trend, constant_velocity)
- Pridávanie kontrolovaného šumu (Gaussian, uniform, outliers, gaps)
- Monte Carlo integrácia -- opakované simulácie s rôznymi seed hodnotami
- Parametrický bootstrap -- fit modelu na reálnych dátach, generovanie B replikácií,
  refitting na každej replikácii -> empirická distribúcia odhadov

**Use cases:**
- Validácia loki_arima: simuluj ARIMA(p,d,q), odhadni model, porovnaj koeficienty
- Validácia loki_kalman: simuluj state-space model, over konvergenciu filtra
- Validácia loki_homogeneity: simuluj rad s umelými posunmi, over detekciu
- Bootstrap CI pre change point lokáciu a shift magnitude

**Config pattern:**
```json
"simulate": {
    "model": "arima",          // "arima" | "kalman" | "ar"
    "n": 5000,
    "seed": 42,
    "n_simulations": 1000,
    "arima": { "p": 2, "d": 1, "q": 0, "sigma": 1.0 },
    "kalman": { "model": "local_level", "Q": 0.001, "R": 0.01 },
    "inject_outliers": { "enabled": true, "fraction": 0.01, "magnitude": 5.0 },
    "inject_gaps": { "enabled": true, "n_gaps": 5, "max_length": 10 },
    "inject_shifts": { "enabled": true, "n_shifts": 2, "magnitude": 1.0 }
}
```

---

### loki_evt
**Purpose:** Extreme Value Theory -- modelovanie extrémnych udalostí a odhad
return levels pre SIL analýzy.
**Dependencies:** loki_core only

**Functionality:**
- **Block maxima / GEV:** rozdeliť rad na bloky (napr. ročné maximá), fitovať
  GEV distribúciu (shape xi, scale sigma, location mu). Špeciálne prípady:
  xi=0 Gumbel, xi>0 Fréchet, xi<0 Weibull.
- **POT / GPD:** vybrať exceedances nad prahovú hodnotu u, fitovať GPD.
  Preferovaná metóda pre malé datasety -- využíva všetky extrémy, nie len blokové maximá.
- **Return level odhady:** T-ročná návratová hodnota s CI (profile likelihood alebo bootstrap).
- **Threshold selection:** mean excess plot, stability plot pre xi a sigma.
- **GoF testy:** Anderson-Darling, Kolmogorov-Smirnov pre EVT fit.
- **SIL 4 use case:** modelovanie chybových intervalov pri pravdepodobnostiach
  < 1e-8/h. GPD/POT preferovaný -- block maxima plytvá dátami.

**Key domain knowledge:**
- Pre GPS/IWV dáta: extrémy troposférického oneskorenia pri búrkových udalostiach.
- Pre train senzory: extrémy vibrácií, zrýchlení, teplotných šokov.
- Return period T = 1/p kde p = pravdepodobnosť exceedance za jednotku času.
- CI pre return levels: profile likelihood stabilnejší ako delta metóda pre malé n.

**Config pattern:**
```json
"evt": {
    "method": "pot",           // "pot" | "block_maxima"
    "threshold_auto": true,    // auto via mean excess plot elbow
    "threshold": 0.0,          // manual threshold (ak auto=false)
    "block_size": 365,         // pre block_maxima (v sampoch)
    "return_periods": [10, 50, 100, 1000, 1000000],
    "confidence_level": 0.95,
    "ci_method": "profile_likelihood",  // "profile_likelihood" | "bootstrap"
    "n_bootstrap": 1000,
    "significance_level": 0.05
}
```

---

### loki_kriging
**Purpose:** Priestorová/časová interpolácia pomocou Kriging metód.
**Dependencies:** loki_core only

**Functionality:**
- **Simple Kriging (SK):** predpokladá známy priemer mu. Vhodné keď je
  stacionarita zaručená a priemer odhadnutý z dlhých radov.
- **Ordinary Kriging (OK):** neznámy lokálny priemer -- najčastejšie používaný.
  Podmienka: suma váh = 1. Vhodné pre väčšinu praktických aplikácií.
- **Universal Kriging (UK):** priemer je polynomický trend. Vhodné keď je
  v dátach deterministický trend (napr. výšková závislosť teploty).
- **Variogram modelovanie:**
  - Empirický variogram: gamma(h) = 0.5 * mean[(Z(x+h) - Z(x))^2]
  - Teoretické modely: sférický, exponenciálny, Gaussovský, nugget, power
  - Fitting: WLS (weighted least squares) na empirický variogram
  - Nugget efekt: diskontinuita pri h=0 (merací šum + mikrovariabilita)
- **Cross-validácia:** leave-one-out -- pre každý bod odhadni hodnotu z okolia,
  porovnaj s meranou. RMSE a standardized residuals ako diagnostika.
- **Kriging variance:** automatický výstup -- udáva neistotu odhadu v každom bode.

**Key domain knowledge:**
- Variogram fitting je najcitlivejší krok -- zlý variogram -> zlé predikcie.
- Sill = celková variancia procesu (asymptota variogramu).
- Range = vzdialenosť kde variogram dosahuje sill (dosah priestorovej korelácie).
- Pre GNSS siete: interpolácia troposférického oneskorenia medzi stanicami.
- Pre klimatologické dáta: priestorová interpolácia medzi meraniami.
- Anizotropia: variogram závisí od smeru (napr. horizontálne vs vertikálne).
  Základná implementácia predpokladá izotropiu.

**Config pattern:**
```json
"kriging": {
    "method": "ordinary",      // "simple" | "ordinary" | "universal"
    "variogram_model": "spherical",  // "spherical" | "exponential" | "gaussian" | "power"
    "nugget": 0.0,             // 0 = fit automaticky
    "sill": 0.0,               // 0 = fit automaticky
    "range": 0.0,              // 0 = fit automaticky
    "trend_degree": 1,         // pre universal kriging
    "cross_validate": true,
    "known_mean": 0.0,         // pre simple kriging
    "significance_level": 0.05
}
```

---

### loki_spline
**Purpose:** Pokročilá spline interpolácia a aproximácia -- rozšírenie nad rámec
cubic spline ktorý je v loki_core/math/spline.hpp.
**Dependencies:** loki_core only

**Functionality:**
- **Cubic spline:** základ existuje v `loki_core/math/spline.hpp` (CubicSpline,
  BoundaryCondition: NATURAL / NOT_A_KNOT / CLAMPED). Táto aplikácia ho exponuje
  priamo ako samostatný analytický nástroj.
- **B-spline:** basis spline s kontrolnými bodmi. Nekopíruje dáta -- aproximuje.
  Stupeň k (typicky 3 = cubic). Knot vector určuje tvar krivky.
  Výhoda oproti interpolačnému spline: hladší výsledok, menej citlivý na šum.
- **NURBS (Non-Uniform Rational B-Splines):** rozšírenie B-spline o váhy.
  Umožňuje presné reprezentácie kužeľosečiek (kruh, elipsa, parabola).
  Relevantné pre rekonštrukciu trajektórie vlaku/vozidla zo senzorových dát
  kde krivky obsahujú oblúky a prechodnice.
- **Knot placement stratégie:** uniformné, chord-length parametrizácia,
  centripetal parametrizácia (lepšia pre krivky s veľkými zmenami smeru).
- **Smoothing spline:** minimalizácia kompromisu medzi fitom a hladkosťou
  (lambda parameter). Vhodné pre zašumené senzorové dáta.

**Key domain knowledge:**
- Pre train/vehicle senzorové dáta (1000 Hz): rekonštrukcia plynulej trajektórie
  z diskrétnych meraní polohy/rýchlosti/zrýchlenia.
- NURBS má zmysel ak trajektória obsahuje geometricky definované úseky
  (oblúky konštantného polomeru, prechodnice).
- B-spline aproximácia preferovaná pred interpoláciou pre zašumené dáta --
  interpolácia prechádza každým bodom vrátane šumových špičiek.
- SplineFilter v loki_filter už používa cubic spline z loki_core/math.
  loki_spline je samostatná aplikácia zameraná na analýzu a vizualizáciu
  spline modelov, nie len filtrovanie.
- NURBS implementácia: de Boor algoritmus pre evaluáciu, homogénne súradnice
  pre racionálne váhy.

**Config pattern:**
```json
"spline": {
    "method": "cubic",         // "cubic" | "bspline" | "nurbs" | "smoothing"
    "degree": 3,               // stupeň B-spline / NURBS
    "bc": "not_a_knot",        // pre cubic: "natural" | "not_a_knot" | "clamped"
    "knot_placement": "chord_length",  // "uniform" | "chord_length" | "centripetal"
    "smoothing_lambda": 0.0,   // 0 = interpolacia, >0 = smoothing spline
    "n_control_points": 0,     // 0 = auto (= n/10)
    "gap_fill_strategy": "linear",
    "gap_fill_max_length": 0
}
```