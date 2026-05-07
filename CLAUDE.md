# CLAUDE.md -- Project Instructions for LOKI

This file contains instructions for Claude when working on the LOKI project.
It should be placed in the root of the repository and kept up to date.

---

## Project Overview

LOKI is a modular C++20 scientific data analysis framework.

The name originates from the Norse god of mischief -- the project began as a
toolkit for detecting false (inhomogeneous) events in climatological time series.
It has since grown into a general-purpose framework covering time series analysis,
spatial interpolation, multivariate statistics, geodetic computation, and GNSS
processing.

*LOKI: originally a detector of false events in observational data; now a
general-purpose toolkit for quantitative analysis of scientific datasets.*

Current domains:
- Time series analysis (1D): complete, see module inventory below.
- Spatial analysis (2D): complete.
- Multivariate analysis: complete -- loki_multivariate.
- Geodetic computations: complete -- loki_geodesy.
- GNSS processing: loki_gnss -- IN PROGRESS (parsers + SPP working, see below).

Planned domains (see Long-Term Roadmap section):
- Climatological GNSS analysis: loki_climatology
- Railway GNSS navigation: loki_gnss_rail
- Physical geodesy and gravity: loki_gravity
- Earth observation: loki_eo
- Seismology: loki_seismology

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

### CRITICAL -- always request current files
- Before diagnosing any bug or writing any patch, always ask for the current version
  of the relevant file from the user's repository.
- Never rely on memory of previously delivered code -- it may have been partially applied,
  modified, or contain already-fixed bugs.
- This rule prevents duplicate struct definitions, wrong patch targets, and wasted cycles.

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
  `namespace loki::spatial { }`, `namespace loki::gnss { }`, and any future sub-namespaces.

---

## Plot Output Naming Convention

All plot output files follow this naming convention:
```
[program]_[dataset]_[parameter]_[plottype].[format]
```

- **program** : `core` | `homogeneity` | `outlier` | `filter` | `regression` |
                `stationarity` | `arima` | `ssa` | `decomposition` | `spectral` |
                `kalman` | `qc` | `clustering` | `simulate` | `evt` | `kriging` |
                `spline` | `spatial` | `multivariate` | `geodesy` | `gnss` |
                `climatology` | `gnss_rail` | `gravity` | `eo` | `seismology`
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
- **Gnuplot `-persist` flag is REMOVED** from `gnuplot.cpp`. Do not add it back.
- **CRITICAL -- multiplot and inline data**: `plot '-'` inside `set multiplot` is NOT
  supported. Always use datablocks (`$name << EOD`) defined before `set multiplot`.
- **CRITICAL -- gnuplot heatmaps**: Never use `matrix rowheaders columnheaders` in
  `plot $data matrix using 1:2:3 with image` commands. Use `matrix using 1:2:3 with image`
  only, with axis labels set via `set xtics` / `set ytics` explicitly.
- **CRITICAL -- gnuplot user-defined functions via pipe**: Do NOT define gnuplot
  variables and then reference them in a function definition via separate `gp()` calls.
  Each `gp()` call is a separate `fputs` -- variables are not committed before the
  function definition is parsed on Windows/MinGW. Solution: compute model curves in
  C++ (e.g. 200 sample points) and send as a named datablock `$model << EOD ... EOD`.

---

## Library Architecture

### Project identity and domain scope
LOKI is no longer limited to time series. The framework covers:
- 1D time series analysis (complete)
- 2D spatial analysis (loki_spatial -- complete)
- Multivariate analysis (loki_multivariate -- complete)
- Geodetic computations (loki_geodesy -- complete)
- GNSS processing (loki_gnss -- in progress)
- Climatology, gravity, EO, seismology (planned)

### Architecture principle -- math primitives in loki_core
All reusable math primitives go into `loki_core/math/` (flat, no sub-directories).
Module libraries are thin orchestrators: pipeline, protocol, CSV, plots.
This enables all future modules to reuse the same math without cross-module dependencies.

