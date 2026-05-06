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
    +-- gnss_download/               -- shell script for GNSS product download
        +-- download.sh              -- COMPLETE, see README.md in same directory
        +-- README.md                -- usage, product status, known issues
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

#### 0.1 GNSS Data Downloader (`tools/gnss_download/`) -- COMPLETE

`tools/gnss_download/download.sh` is a bash script for downloading GNSS products
from verified institutional sources. See `tools/gnss_download/README.md` for full
usage documentation.

Working products and sources:
```
antex    -- files.igs.org (HTTPS, anon)
nav      -- cddis.nasa.gov (HTTPS, NASA Earthdata ~/.netrc)
obs      -- epncb.oma.be EUREF/EPN (FTP anon), format: .crx.gz (Hatanaka)
sp3      -- ftp.aiub.unibe.ch CODE_MGEX/CODE/YYYY/ (FTP anon)
clk      -- ftp.aiub.unibe.ch CODE_MGEX/CODE/YYYY/ (FTP anon)
ionex    -- ftp.aiub.unibe.ch CODE/YYYY/ (FTP anon)
bias     -- ftp.aiub.unibe.ch CODE_MGEX/CODE/YYYY/ (FTP anon)
dcb      -- ftp.aiub.unibe.ch CODE/YYYY/ (FTP anon), P1C1 + P1P2 only
vmf3     -- vmf.geo.tuwien.ac.at VMF3_OP/YYYY/ (HTTPS anon), format: YYYYMMDD
misc     -- datacenter.iers.org + hpiers.obspm.fr (HTTPS anon)
```

Known issues with downloader (pending fix):
```
sinex    -- CDDIS OAuth cookie not handled in curl (404 in script, works in browser)
tropo    -- Same CDDIS OAuth issue
met      -- PECNY FTP denies access; EUREF has no MET files for GOPE
egnos    -- ESA GSSC SFTP port 2200 refused; no working public source found
era5     -- Not yet tested; requires ~/.cdsapirc
```

Key implementation notes:
- OBS files are Hatanaka compressed (.crx.gz). Parsers must handle gunzip + crx2rnx.
- VMF3 uses YYYYMMDD date format in filenames (NOT DOY format).
- CODE Bern SP3/CLK/IONEX/BIAS: files live in CODE_MGEX/CODE/YYYY/ (with year subdir).
- CODE Bern DCB: files live in CODE/YYYY/ (with year subdir).
- CDDIS NAV requires cookie jar + ~/.netrc for OAuth redirect handling.
- P2C2 DCB does not exist on CODE Bern -- do not attempt to download it.
- GPS week calculation: epoch_unix - 315964800 (GPS epoch 1980-01-06) / 604800.
- Data directory: INPUT/GNSS/gnss_data/ (sibling of LOKI/, not inside it).
- Log file: INPUT/GNSS/download.log

#### 0.2 New loki_core/io/ loaders (C++) -- PLANNED (next)

Parsers to implement (in dependency order for SPP):
```
gnssTypes.hpp             -- data structures (no .cpp needed)
rinexNavParser.hpp/.cpp   -- broadcast ephemeris (RINEX 2.x + 3.x nav)
rinexObsParser.hpp/.cpp   -- observations (RINEX 2.x + 3.x obs, Hatanaka .crx)
sp3Parser.hpp/.cpp        -- SP3a/SP3c precise ephemeris
clkParser.hpp/.cpp        -- RINEX CLK precise clocks
ionexParser.hpp/.cpp      -- IONEX TEC maps
antexParser.hpp/.cpp      -- ANTEX PCO/PCV antenna calibrations
dcbParser.hpp/.cpp        -- CODE DCB / SINEX BIAS OSB
vmf3Parser.hpp/.cpp       -- VMF3 gridded mapping function
sinexLoader.hpp/.cpp      -- SINEX station coordinates, ZTD products
```

#### 0.3 SQLite database layer (loki_core/io/) -- PLANNED (after loki_gnss)

