# CLAUDE.md -- Project Instructions for LOKI

This file contains instructions for Claude when working on the LOKI project.
It should be placed in the root of the repository and kept up to date.

---

## Project Overview

LOKI is a professional C++ toolkit for statistical analysis of time series data.
The long-term goal is a modular ecosystem of analysis libraries with a GUI/IDE frontend.
Current focus: outlier detection (`loki_outlier`), homogeneity testing (`loki_homogeneity`),
and signal filtering (`loki_filter`).

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
- In `apps/` (main), use fully qualified `loki::LOKIException` in catch blocks.
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

---

## Plot Output Naming Convention

All plot output files follow this naming convention:
```
[program]_[dataset]_[parameter]_[plottype].[format]
```

- **program** : `core` | `homogeneity` | `outlier` | `filter`
- **dataset** : input filename stem without extension (e.g. `CLIM_DATA_EX1`)
- **parameter** : series `componentName` from metadata (e.g. `col_3`, `dN`, `temperature`)
- **plottype** : descriptive short name (e.g. `original`, `filtered`, `residuals`, `overlay`)
- **format** : `png` | `eps` | `svg`

### Rules for plotters
- `loki::Plot` (loki_core) uses prefix `core_`.
- `PlotHomogeneity` (loki_homogeneity) uses prefix `homogeneity_`.
- `PlotOutlier` (loki_outlier) uses the `programName` constructor argument.
- `PlotFilter` (loki_filter) uses prefix `filter_`.
- Temporary data files use `.tmp_` prefix and are deleted immediately after gnuplot runs.
- Gnuplot on Windows requires forward slashes -- use `fwdSlash()` helper on all paths.
- gnuplot terminal: `pngcairo noenhanced font 'Sans,12'`

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
```

### Rules
- Every module is a separate CMake library with its own `include/` and `src/` tree.
- Include paths follow `#include <loki/<module>/<class>.hpp>`.
- No circular dependencies between modules.
- `loki_core` must not depend on any other loki module.
- `loki_filter` depends on `loki_core` only -- no dependency on `loki_outlier` or `loki_homogeneity`.

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
|   +-- loki/
|   +-- loki_homogeneity/
|   |   +-- CMakeLists.txt
|   |   +-- main.cpp
|   +-- loki_outlier/
|   |   +-- CMakeLists.txt
|   |   +-- main.cpp
|   +-- loki_filter/                 <- F6: to be created
|       +-- CMakeLists.txt
|       +-- main.cpp
+-- libs/
|   +-- loki_core/
|   |   +-- include/loki/
|   |       +-- core/         (exceptions, version, config, configLoader, logger, nanPolicy)
|   |       +-- timeseries/   (timeSeries, timeStamp, gapFiller, deseasonalizer, medianYearSeries)
|   |       +-- stats/        (descriptive, filter)
|   |       +-- io/           (loader, dataManager, gnuplot, plot)
|   |       +-- math/         (lsqResult, lsq, designMatrix)    <- C1: NEW
|   +-- loki_outlier/
|   +-- loki_homogeneity/
|   +-- loki_filter/                 <- F1-F5: scaffold + implementations complete
|       +-- CMakeLists.txt
|       +-- include/loki/filter/
|       |   +-- filter.hpp               <- abstract base class
|       |   +-- filterResult.hpp         <- FilterResult struct
|       |   +-- movingAverageFilter.hpp  <- F2
|       |   +-- emaFilter.hpp            <- F2
|       |   +-- weightedMovingAverageFilter.hpp  <- F2
|       |   +-- kernelSmoother.hpp       <- F3a
|       |   +-- loessFilter.hpp          <- F3b
|       |   +-- savitzkyGolayFilter.hpp  <- F4
|       |   +-- filterWindowAdvisor.hpp  <- F5
|       +-- src/
|           +-- movingAverageFilter.cpp
|           +-- emaFilter.cpp
|           +-- weightedMovingAverageFilter.cpp
|           +-- kernelSmoother.cpp
|           +-- loessFilter.cpp
|           +-- savitzkyGolayFilter.cpp
|           +-- filterWindowAdvisor.cpp
+-- tests/
|   +-- CMakeLists.txt
|   +-- unit/
|   |   +-- core/
|   |   +-- homogeneity/
|   |   +-- filter/
|   |       +-- test_filters.cpp     <- F2+F3a tests written, F3b+F4+F5 tests pending
+-- config/
|   +-- homogenization.json
|   +-- outlier.json
|   +-- filter.json                  <- F6: to be created
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
# Build all apps (debug)
./scripts/loki.sh build all --copy-dlls

