# Roadmap

## Completed

All planned time series analysis modules are complete and validated on real observational data.

| Module | Description |
|--------|-------------|
| `loki_core` | Shared foundation: TimeSeries, math primitives, I/O, statistics |
| `loki_homogeneity` | Change point detection (SNHT, PELT, BOCPD), homogenization |
| `loki_outlier` | Outlier detection and replacement |
| `loki_filter` | Signal filtering suite |
| `loki_regression` | Parametric regression |
| `loki_stationarity` | Unit root and stationarity tests |
| `loki_arima` | ARIMA/SARIMA modelling and forecasting |
| `loki_ssa` | Singular Spectrum Analysis |
| `loki_decomposition` | Classical and STL decomposition |
| `loki_spectral` | FFT, Lomb-Scargle, STFT spectrogram |
| `loki_kalman` | Kalman filter, RTS smoother, EM estimation |
| `loki_qc` | Quality control pipeline |
| `loki_clustering` | k-means, DBSCAN |
| `loki_simulate` | Bootstrap simulation and confidence intervals |
| `loki_evt` | Extreme Value Theory (GPD/POT, GEV) |
| `loki_kriging` | Temporal Kriging with variogram fitting |
| `loki_spline` | B-spline approximation with CV |

---

## In development

### `loki_spatial`

2D spatial Kriging and field interpolation.

Extension of the temporal Kriging framework to two-dimensional spatial fields.
Variogram fitting using Euclidean lag distance, isotropy verification via
directional variograms, and grid-based prediction output.

Planned variants: Simple, Ordinary, and Universal Spatial Kriging.

---

## Planned

### `loki_multivariate`

Multivariate time series analysis.

PCA/SVD-based decomposition, cross-correlation analysis, vector autoregression (VAR),
and multivariate outlier detection.

### `loki_wavelet`

Discrete and Continuous Wavelet Transform.

DWT via Mallat filter bank (Haar, Daubechies, Symlets), CWT with Morlet and
Mexican hat wavelets, wavelet power spectrum with significance testing
(Torrence & Compo 1998), and wavelet denoising (VisuShrink / SureShrink).

### `loki_geodesy`

Geodetic computations integrated into the LOKI ecosystem.

Coordinate transformations, datum conversions, and geodetic time series
utilities tailored to GNSS analysis workflows.

---

## Future

### `loki_realtime`

Online and streaming change point detection for real-time sensor data.
Complement to the offline batch processing in existing modules.

### `loki_ml`

Machine learning based anomaly detection: Isolation Forest, Local Outlier Factor,
autoencoder-based detection.

---

## Separate project

### GeoKit

A standalone geodetic computation toolkit. Scope and relationship to `loki_geodesy`
to be defined.
