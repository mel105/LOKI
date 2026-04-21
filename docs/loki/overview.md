# LOKI — Overview

**LOKI** is a modular C++20 framework for statistical analysis of scientific time series.

The name originates from the Norse god of mischief — the project began as a toolkit for
detecting false (inhomogeneous) events in climatological observations. It has since grown
into a general-purpose framework covering the full analysis pipeline from raw data ingestion
to publication-quality output.

---

## What LOKI does

LOKI provides a suite of standalone command-line programs, each driven by a JSON
configuration file. A typical workflow:

1. Load a time series from a structured text file
2. Run one or more analysis pipelines (outlier detection, homogeneity testing, spectral analysis, ...)
3. Write protocol files, CSV exports, and plots to a structured output directory

```
INPUT/
  my_data.txt          # raw observational data

OUTPUT/
  IMG/                 # plots (PNG, EPS, SVG)
  CSV/                 # numerical results
  PROTOCOLS/           # plain-text analysis reports
  LOG/                 # execution logs
```

---

## Supported data domains

| Domain | Description |
|--------|-------------|
| Climatological | 6-hour IWV, temperature, precipitation; ~36 000 observations over 25 years |
| GNSS | Station coordinates and velocities; draconitic period ~351.4 days |
| Sensor | Train and vehicle sensor data; 1 Hz to millisecond resolution |

---

## Architecture

LOKI follows a strict layered architecture:

```
loki_core                          (shared foundation)
  timeseries/  -- TimeSeries, TimeStamp, GapFiller, Deseasonalizer
  math/        -- LSQ, SVD, B-spline, Kriging primitives, Nelder-Mead
  stats/       -- descriptive stats, bootstrap, hypothesis tests
  io/          -- Loader, DataManager, Plot, Gnuplot

loki_<module>                      (thin orchestrator per analysis domain)
  <module>Analyzer     -- top-level entry point
  <module>Result       -- structured result type
  plot<Module>         -- gnuplot-based visualisation
```

All mathematical primitives live in `loki_core/math/`. Module libraries are thin
orchestrators that call core primitives and format results. This keeps module code
small and testable, and avoids duplication across modules.

---

## Design principles

- **No silent failures** — all errors propagate as typed exceptions (`LOKIException` hierarchy).
- **Configuration-driven** — every parameter is controlled via JSON; no recompilation needed.
- **Reproducible output** — protocol files record all parameters, statistics, and results in plain text.
- **Dependency-free deployment** — only Eigen3, nlohmann_json, and Catch2; all fetched automatically by CMake.

---

## Current status

| Domain | Status |
|--------|--------|
| Time series analysis (1D) | Complete — 16 analysis modules |
| Spatial analysis (2D) | In development — `loki_spatial` |
| Multivariate analysis | Planned |
| Geodetic computations | Planned |

See the full [Module list](modules.md) and [Roadmap](roadmap.md).
