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
- GNSS processing: loki_gnss -- IN PROGRESS (SPP complete, PPP next).

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

### Ask before writing code
- Before writing any code, ask the user if they want the code written.
- Discuss approach, propose signatures, wait for approval -- THEN implement.
- This avoids wasting conversation context on unwanted refactors.

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
- **Skyplot**: do NOT use `set polar` -- use manual Cartesian transformation
  x=(90-el)*sin(az), y=(90-el)*cos(az). Draw elevation rings and azimuth ticks as
  explicit datablocks. `set polar` produces unusable axis labels on Windows gnuplot.

---

## Library Architecture

### Architecture principle -- math primitives in loki_core
All reusable math primitives go into `loki_core/math/` (flat, no sub-directories).
Module libraries are thin orchestrators: pipeline, protocol, CSV, plots.
This enables all future modules to reuse the same math without cross-module dependencies.

### Dependency graph
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

loki_gnss             (depends on loki_core + loki_geodesy)         <- IN PROGRESS
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
- `loki_gnss` depends on `loki_geodesy` (uses `ecef2geod`, `ecefEnuRotMat`).

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
|   |       |                  geodesyLoader, gnssLoader, sp3Loader, ionexLoader,
|   |       |                  antexLoader, sinexLoader, vmf3Loader, netcdfLoader,
|   |       |                  geotiffLoader, dbManager, dbWriter, dbReader, dbSchema)
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
|   |   |   +-- gnssTypes.hpp
|   |   |   +-- correctionModel.hpp  <- abstract base CorrectionModel
|   |   |   +-- orbitModel.hpp       <- abstract base OrbitModel
|   |   |   +-- broadcastOrbit.hpp/.cpp
|   |   |   +-- ionosphere.hpp/.cpp  <- KlobucharModel
|   |   |   +-- troposphere.hpp/.cpp <- SaastamoinenModel
|   |   |   +-- relativity.hpp/.cpp  <- Sagnac, rotateSatPosition
|   |   |   +-- solidTides.hpp/.cpp  <- IERS2010 step-1
|   |   |   +-- gnssUtils.hpp/.cpp   <- selectPseudorange, elevationWeight, isbKey
|   |   |   +-- keplerOrbit.hpp/.cpp <- GPS/GAL/BDS Kepler + GLONASS RK4
|   |   |   +-- satVisibility.hpp/.cpp
|   |   |   +-- rinexNavParser.hpp/.cpp
|   |   |   +-- rinexObsParser.hpp/.cpp
|   |   |   +-- sppSolver.hpp/.cpp   <- injected OrbitModel + CorrectionModel vector
|   |   |   +-- gnssResult.hpp       <- ParseResult, SppSummary, GnssResult
|   |   |   +-- gnssAnalyzer.hpp/.cpp
|   |   |   +-- gnssProtocol.hpp/.cpp
|   |   |   +-- plotGnss.hpp/.cpp
|   |   |   +-- gnssCsvExport.hpp/.cpp
|   +-- loki_climatology/            <- PLANNED
|   +-- loki_gnss_rail/              <- PLANNED
|   +-- loki_gravity/                <- PLANNED
|   +-- loki_eo/                     <- PLANNED
|   +-- loki_seismology/             <- PLANNED
+-- tests/
+-- config/
|   +-- gnss.json
+-- scripts/
|   +-- loki.sh
+-- data/
|   +-- gnss/                        -- downloaded GNSS products (gitignored)
|   +-- era5/                        -- ERA5/NWM NetCDF files (gitignored)
|   +-- geotiff/                     -- SAR/optical raster data (gitignored)
+-- db/
|   +-- loki.db                      -- SQLite database (gitignored)
+-- docs/
+-- tools/
|   +-- hatanaka/
|   |   +-- CRX2RNX.exe
|   |   +-- CRX2RNX
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
- `set terminal pngcairo ... size W,H` -- no duplicate size keyword; `terminal()` helper
  returns base string without size, each plot appends its own `size W,H`

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
- Temp paths: use `TEMP` env var on Windows (not `fs::temp_directory_path()`).
- loki_gnss CMakeLists must link `-lz` for zlib.