# Build specific app
./scripts/loki.sh build filter --copy-dlls

# Run
./scripts/loki.sh run filter

# Test
./scripts/loki.sh test --rebuild
```

### CMake target name collision
`loki_outlier` is already the library target name. The executable target must be
named `loki_outlier_app` with `OUTPUT_NAME "loki_outlier"` set via `set_target_properties`.
Same pattern applies to `loki_filter` app if needed.

### Runtime DLLs (Windows)
Three DLLs must be present next to every executable:
`libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`
`--copy-dlls` flag in `loki.sh` handles this automatically.

---

## Implemented Modules

### loki_core -- complete
- `exceptions.hpp` -- full hierarchy
- `version.hpp` -- `0.1.0`
- `nanPolicy.hpp` -- `NanPolicy { THROW, SKIP, PROPAGATE }`
- `logger.hpp / .cpp` -- Meyers singleton, file + stdout/stderr, macros
- `config.hpp` -- all config structs including `PlotOptionsConfig`
- `configLoader.hpp / .cpp` -- JSON loader, path resolution
- `timeStamp.hpp / .cpp` -- MJD internal, GPS/UTC/Unix conversions
- `timeSeries.hpp / .cpp` -- Observation, SeriesMetadata, append, slice, indexOf
- `gapFiller.hpp / .cpp` -- LINEAR, FORWARD_FILL, MEAN, NONE, MEDIAN_YEAR strategies
- `deseasonalizer.hpp / .cpp` -- MOVING_AVERAGE, MEDIAN_YEAR, NONE
- `medianYearSeries.hpp / .cpp` -- median annual profile
- `descriptive.hpp / .cpp` -- mean, median, variance, IQR, MAD, ACF, Hurst, summarize()
- `filter.hpp / .cpp` -- movingAverage, EMA, WMA (free functions, used by Deseasonalizer)
- `loader.hpp / .cpp`, `dataManager.hpp / .cpp`
- `gnuplot.hpp / .cpp` -- RAII gnuplot pipe wrapper
- `plot.hpp / .cpp` -- timeSeries, histogram, ACF, QQ, boxplot, comparison

### loki_core/math -- NEW (C1, this thread)
- `lsqResult.hpp` -- LsqResult struct: coefficients, residuals, sigma0, vTPv, cofactorX, dof, converged
- `lsq.hpp / .cpp` -- LsqSolver: static solve(), weighted + IRLS (HUBER, BISQUARE)
- `designMatrix.hpp / .cpp` -- DesignMatrix: polynomial(), harmonic(), identity()

### loki_outlier -- complete
### loki_homogeneity -- complete

### loki_filter -- F1-F5 complete (this thread)

#### Filter base (F1)
- `filter.hpp` -- abstract base: `apply(TimeSeries) -> FilterResult`, `name() -> string`
- `filterResult.hpp` -- `FilterResult { filtered, residuals, filterName }`

#### F2 -- MA/EMA/WMA wrappers
All three wrap existing `loki::stats::` free functions. No logic duplication.
Edge NaN (from centered MA/WMA) filled with nearest-neighbour (ffill left, bfill right).
EMA is causal -- no edge NaN produced.

- `movingAverageFilter.hpp / .cpp` -- wraps `stats::movingAverage()`
- `emaFilter.hpp / .cpp` -- wraps `stats::exponentialMovingAverage()`
- `weightedMovingAverageFilter.hpp / .cpp` -- wraps `stats::weightedMovingAverage()`

#### F3a -- KernelSmoother
- `kernelSmoother.hpp / .cpp`
- Kernels: EPANECHNIKOV, GAUSSIAN, UNIFORM, TRIANGULAR
- `bandwidth` as fraction of series length (not absolute samples) -- sampling-rate agnostic
- O(n * w) complexity -- no matrices, no O(n^2)
- GAUSSIAN truncated at `gaussianCutoff * h` samples

#### F3b -- LoessFilter
- `loessFilter.hpp / .cpp`
- k-nearest-neighbours (not fixed symmetric window) -- no edge NaN, no fill needed
- Kernels: TRICUBE (default/classic), EPANECHNIKOV, GAUSSIAN
- degree: 1 (linear) or 2 (quadratic) -- ConfigException otherwise
- Robust option: IRLS with BISQUARE weight function (standard for LOESS)
- Uses `LsqSolver` + `DesignMatrix` from `loki_core/math`
- Slower than KernelSmoother -- not recommended for ms-resolution data

#### F4 -- SavitzkyGolayFilter
- `savitzkyGolayFilter.hpp / .cpp`
- Convolution coefficients precomputed once in constructor via hat matrix
- Edge coefficients precomputed per edge position -- no NaN, no fill needed
- Interior: pure convolution O(n * w)
- Uses `LsqSolver` + `DesignMatrix` from `loki_core/math`
- Recommended for ms/high-frequency data (fast, preserves peak shapes)

#### F5 -- FilterWindowAdvisor
- `filterWindowAdvisor.hpp / .cpp`
- Three estimation methods:
  - `SILVERMAN_MAD` (default): `sigma = MAD/0.6745`, `bw = sigma * (4/(3n))^0.2`
  - `SILVERMAN`: `bw = 0.9 * min(std, IQR/1.34) * n^(-0.2)`
  - `ACF_PEAK`: first local maximum in ACF -> estimated period -> window
- Uses `stats::mad()`, `stats::stddev()`, `stats::iqr()`, `stats::acf()` -- no duplication
- Output always odd integer >= minWindow (3 by default)
- `Advice::rationale` string for logging

#### Unit tests (partial)
- `tests/unit/filter/test_filters.cpp` -- covers F2 (MA, EMA, WMA) and F3a (KernelSmoother)
- F3b (LOESS), F4 (SG), F5 (Advisor) tests: **pending** (to be added in F7 thread)

---

## loki_filter -- Pending Work

### F6 -- apps/loki_filter + config/filter.json (NEXT THREAD)

Pipeline in main:
```
load -> GapFiller -> (optional: FilterWindowAdvisor) -> Filter::apply() -> plot -> CSV export
```

Filter method selectable via JSON: `moving_average | ema | weighted_ma | kernel | loess | savitzky_golay`

#### JSON config structure (draft for F6)
```json
{
  "workspace": "...",
  "input":     { "file": "DATA.txt" },
  "output":    { "dir": "OUTPUT" },
  "gap_filler": {
    "strategy": "linear"
  },
  "filter": {
    "method": "kernel",
    "auto_window": false,
    "auto_window_method": "silverman_mad",
    "moving_average": { "window": 365 },
    "ema":            { "alpha": 0.1 },
    "weighted_ma":    { "weights": [1.0, 2.0, 3.0, 2.0, 1.0] },
    "kernel": {
      "bandwidth": 0.1,
      "kernel_type": "epanechnikov",
      "gaussian_cutoff": 3.0
    },
    "loess": {
      "bandwidth": 0.25,
      "degree": 1,
      "kernel_type": "tricube",
      "robust": false,
      "robust_iterations": 3
    },
    "savitzky_golay": {
      "window": 11,
      "degree": 2
    }
  },
  "plots": {
    "original":  true,
    "filtered":  true,
    "residuals": true,
    "overlay":   true,
    "plot_options": {
      "acf_max_lag": 100,
      "histogram_bins": 40
    }
  },
  "stats": { "compute_hurst": false }
}
```

When `auto_window: true`, `FilterWindowAdvisor` is called first and its result
overrides the manual window/bandwidth setting. `auto_window_method` selects the
estimation strategy (`silverman_mad` | `silverman` | `acf_peak`).

#### What Claude needs for F6 thread
Paste CLAUDE.md + provide:
- `config.hpp` (AppConfig struct, to know how to extend it with FilterConfig)
- `configLoader.cpp` (to know JSON parsing patterns used in the project)
- `apps/loki_outlier/main.cpp` (as pipeline reference -- structure to follow)
- `loki.sh` (to add filter build target)

### F7 -- PlotFilter + integration + remaining tests (after F6)

- `PlotFilter` class in `loki_filter`, analogous to `PlotOutlier`
- Plots: original vs filtered overlay, residuals, (optional) ACF of residuals
- Remaining unit tests: F3b (LOESS), F4 (SG), F5 (Advisor)
- Integration test on 6h climatological data
- Integration test on ms-resolution data

#### What Claude needs for F7 thread
Paste CLAUDE.md + provide:
- `plotOutlier.hpp / .cpp` (as PlotFilter reference)
- `plot.hpp` (core plot functions available)
- `gnuplot.hpp` (GnuplotWriter API)

---

## Planned Future Work

### SplineFilter (after F7)
Cubic smoothing spline as an alternative smoother. Minimises:
  integral[ (f''(x))^2 dx ] + lambda * sum[ (y_i - f(x_i))^2 ]
- Lambda controls smoothing vs. fidelity tradeoff
- To be added to `loki_filter` as `SplineFilter`
- Can also serve as a `GapFiller` interpolation strategy (`SPLINE` option)
- Implement after `loki_filter` is stable; revisit when working on `loki_kalman` or `loki_regression`

### loki_kalman
- Standalone module, NOT a wrapper in `loki_filter`
- Focus: state prediction and smoothing, not just filtering
- LSQ from `loki_core/math` used for state initialisation

---

## Key Architecture Decisions

### loki_core/math (C1)
LSQ solver reused across: `loki_filter` (LOESS F3b, SG F4), `loki_regression`,
`loki_homogeneity` (optional trend removal), `loki_kalman` (init).
- `LsqSolver` is stateless -- all config per call via `Config` struct
- `DesignMatrix` is a static factory -- no instantiation
- Weighted LSQ: diagonal weight matrix as `Eigen::VectorXd`
- Robust LSQ: IRLS with HUBER or BISQUARE weight function
- Solved via `ColPivHouseholderQR` -- numerically stable

### Filter edge handling summary
| Filter              | Edge strategy                        |
|---------------------|--------------------------------------|
| MovingAverageFilter | nearest-neighbour fill (ffill/bfill) |
| EmaFilter           | none needed (causal)                 |
| WeightedMAFilter    | nearest-neighbour fill (ffill/bfill) |
| KernelSmoother      | nearest-neighbour fill (ffill/bfill) |
| LoessFilter         | none needed (k-NN shifts at edges)   |
| SavitzkyGolayFilter | precomputed asymmetric edge coeffs   |

### Filter complexity summary
| Filter              | Complexity  | Recommended for         |
|---------------------|-------------|-------------------------|
| MovingAverageFilter | O(n * w)    | any, fast               |
| EmaFilter           | O(n)        | streaming, ms data      |
| WeightedMAFilter    | O(n * w)    | any, fast               |
| KernelSmoother      | O(n * w)    | climatological, GNSS    |
| LoessFilter         | O(n * w * p^2) | climatological only  |
| SavitzkyGolayFilter | O(n * w)    | ms/high-frequency data  |

### bandwidth parameter convention
All filters using a window expressed as a fraction use `bandwidth in (0, 1]`
meaning `k = ceil(bandwidth * n)` samples. This is sampling-rate agnostic.
`FilterWindowAdvisor` outputs both `windowSamples` and `bandwidth` for convenience.

### Deseasonalizer and MedianYearSeries in loki_core
Both moved from `loki_homogeneity` to `loki_core/timeseries/`.
Namespace: `loki`. Include: `<loki/timeseries/deseasonalizer.hpp>`.

### loki_filter pipeline responsibility
- `GapFiller` handles NaN in the input series (pipeline / main)
- `Filter::apply()` handles only edge NaN produced by the filter itself
- Filters assume clean (NaN-free) input -- DataException propagates to caller

---

## Config Structs (config.hpp)

### FilterConfig (to be added in F6)
```cpp
enum class FilterMethod {
    MOVING_AVERAGE, EMA, WEIGHTED_MA, KERNEL, LOESS, SAVITZKY_GOLAY
};

