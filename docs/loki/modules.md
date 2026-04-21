# Modules

LOKI consists of a shared core library and sixteen analysis modules.
Each module is a standalone executable driven by a JSON configuration file.

---

## loki_core

The shared foundation used by all modules.

| Subsystem | Contents |
|-----------|----------|
| `timeseries/` | `TimeSeries`, `TimeStamp`, `GapFiller`, `Deseasonalizer`, `MedianYearSeries` |
| `math/` | LSQ solver, SVD, randomized SVD, LM solver, hat matrix, B-spline basis, Kriging math primitives, Nelder-Mead optimiser, cubic spline, embed matrix, w-correlation |
| `stats/` | Descriptive statistics, bootstrap, permutation tests, hypothesis tests, distributions, sampling |
| `io/` | `Loader`, `DataManager`, `Plot`, `Gnuplot` pipe wrapper |
| `core/` | Exception hierarchy, logger, configuration loader, version |

---

## Analysis modules

### Homogeneity

**`loki_homogeneity`** — Change point detection and series correction

Detects structural breaks (shifts in mean) in time series using multiple algorithms.
Adjusts the series to remove detected inhomogeneities.

| Algorithm | Description |
|-----------|-------------|
| SNHT | Standard Normal Homogeneity Test (Alexandersson 1986) |
| PELT | Pruned Exact Linear Time (Killick et al. 2012) |
| BOCPD | Bayesian Online Change Point Detection (Adams & MacKay 2007) |

---

### Outlier detection

**`loki_outlier`** — Outlier identification and replacement

Identifies and replaces anomalous observations using statistical thresholds or
leverage-based detection. Supports optional deseasonalization before detection.

| Method | Description |
|--------|-------------|
| IQR | Interquartile range fence |
| MAD | Median absolute deviation |
| Z-score | Standard score threshold |
| Hat matrix | Leverage-based detection (O4 algorithm) |

---

### Filtering

**`loki_filter`** — Signal smoothing and noise reduction

| Filter | Description |
|--------|-------------|
| Moving average | Simple and weighted |
| EMA | Exponential moving average |
| LOESS | Locally weighted scatterplot smoothing |
| Savitzky-Golay | Polynomial smoothing with derivative estimation |
| Spline | Cubic spline gap-filling and smoothing |
| Kernel | Gaussian and Epanechnikov kernel smoother |

---

### Regression

**`loki_regression`** — Parametric curve fitting

| Regressor | Description |
|-----------|-------------|
| Linear | OLS with diagnostics |
| Polynomial | Degree-n polynomial |
| Harmonic | Fourier series (annual, semi-annual, ...) |
| Robust | IRLS with Huber and Tukey weights |
| Nonlinear | Levenberg-Marquardt with user-defined model |
| Calibration | Sensor-to-reference calibration via SVD |

---

### Stationarity

**`loki_stationarity`** — Unit root and stationarity tests

| Test | Description |
|------|-------------|
| ADF | Augmented Dickey-Fuller |
| KPSS | Kwiatkowski-Phillips-Schmidt-Shin |
| PP | Phillips-Perron |
| Ljung-Box | Residual autocorrelation test |

---

### ARIMA

**`loki_arima`** — Time series modelling and forecasting

Full ARIMA and SARIMA pipeline with automatic order selection (AIC/BIC grid search),
parameter estimation, residual diagnostics, and multi-step forecasting.

---

### Singular Spectrum Analysis

**`loki_ssa`** — SSA decomposition and reconstruction

Trajectory matrix embedding, randomized SVD, eigengroup identification,
w-correlation analysis, selective reconstruction, and forecasting.

---

### Decomposition

**`loki_decomposition`** — Trend-seasonal-residual decomposition

| Method | Description |
|--------|-------------|
| Classical | Moving average trend + stable seasonal pattern |
| STL | Seasonal-Trend decomposition using LOESS; handles evolving seasonality |

---

### Spectral analysis

**`loki_spectral`** — Frequency domain analysis

| Tool | Description |
|------|-------------|
| FFT | Fast Fourier Transform with windowing |
| Lomb-Scargle | Spectral estimation for unevenly sampled data |
| STFT | Short-Time Fourier Transform spectrogram |
| Peak detection | Automated identification of spectral peaks |

---

### Kalman filter

**`loki_kalman`** — State space modelling

Kalman filter (Joseph-form covariance update for numerical stability),
RTS smoother, EM-based parameter estimation for Q and R matrices, and forecasting.

---

### Quality control

**`loki_qc`** — Automated data quality assessment

Five-stage pipeline: gap detection, range checks, spike detection, step detection,
and consistency checks. Produces flagged output and a QC report.

---

### Clustering

**`loki_clustering`** — Time series segmentation and pattern discovery

| Algorithm | Description |
|-----------|-------------|
| k-means | k-means++ initialisation with silhouette-based automatic k selection |
| DBSCAN | Density-based spatial clustering; no k required |

---

### Simulation

**`loki_simulate`** — Bootstrap uncertainty quantification

Block bootstrap simulation for ARIMA and Kalman models.
Produces empirical confidence intervals for forecasts and fitted values.

---

### Extreme Value Theory

**`loki_evt`** — Tail risk and return period estimation

| Method | Description |
|--------|-------------|
| GPD / POT | Generalised Pareto Distribution, Peaks Over Threshold |
| GEV / Block maxima | Generalised Extreme Value distribution |

Profile likelihood confidence intervals — required for SIL 4 reliability targets
(failure intensity below 10⁻⁸).

---

### Kriging

**`loki_kriging`** — Temporal Kriging and spatial interpolation

| Variant | Description |
|---------|-------------|
| Simple Kriging | Known mean |
| Ordinary Kriging | Unknown constant mean |
| Universal Kriging | Unknown trend (polynomial drift) |

Variogram fitting with multiple models (spherical, exponential, Gaussian, Matern).
O(n²) leave-one-out cross-validation shortcut.

---

### B-spline approximation

**`loki_spline`** — Smooth curve fitting with automatic complexity selection

B-spline approximation with configurable degree (default: cubic).
k-fold cross-validation with one-standard-error elbow rule for automatic
control point selection. Supports uniform and chord-length knot placement.