### loki_gnss -- RINEX time system -- CRITICAL
- RINEX 3 NAV toc epoch: GPS system time. Parser subtracts 18 leap seconds before
  constructing TimeStamp so that fromTimeStamp() adds them back = original GPS time.
- RINEX 3 OBS epoch: GPS system time (Trimble ALLOY confirmed). Same subtraction.
- toe/week from broadcast record body: GPS time directly, NO conversion.
- Both OBS and NAV apply the same 18s shift -- difference tk = t_obs - toe cancels.
- MISTAKE TO AVOID: applying leap-second correction to only one of OBS/NAV.

### loki_gnss -- SPP accuracy
- GPS + Galileo, Klobuchar + Saastamoinen + Sagnac: ~1.6 m RMS (GOPE, DOY 075/2024).
- Sagnac correction is critical: without it error is ~24 m, with it ~1.6 m.
- Solid Earth Tides (IERS2010 step-1): tested, marginally worsens SPP (~3 cm).
  Cause: low-precision analytical Moon/Sun ephemeris (Meeus truncated series, ~1 arcmin).
  Recommendation: keep solid_tides=false for SPP; use full JPL DE440 for PPP.
- Default constellations: GPS + GALILEO only (see GLONASS/BeiDou note below).

### loki_gnss -- GLONASS and BeiDou -- TODO (deferred)
GLONASS and BeiDou are parsed and have orbit propagators (RK4 for GLO, Kepler for BDS)
but produce incorrect SPP results when added to the solution. Root cause: time system
conversion errors.
- GLONASS uses UTC+3h (Moscow time) -- conversion to GPS time in parser/propagator
  not fully verified.
- BeiDou uses BDT with 14-second offset from GPS (NOT 18s like UTC).
  The offset may be applied incorrectly in some code paths.
- Both constellations are DISABLED by default in gnss.json (constellations: GPS, GALILEO).
- Debug deferred to a separate thread. Do NOT enable GLO/BDS in SPP until fixed.
- When debugging: verify tk = t_obs - toe for a known satellite against RTKLIB output.

### loki_gnss -- CorrectionModel architecture
All signal corrections implement `CorrectionModel` interface (correctionModel.hpp).
`CorrectionInput` contains: lat/lon/h [rad/m], elevation/azimuth [rad], gpsSow,
gpsWeek, satX/Y/Z [m], staX/Y/Z [m].
`SppSolver` fills all CorrectionInput fields before calling each model's `delay()`.
Models: KlobucharModel, SaastamoinenModel, SolidTidesModel (all in loki_gnss).
`relativistic` config flag removed -- dtr correction is always active in KeplerOrbit.

### loki_gnss -- KalmanFilter not suitable for PPP
`loki_kalman::KalmanFilter` is a scalar 1D filter (H: 1xn, R: 1x1).
PPP requires:
- Multi-observation update: H is mxn, R is mxm (30+ observations per epoch).
- Variable-size state: ambiguity parameters added/removed per satellite arc.
- Arc management: cycle slip detection resets individual ambiguity parameters.
Solution: implement dedicated `PppFilter` in loki_gnss using same Joseph-form update
logic but with dynamic state management. Do NOT force-fit loki_kalman.

---

## loki_gnss -- Current State