### Dependency graph (complete + planned)
```
loki_core
    ^
    |
loki_outlier          (depends on loki_core only)                   <- COMPLETE
    ^
    |
loki_homogeneity      (depends on loki_core + loki_outlier)         <- COMPLETE
loki_filter           (depends on loki_core only)                   <- COMPLETE
loki_regression       (depends on loki_core only)                   <- COMPLETE
loki_stationarity     (depends on loki_core only)                   <- COMPLETE
loki_arima            (depends on loki_core + loki_stationarity)    <- COMPLETE
loki_ssa              (depends on loki_core only)                   <- COMPLETE
loki_decomposition    (depends on loki_core only)                   <- COMPLETE
loki_spectral         (depends on loki_core only)                   <- COMPLETE
loki_kalman           (depends on loki_core only)                   <- COMPLETE
loki_qc               (depends on loki_core + loki_outlier)         <- COMPLETE
loki_clustering       (depends on loki_core only)                   <- COMPLETE
loki_simulate         (depends on loki_core + loki_arima +
                        loki_kalman)                                <- COMPLETE
loki_evt              (depends on loki_core only)                   <- COMPLETE
loki_kriging          (depends on loki_core only)                   <- COMPLETE
loki_spline           (depends on loki_core only)                   <- COMPLETE
loki_spatial          (depends on loki_core only)                   <- COMPLETE
loki_geodesy          (depends on loki_core only)                   <- COMPLETE
loki_multivariate     (depends on loki_core only)                   <- COMPLETE

loki_gnss             (depends on loki_core only)                   <- IN PROGRESS
loki_climatology      (depends on loki_core + loki_gnss +
                        loki_spatial + existing analysis modules)  <- PLANNED
loki_gnss_rail        (depends on loki_core + loki_gnss +
                        loki_kalman + loki_clustering)             <- PLANNED
loki_gravity          (depends on loki_core + loki_geodesy +
                        loki_spatial)                              <- PLANNED
loki_eo               (depends on loki_core + loki_spatial +
                        loki_homogeneity)                          <- PLANNED
loki_seismology       (depends on loki_core + loki_gnss +
                        loki_spectral + loki_kalman)               <- PLANNED
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
|   +-- loki_evt/
|   +-- loki_kriging/
|   +-- loki_spline/
|   +-- loki_spatial/
|   +-- loki_geodesy/
|   +-- loki_multivariate/           <- COMPLETE
|   +-- loki_gnss/                   <- IN PROGRESS
|   +-- loki_climatology/            <- PLANNED
|   +-- loki_gnss_rail/              <- PLANNED
|   +-- loki_gravity/                <- PLANNED
|   +-- loki_eo/                     <- PLANNED
|   +-- loki_seismology/             <- PLANNED
+-- libs/
|   +-- loki_core/
|   |   +-- include/loki/
|   |       +-- core/         (exceptions, version, config, configLoader, logger, nanPolicy)
|   |       +-- timeseries/   (timeSeries, timeStamp, gapFiller, deseasonalizer,
|   |       |                  medianYearSeries)
|   |       +-- stats/        (descriptive, filter, distributions, hypothesis, metrics,
|   |       |                  wCorrelation, sampling, bootstrap, permutation)
|   |       +-- io/           (loader, dataManager, gnuplot, plot, spatialLoader,
|   |       |                  geodesyLoader,
|   |       |                  gnssLoader, sp3Loader, ionexLoader, antexLoader,
|   |       |                  sinexLoader, vmf3Loader, netcdfLoader, geotiffLoader,
|   |       |                  dbManager, dbWriter, dbReader, dbSchema)
|   |       +-- math/         (lsqResult, lsq, designMatrix, hatMatrix, svd, lm,
|   |                          lagMatrix, embedMatrix, randomizedSvd, spline, nelderMead,
|   |                          krigingTypes, krigingVariogram, krigingBase,
|   |                          simpleKriging, ordinaryKriging, universalKriging,
|   |                          krigingFactory, bspline, bsplineFit,
|   |                          spatialTypes, spatialVariogram, rbf, naturalNeighbor,
|   |                          ellipsoid, interpolation,
|   |                          keplerOrbit, sphericalHarmonics, legendrePolynomials,
|   |                          stokesIntegral)
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
|   +-- loki_evt/
|   +-- loki_kriging/
|   +-- loki_spline/
|   +-- loki_spatial/
|   +-- loki_geodesy/
|   +-- loki_multivariate/           <- COMPLETE
|   +-- loki_gnss/                   <- IN PROGRESS
|   |   +-- include/loki/gnss/
|   |   |   +-- gnssTypes.hpp        <- COMPLETE
|   |   |   +-- rinexNavParser.hpp   <- COMPLETE
|   |   |   +-- rinexObsParser.hpp   <- COMPLETE
|   |   |   +-- keplerOrbit.hpp      <- COMPLETE
|   |   |   +-- satVisibility.hpp    <- COMPLETE
|   |   |   +-- sppSolver.hpp        <- COMPLETE (SPP working, ~3.8km error, see issues)
|   |   +-- src/
|   |       +-- rinexNavParser.cpp
|   |       +-- rinexObsParser.cpp
|   |       +-- keplerOrbit.cpp
|   |       +-- satVisibility.cpp
|   |       +-- sppSolver.cpp
|   +-- loki_climatology/            <- PLANNED
|   +-- loki_gnss_rail/              <- PLANNED
|   +-- loki_gravity/                <- PLANNED
|   +-- loki_eo/                     <- PLANNED
|   +-- loki_seismology/             <- PLANNED
+-- tests/
+-- config/
|   +-- gnss.json                    <- COMPLETE (GPS-only, spp task)
+-- scripts/
|   +-- loki.sh                      <- gnss app registered
+-- data/
|   +-- gnss/                        -- downloaded GNSS products (gitignored)
|   +-- era5/                        -- ERA5/NWM NetCDF files (gitignored)
|   +-- geotiff/                     -- SAR/optical raster data (gitignored)
+-- db/
|   +-- loki.db                      -- SQLite database (gitignored)
+-- docs/
+-- tools/
|   +-- hatanaka/
|   |   +-- CRX2RNX.exe              -- Hatanaka decompressor (Windows PE32+)
|   |   +-- CRX2RNX                  -- Hatanaka decompressor (Linux)
|   |   +-- RNX2CRX.exe
|   |   +-- RNX2CRX
|   +-- gnss_download/
|       +-- download.sh              -- COMPLETE
|       +-- README.md
```

