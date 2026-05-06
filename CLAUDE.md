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

Planned domains (see Long-Term Roadmap section):
- GNSS processing: loki_gnss, loki_gnss_rail
- Climatological GNSS analysis: loki_climatology
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
  only, with axis labels set via `set xtics` / `set ytics` explicitly. Using rowheaders
  causes gnuplot to consume the first row/column as labels, shifting all data indices
  and displaying numeric values instead of channel names on axes.
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
- GNSS processing, climatology, gravity, EO, seismology (planned)

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

loki_gnss             (depends on loki_core + loki_geodesy)        <- PLANNED
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
|   +-- loki_gnss/                   <- PLANNED
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
|   +-- loki_gnss/                   <- PLANNED
|   +-- loki_climatology/            <- PLANNED
|   +-- loki_gnss_rail/              <- PLANNED
|   +-- loki_gravity/                <- PLANNED
|   +-- loki_eo/                     <- PLANNED
|   +-- loki_seismology/             <- PLANNED
+-- tests/
+-- config/
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
    +-- gnss_download/               -- shell scripts for GNSS product download
```

---

## Dependencies

| Library | Version | Purpose |
|---|---|---|
| `Eigen3` | 3.4.0 | Linear algebra, LSQ, SVD, Kalman |
| `nlohmann_json` | 3.11.3 | Configuration files |
| `Catch2` | 3.5.2 | Unit and integration tests |
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

### MedianYearSeries API
Constructor: `MedianYearSeries(const TimeSeries& series, Config cfg = Config{})`
Config field: `cfg.minYears` (NOT `minYearsPerSlot`).

### nelderMead -- must be in loki_core CMakeLists
`nelderMead.cpp` must be listed in `add_library(loki_core STATIC ...)`.
Same applies to all new math `.cpp` files.

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

---

## Long-Term Roadmap

This section describes planned modules not yet implemented.
Implementation order follows the dependency graph.

### Infrastructure (Faza 0) -- prerequisite for all planned modules

#### 0.1 GNSS Data Downloader (`tools/gnss_download/`)

Shell scripts (consistent with C++ project, no Python dependency) for downloading
GNSS products from public archives. Manual download is acceptable for first tests.
Key sources:
- PECNY / GOP: `ftp.pecny.cz/pub/obs/` -- anonymous FTP, no auth required.
  Station GOPE (Geodetic Observatory Pecny) -- primary test station.
- CODE Bern: `ftp.aiub.unibe.ch/CODE/YYYY/` -- SP3, CLK, DCB, IONEX, no auth.
- CDDIS: `cddis.nasa.gov` -- requires NASA Earthdata account (OAuth).
  Use Earthdata Download CLI (`pip install earthdata`) or browser download.
- IGS: `files.igs.org/pub/station/general/` -- ANTEX, no auth.
- TU Wien: `vmf.geo.tuwien.ac.at` -- VMF3, GPT3, no auth.
- IERS: `iers.org` -- leap-seconds.list, EOP, no auth.
- EUREF: `epncb.oma.be` -- EUREF EPN siet, ETRS89

Products needed per processing mode:
```
SPP (minimal test):
  RINEX obs (.rnx)          -- GOPE, 1 day
  RINEX nav mixed (.rnx)    -- broadcast ephemeris, same day
  igs20.atx                 -- antenna calibration, one-time download

PPP (adds):
  SP3c (.sp3)               -- precise ephemeris, IGS final (~13d delay)
  RINEX CLK (.clk)          -- precise clocks, IGS or CODE
  IONEX (.ionex)            -- TEC maps, CODE or IGS
  DCB / SINEX BIAS (.dcb / .bia) -- code biases, CODE

ZTD validation (adds):
  IGS ZTD products (SINEX TRO .tro)
  VMF3 grid (.H00/.H06/.H12/.H18)
  RINEX MET (.met)          -- meteorological observations

One-time manual steps:
  Ocean loading BLQ         -- holt.oso.chalmers.se/loading/
  leap-seconds.list         -- iers.org