### Completed (this and previous threads)
- `gnssTypes.hpp` -- all data structures (GPS/GAL/GLO/BDS/SBAS/SP3/CLK/IONEX/ANTEX/DCB/VMF3)
- `rinexNavParser.hpp/.cpp` -- RINEX 2.x + 3.x NAV, all constellations
- `rinexObsParser.hpp/.cpp` -- RINEX 3 OBS with Hatanaka+gzip decompression
- `keplerOrbit.hpp/.cpp` -- GPS/GAL/BDS Kepler + GLONASS RK4
- `satVisibility.hpp/.cpp` -- elevation/azimuth (uses loki_geodesy ecef2geod)
- `correctionModel.hpp` -- abstract CorrectionModel base with full CorrectionInput
- `orbitModel.hpp` -- abstract OrbitModel base
- `broadcastOrbit.hpp/.cpp` -- BroadcastOrbit : OrbitModel (wraps KeplerOrbit)
- `ionosphere.hpp/.cpp` -- KlobucharModel : CorrectionModel
- `troposphere.hpp/.cpp` -- SaastamoinenModel : CorrectionModel
- `relativity.hpp/.cpp` -- Sagnac: rotateSatPosition(), sagnacDelay()
- `solidTides.hpp/.cpp` -- SolidTidesModel : CorrectionModel (IERS2010 step-1,
  low-precision Meeus ephemeris -- adequate for testing, not for PPP)
- `gnssUtils.hpp/.cpp` -- selectPseudorange(), elevationWeight(), isbKey()
- `sppSolver.hpp/.cpp` -- injected OrbitModel + vector<CorrectionModel>, ISB for GAL
- `gnssResult.hpp` -- ParseResult (with ObsCodeMap), SppSummary (with geodetic coords),
  GnssResult
- `gnssAnalyzer.hpp/.cpp` -- full pipeline orchestrator
- `gnssProtocol.hpp/.cpp` -- text protocol with DMS coords, obs codes, corrections section
- `plotGnss.hpp/.cpp` -- 10 plots: satcount, elevation, skyplot (x2), dop, clockbias,
  residuals, isb, positionEcef, positionError, positionScatter
- `gnssCsvExport.hpp/.cpp` -- spp_epochs.csv, spp_clk.csv, spp_tropo.csv
- `config.hpp` -- GnssConfig with GnssSppConfig, GnssPppConfig, GnssRtkConfig,
  GnssKinematicConfig, GnssCorrectionsConfig (no relativistic flag),
  GnssFiguresConfig (figures per method: spp/ppp/rtk)
- `configLoader.cpp` -- _parseGnss() with all new sections
- `gnss.json` -- GPS+Galileo, SPP enabled, Sagnac+Klobuchar+Saastamoinen

### Test data (GOPE station, Czech Republic)
- Station: GOPE00CZE (permanent EUREF/EPN station)
- Date: 2024-03-15 (DOY 075, GPS week 2307)
- NAV: `INPUT/GNSS/gnss_data/nav/2024/075/BRDC00IGS_R_20240750000_01D_MN.rnx.gz`
- OBS: `INPUT/GNSS/gnss_data/obs/2024/075/gope/GOPE00CZE_R_20240750000_01D_30S_MO.crx.gz`
- Reference ITRF2020: X=3979316.439 Y=1050312.253 Z=4857066.904 [m]
- SPP result (GPS+GAL, Sagnac+Klobuchar+Saastamoinen): ~1.6 m RMS
- PPP products available (CODE Bern, MGEX final):
  SP3:  `INPUT/GNSS/gnss_data/sp3/2024/075/COD0MGXFIN_20240750000_01D_05M_ORB.SP3.gz`
  CLK:  `INPUT/GNSS/gnss_data/clk/2024/075/COD0MGXFIN_20240750000_01D_30S_CLK.CLK.gz`
  ANTEX:`INPUT/GNSS/gnss_data/antex/igs20.atx`
  VMF3: `INPUT/GNSS/gnss_data/vmf3/2024/20240315.h00`
  BLQ:  `INPUT/GNSS/gnss_data/blq/GOPE.BLQ`
- CSRS-PPP reference (for comparison): ~mm accuracy, static final mode,
  GPS+GLO+GAL, IF combination, 98.81% fixed ambiguities

---

## loki_gnss -- PPP Next Thread

### Goal
Implement PPP (Precise Point Positioning) for static stations.
Expected accuracy: ~1-5 cm after convergence (~20-30 min).
Comparison target: CSRS-PPP output (already available for GOPE DOY 075/2024).