---

## Dependencies

| Library | Version | Purpose |
|---|---|---|
| `Eigen3` | 3.4.0 | Linear algebra, LSQ, SVD, Kalman |
| `nlohmann_json` | 3.11.3 | Configuration files |
| `Catch2` | 3.5.2 | Unit and integration tests |
| `zlib` | system | gzip decompression in rinexObsParser (links as `-lz`) |
| `SQLite3` | system | Persistent data storage (planned, loki_core/db) |

---

## Known Issues and Workarounds

### GCC 13 aggregate-init bug -- CRITICAL
Any `Config` struct with enum member + default value CANNOT be a nested struct inside
a class. Define outside with descriptive name, add `using` alias inside class.
All `XxxConfig` structs in `config.hpp` follow this pattern.
Same applies to loki_gnss: `RinexNavParserConfig`, `RinexObsParserConfig`,
`SppSolverConfig` are all defined outside their respective classes.

### Eigen BDCSVD linking bug -- CRITICAL
`Eigen::BDCSVD` causes `undefined reference` errors when used in `.cpp` files compiled
into static libraries on Windows/GCC. Workarounds:
1. `SvdDecomposition` (`svd.hpp`) is **header-only**.
2. `CalibrationRegressor` uses `Eigen::JacobiSVD` directly.
3. `loki_multivariate` uses `Eigen::JacobiSVD` for PCA/MSSA/CCA/LDA/Factor.
Do NOT use `BDCSVD` in any `.cpp` compiled into a static library.