```

#### 0.2 New loki_core/io/ loaders (C++)

```
gnssLoader.hpp/.cpp       -- RINEX obs 2.x/3.x, nav, MET parser
sp3Loader.hpp/.cpp        -- SP3a/SP3c precise ephemeris
ionexLoader.hpp/.cpp      -- IONEX TEC maps
antexLoader.hpp/.cpp      -- ANTEX antenna calibrations (PCO/PCV)
sinexLoader.hpp/.cpp      -- SINEX station coordinates, ZTD products
vmf3Loader.hpp/.cpp       -- VMF3 gridded mapping function data
netcdfLoader.hpp/.cpp     -- ERA5/GRACE NetCDF (for loki_eo, loki_gravity)
geotiffLoader.hpp/.cpp    -- GeoTIFF raster data (for loki_eo)
```

All loaders return standard LOKI types (TimeSeries, spatial datasets).

#### 0.3 SQLite database layer (loki_core/io/)

Central persistent store for multi-session and multi-station data. Enables building
long time series (e.g. GOPE ZTD archive) across multiple download sessions and
feeding them into analysis modules without re-parsing raw files.

```
dbSchema.hpp              -- CREATE TABLE definitions
dbManager.hpp/.cpp        -- open/close connection, transaction management
dbWriter.hpp/.cpp         -- INSERT: positions, ztd_series, era5_grid, qc_log, ...
dbReader.hpp/.cpp         -- SELECT -> TimeSeries, SpatialDataset
```

Schema (key tables):
```sql
stations      (id, name, lat, lon, h, itrf_x, itrf_y, itrf_z, network)
obs_summary   (station_id, epoch, n_gps, n_glo, n_gal, n_bds,
               pdop, hdop, mean_snr, cycle_slip_count, mp1_rms, mp2_rms)
positions     (station_id, epoch, method, x, y, z, lat, lon, h,
               sigma_x, sigma_y, sigma_z, n_sat, raim_ok, hpl, vpl)
ztd_series    (station_id, epoch, source, ztd, zhd, zwd, sigma_ztd)
iwv_series    (station_id, epoch, source, iwv, sigma_iwv)
era5_grid     (lat, lon, epoch, t2m, sp, tcwv, source)
integrity_log (station_id, epoch, pdop, hpl, vpl, raim_status,
               n_excluded_sats, alert_flag)
track_positions (session_id, epoch, s_along_track, lat, lon, h,
                 v, sigma_pos, sigma_v, integrity_flag)
qc_log        (station_id, date, obs_completeness, mean_snr,
               cycle_slip_rate, mp1_rms, qc_grade)
```

ERA5/NWM data stored in `era5_grid` allows selection of time series at any
(lat, lon) point for blind model analysis, homogeneity testing, and validation.

#### 0.4 New loki_core/math/ primitives

```
interpolation.hpp/.cpp    -- Lagrange order 1-10 (SP3 standard: order 9)
                             cubic spline interpolation (alternative)
keplerOrbit.hpp/.cpp      -- Kepler equations -> ECEF satellite position
                             (broadcast ephemeris -> satellite position)
sphericalHarmonics.hpp/.cpp -- associated Legendre functions,
                               synthesis/analysis of spherical harmonics
                               (for loki_gravity)