struct MovingAverageFilterConfig { int window{365}; };
struct EmaFilterConfig           { double alpha{0.1}; };
struct WeightedMaFilterConfig    { std::vector<double> weights; };

struct KernelFilterConfig {
    double bandwidth{0.1};
    std::string kernelType{"epanechnikov"}; // epanechnikov|gaussian|uniform|triangular
    double gaussianCutoff{3.0};
};

struct LoessFilterConfig {
    double bandwidth{0.25};
    int    degree{1};
    std::string kernelType{"tricube"};     // tricube|epanechnikov|gaussian
    bool   robust{false};
    int    robustIterations{3};
};

struct SavitzkyGolayFilterConfig {
    int window{11};
    int degree{2};
};

struct FilterConfig {
    FilterMethod method{FilterMethod::KERNEL};
    bool         autoWindow{false};
    std::string  autoWindowMethod{"silverman_mad"}; // silverman_mad|silverman|acf_peak
    MovingAverageFilterConfig movingAverage;
    EmaFilterConfig           ema;
    WeightedMaFilterConfig    weightedMa;
    KernelFilterConfig        kernel;
    LoessFilterConfig         loess;
    SavitzkyGolayFilterConfig savitzkyGolay;
};
```

---

## Known Issues and Workarounds

### Unicode in source files (Windows GCC)
GCC on Windows misparsed tokens when source files contain UTF-8 box-drawing
characters in comments. **Rule: all comments ASCII only.**

### exceptions.hpp must be included early
`logger.hpp` includes `exceptions.hpp`. Including `logger.hpp` is sufficient.

### namespace loki in .cpp files
All library `.cpp` files add `using namespace loki;` after the last `#include`.

