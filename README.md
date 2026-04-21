# LOKI

**Modular C++20 framework for statistical analysis of scientific time series**

LOKI originated as a toolkit for detecting false (inhomogeneous) events in climatological
observations — named after the Norse god of mischief, a fitting patron for a detector of
deception in data. It has since grown into a general-purpose framework covering time series
analysis, spatial interpolation, and multivariate statistics.

---

## What LOKI does

LOKI provides a suite of command-line analysis programs, each driven by a JSON configuration
file. A typical workflow loads a time series (climatological, GNSS, or sensor data),
runs one or more analysis pipelines, and writes protocol files, CSV exports, and plots
to a structured output directory.

**Supported data domains:**
- Climatological time series (6-hour IWV, temperature, precipitation)
- GNSS station coordinates and tropospheric delays
- Train and vehicle sensor data (1 Hz to millisecond resolution)

---

## Modules

| Module | Description |
|--------|-------------|
| [`loki_homogeneity`](libs/loki_homogeneity) | Change point detection (SNHT, PELT, BOCPD), series adjustment, homogenization |
| [`loki_outlier`](libs/loki_outlier) | Outlier detection (IQR, MAD, Z-score, hat matrix leverage) and cleaning |
| [`loki_filter`](libs/loki_filter) | Signal filtering (moving average, EMA, LOESS, Savitzky-Golay, spline, kernel) |
| [`loki_regression`](libs/loki_regression) | Linear, polynomial, harmonic, robust, nonlinear, and calibration regression |
| [`loki_stationarity`](libs/loki_stationarity) | Stationarity tests: ADF, KPSS, Phillips-Perron, Ljung-Box |
| [`loki_arima`](libs/loki_arima) | ARIMA and SARIMA modelling, automatic order selection, forecasting |
| [`loki_ssa`](libs/loki_ssa) | Singular Spectrum Analysis with randomized SVD |
| [`loki_decomposition`](libs/loki_decomposition) | Classical and STL decomposition (trend, seasonal, residual) |
| [`loki_spectral`](libs/loki_spectral) | FFT, Lomb-Scargle, STFT spectrogram, peak detection |
| [`loki_kalman`](libs/loki_kalman) | Kalman filter (Joseph form), RTS smoother, EM parameter estimation |
| [`loki_qc`](libs/loki_qc) | Quality control: gap detection, spike and range checks, flagging |
| [`loki_clustering`](libs/loki_clustering) | k-means (k-means++), DBSCAN, silhouette-based automatic k selection |
| [`loki_simulate`](libs/loki_simulate) | ARIMA and Kalman bootstrap simulation with block confidence intervals |
| [`loki_evt`](libs/loki_evt) | Extreme Value Theory: GPD/POT, GEV/block maxima, profile likelihood CI |
| [`loki_kriging`](libs/loki_kriging) | Temporal Kriging: Simple, Ordinary, Universal; variogram fitting, LOO CV |
| [`loki_spline`](libs/loki_spline) | B-spline approximation with k-fold CV for automatic control point selection |

All modules share a common [`loki_core`](libs/loki_core) library providing the `TimeSeries`
and `TimeStamp` types, data loading, logging, mathematical primitives (LSQ, SVD, Kriging
math, B-spline basis), and statistical utilities.

---

## Build

**Requirements:** GCC 13+, CMake 3.25+, MSYS2 UCRT64 (Windows) or standard GCC toolchain (Linux)

All dependencies are fetched automatically via CMake `FetchContent` — no manual installation needed.

```bash
# Clone
git clone https://github.com/mel105/LOKI.git
cd LOKI

# Configure and build (debug)
cmake --preset debug
cmake --build --preset debug

# Run tests
ctest --preset debug
```

**Dependencies fetched automatically:**

| Library | Version | Purpose |
|---------|---------|---------|
| Eigen3 | 3.4.0 | Linear algebra, LSQ, SVD |
| nlohmann_json | 3.11.3 | Configuration files |
| Catch2 | 3.5.2 | Unit and integration tests |

---

## Usage

Each module is a standalone executable that reads a JSON configuration file:

```bash
loki_homogeneity.exe config/homogeneity.json
loki_outlier.exe     config/outlier.json
loki_spectral.exe    config/spectral.json
```

Output is written to a structured workspace:

```
OUTPUT/
  IMG/        # plots (PNG, EPS, SVG)
  CSV/        # numerical results
  PROTOCOLS/  # plain-text analysis reports
  LOG/        # execution logs
```

Configuration reference: [`CONFIG_REFERENCE.md`](CONFIG_REFERENCE.md)

---

## Examples

*Screenshots and worked examples coming soon.*

---

## Documentation

API reference (Doxygen): run `doxygen Doxyfile` in the repository root.
Output: `docs/doxygen/html/index.html`

---

## Roadmap

- `loki_spatial` — 2D spatial Kriging and interpolation *(in development)*
- `loki_multivariate` — multivariate time series analysis *(planned)*
- `loki_wavelet` — discrete and continuous wavelet transform *(planned)*
- `loki_geodesy` — geodetic computations *(planned)*

---

## Author

Michal — [github.com/mel105](https://github.com/mel105)