legendrePolynomials.hpp/.cpp -- normalized associated Legendre functions
stokesIntegral.hpp/.cpp   -- gravity anomaly -> geoid undulation
```

Note: `ellipsoid.hpp` already exists in `loki_core/math/` (from loki_geodesy).
`interpolation.hpp` is generic and reusable across all future modules.

---

### loki_gnss -- GNSS Processing Engine

**Philosophy:** Not a replacement for RTKLIB. An analytical layer over GNSS data
integrated with the LOKI ecosystem. Every output (ZTD, residuals, position,
integrity) is a TimeSeries or spatial dataset directly consumable by other modules.

**Prerequisite:** loki_geodesy COMPLETE. loki_core/math/interpolation COMPLETE.

**Internal structure:**
```
libs/loki_gnss/include/loki/gnss/
  io/
    gnssTypes.hpp           -- GnssObs, ObsEpoch, ObsFile,
                               BroadcastEph (GPS/GLO/GAL/BDS),
                               SP3Epoch, ClockCorrection,
                               TecMap, AntennaCalib, DcbBias
    rinexObsParser.hpp/.cpp -- RINEX 2.x + 3.x obs (GPS-only first)
    rinexNavParser.hpp/.cpp -- RINEX 2.x + 3.x nav (multi-GNSS)
    sp3Parser.hpp/.cpp      -- SP3a/SP3c
    clkParser.hpp/.cpp      -- RINEX CLK
    ionexParser.hpp/.cpp    -- IONEX TEC maps
    antexParser.hpp/.cpp    -- ANTEX PCO/PCV
    dcbParser.hpp/.cpp      -- CODE DCB / SINEX BIAS OSB
    leapSeconds.hpp/.cpp    -- GPS<->UTC conversion (IERS table)
    sbasParser.hpp/.cpp     -- SBAS L1 messages (RINEX B / raw binary stream)
                               EGNOS v2 message types 0-28
                               EGNOS v3 DFMC message types 31-40
                               GEO satellite almanac (type 17)
                               supported GEO: PRN 120, 123, 126, 136 (EGNOS)
  geometry/
    satPosition.hpp/.cpp    -- satellite position from broadcast (Kepler->ECEF)
                               position from SP3 (Lagrange interpolation order 9)
                               clock correction (broadcast / CLK RINEX)
    satVisibility.hpp/.cpp  -- elevation, azimuth
                               (calls loki_geodesy::CoordTransform)
                               elevation mask (configurable, default 5 deg)
    dopCalc.hpp/.cpp        -- GDOP, PDOP, HDOP, VDOP, TDOP per epoch
                               design matrix H assembly
    skyplot.hpp/.cpp        -- gnuplot polar diagram (elevation vs azimut)
                               coloring by SNR / system / RAIM status
  corrections/
    ionosphere.hpp/.cpp     -- Klobuchar blind model (broadcast coefficients)
                               IF combination L1/L2 (dual-freq elimination)
                               IONEX bilinear interpolation
                               NeQuick (Galileo, v2)
    troposphere.hpp/.cpp    -- Saastamoinen blind model
                               GPT3 (no met. measurement needed)
                               VMF3 mapping function
                               ZTD from external source
                               (calls loki_spatial for regional ZTD grid)
    relativity.hpp/.cpp     -- clock relativistic correction, Sagnac effect
    antenna.hpp/.cpp        -- PCO + PCV from ANTEX, ARP correction
    tides.hpp/.cpp          -- solid Earth tides (IERS 2010), ocean loading (BLQ)
    windup.hpp/.cpp         -- phase windup effect for PPP
  positioning/
    sppSolver.hpp/.cpp      -- SPP: weighted LSQ (calls loki_core/math/lsq)
                               weights: sin2(el), SNR model
                               iterative solution (pseudorange nonlinearity)
                               output: ECEF + covariance matrix
    pppSolver.hpp/.cpp      -- PPP: IF combination L1/L2
                               SP3 + CLK + DCB/BIAS
                               ZTD as estimated float parameter
                               float ambiguity resolution
                               output: position + ZTD + covariance
    kalmanTracker.hpp/.cpp  -- Kalman filter tracking
                               calls loki_kalman primitives
                               state: [X,Y,Z,cdt,ZTD] or [X,Y,Z,Vx,Vy,Vz,cdt]
                               Q/R estimation via EM (already in loki_kalman)
  quality/
    residuals.hpp/.cpp      -- post-fit residuals per satellite per epoch
                               pseudorange + phase residuals
                               normalized residuals (w-test)
    snrAnalysis.hpp/.cpp    -- SNR vs elevation model fitting
                               anomaly detection (multipath indicator)
                               MP1/MP2 combinations
    cycleSlip.hpp/.cpp      -- geometry-free L4 = L1 - L2
                               Melbourne-Wubbena combination
                               calls loki_homogeneity (change point)
    multipath.hpp/.cpp      -- MP1/MP2 combinations
                               spectral analysis (sidereal period ~23h56min)
                               calls loki_spectral, loki_ssa
    raim.hpp/.cpp           -- classical RAIM: chi2 test on residual vector
                               FDE: iterative satellite exclusion
                               Protection Levels HPL, VPL
                               ARAIM multi-constellation (v2)
                               robust RAIM heavy-tail noise (research)
    integrityMonitor.hpp/.cpp -- integrity status per epoch
                                 RAIM available / not available
                                 Alert limit comparison (HAL, VAL)
                                 continuous integrity time series
    sbasQc.hpp/.cpp           -- EGNOS signal quality monitoring:
                                 integrity availability (% time HPL < HAL)
                                 UDRE distribution per satellite -> loki_outlier
                                 GIVE map -> loki_spatial (IGP grid visualization)
                                 SBAS-derived vs. PPP-derived position comparison
                                 SBAS HPL vs. own RAIM HPL comparison
                                 Service Level assessment: PA / NPA / oceanic
                                 availability time series -> loki_homogeneity
                                 (EGNOS degradation during ionospheric storm)
  output/
    plotGnss.hpp/.cpp       -- skyplot, DOP vs time, SNR vs elevation,
                               residuals vs time/elevation,
                               RAIM test statistic with threshold,
                               ZTD/IWV time series,
                               integrity heatmap (epoch x satellite)
    mapExport.hpp/.cpp      -- trajectory -> GeoJSON
                               HTML + Leaflet.js + OSM background
                               (tile download, Mercator -> pixel,
                                calls loki_geodesy for projection)
    gnssProtocol.hpp/.cpp   -- positioning summary, satellite statistics,
                               RAIM report, atmosphere summary
  gnssAnalyzer.hpp/.cpp     -- main orchestrator, config-driven pipeline