### What needs to be implemented

**1. loki_core/math/interpolation.hpp/.cpp** (NEW, goes into loki_core)
- Lagrange polynomial interpolation, order 9 (IGS standard for SP3).
- API: `lagrangeInterp(times, values, t_query, order) -> double`
- Used by sp3Parser and clkParser for epoch interpolation.

**2. sp3Parser.hpp/.cpp** (NEW, loki_gnss)
- Parses SP3-c and SP3-d format (precise satellite orbits).
- Stores positions [km] and velocities [dm/s] at 5-min or 15-min intervals.
- Lagrange order-9 interpolation to arbitrary epoch.
- Handles clock values in SP3 (microseconds -> seconds).

**3. clkParser.hpp/.cpp** (NEW, loki_gnss)
- Parses RINEX CLK format (satellite + receiver clock corrections).
- 30s or 5s intervals; linear interpolation to arbitrary epoch.
- Distinguishes AS (satellite) and AR (receiver) records.

**4. antexParser.hpp/.cpp** (NEW, loki_gnss)
- Parses ANTEX format (IGS antenna phase center corrections).
- Satellite PCO [mm] per frequency per constellation.
- Receiver PCO/PCV [mm] per frequency -- looked up by antenna type from OBS header.

**5. phaseWindup.hpp/.cpp** (NEW, loki_gnss)
- Phase windup correction for carrier phase observations.
- Analytical model: depends on satellite attitude + station-satellite geometry.
- Returns correction in cycles (convert to metres: cycles * lambda).
- Requires satellite ECEF velocity (from SP3) for attitude computation.

**6. PppFilter** (NEW, loki_gnss) -- NOT loki_kalman
- Sequential Kalman filter with variable-size state vector.
- State: [X, Y, Z, clk_GPS, ZTD_wet, N_1, ..., N_n]
- Multi-observation update per epoch (H: mxn, R: mxm).
- Arc management: add/remove ambiguity parameters per satellite arc.
- Cycle slip detection: geometry-free combination L1-L2.
- Joseph-form covariance update (same as loki_kalman but multi-obs).

**7. pppSolver.hpp/.cpp** (NEW, loki_gnss)
- IF combination: P_IF = (f1^2*P1 - f2^2*P2)/(f1^2-f2^2) [eliminates iono]
- Phase IF: L_IF = (f1^2*L1 - f2^2*L2)/(f1^2-f2^2)
- Satellite position from SP3 (not broadcast ephemeris).
- Satellite clock from CLK file.
- PCO/PCV from ANTEX.
- Phase windup correction.
- ZTD_wet estimated as filter parameter (Saastamoinen as prior/initial).
- Ocean loading: optional, requires BLQ (small effect, can be added later).

### Files to request at start of PPP thread
- `libs/loki_gnss/include/loki/gnss/gnssTypes.hpp` (current state)
- `libs/loki_gnss/include/loki/gnss/correctionModel.hpp`
- `libs/loki_gnss/include/loki/gnss/orbitModel.hpp`
- `libs/loki_gnss/include/loki/gnss/sppSolver.hpp`
- `libs/loki_gnss/src/sppSolver.cpp`
- `libs/loki_gnss/CMakeLists.txt`
- `config/gnss.json`

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
- Sagnac correction is critical for SPP: without it error is ~24 m vs ~1.6 m with it.
- SP3 interpolation: Lagrange polynomial order 9 (IGS standard).
- PPP IF combination eliminates ionosphere to ~0.1 mm level.
- PPP convergence time: ~20-30 min for static station; PPP-AR much faster.
- Solid Earth Tides: for PPP use full IERS 2010 step-2 with JPL DE440 ephemeris.
  Meeus truncated analytical series is insufficient for PPP accuracy.

---

## Long-Term Roadmap

### Infrastructure (Faza 0)

#### 0.1 GNSS Data Downloader -- COMPLETE