DB is populated by loki_gnss computed outputs, NOT raw files.
Raw files remain in INPUT/GNSS/gnss_data/ as archive.
DB contains only derived/computed quantities:
```
positions     -- computed ECEF + geodetic coordinates per epoch
ztd_series    -- ZTD/ZHD/ZWD time series from PPP
obs_summary   -- SNR, PDOP, cycle slip statistics per epoch
integrity_log -- RAIM status, HPL, VPL per epoch
iwv_series    -- IWV/PWV derived from ZWD
era5_grid     -- NWM grid values at selected nodes
```

#### 0.4 New loki_core/math/ primitives -- PLANNED

```
interpolation.hpp/.cpp    -- Lagrange order 1-10 (SP3 standard: order 9)
keplerOrbit.hpp/.cpp      -- Kepler equations -> ECEF satellite position
sphericalHarmonics.hpp/.cpp -- for loki_gravity
legendrePolynomials.hpp/.cpp -- normalized associated Legendre functions
stokesIntegral.hpp/.cpp   -- gravity anomaly -> geoid undulation
```

---

### loki_gnss -- GNSS Processing Engine

**Philosophy:** Not a replacement for RTKLIB. An analytical layer over GNSS data
integrated with the LOKI ecosystem. Every output (ZTD, residuals, position,
integrity) is a TimeSeries or spatial dataset directly consumable by other modules.

**Prerequisite:** loki_geodesy COMPLETE. loki_core/math/interpolation COMPLETE.
loki_core/io/ GNSS parsers COMPLETE.

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
    sbasParser.hpp/.cpp     -- SBAS L1 messages (RINEX B)
                               EGNOS v2 message types 0-28
                               EGNOS v3 DFMC message types 31-40
                               supported GEO: PRN 120, 123, 126, 136 (EGNOS)
  geometry/
    satPosition.hpp/.cpp    -- satellite position from broadcast (Kepler->ECEF)
                               position from SP3 (Lagrange interpolation order 9)
                               clock correction (broadcast / CLK RINEX)
    satVisibility.hpp/.cpp  -- elevation, azimuth
                               (calls loki_geodesy::CoordTransform)
                               elevation mask (configurable, default 5 deg)
    dopCalc.hpp/.cpp        -- GDOP, PDOP, HDOP, VDOP, TDOP per epoch
    skyplot.hpp/.cpp        -- gnuplot polar diagram (elevation vs azimuth)
  corrections/
    ionosphere.hpp/.cpp     -- Klobuchar, IF combination, IONEX interpolation
    troposphere.hpp/.cpp    -- Saastamoinen, GPT3, VMF3 mapping function
    relativity.hpp/.cpp     -- clock relativistic correction, Sagnac effect
    antenna.hpp/.cpp        -- PCO + PCV from ANTEX, ARP correction
    tides.hpp/.cpp          -- solid Earth tides (IERS 2010), ocean loading (BLQ)
    windup.hpp/.cpp         -- phase windup effect for PPP
  positioning/
    sppSolver.hpp/.cpp      -- SPP: weighted LSQ, iterative solution
    pppSolver.hpp/.cpp      -- PPP: IF combination L1/L2, float ambiguity
    kalmanTracker.hpp/.cpp  -- Kalman filter tracking (calls loki_kalman)
  quality/
    residuals.hpp/.cpp      -- post-fit residuals, w-test
    snrAnalysis.hpp/.cpp    -- SNR vs elevation, MP1/MP2
    cycleSlip.hpp/.cpp      -- geometry-free L4, Melbourne-Wubbena
    multipath.hpp/.cpp      -- MP1/MP2, spectral analysis (sidereal ~23h56min)
    raim.hpp/.cpp           -- classical RAIM, FDE, HPL/VPL, ARAIM
    integrityMonitor.hpp/.cpp -- integrity status per epoch
    sbasQc.hpp/.cpp         -- EGNOS signal quality monitoring
  output/
    plotGnss.hpp/.cpp       -- skyplot, DOP, SNR, residuals, ZTD, integrity
    mapExport.hpp/.cpp      -- trajectory -> GeoJSON + HTML/Leaflet
    gnssProtocol.hpp/.cpp   -- positioning summary, RAIM report
  gnssAnalyzer.hpp/.cpp     -- main orchestrator, config-driven pipeline
