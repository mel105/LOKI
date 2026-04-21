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

## Getting Started

### Prerequisites

#### Windows (MSYS2 UCRT64) -- tested and supported

1. Install [MSYS2](https://www.msys2.org/). During setup, choose the **UCRT64** environment.

2. Open the **UCRT64** shell and install the required toolchain:

```bash
pacman -S --needed \
    mingw-w64-ucrt-x86_64-gcc \
    mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-ninja \
    mingw-w64-ucrt-x86_64-gnuplot \
    git
```

3. Verify the installation:

```bash
gcc --version      # expect 13.x or newer
cmake --version    # expect 3.25 or newer
gnuplot --version  # expect 5.x or newer
```

> **Note:** Use the UCRT64 shell for all build and run commands.
> The MINGW64 shell also works but UCRT64 is the recommended environment.

#### Linux -- build support planned, not yet tested

LOKI is developed on Windows/MSYS2. Linux support is planned and the codebase uses
standard C++20 with no platform-specific APIs, so a GCC 13+ toolchain with CMake 3.25+
and gnuplot should work. Verified Linux instructions will be added once testing is complete.

---

### Clone

```bash
git clone https://github.com/mel105/LOKI.git
cd LOKI
```

---

### Build

All dependencies (Eigen3, nlohmann_json, Catch2) are fetched automatically via CMake
`FetchContent` on first configure -- no manual installation needed. An internet connection
is required for the first build.

```bash
# Debug build (with tests and sanitizers)
cmake --preset debug
cmake --build --preset debug

# Release build (optimized, no tests)
cmake --preset release
cmake --build --preset release
```

Build output lands in `build/debug/` or `build/release/`.

On Windows, copy the required UCRT64 runtime DLLs alongside the executables if running
them outside the MSYS2 shell:

```
libgcc_s_seh-1.dll
libstdc++-6.dll
libwinpthread-1.dll
```

These are found in `C:/msys64/ucrt64/bin/`.

---

### Run tests

```bash
ctest --preset debug
```

Or run individual test executables directly from `build/debug/tests/`.

---

### First run

Each module reads a JSON configuration file that controls input data, analysis parameters,
and output. A minimal example for `loki_outlier`:

1. Create a workspace directory with your input file:

```
my_workspace/
  INPUT/
    my_data.txt
  config.json
```

2. Write/use a minimal `config.json`:

```json
{
    "input": {
        "file": "my_data.txt",
        "time_format": "mjd",
        "time_columns": [0],
        "delimiter": ";",
        "columns": [1]
    },
    "output": {
        "log_level": "info"
    },
    "plots": {
        "output_format": "png",
        "enabled": {
            "time_series": true,
            "original_series": true,
            "adjusted_series": true
        }
    },
    "outlier": {
        "deseasonalization": {
            "method": "none"
        },
        "detection": {
            "method": "iqr",
            "iqr_multiplier": 1.5
        },
        "replacement_strategy": "linear"
    }
}
```

3. Run from the workspace directory:

```bash
loki_outlier.exe config.json (config contains the appropriate json file.) 
```

4. Results appear in:

```
my_workspace/
  OUTPUT/
    IMG/        # plots
    CSV/        # numerical results
    PROTOCOLS/  # plain-text analysis report
    LOG/        # execution log
```

For a full description of all configuration options, see [`CONFIG_REFERENCE.md`](CONFIG_REFERENCE.md).

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

## Examples

*Screenshots and worked examples coming soon.*

---

## Documentation

API reference (Doxygen): run `doxygen Doxyfile` in the repository root.
Output: `docs/doxygen/html/index.html`

Configuration reference: [`CONFIG_REFERENCE.md`](CONFIG_REFERENCE.md)

---

## Roadmap

- `loki_spatial` -- 2D spatial Kriging and interpolation *(in development)*
- `loki_multivariate` -- multivariate time series analysis *(planned)*
- `loki_wavelet` -- discrete and continuous wavelet transform *(planned)*
- `loki_geodesy` -- geodetic computations *(planned)*

---

## Author

Michal -- [github.com/mel105](https://github.com/mel105)