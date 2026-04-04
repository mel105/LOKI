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
                `stationarity` | `arima` | `ssa` | `decomposition`
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

loki_decomposition    (depends on loki_core only)                  <- NEXT
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
|   +-- loki_decomposition/              <- NEXT
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
|   +-- loki_decomposition/              <- NEXT
|       +-- CMakeLists.txt
|       +-- include/loki/decomposition/
|       |   +-- decompositionResult.hpp
|       |   +-- classicalDecomposer.hpp/.cpp
|       |   +-- stlDecomposer.hpp/.cpp
|       |   +-- decompositionAnalyzer.hpp/.cpp
|       |   +-- plotDecomposition.hpp/.cpp
|       +-- src/
+-- tests/
+-- config/
|   +-- outlier.json
|   +-- regression.json
|   +-- stationarity.json
|   +-- arima.json
|   +-- ssa.json
|   +-- decomposition.json               <- NEXT
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
./scripts/loki.sh build ssa --copy-dlls
./scripts/loki.sh run ssa
./scripts/loki.sh test --rebuild
```

### CMake target name collision
Executable: `loki_ssa_app` with `OUTPUT_NAME "loki_ssa"`.
Same pattern for all other apps: `loki_decomposition_app` with `OUTPUT_NAME "loki_decomposition"`.

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
                  `SsaReconstructionConfig`
- `configLoader.hpp / .cpp`
- `timeStamp.hpp / .cpp`
- `timeSeries.hpp / .cpp`
- `gapFiller.hpp / .cpp`
- `deseasonalizer.hpp / .cpp`
- `medianYearSeries.hpp / .cpp`
- `descriptive.hpp / .cpp`
- `filter.hpp / .cpp` -- MovingAverageFilter, EmaFilter, KernelFilter, LoessFilter,
                          SavitzkyGolayFilter, WeightedMovingAverageFilter
- `distributions.hpp / .cpp`
- `hypothesis.hpp / .cpp`
- `metrics.hpp / .cpp`
- `wCorrelation.hpp / .cpp` -- w-correlation matrix for SSA
- `loader.hpp / .cpp`, `dataManager.hpp / .cpp`
- `gnuplot.hpp / .cpp`
- `plot.hpp / .cpp`

### loki_core/math -- complete
- `lsqResult.hpp`
- `lsq.hpp / .cpp`
- `designMatrix.hpp / .cpp`
- `hatMatrix.hpp / .cpp`
- `svd.hpp` -- header-only (BDCSVD linking bug workaround)
- `lm.hpp / .cpp`
- `lagMatrix.hpp / .cpp`
- `embedMatrix.hpp / .cpp` -- Hankel trajectory matrix for SSA
- `randomizedSvd.hpp / .cpp` -- Halko et al. (2011) randomized truncated SVD

### loki_outlier -- complete (O1-O4)
### loki_homogeneity -- complete
### loki_filter -- complete
### loki_regression -- complete (R1-R8, including nonlinear/LM)
### loki_stationarity -- complete
### loki_arima -- complete

### loki_ssa -- complete (S1-S5)
- `embedMatrix.hpp/.cpp` in `loki_core/math/`
- `wCorrelation.hpp/.cpp` in `loki_core/stats/`
- `randomizedSvd.hpp/.cpp` in `loki_core/math/`
- `ssaResult.hpp` -- `SsaGroup`, `SsaResult`
- `ssaGrouper.hpp/.cpp` -- manual, wcorr (average linkage), kmeans, variance methods
- `ssaReconstructor.hpp/.cpp` -- diagonal averaging + simple (public API, not used in pipeline)
- `ssaAnalyzer.hpp/.cpp` -- orchestrator: window resolution, trajectory matrix,
                             randomized SVD / full eigendecomposition, vectorized
                             diagonal averaging, w-correlation, grouping, reconstruction
- `plotSsa.hpp/.cpp` -- scree plot, w-corr heatmap, stacked component multiplot,
                         reconstruction overlay
- `apps/loki_ssa/main.cpp` -- full pipeline

---

## NEXT THREAD: loki_decomposition

### What classical decomposition is
Additive decomposition of a time series into three components:
```
Y[t] = T[t] + S[t] + R[t]
```
where T = trend, S = seasonal, R = residual.

Two methods planned:
- **Classical**: MovingAverage trend + MedianYear seasonal (fast, simple)
- **STL** (Cleveland et al. 1990): LOESS trend + LOESS-smoothed seasonal,
  iterative with robustness weights (accurate, handles outliers)

### Relation to existing modules
- **vs SSA**: SSA is non-parametric and data-driven; decomposition assumes
  a fixed additive structure with a known period. Both complement each other.
- **vs loki_filter**: `LoessFilter` and `MovingAverageFilter` from `loki_core`
  are reused directly -- no new core additions needed for Classical.
- **vs Deseasonalizer**: `Deseasonalizer` in `loki_core` does Y-S only and
  does not return T separately. `DecompositionAnalyzer` supersedes it for
  the decomposition use case.

### Key open question before implementation
- Does `LoessFilter` in `loki_core/stats/filter.hpp` support **external
  robustness weights**? STL requires weighted LOESS in the outer iteration.
  If not, we need to either extend `LoessFilter` or implement a private
  weighted LOESS inside `stlDecomposer.cpp`.

### Planned module files
```
libs/loki_decomposition/include/loki/decomposition/
    decompositionResult.hpp      -- DecompositionResult {trend, seasonal, residual, method}
    classicalDecomposer.hpp/.cpp -- MA trend + per-slot seasonal mean/median
    stlDecomposer.hpp/.cpp       -- STL: iterative LOESS trend + seasonal
    decompositionAnalyzer.hpp/.cpp -- orchestrator: gap fill, decompose, log, output
    plotDecomposition.hpp/.cpp   -- 3-panel: original+trend, seasonal, residual