#### 0.2 loki_gnss parsers
Completed: gnssTypes, rinexNavParser, rinexObsParser, keplerOrbit, satVisibility,
           sppSolver, corrections (Klobuchar, Saastamoinen, Sagnac, SolidTides),
           gnssAnalyzer, gnssProtocol, plotGnss, gnssCsvExport.
Remaining: sp3Parser, clkParser, antexParser, phaseWindup (all needed for PPP).
           ionexParser, dcbParser, vmf3Parser (lower priority, PPP phase 2).

#### 0.3 SQLite database layer -- PLANNED (after loki_gnss PPP complete)

#### 0.4 loki_core/math/ primitives
interpolation (Lagrange order 9) -- NEEDED FOR PPP (next thread).
sphericalHarmonics, legendrePolynomials, stokesIntegral -- gravity module (later).

### Module Roadmap

| Priority | Module | Status |
|---|---|---|
| COMPLETE | all time series modules | COMPLETE |
| COMPLETE | loki_spatial, loki_geodesy, loki_multivariate | COMPLETE |
| Faza 0 | GNSS downloader | COMPLETE |
| Faza 0 | loki_gnss SPP (GPS+Galileo, ~1.6m RMS) | COMPLETE |
| Faza 0 | loki_gnss GLONASS+BeiDou SPP | TODO (time system debug deferred) |
| Faza 1 | loki_core interpolation (Lagrange order 9) | NEXT THREAD |
| Faza 1 | loki_gnss sp3Parser, clkParser, antexParser | NEXT THREAD |
| Faza 1 | loki_gnss phaseWindup, PppFilter, pppSolver | NEXT THREAD |
| Faza 0 | loki_core/io/ DB layer (SQLite) | PLANNED |
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
- `ellipsoid.hpp` is in `loki_core/math/`.
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
- GnssFiguresConfig lives in GnssConfig (not PlotConfig) -- per-method figures
  sections (spp/ppp/rtk) follow the GNSS-specific hierarchy in gnss.json.
  Other modules use PlotConfig booleans as before.
- PlotConfig in config.hpp has geodesy plot flags (geodCovariancePanel,
  geodDistanceBar) -- added but parsePlots patch not yet applied to configLoader.

  ---

## loki_gnss -- PPP Current State (Thread May 2026)

### Status
PPP pipeline compiles and runs but produces WRONG results:
- Position error: ~10 m RMS (expected: ~1-5 cm after convergence)
- ZTD wet: oscillates -4000 to +3000 mm (expected: ~100 mm stable)
- ZTD total: oscillates wildly (CSRS-PPP reference: 2.22-2.29 m stable)
- Convergence criterion fires but filter is in wrong local minimum

### Files implemented this thread
All new PPP files are in libs/loki_gnss/:
  include/loki/gnss/sp3Parser.hpp/.cpp       -- SP3-d parser (fixed: was 22-line header limit, SP3-d has 32+ lines)
  include/loki/gnss/clkParser.hpp/.cpp       -- RINEX CLK parser (token-based, handles nvals+bias glued)
  include/loki/gnss/antexParser.hpp/.cpp     -- ANTEX parser (implemented, NOT YET APPLIED to orbits)
  include/loki/gnss/sp3Orbit.hpp/.cpp        -- SP3+CLK orbit model; pre-built O(log n) clock index
  include/loki/gnss/phaseWindup.hpp/.cpp     -- Phase windup accumulator (Meeus Sun ephemeris)
  include/loki/gnss/pppFilter.hpp/.cpp       -- Kalman filter (broken -- see diagnosis below)
  include/loki/gnss/pppSolver.hpp/.cpp       -- PPP orchestrator
  include/loki/gnss/obsBiasParser.hpp/.cpp   -- BIAS-SINEX OSB parser
  loki_core/math/interpolation.hpp/.cpp      -- Lagrange order-9 (for SP3)