### Free functions in sub-namespaces -- CRITICAL
`using namespace loki::stats` in `.cpp` is NOT sufficient for defining free functions.
Functions end up in global namespace. Wrap all definitions in explicit namespace block.

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
- Do NOT use gnuplot user-defined functions via pipe -- compute curves in C++ as datablocks
- Heatmaps: NEVER use `matrix rowheaders columnheaders`

### MedianYearSeries API
Constructor: `MedianYearSeries(const TimeSeries& series, Config cfg = Config{})`
Config field: `cfg.minYears` (NOT `minYearsPerSlot`).

### nelderMead -- must be in loki_core CMakeLists
`nelderMead.cpp` must be listed in `add_library(loki_core STATIC ...)`.
Same applies to all new math `.cpp` files.

### loki_gnss -- rinexObsParser decompression -- CRITICAL
CRX2RNX is a native Windows PE32+ binary -- bash/sh cannot execute it directly.
Pipeline: gzip via zlib (C++ API, no subprocess), CRX2RNX via `_popen("cmd /c ...")`.
- gzip step: `gzopen/gzread/gzclose` from zlib -- no external process needed.
- CRX2RNX step: `_popen` with `cmd /c` -- runs under cmd.exe (correct for PE32+).
- CRX2RNX output: auto-derives .rnx from .crx basename -- do NOT provide output path arg.
- Use same base name for both temp files: `loki_gnss_NNN.crx` -> `loki_gnss_NNN.rnx`.
- CRX2RNX refuses if .rnx already exists -- use `-f` flag for force overwrite.
- Temp paths: use `TEMP` env var on Windows (not `fs::temp_directory_path()` which
  returns POSIX /tmp/ -- cmd.exe cannot resolve POSIX paths).
- loki_gnss CMakeLists must link `-lz` for zlib.

### loki_gnss -- RINEX time system -- CRITICAL
RINEX 3 time handling -- different for NAV and OBS:
- RINEX 3 NAV epoch lines (toc): GPS system time. Parser must subtract 18 leap
  seconds before constructing TimeStamp (which is UTC-based), so that
  fromTimeStamp() adds them back yielding the original GPS time.
  Function: `epochToGpsTime()` in rinexNavParser.cpp -- already implements this.
- RINEX 3 OBS epoch lines: GPS system time for GPS receivers (Trimble ALLOY confirmed).
  Parser must apply same leap-second subtraction as NAV.
  Function: `parseEpochLine()` in rinexObsParser.cpp -- already implements this.
- toe/week from broadcast record body (f[8], f[18]): GPS time directly -- NO conversion.
  `eph.toe = GpsTime{ static_cast<int>(eph.week), toeSow }` -- correct as-is.
- CONSEQUENCE: if both OBS and NAV epochToGpsTime add 18s, tk = t_obs - toe is correct
  (both shifted by same amount, difference cancels).
- MISTAKE TO AVOID: applying leap-second correction to only one of OBS/NAV but not both.

### loki_gnss -- SPP solver known issues -- OPEN
Current SPP accuracy: ~3.8 km with GPS-only, iono=none, tropo=none.
Expected SPP accuracy with broadcast ephemeris: 3-10 m.
Root cause not yet identified. Suspected issues:
1. Possible remaining time system inconsistency (tk not exactly correct).
2. `selectPseudorange` may pick wrong code for some satellites.
3. LSQ convergence with only 10 iterations may be insufficient for epochs
   where initial position error is large.
Known NOT to be the cause:
- Satellite clock sign (fixed: prCorr = pr + clkBias*c).
- Multi-constellation time offsets (fixed: GPS-only mode).
- Sagnac correction (tested: same result with sagnac=false).
- Ionosphere/troposphere corrections (tested: same result with none).
Next steps for debugging:
- Compare satellite positions against RTKLIB or online GNSS calculator.
- Verify pseudorange residuals per satellite after convergence.
- Check if error is constant across all epochs (systematic) or variable.

