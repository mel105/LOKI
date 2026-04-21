@mainpage LOKI -- Statistical Analysis Framework

## Overview

**LOKI** is a modular C++20 framework for statistical analysis of scientific time series data.

Originally developed for detecting inhomogeneities in climatological observations,
LOKI has grown into a general-purpose toolkit covering time series analysis,
spatial interpolation, and multivariate statistics.

**Current domains:**
- Time series analysis (1D) -- complete, 19 modules
- Spatial analysis (2D) -- in development
- Multivariate analysis -- planned
- Geodetic computations -- planned

---

## Module Index

| Module | Namespace | Description |
|--------|-----------|-------------|
| `loki_core` | `loki` | Core types: TimeSeries, TimeStamp, Loader, Logger, math primitives, statistical utilities |
| `loki_homogeneity` | `loki::homogeneity` | Change point detection (SNHT, PELT, BOCPD), series adjustment, homogenization |
| `loki_outlier` | `loki::outlier` | Outlier detection (IQR, MAD, Z-score, hat matrix leverage) and cleaning |
| `loki_filter` | `loki::filter` | Signal filtering (moving average, EMA, LOESS, Savitzky-Golay, spline, kernel) |
| `loki_regression` | `loki::regression` | Linear, polynomial, harmonic, robust, nonlinear, and calibration regression |
| `loki_stationarity` | `loki::stationarity` | Stationarity tests (ADF, KPSS, Phillips-Perron, Ljung-Box, PACF) |
| `loki_arima` | `loki::arima` | ARIMA and SARIMA modelling, order selection, forecasting |
| `loki_ssa` | `loki::ssa` | Singular Spectrum Analysis with randomized SVD |
| `loki_decomposition` | `loki::decomposition` | Classical and STL decomposition (trend, seasonal, residual) |
| `loki_spectral` | `loki::spectral` | FFT, Lomb-Scargle, STFT spectrogram, peak detection |
| `loki_kalman` | `loki::kalman` | Kalman filter (Joseph form), RTS smoother, EM parameter estimation |
| `loki_qc` | `loki::qc` | Quality control pipeline: gap detection, flagging, range and spike checks |
| `loki_clustering` | `loki::clustering` | k-means (k-means++), DBSCAN, silhouette-based automatic k selection |
| `loki_simulate` | `loki::simulate` | ARIMA and Kalman bootstrap simulation with confidence interval estimation |
| `loki_evt` | `loki::evt` | Extreme Value Theory: GPD/POT, GEV/block maxima, profile likelihood CI |
| `loki_kriging` | `loki::kriging` | Temporal Kriging: Simple, Ordinary, Universal; variogram fitting, LOO CV |
| `loki_spline` | `loki::spline` | B-spline approximation with k-fold CV for automatic control point selection |

---

## Build

```bash
# Configure
cmake --preset debug

# Build
cmake --build --preset debug

# Run tests
ctest --preset debug
```

**Requirements:** C++20, GCC 13+, CMake 3.25+, Eigen3, nlohmann_json, Catch2 (auto-fetched).

---

## Dependencies

| Library | Purpose |
|---------|---------|
| Eigen3 3.4.0 | Linear algebra, LSQ, SVD |
| nlohmann_json 3.11.3 | Configuration files |
| Catch2 3.5.2 | Unit and integration tests |

All dependencies are fetched automatically via CMake `FetchContent`.