```

**Input data QC** (integrated into gnssAnalyzer before processing):
- Observational: epoch completeness, cycle slip rate, SNR distribution,
  MP1/MP2 indicators, LLI flags, constellation coverage
- Ephemeris/SP3: broadcast vs. SP3 orbit difference, clock jumps
- Ionosphere (IONEX): ROTI, scintillation indicator
- Output: QC protocol + flagged epochs + recommendations

**DB integration:** positions, integrity_log, obs_summary -> SQLite

---

### loki_climatology -- GNSS Climatology

**Philosophy:** Pure analytical layer over loki_gnss outputs. Requires loki_gnss
ZTD time series as input. Connects GNSS atmosphere to the existing LOKI analysis
ecosystem.

**Single station analysis:**
- ZTD -> ZWD -> IWV/PWV (Tm factor, Bevis 1992 or GPT3)
- Validation: ZTD_GNSS vs. IGS ZTD products, NWM (ERA5), radiosonde
  (residual time series -> loki_homogeneity for drift detection)
- Long-term trends: loki_regression, loki_homogeneity
- Seasonal decomposition: loki_decomposition
- Draconitic period 351.4d in ZTD residuals: loki_spectral
- Extreme events: loki_evt
- Stationarity: loki_stationarity

**Network analysis (CZEPOS/SKPOS network, not a single station):**
- ZTD per station -> loki_spatial (kriging interpolation -> PWV map)
- Spatial trends -> loki_spatial + loki_regression
- Front / storm system detection

**Blind model research (publishable):**
- Saastamoinen/GPT3 vs. PPP ZTD -> residual time series
- loki_spectral: when does blind model fail (season, time of day)
- loki_spatial: where does it fail (geographic distribution)
- Local model calibration for Central Europe from GOPE archive

**NWM comparison:**
- ERA5 grid from db (era5_grid table) -> time series at station (lat, lon)
- ZTD_GNSS vs. ZTD_ERA5: bias, RMSE, correlation
- Residual -> loki_homogeneity, loki_decomposition

**DB integration:** ztd_series, iwv_series -> SQLite

---

### loki_gnss_rail -- Railway GNSS Navigation

**Philosophy:** SIL 4 context. GNSS alone does not achieve SIL 4 -- serves as
support for odometers and radars. Key value: integrity quantification, signal
quality monitoring, sensor fusion, degraded section mapping.

**Prerequisite:** loki_gnss fully COMPLETE.

**Internal structure:**
```
libs/loki_gnss_rail/include/loki/gnss_rail/
  railNetwork.hpp/.cpp      -- GeoJSON / OSM railway network loading
                               (OpenRailwayMap via Overpass API export)
                               R-tree indexing of segments
                               segment parametrization (chainage along track)
  mapMatcher.hpp/.cpp       -- project GNSS position onto nearest segment
                               1D parametrization: position = chainage s [m]
                               covariance reduction: 2D/3D -> 1D
                               Kalman update with rail geometry constraint
  sensorFusion.hpp/.cpp     -- tightly-coupled GNSS + odometer
                               state vector: [s, v, a, cdt]
                               odometric constraint: delta_s = v * dt
                               GNSS pseudorange constraint
                               calls loki_kalman (RTS smoother, EM Q/R)
                               loosely-coupled variant for comparison
  integrityRail.hpp/.cpp    -- SIL 4 relevant computations
                               positioning error bound per epoch
                               integrity flag (OK / WARNING / FAIL)
                               degraded mode: odometer only (no GNSS)
                               Alert limit comparison (HAL, VAL)
  degradationMapper.hpp/.cpp -- SNR degradation detection: bridges, tunnels,
                                 stations, reflective structures
                                 GNSS RAIM failures georeference onto track
                                 calls loki_clustering (clustered problem zones)
                                 output: GeoJSON degradation map