### loki_gnss -- corrections architecture -- PLANNED REFACTOR
Currently ionosphere (Klobuchar), troposphere (Saastamoinen), and Sagnac
corrections are implemented inline in `sppSolver.cpp`. This is a temporary
solution. Planned refactor in next thread:
```
corrections/
    ionosphere.hpp/.cpp   -- Klobuchar, IONEX interpolation
    troposphere.hpp/.cpp  -- Saastamoinen, VMF3
    relativity.hpp/.cpp   -- Sagnac, clock relativistic term
    antenna.hpp/.cpp      -- PCO/PCV from ANTEX
    tides.hpp/.cpp        -- solid Earth tides, ocean loading
    windup.hpp/.cpp       -- phase windup for PPP
```

---

## loki_gnss -- Current State and Next Steps

### Completed in this thread
- `gnssTypes.hpp` -- all data structures (GPS/GAL/GLO/BDS/SBAS/SP3/CLK/IONEX/ANTEX/DCB/VMF3)
- `rinexNavParser.hpp/.cpp` -- RINEX 2.x + 3.x NAV, all constellations
- `rinexObsParser.hpp/.cpp` -- RINEX 3 OBS with Hatanaka+gzip decompression
- `keplerOrbit.hpp/.cpp` -- Keplerian propagation (GPS/GAL/BDS) + GLONASS RK4
- `satVisibility.hpp/.cpp` -- elevation/azimuth + DOP computation
- `sppSolver.hpp/.cpp` -- GPS-only SPP with weighted LSQ, Klobuchar, Saastamoinen
- `config.hpp` patch -- `GnssConfig` struct and sub-configs
- `configLoader.cpp` patch -- `_parseGnss()` implementation
- `gnss.json` -- working config for GOPE station, 2024 DOY 075
- `apps/loki_gnss/main.cpp` -- parse + SPP pipeline with CSV export
- `libs/loki_gnss/CMakeLists.txt` -- with zlib dependency

### Test data
- Station: GOPE00CZE (permanent EUREF/EPN station, Czech Republic)
- Date: 2024-03-15 (DOY 075)
- NAV: `INPUT/GNSS/gnss_data/nav/2024/075/BRDC00IGS_R_20240750000_01D_MN.rnx.gz`
- OBS: `INPUT/GNSS/gnss_data/obs/2024/075/gope/GOPE00CZE_R_20240750000_01D_30S_MO.crx.gz`
- CRX2RNX: `tools/hatanaka/CRX2RNX.exe`
- Reference position ITRF2020: X=3979316.439 Y=1050312.253 Z=4857066.904 [m]

### Next thread tasks (priority order)
1. **Debug SPP 3.8 km error** -- compare satellite positions against reference,
   verify pseudorange residuals, check time system consistency.
2. **Refactor corrections into `corrections/` submodule** -- ionosphere, troposphere,
   relativity, antenna, tides, windup as separate classes.
3. **Implement `plotGnss.hpp/.cpp`** -- satcount, skyplot, SNR, DOP, SPP position error.
   Skyplot uses gnuplot polar terminal. SNR: one subplot per constellation.
   Position error: time series of dX/dY/dZ vs ITRF reference.
4. **`gnssAnalyzer.hpp/.cpp`** -- main orchestrator replacing current main.cpp logic.
5. **`gnssProtocol.hpp/.cpp`** -- SPP summary protocol in .txt format.

### Files to request at start of next thread
- `libs/loki_gnss/src/rinexNavParser.cpp` (current state)
- `libs/loki_gnss/src/rinexObsParser.cpp` (current state)
- `libs/loki_gnss/src/keplerOrbit.cpp` (current state)
- `libs/loki_gnss/src/sppSolver.cpp` (current state)
- `apps/loki_gnss/main.cpp` (current state)
- `config/gnss.json` (current state)

---

## Statistical / Domain Key Learnings

- SSA window L should be a multiple of the dominant period for clean separation.
- Classical decomposition: residual variance > 40% is normal for sub-daily climatological data.
- STL outer loop (n_outer >= 1) needed when seasonal amplitude changes over time.
- Period in decomposition is always in samples -- 1461 for 6h data (1 year).
- Draconitic period for GNSS: 351.4 days (not 365.25).
- Lomb-Scargle preferred over FFT for GNSS and sensor data with frequent gaps.
- Kalman local_level K = Q/(Q+R); GPS IWV: Q~1e-4, R~4e-6 gives K~0.97.
- EVT/SIL 4: GPD/POT preferred over GEV/block-maxima for small datasets.
  Profile likelihood CI mandatory for SIL 4 return periods (1e8 hours).