Modified files:
  gnssResult.hpp      -- added PppResult, PppSummary, hasPpp
  gnssAnalyzer.hpp/.cpp -- added _runPpp(), _computePppSummary()
  gnssProtocol.hpp/.cpp -- added _writePppResults() with geodetic coords
  gnssCsvExport.hpp/.cpp -- added exportPppEpochs(), exportPppTropo()
  plotGnss.hpp/.cpp   -- added plotPppPositionError(), plotPppTroposphere(), plotPppClockBias()
  troposphere.hpp/.cpp -- extended: zhd(), zwd(), NiellMappingFunction, IwvConverter
  config.hpp          -- GnssPppConfig.osbFile added (removed duplicate osb_file field)
  configLoader.cpp    -- _parseGnss() ppp block updated

### Test data (confirmed working)
Station: GOPE00CZE (Czech Republic, EUREF/EPN)
Date: 2024-03-15 (DOY 075, GPS week 2305)
SP3:   INPUT/GNSS/gnss_data/sp3/2024/075/COD0MGXFIN_20240750000_01D_05M_ORB.SP3.gz
CLK:   INPUT/GNSS/gnss_data/clk/2024/075/COD0MGXFIN_20240750000_01D_30S_CLK.CLK.gz
OSB:   INPUT/GNSS/gnss_data/bias/COD0MGXFIN_20240750000_01D_01D_OSB.BIA.gz  (727 records)
OBS:   INPUT/GNSS/gnss_data/obs/2024/075/gope/GOPE00CZE_R_20240750000_01D_30S_MO.crx.gz
NAV:   INPUT/GNSS/gnss_data/nav/2024/075/BRDC00IGS_R_20240750000_01D_MN.rnx.gz
Reference ITRF2020: X=3979316.439 Y=1050312.253 Z=4857066.904
CSRS-PPP reference ZTD total: 2.22-2.29 m (stable, slow variation)

### Root cause of wrong results -- CONFIRMED by debug

#### 1. ZWD diverges in code-only phase
Debug output showed:
  [CODE_INNOV ep1] mean_y=-106768  ZWD=0.111   <- hodiny neznáme, OK
  [CODE_INNOV ep2] mean_y=-0.304   ZWD=0.085   <- ZWD zacina klesat
  [CODE_INNOV ep5] mean_y=-0.510   ZWD=-0.001  <- uz zaporne po 5 epochach
  [PHASE_SWITCH]   ZWD=-0.624      <- pri prepnuti na fazu je ZWD zle

Mean code innovation = -0.3 to -0.6 m systematically (after clock correction).
Filter absorbs this into ZWD (only free parameter with large enough gain).
After 30 code-only epochs: ZWD = -0.6 m instead of correct +0.1 m.

#### 2. Ambiguity bootstrap with wrong ZWD locks filter in wrong state
When phase starts (epoch 31), bootstrap: N = ifPhase - ifCode (correct formula).
But ZWD_state = -0.6 m, ZWD_real = +0.1 m => delta = 0.7 m.
Slant ZWD error at el=40 deg: 0.7/sin(40) = 1.09 m.
N_bootstrap = ifPhase - ifCode DOES cancel geometry but NOT the wrong ZWD.
Wait -- actually N = ifPhase - ifCode cancels everything including ZWD error.
The bootstrap itself is correct.

BUT: after bootstrap, phase innovations are ~0 (by construction at bootstrap epoch).
Code innovations remain ~-0.4 m. Filter weight: sigma_phase=0.005m vs sigma_code=3m.
Phase weight 360000x larger than code.
Filter drives ZWD down to make phase residuals zero, ignoring code residuals.
Result: ZWD oscillates wildly following multipath/noise in phase, not physical ZWD.

#### 3. Systematic -0.4 m bias in code after clock correction
Source confirmed NOT to be:
  - OSB correction (verified mathematically: alpha*(C1C-C1W) properly corrected)
  - Phase windup (disabled, same result)
  - Solid tides (disabled, same result)
  - Sagnac (enabled, same as disabled for this test)