```

### DecompositionConfig design (to add to config.hpp)
```cpp
enum class DecompositionMethodEnum { CLASSICAL, STL };

struct ClassicalDecompositionConfig {
    std::string trendFilter  {"moving_average"}; // "moving_average" only for now
    std::string seasonalType {"median"};          // "mean" | "median"
};

struct StlDecompositionConfig {
    int    nInner      {2};    // inner loop iterations
    int    nOuter      {1};    // outer loop (robustness) iterations
    int    sDegree     {1};    // LOESS degree for seasonal smoother
    int    tDegree     {1};    // LOESS degree for trend smoother
    double sBandwidth  {0.0};  // 0 = auto (1/period)
    double tBandwidth  {0.0};  // 0 = auto (based on n and period)
};

struct DecompositionConfig {
    std::string                  gapFillStrategy  {"linear"};
    int                          gapFillMaxLength {0};
    DecompositionMethodEnum      method  {DecompositionMethodEnum::CLASSICAL};
    int                          period  {1461};  // in samples
    ClassicalDecompositionConfig classical{};
    StlDecompositionConfig       stl     {};
    double                       significanceLevel{0.05};
};
```

### PlotDecomposition -- planned plots
| Plot | Flag | Description |
|---|---|---|
| Overlay | `decompOverlay` | Original + trend overlaid |
| 3-panel | `decompPanels` | Trend / Seasonal / Residual stacked panels |
| Residual diagnostics | `decompDiagnostics` | Delegate to `loki::Plot::residualDiagnostics()` |

### Files to request at loki_decomposition thread start
```
libs/loki_core/include/loki/stats/filter.hpp      <- LoessFilter signature + weight support?
libs/loki_core/src/stats/filter.cpp               <- first ~80 lines (LoessFilter structure)
libs/loki_core/include/loki/timeseries/deseasonalizer.hpp  <- what already exists
libs/loki_core/include/loki/timeseries/medianYearSeries.hpp <- valueAt() signature
libs/loki_core/include/loki/core/config.hpp       <- current state (post-SSA)
libs/loki_core/src/core/configLoader.cpp          <- last 80 lines (_parseSsa pattern)
libs/loki_arima/CMakeLists.txt                    <- CMake pattern
apps/loki_arima/CMakeLists.txt                    <- app CMake pattern
```

---

## Known Issues and Workarounds

### GCC 13 aggregate-init bug -- CRITICAL
Any `Config` struct with enum member + default value CANNOT be a nested struct inside
a class. Define outside with descriptive name, add `using` alias inside class.

### Eigen BDCSVD linking bug -- CRITICAL
`Eigen::BDCSVD` causes `undefined reference` errors when used in `.cpp` files compiled
into static libraries on Windows/GCC. Workarounds in place:
1. `SvdDecomposition` (`svd.hpp`) is **header-only**.
2. `CalibrationRegressor` uses `Eigen::JacobiSVD` directly.
3. `SsaAnalyzer` uses `Eigen::JacobiSVD` inside `randomizedSvd.cpp` (small matrix,
   safe) and `SelfAdjointEigenSolver` for the full eigendecomposition path.
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
- All module-specific plotters use local `fwdSlash()` helper (static private method)
- Font: `'Sans,12'` (not `'Helvetica,12'`)
- Terminal: `noenhanced`

### SSA performance -- key learnings
- JacobiSVD on K x L trajectory matrix (K=35000, L=1461) takes hours -- never use
- SelfAdjointEigenSolver on C = X^T*X (L x L): X^T*X multiplication is O(K*L^2),
  also too slow for L=1461
- **Solution**: randomized SVD (Halko et al. 2011) in `randomizedSvd.hpp/.cpp`
  computes first k singular vectors in O(K*L*k) -- seconds for k=40
- Diagonal averaging: O(n*L*r) -- vectorized via Eigen segment dot products
- W-correlation: O(r^2*n) -- limit via `wcorr_max_components` (default 30)
- For 6h climatological data (n=36524): L=365, k=40 -> ~2.5 min total

### SsaConfig fields (post-SSA thread additions)
```
svd_rank            int   40   -- randomized SVD rank (0 = full eigendecomposition)
svd_oversampling    int   10   -- extra columns for accuracy
svd_power_iter      int    2   -- subspace power iterations
wcorr_max_components int  30   -- limit w-corr computation to first N components
```

### `distributions.hpp` namespace
Functions are in `loki::stats::` -- e.g. `loki::stats::chi2Quantile(p, df)`.

### PP test -- critical implementation note
The correction term uses `XtX(levelCol, levelCol)` -- NOT `XtXinv`. Confirmed.

### lagMatrix.cpp / embedMatrix.cpp / wCorrelation.cpp / randomizedSvd.cpp
All must be listed in `libs/loki_core/CMakeLists.txt` sources.

### loki.sh -- app registration
Register new apps in the APP_EXE map: `["decomposition"]="loki_decomposition_app"`

---

## Config Structs (current state)

### SsaConfig (config.hpp) -- complete
```cpp
struct SsaWindowConfig {
    int windowLength{0}; int period{0}; int periodMultiplier{2}; int maxWindowLength{20000};
};
struct SsaGroupingConfig {
    std::string method{"wcorr"}; double wcorrThreshold{0.8}; int kmeansK{0};
    double varianceThreshold{0.95};
    std::map<std::string, std::vector<int>> manualGroups;
};
struct SsaReconstructionConfig { std::string method{"diagonal_averaging"}; };
struct SsaConfig {
    std::string gapFillStrategy{"linear"}; int gapFillMaxLength{0};
    DeseasonalizationConfig deseasonalization{};
    SsaWindowConfig window{}; SsaGroupingConfig grouping{};
    SsaReconstructionConfig reconstruction{};
    bool computeWCorr{true};
    int svdRank{40}; int svdOversampling{10}; int svdPowerIter{2};
    int wcorrMaxComponents{30};
    double significanceLevel{0.05}; int forecastTail{1461};
};
```

### PlotConfig SSA additions (config.hpp) -- complete
```cpp
bool ssaScree         {true};
bool ssaWCorr         {true};
bool ssaComponents    {true};
bool ssaReconstruction{true};
```

---

## Statistical / Domain Key Learnings

- SSA window L should be a multiple of the dominant period for clean separation
- SSA grouping: eigentriples belonging to the same oscillation appear as pairs
  with nearly equal eigenvalues -- w-correlation identifies these correctly
- For ms-resolution data: always set `maxWindowLength` explicitly
- Draconitic period for GNSS: 351.4 days (not 365.25)
- For 6h climatological data: L=365 (quarter-year) is a good starting point;
  L=1461 (1 year) gives better separation but is slower
- Variance fractions from randomized SVD are relative to captured variance
  (sum of first r eigenvalues), not total variance of the series
- Classical decomposition period in samples: 1461 for 6h data, 365 for daily,
  8760 for hourly

---

## Module Roadmap (priority order)

| Priority | Module | Primary use case | Status |
|---|---|---|---|
| 1 | `loki_arima` | Climatological residual modelling, forecasting | COMPLETE |
| 2 | `loki_ssa` | SSA decomposition: trend + oscillations + noise | COMPLETE |
| 3 | `loki_decomposition` | Classical + STL trend/seasonal/residual decomposition | NEXT |
| 4 | `loki_kalman` | GNSS processing, train sensor fusion | planned |
| 5 | `loki_spectral` | FFT + Lomb-Scargle for period identification | planned |
| 6 | `loki_clustering` | Train phase segmentation (DBSCAN, k-means) | planned |
| 7 | `loki_qc` | Quality control, gap flagging, automated reporting | planned |

---

## Approach & Patterns

- **Thread-based workflow:** Each conversation handles a specific milestone.
  Always begins with "Working on LOKI -- see CLAUDE.md" + attached CLAUDE.md +
  relevant source files.
- **Design-first:** Discuss architecture and approve signatures before any implementation.
- **Iterative debugging:** Build errors shared in chunks; systematic resolution.
- **Documentation artifacts:** Each thread concludes with updated CLAUDE.md and
  CONFIG_REFERENCE.md section.
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
                  `stationarity` | `arima` | `ssa` | `decomposition`
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
- `loki_ssa` CMake: library `loki_ssa`, executable `loki_ssa_app`
  with `OUTPUT_NAME "loki_ssa"`.
- `loki_decomposition` CMake: library `loki_decomposition`, executable
  `loki_decomposition_app` with `OUTPUT_NAME "loki_decomposition"`.
- `window` parameters in filters and SSA are in samples, not time units.
- `SsaReconstructor` exists as public API but is not used in the main pipeline --
  `SsaAnalyzer` performs reconstruction inline for efficiency.
- Classical decomposition and SSA are separate applications serving different
  mathematical purposes -- do not merge them.