### DLL mismatch on Windows (exit code 0xc0000139)
Three DLLs must be present next to every executable:
`libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`.

### TimeStamp construction from MJD
Use `TimeStamp::fromMjd(double)` -- there is no `setMjd()` method.

### ::TimeStamp in loki namespace code
`TimeStamp` is not in namespace `loki`. Always qualify as `::TimeStamp`.

### Config struct with enum member -- GCC 13 aggregate init
Use explicit constructor with member-initializer list.

### CMake target name collision
`loki_outlier` is already the static library. The app executable target must be
named `loki_outlier_app` with `OUTPUT_NAME "loki_outlier"`.

### std::erase_if on Windows GCC
Use `vec.erase(std::remove_if(...), vec.end())` instead of `std::erase_if`.

### OutlierCleaner constructor order
`OutlierCleaner(Config cfg, const OutlierDetector& detector)` -- Config first, detector second.

### plot.cpp -- makeStem helper
`makeStem()` must be defined BEFORE the first `Plot::` method that uses it.

---

## Planned Modules

| Module | Description | Status |
|---|---|---|
| `loki_homogeneity` | Change point detection, SNHT, homogenization | complete |
| `loki_outlier` | IQR, MAD, Z-score, OutlierCleaner | complete |
| `loki_filter` | MA/EMA/WMA/Kernel/LOESS/SavitzkyGolay, FilterWindowAdvisor | F1-F5 done, F6-F7 next |
| `loki_decomposition` | Trend, seasonality, residuals (STL, classical) | planned |
| `loki_svd` | SVD/PCA decomposition | planned |
| `loki_kalman` | Kalman filter, smoother, EKF | planned |
| `loki_arima` | AR, ARMA, ARIMA, SARIMA | planned |
| `loki_spectral` | FFT, power spectrum, Lomb-Scargle | planned |
| `loki_stationarity` | ADF, KPSS, unit root tests | planned |
| `loki_clustering` | k-means, DBSCAN, Hungarian algorithm | planned |
| `loki_qc` | Quality control, gap filling, flagging | planned |
| `loki_regression` | Linear, polynomial, robust regression | planned |