```

**DB integration:** positions, integrity_log, obs_summary -> SQLite (after loki_gnss)

---

### loki_climatology -- GNSS Climatology

**Philosophy:** Pure analytical layer over loki_gnss outputs. Requires loki_gnss
ZTD time series as input.

**Single station analysis:**
- ZTD -> ZWD -> IWV/PWV (Tm factor, Bevis 1992 or GPT3)
- Validation: ZTD_GNSS vs. IGS ZTD products, NWM (ERA5), radiosonde
- Long-term trends: loki_regression, loki_homogeneity
- Seasonal decomposition: loki_decomposition
- Draconitic period 351.4d in ZTD residuals: loki_spectral
- Extreme events: loki_evt

**Network analysis:**
- ZTD per station -> loki_spatial (kriging -> PWV map)
- Front / storm system detection

**DB integration:** ztd_series, iwv_series -> SQLite

---

### loki_gnss_rail -- Railway GNSS Navigation

**Philosophy:** SIL 4 context. GNSS serves as support for odometers and radars.
Key value: integrity quantification, signal quality monitoring, sensor fusion.

**Prerequisite:** loki_gnss fully COMPLETE.

**DB integration:** track_positions, integrity_log -> SQLite

---

### loki_gravity -- Physical Geodesy

**Prerequisite:** loki_geodesy COMPLETE. loki_core/math/sphericalHarmonics COMPLETE.

---

### loki_eo -- Earth Observation

**New loaders in loki_core/io/:**
- `netcdfLoader.hpp/.cpp` -- ERA5, GRACE NetCDF
- `geotiffLoader.hpp/.cpp` -- GeoTIFF raster data

---

### loki_seismology -- GNSS Seismology

Input: 1Hz or higher PPP / RTK positions from loki_gnss.

---

## Module Roadmap (full picture)

| Priority | Module | Status |
|---|---|---|
| COMPLETE | all time series modules | COMPLETE |
| COMPLETE | loki_spatial, loki_geodesy, loki_multivariate | COMPLETE |
| Faza 0 | GNSS downloader (`tools/gnss_download/download.sh`) | COMPLETE |
| Faza 0 | loki_core/io/ GNSS parsers (gnssTypes, rinexNav, rinexObs, sp3, ...) | PLANNED |
| Faza 0 | loki_core/math/ interpolation, keplerOrbit | PLANNED |
| Faza 1 | loki_gnss | PLANNED |
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
  artifact). Do NOT request them. Do NOT ask about their content. The SNHT
  implementation lives in changePointDetector.hpp/.cpp and related files.
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
  Parsers must handle: gunzip -> crx2rnx -> standard RINEX 3.
- VMF3 filenames use YYYYMMDD format (NOT DOY): VMF3_20240315.H00
  VMF3_OP contains operational data (2008-present).
  VMF3_FC contains forecast data (2018-present).
  VMF3_EI (ERA5-based reanalysis) ends at 2019 -- do not use for 2020+.
- CODE Bern directory structure:
  SP3/CLK/BIAS: ftp.aiub.unibe.ch/CODE_MGEX/CODE/YYYY/
  IONEX/DCB:    ftp.aiub.unibe.ch/CODE/YYYY/
  P2C2 DCB does not exist -- only P1C1 and P1P2 are available.
- GPS week = (unix_epoch - 315964800) / 604800
- EGNOS v3 (DFMC): L1+L5, GPS+Galileo, ~0.5-1m, LPV-200 capable.
  GEO satellites: PRN 120, 123, 126, 136.
  No working public download source currently available (ESA GSSC SFTP down).
- DB architecture decision: DB is populated AFTER loki_gnss computes results.
  Raw RINEX/SP3/CLK files are NEVER stored in DB -- they stay in INPUT/GNSS/.
  DB contains only computed outputs: positions, ZTD, integrity, obs_summary.
- Ocean loading BLQ: manual generation at holt.oso.chalmers.se/loading/
  (enter station name + coordinates -> download .blq file, one-time per station).