- Bootstrap: iid bootstrap INVALID for autocorrelated time series. Block bootstrap required.
- Kriging: variogram fitting is the most sensitive step.
- B-spline: nCtrl controls smoothing. CV (one-SE elbow) selects optimal nCtrl automatically.
- Spatial RBF: coordinate normalisation to [0,1] mandatory for CPD kernels.
- Multivariate GPST+UTC sync: tolerance >= 40s for 2023 data (36s leap offset).
- Multivariate RADAR -999: not treated as NaN by GapFiller -- dominates PCA/VAR.
- VAR Granger: assumes stationarity. Non-stationary series must be differenced first.
- MCD Mahalanobis: approximate (10 C-steps, h=0.75n). Adequate for n>100, contamination<20%.
- Factor Analysis Varimax: may not converge if max_iter too low. Increase to 1000.
- GNSS multipath has sidereal periodicity (~23h 56min) -- visible in loki_spectral.
- BOCPD: prior_beta must be tuned close to actual series variance (default 1.0 fails for
  climatological variance ~0.001).
- SNHT recursive mode: min_segment_points >= 3-4 years.
- PELT mbic is most conservative penalty -- recommended default.
- SPP satellite clock correction sign: prCorr = pr + clkBias*c (ADD, not subtract).
  clkBias = af0 + af1*dt + dtr - TGD [seconds]. Positive clkBias = satellite fast.
- Multi-constellation SPP with single receiver clock parameter is WRONG.
  GPS/GLONASS/Galileo/BeiDou have different time offsets. Use GPS-only for basic SPP,
  or add per-constellation inter-system bias (ISB) parameters.
- RINEX 3 NAV and OBS epoch times are in GPS system time for GPS receivers.
  Both must subtract 18 leap seconds before constructing UTC TimeStamp.

---

## Long-Term Roadmap

This section describes planned modules not yet implemented.
Implementation order follows the dependency graph.

### Infrastructure (Faza 0)

#### 0.1 GNSS Data Downloader -- COMPLETE
See `tools/gnss_download/README.md`.

#### 0.2 loki_gnss parsers -- IN PROGRESS
Completed: gnssTypes, rinexNavParser, rinexObsParser, keplerOrbit, satVisibility, sppSolver.
Remaining: sp3Parser, clkParser, ionexParser, antexParser, dcbParser, vmf3Parser.

#### 0.3 SQLite database layer -- PLANNED (after loki_gnss complete)

#### 0.4 loki_core/math/ primitives -- PLANNED
interpolation (Lagrange order 9 for SP3), sphericalHarmonics, legendrePolynomials,
stokesIntegral.

### loki_gnss -- IN PROGRESS
See "loki_gnss -- Current State and Next Steps" section above.

### loki_climatology -- PLANNED
### loki_gnss_rail -- PLANNED
### loki_gravity -- PLANNED
### loki_eo -- PLANNED
### loki_seismology -- PLANNED

---

## Module Roadmap (full picture)