```

**Research contribution:**
Tropospheric correction for kinematic rail GNSS using interpolated ZTD field
from regional GNSS network (CZEPOS) via loki_spatial. Connects loki_climatology
(static network ZTD) with loki_gnss_rail (dynamic vehicle). Not standard practice
in railway GNSS applications -- publishable.

**Robust RAIM for railway:**
Classical RAIM assumes Gaussian noise. Railway multipath is strongly non-Gaussian
(hard reflections from structures). Robust RAIM with heavy-tail noise model is
a research contribution.

**DB integration:** track_positions, integrity_log -> SQLite

---

### loki_gravity -- Physical Geodesy

**Philosophy:** Gravitational field of the Earth, geoid, height systems, and
temporal gravity changes. Closely connected to loki_geodesy (height systems)
and loki_spatial (anomaly interpolation).

**Prerequisite:** loki_geodesy COMPLETE. loki_core/math/sphericalHarmonics COMPLETE.

**What it computes:**
- Normal gravity field (GRS80 -- Somigliana formula)
- Gravitational acceleration on ellipsoid and at height
- Spherical harmonic functions: EGM2008, EIGEN, GOCO coefficients
- Geoid undulation N (ellipsoidal - orthometric height)
- Stokes integral: gravity anomalies -> geoid undulation
- Vening-Meinesz integral: anomalies -> vertical deflections
- Height system connection: h_ellip = H_orthom + N (calls loki_geodesy)
- Free-air anomaly (Faye), Bouguer anomaly, isostatic anomaly
- Gravimetry interpolation -> loki_spatial (kriging on gravity grid)

**GRACE/GRACE-FO temporal changes (climatological connection):**
- Monthly solutions -> time series of spherical harmonic coefficients
- Terrestrial water storage anomalies (TWSA)
- Ice sheet melting, groundwater depletion
- calls loki_homogeneity, loki_spectral, loki_decomposition
- Connects to loki_climatology (climate signal in gravity field)

**New math in loki_core/math/:**
- `sphericalHarmonics.hpp/.cpp` -- synthesis and analysis
- `legendrePolynomials.hpp/.cpp` -- associated Legendre functions (normalized)
- `stokesIntegral.hpp/.cpp`

---

### loki_eo -- Earth Observation

**Philosophy:** Time series analysis of satellite-derived geophysical products.
v1 focuses on ready-made products (PSInSAR/SBAS outputs, Sentinel-2 indices)
not on raw SAR processing. New loaders needed in loki_core/io/.

**What it computes:**
- InSAR deformation time series (input: PSInSAR/SBAS outputs, not raw SAR):
  surface deformation [mm] per point per epoch
  calls loki_homogeneity (abrupt changes: landslide, subsidence onset)
  calls loki_decomposition (seasonal vs. linear deformation)
  calls loki_spectral (periodic movements, e.g. moisture-driven)
- Sentinel-2 optical products:
  NDVI time series per pixel / area
  land cover change: loki_clustering, loki_homogeneity
  methane index (SWIR combination) -- landfill monitoring
  vegetation cycle vs. anomalies: loki_decomposition
- Infrastructure monitoring:
  bridge, dam, building: InSAR deformation monitoring
  combination with GNSS reference station
- Landfill / contaminated site monitoring:
  InSAR surface subsidence + Sentinel-2 vegetation anomaly -> risk map

**New loaders in loki_core/io/:**
- `netcdfLoader.hpp/.cpp` -- ERA5, GRACE NetCDF
- `geotiffLoader.hpp/.cpp` -- GeoTIFF raster data

---

### loki_seismology -- GNSS Seismology

**Philosophy:** Detection of seismic signals in high-frequency GNSS coordinate
time series. Input: 1Hz or higher PPP / RTK positions from loki_gnss.

**What it computes:**
- Transient signal detection -> loki_kalman, loki_homogeneity
- loki_wavelet (planned): time-frequency analysis of seismic waves
- loki_spectral: Rayleigh/Love wave frequency content
- Earthquake detection:
  P/S wave arrival in GNSS series (position change ~cm)
  arrival time -> epicenter localization (calls loki_spatial)
  magnitude estimate from permanent displacement
- Post-seismic relaxation:
  logarithmic / exponential model -> loki_regression
- Inter-seismic:
  draconitic period 351.4d separation
  loki_spectral: geophysical signal vs. artefacts
  loki_ssa: signal from noise separation
- Slow slip events, aseismic deformation

---

## Module Roadmap (full picture)

| Priority | Module | Status |
|---|---|---|
| COMPLETE | all time series modules | COMPLETE |
| COMPLETE | loki_spatial, loki_geodesy, loki_multivariate | COMPLETE |
| Faza 0 | GNSS downloader scripts | PLANNED |
| Faza 0 | loki_core loaders (RINEX, SP3, IONEX, ...) | PLANNED |
| Faza 0 | loki_core db layer (SQLite) | PLANNED |
| Faza 0 | loki_core/math interpolation, keplerOrbit | PLANNED |
| Faza 1 | loki_gnss | PLANNED |
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
- CRITICAL: snhtDetector.hpp and snhtDetector.cpp exist in the repository as
  EMPTY FILES -- this is intentional. NEVER ask whether they are attached.
  Simply implement them according to the design. DO NOT DELETE THIS NOTE.
  (Poznamka: snht v ramci homogenizacie casoveho radu mame implementovany. 
  Ale z nejakeho divneho dovodu, si vzdy na zaciatku vlakna pytas snhtDetector - 
  co mi nedava zmysel a je to sakra otravne. Ak to mas v pamati ako systemovu vec,
  tak si to prosim vymaz!!!!!)
- snhtDetector.hpp and snhtDetector.cpp: SNHT is fully implemented inside
  loki_homogeneity. The files in the repository are EMPTY by design (system
  artifact). Do NOT request them. Do NOT ask about their content. The SNHT
  implementation lives in changePointDetector.hpp/.cpp and related files.
- CZEPOS is a network/service (GNSS reference network in Czech Republic),
  NOT a single station. SKPOS is the Slovak equivalent.
- loki_gnss is NOT a replacement for RTKLIB -- it is an analytical layer
  with deep integration into the LOKI ecosystem.
- GNSS multipath has sidereal periodicity (~23h 56min, not 24h).
- Draconitic period: 351.4 days (GPS orbital resonance with Sun).
- SP3 interpolation: Lagrange polynomial order 9 (IGS standard).
- CDDIS requires NASA Earthdata account for HTTPS access.
  Use browser download or earthdata Python CLI for first tests.
  CODE Bern and PECNY are freely accessible without authentication.
- Ocean loading BLQ: manual generation at holt.oso.chalmers.se/loading/
  (enter station name + coordinates -> download .blq file, one-time per station).
- EGNOS v3 (DFMC): Dual Frequency Multi Constellation (L1/L5, GPS+Galileo).
  HPL/VPL broadcast directly in SBAS messages -- no own RAIM computation needed.
  For loki_gnss_rail: SBAS integrity directly usable in SIL context without
  independent RAIM, which simplifies the integrity pipeline significantly.
  SBAS message format: RINEX B (.rnb) or raw binary (receiver-dependent).
  GEO satellites for EGNOS: PRN 120, 123, 126, 136.
  EGNOS v2: L1 only, GPS only, ~1-3m. v3: DFMC, ~0.5-1m, LPV-200 capable.
  Research: GIVE time series during geomagnetic storms -> loki_homogeneity +
  loki_spectral (periodicity of ionospheric degradation events).