Source suspected to be:
  - Satellite PCO: SP3 orbits are to Center of Mass (CoM), CLK consistent with APC.
    GPS IF PCO = alpha*280mm - beta*393mm = ~105 mm. Direction-dependent, averages ~0.1 m.
    This alone does not explain -0.4 m but contributes.
  - Real atmospheric ZHD error: Saastamoinen standard atmosphere may not match
    actual pressure at GOPE on 2024-03-15. No met data available to verify.
    Real ZTD from CSRS = 2.25 m, our ZHD = 2.149 m => real ZWD = 0.10 m (prior=0.135 m, close).
    But if actual ZHD = 2.20 m (higher pressure), our ZHD is 0.05 m too small
    => code innovations would be +0.05*mf ≈ +0.07 m (positive, not -0.4 m).
  - CONCLUSION: The -0.4 m source is still not definitively identified.
    Most likely combination of satellite PCO + receiver antenna PCO + small ZHD error.

### What CSRS-PPP does that we don't (missing corrections)
1. Satellite PCO/PCV from ANTEX (AntexParser exists but not applied to satellite position)
2. Receiver antenna PCO/PCV from ANTEX (not implemented)
3. VMF3 mapping functions (have files, not implemented -- using NMF instead)
4. Phase windup lambda: using lambda_L1 (0.1903 m) instead of lambda_IF (~0.107 m)
5. Ambiguity fixing (float only -- dm-level accuracy)
6. Proper filter initialization from SPP (we use approxPos from OBS header)

### PppFilter design problems
Current implementation has these structural issues:

a) NO EXTERNAL INITIALIZATION OF CLOCK:
   filter.init() sets m_x(3) = 0 (clock unknown).
   First epoch innovation = -106768 m (real clock offset for TRIMBLE ALLOY at GOPE).
   With p0Clk = 1e10, filter correctly assigns this to clock in ep1.
   But correlation between ZWD and clock in ep1 causes ZWD shift.
   FIX: Initialize clock from SPP result before starting PPP filter.

b) CODE-ONLY PHASE DOES NOT CONVERGE ZWD:
   30 epochs of code-only with sigma_code=3m, 15 satellites.
   Kalman gain for ZWD ≈ 0.5 per epoch.
   But mean innovation = -0.4 m => ZWD drifts -0.4*0.5/mf ≈ -0.13 m per epoch.
   After 30 epochs: ZWD ≈ 0.135 - 30*0.13 ≈ -3.7 m (even worse with 120 epochs).
   The -0.4 m bias in code is the real problem -- filter cannot converge to correct ZWD
   while this bias exists.

c) FILTER CONFIG NOT IN JSON:
   PppFilterConfig parameters (p0Pos, p0Clk, p0Ztd, qPos, qClk, qZtd, qAmb,
   sigmaCodeM, sigmaPhaseM, codeOnlyEpochs) are hardcoded in pppFilter.hpp defaults.
   Should remain as struct defaults -- these are algorithm tuning, not user config.
   Current values:
     p0Pos = 100 m^2, p0Clk = 1e10 m^2, p0Ztd = 0.25 m^2, p0Amb = 1e6 m^2
     qPos = 1e-8, qClk = 100, qZtd = 1e-8, qZtdCode = 1e-5
     sigmaCodeM = 3.0 m, sigmaPhaseM = 0.005 m
     codeOnlyEpochs = 30 (was tested with 5, 30, 120 -- all fail for same reason)

### Files to request at start of PPP fix thread
Request ALL of these before writing any code:
  libs/loki_gnss/include/loki/gnss/pppFilter.hpp
  libs/loki_gnss/src/pppFilter.cpp
  libs/loki_gnss/include/loki/gnss/pppSolver.hpp
  libs/loki_gnss/src/pppSolver.cpp
  libs/loki_gnss/include/loki/gnss/sp3Orbit.hpp
  libs/loki_gnss/src/sp3Orbit.cpp
  libs/loki_gnss/include/loki/gnss/gnssResult.hpp
  libs/loki_gnss/src/gnssAnalyzer.cpp
  config/gnss.json (the PPP config)