---

## Notes and Reminders

- Data files and plot outputs are **not** committed to the repository.
- Third-party dependencies via CMake `FetchContent` only -- no vendored source.
- Build directory `build/` is gitignored.
- `newmat` replaced by `Eigen3` throughout.
- Do NOT add license/copyright blocks at the top of source files.
- gnuplot font: `'Sans,12'` (not `'Helvetica,12'`).
- gnuplot terminal: `'noenhanced'` (except boxplot stats panel which uses `enhanced`).
- `loader.hpp` is in `loki_core/io/`, NOT in `timeseries/`.
- `deseasonalizer.hpp` and `medianYearSeries.hpp` are in `loki_core/timeseries/`.
- `SeriesMetadata` must be populated by Loader after loading.
- Plot output -> `OUTPUT/IMG/`, temp files use `.tmp_` prefix.
- Time series input may have no periodic component (GNSS, non-climatological data).
- Sampling rate varies from milliseconds to 6 hours. Detectors must not assume any fixed time step.
- For ms data: MedianYearSeries throws ConfigException (resolution < 1h). Use MOVING_AVERAGE or KernelSmoother.
- MA window for deseasonalization must cover one full period.
- `loki_filter` free functions in `loki_core/stats/filter.hpp` are kept as-is (used by Deseasonalizer).
  `loki_filter` adds object-oriented wrappers -- no logic duplication.