| Priority | Module | Status |
|---|---|---|
| COMPLETE | all time series modules | COMPLETE |
| COMPLETE | loki_spatial, loki_geodesy, loki_multivariate | COMPLETE |
| Faza 0 | GNSS downloader | COMPLETE |
| Faza 0 | loki_gnss parsers (gnssTypes, rinexNav, rinexObs) | COMPLETE |
| Faza 0 | loki_gnss keplerOrbit, satVisibility, sppSolver | COMPLETE (SPP ~3.8km, debug needed) |
| Faza 0 | loki_gnss corrections refactor | NEXT THREAD |
| Faza 0 | loki_gnss plotGnss, gnssAnalyzer, protocol | NEXT THREAD |
| Faza 0 | SPP accuracy debug | NEXT THREAD |
| Faza 0 | sp3Parser, clkParser, ionexParser, antexParser | PLANNED |
| Faza 0 | loki_core/math/ interpolation, keplerOrbit | PLANNED |
| Faza 1 | loki_gnss PPP solver | PLANNED |
| Faza 0 | loki_core/io/ DB layer (SQLite) | PLANNED (after loki_gnss) |
| Faza 2 | loki_climatology | PLANNED |
| Faza 3 | loki_gnss_rail | PLANNED |
| Faza 4 | loki_gravity | PLANNED |
| Faza 5 | loki_eo | PLANNED |
| Faza 6 | loki_seismology | PLANNED |
| future | loki_wavelet | FUTURE |
| future | loki_realtime | FUTURE |
| future | loki_ml | FUTURE |

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
- SQLite database `db/loki.db` is gitignored.
- Downloaded GNSS products in `data/` are gitignored.
- Do NOT add license/copyright blocks at the top of source files.
- `loader.hpp` is in `loki_core/io/`, NOT in `timeseries/`.
- `spatialLoader.hpp` is in `loki_core/io/`, namespace `loki::io`.
- `geodesyLoader.hpp` is in `loki_core/io/`, namespace `loki::io`.
- `ellipsoid.hpp` is in `loki_core/math/` (placed there during loki_geodesy).
- `svd.cpp` does NOT exist -- `SvdDecomposition` is header-only in `svd.hpp`.
- `krigingFactory.hpp` is header-only.
- gnuplot.cpp: `-persist` flag is REMOVED. Do not restore it.
- TimeStamp: `.mjd()` | `.utcString()` | `.gpsTotalSeconds()`
- SNHT recursive mode: min_segment_points >= 3-4 years.
- snhtDetector.hpp and snhtDetector.cpp: SNHT is fully implemented inside
  loki_homogeneity. The files in the repository are EMPTY by design (system
  artifact). Do NOT request them. Do NOT ask about their content.
- CZEPOS is a network/service (GNSS reference network in Czech Republic),
  NOT a single station. SKPOS is the Slovak equivalent.
- loki_gnss is NOT a replacement for RTKLIB -- it is an analytical layer
  with deep integration into the LOKI ecosystem.
- GNSS multipath has sidereal periodicity (~23h 56min, not 24h).
- Draconitic period: 351.4 days (GPS orbital resonance with Sun).
- SP3 interpolation: Lagrange polynomial order 9 (IGS standard).
- CDDIS requires NASA Earthdata account for HTTPS access (~/.netrc).
  CDDIS OAuth redirect requires cookie jar (--cookie-jar ~/.cddis_cookies).
  CODE Bern and EUREF/EPN are freely accessible without authentication.
- OBS RINEX from EUREF/EPN is Hatanaka compressed (.crx.gz).
  Parsers must handle: gunzip (zlib) -> CRX2RNX (cmd /c) -> standard RINEX 3.
- VMF3 filenames use YYYYMMDD format (NOT DOY): VMF3_20240315.H00
- CODE Bern directory structure:
  SP3/CLK/BIAS: ftp.aiub.unibe.ch/CODE_MGEX/CODE/YYYY/
  IONEX/DCB:    ftp.aiub.unibe.ch/CODE/YYYY/
  P2C2 DCB does not exist -- only P1C1 and P1P2 are available.
- GPS week = (unix_epoch - 315964800) / 604800
- EGNOS v3 (DFMC): L1+L5, GPS+Galileo, ~0.5-1m, LPV-200 capable.
  No working public download source currently available (ESA GSSC SFTP down).
- DB architecture: DB populated AFTER loki_gnss computes results.
  Raw RINEX/SP3/CLK files NEVER stored in DB.
- Ocean loading BLQ: manual generation at holt.oso.chalmers.se/loading/
- loki_gnss CMakeLists: must include Eigen3 system includes and link zlib (-lz).
  See libs/loki_gnss/CMakeLists.txt for correct target_include_directories pattern.