### Recommended fix strategy for next thread

STEP 1 -- Fix systematic code bias (most important):
  Apply satellite PCO correction to SP3 orbit position.
  In sp3Orbit.cpp, after Lagrange interpolation, add PCO offset:
    sat_apc = sat_com + R_body * pco_vector
  where R_body is satellite body frame rotation (nadir + solar panel direction).
  Use AntexParser to get PCO for each satellite SVN.
  This should reduce the -0.4 m systematic bias to near zero.

STEP 2 -- Fix filter initialization:
  In gnssAnalyzer.cpp _runPpp():
    if (m_cfg.gnss.spp.enabled && result.hasSpp) {
        // use last valid SPP epoch to initialize PPP clock
        const auto& lastSpp = result.spp.back(); // find last valid
        filter.initClock(lastSpp.clkBiasM);
    }
  Add PppFilter::initClock(double clkM) method.

STEP 3 -- Fix phase windup wavelength:
  In pppSolver.cpp, replace:
    o.phaseWindupM = cycles * (SPEED_OF_LIGHT / GPS_F1);  // 0.1903 m/cycle
  With:
    constexpr double LAMBDA_IF = SPEED_OF_LIGHT / (GPS_F1 - GPS_F2); // ~0.862 m
    // Or more precisely for IF combination narrow-lane:
    constexpr double LAMBDA_NL = SPEED_OF_LIGHT / (GPS_F1 + GPS_F2); // ~0.1070 m
    o.phaseWindupM = cycles * LAMBDA_NL;
  Note: effect is small (<2 cm) but correct.

STEP 4 -- Remove all debug cerr output:
  pppFilter.cpp: remove [CODE_INNOV], [PHASE_SWITCH], #include <iostream>
  pppSolver.cpp: remove [G01 ep], [RES], [WINDUP] debug blocks
  pppFilter.hpp: remove codeOnlyEpochs tuning comment clutter

STEP 5 -- Config cleanup:
  gnss.json corrections section: valid values are:
    ionosphere: "if_combination" (PPP only), "klobuchar" (SPP only), "none"
    troposphere: "saastamoinen" (uses NMF internally for PPP)
  These are informational strings for the protocol -- no functional effect for PPP
  since ionosphere is eliminated by IF combination and troposphere uses
  Saastamoinen+NMF regardless of this string.

### Known working: SP3 parser fix
Original sp3Parser.cpp had hardcoded "if (lineNo >= 22) break" in parseHeader().
SP3-d files (CODE MGEX) have 32+ header lines.
Fix: read until first '*' epoch line regardless of line count.
This fix is in the current code -- DO NOT revert.

### Known working: CLK index
sp3Orbit.cpp builds O(log n) clock index at construction.
346314 CLK records, ~11500 per satellite.
Processing time: ~19 seconds (was 15 minutes with linear search).
DO NOT revert to linear search.

### OSB correction -- verified correct
osbCorrection() in pppSolver.cpp:
  GPS: correction = -GPS_ALPHA * (OSB_C1C - OSB_C1W) * c/1e9  [m, added to ifCode]
  GAL: correction = -(GAL_ALPHA*(OSB_C1X-OSB_C1X) - GAL_BETA*(OSB_C5X-OSB_C5X)) = 0
Verified analytically: formula aligns C1C+C2W user obs to C1W+C2W CLK reference.
OSB file: COD0MGXFIN_20240750000_01D_01D_OSB.BIA.gz (727 records, loads correctly).

### Performance (current)
Parsing:    ~4 seconds (OBS Hatanaka + gzip)
SP3+CLK:    ~4 seconds
PPP filter: ~19 seconds for 2880 epochs
Total:      ~27 seconds

### IMPORTANT: debug output still in code
pppFilter.cpp has: #include <iostream> and cerr blocks -- REMOVE before next session
pppSolver.cpp may have cerr blocks -- REMOVE before next session