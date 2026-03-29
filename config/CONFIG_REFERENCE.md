# LOKI Configuration Reference

This document describes all configuration options for LOKI programs.
Each program reads a JSON file whose path is passed as the first argument
(default paths shown in each section).

Common sections (`input`, `output`, `plots`, `stats`) are shared across all programs.
Program-specific sections (`outlier`, `homogeneity`) are ignored by programs that do not use them.

---

## Table of Contents

1. [Common: input](#1-common-input)
2. [Common: output](#2-common-output)
3. [Common: plots](#3-common-plots)
4. [Common: stats](#4-common-stats)
5. [loki_outlier](#5-loki_outlier)
6. [loki_homogeneity](#6-loki_homogeneity)
7. [loki_filter](#7-loki_filter)
8. [loki_regression](#8-loki_regression)

---

## 1. Common: input

Controls how input data files are located and parsed.

| Key | Type | Default | Description |
|---|---|---|---|
| `file` | string | — | Input filename. Resolved relative to `<workspace>/INPUT/`. |
| `time_format` | string | `gpst_seconds` | Time encoding. See options below. |
| `time_columns` | int[] | `[0]` | 0-based field indices that form the time token. Use `[0, 1]` when date and time are in separate columns. |
| `delimiter` | string | `";"` | Field separator. Use `" "` for space-delimited files. |
| `comment_char` | string | `"%"` | Lines starting with this character are skipped. |
| `columns` | int[] | all | 1-based indices of value columns to load. Index counts from 1 after the time field(s). |
| `merge_strategy` | string | `separate` | How to combine multiple files. See options below. |

### time_format options

| Value | Description |
|---|---|
| `gpst_seconds` | Seconds since GPS epoch 1980-01-06 |
| `gpst_week_sow` | Two columns: GPS week + seconds of week |
| `utc` | ISO string: `YYYY-MM-DD HH:MM:SS[.sss]` |
| `mjd` | Modified Julian Date (floating point) |
| `unix` | Seconds since Unix epoch 1970-01-01 |
| `index` | Sequential integer index (no absolute time) |

### merge_strategy options

| Value | Description |
|---|---|
| `separate` | Each file produces an independent time series |
| `merge` | All files are concatenated and sorted chronologically |

### Example

```json
"input": {
    "file": "CLIM_DATA_EX1.txt",
    "time_format": "utc",
    "time_columns": [0, 1],
    "delimiter": " ",
    "comment_char": "%",
    "columns": [3],
    "merge_strategy": "separate"
}
```

---

## 2. Common: output

Controls log verbosity.

| Key | Type | Default | Description |
|---|---|---|---|
| `log_level` | string | `info` | Minimum severity to write to log. |

### log_level options

| Value | Description |
|---|---|
| `debug` | All messages including internal diagnostics |
| `info` | Normal operational messages |
| `warning` | Warnings and errors only |
| `error` | Errors only |

### Example

```json
"output": {
    "log_level": "info"
}
```

---

## 3. Common: plots

Controls output image format, time axis format, and which plots are generated.
All plots land in `<workspace>/OUTPUT/IMG/`.

| Key | Type | Default | Description |
|---|---|---|---|
| `output_format` | string | `png` | Image format: `png`, `eps`, `svg` |
| `time_format` | string | `""` | X-axis label format. Empty = inherit from `input.time_format`. |
| `enabled` | object | — | Map of plot name -> bool. See tables below. |

### Generic plots (all programs)

| Flag | Default | Description |
|---|---|---|
| `time_series` | `true` | Basic time series line plot |
| `histogram` | `true` | Value histogram |
| `acf` | `false` | Autocorrelation function |
| `qq_plot` | `false` | Quantile-quantile plot |
| `boxplot` | `false` | Box-and-whisker plot |
| `comparison` | `false` | Two-series overlay (generic) |

### Outlier pipeline plots

| Flag | Default | Description |
|---|---|---|
| `original_series` | `true` | Original series with outlier markers (triangles) |
| `adjusted_series` | `true` | Cleaned (outlier-replaced) series |
| `homog_comparison` | `true` | Original vs cleaned overlay |
| `deseasonalized` | `true` | Residuals with outlier markers (only when deseasonalization is active) |
| `seasonal_overlay` | `true` | Original series + seasonal model overlay (only when deseasonalization is active) |
| `residuals_with_bounds` | `true` | Residuals + detection threshold lines |
| `outlier_overlay` | `false` | Pre-outlier and post-outlier detections on one plot. **Relevant only for `loki_homogeneity`** — ignored by `loki_outlier` which has a single detection pass. |

### Homogeneity pipeline plots

| Flag | Default | Description |
|---|---|---|
| `seasonal_overlay` | `true` | Original series + seasonal model (shared with outlier) |
| `change_points` | `true` | Residuals with vertical lines at detected change points |
| `shift_magnitudes` | `true` | Bar chart of shift magnitude at each change point |
| `correction_curve` | `true` | Original series + cumulative correction step function |

### Example

```json
"plots": {
    "output_format": "png",
    "time_format": "utc",
    "enabled": {
        "time_series":           true,
        "histogram":             true,
        "acf":                   true,
        "qq_plot":               true,
        "boxplot":               true,
        "original_series":       true,
        "adjusted_series":       true,
        "homog_comparison":      true,
        "deseasonalized":        true,
        "seasonal_overlay":      true,
        "residuals_with_bounds": true,
        "outlier_overlay":       false,
        "change_points":         true,
        "shift_magnitudes":      true,
        "correction_curve":      true
    }
}
```

---

## 4. Common: stats

Controls computation of descriptive statistics logged at startup.

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `true` | Compute and log descriptive statistics for each loaded series |
| `nan_policy` | string | `skip` | How to handle NaN values in statistics |
| `hurst` | bool | `true` | Compute Hurst exponent (can be slow for large series) |

### nan_policy options

| Value | Description |
|---|---|
| `skip` | Ignore NaN values |
| `throw` | Throw an exception if NaN is encountered |
| `propagate` | Return NaN if any input value is NaN |

### Example

```json
"stats": {
    "enabled": true,
    "nan_policy": "skip",
    "hurst": false
}
```

---

## 5. loki_outlier

Default config: `config/outlier.json`

The outlier pipeline runs the following steps in order:

```
1. (optional) Deseasonalizer  -- subtract seasonal component
2. Outlier detector           -- detect anomalies in residuals
3. GapFiller                  -- replace detected outliers
4. (optional) Reconstruct     -- add seasonal component back
```

### 5.1 outlier.deseasonalization

Subtracts a seasonal component before detection so that detectors operate on
residuals rather than the raw signal.

| Key | Type | Default | Description |
|---|---|---|---|
| `strategy` | string | `none` | Deseasonalization method. See options below. |
| `ma_window_size` | int | `365` | Window length in samples for `moving_average`. |
| `median_year_min_years` | int | `5` | Minimum years per day-of-year slot for `median_year`. Slots with fewer years return NaN and are not subtracted. |

### strategy options

| Value | Best for |
|---|---|
| `median_year` | Climatological and GNSS data with a clear annual cycle |
| `moving_average` | Sensor or signal data without a strict annual period |
| `none` | Series with no periodic component (economic data, pre-computed residuals) |

> **Note on sampling rate**: `median_year` requires a series resolution >= 1 hour.
> For sub-hourly data use `moving_average`. The `ma_window_size` should cover
> one full period: daily data -> 365, 6-hourly -> 1461, hourly -> 8760.

### 5.2 outlier.detection

| Key | Type | Default | Description |
|---|---|---|---|
| `method` | string | `mad` | Detection algorithm. See options below. |
| `iqr_multiplier` | float | `1.5` | Fence width for `iqr`. Standard: 1.5 (mild), 3.0 (extreme). |
| `mad_multiplier` | float | `3.0` | Threshold multiplier for `mad` and `mad_bounds`. Roughly equivalent to sigma units after normalisation by 0.6745. |
| `zscore_threshold` | float | `3.0` | Z-score threshold for `zscore`. Flags values with `|z| > threshold`. |

### method options

| Value | Description |
|---|---|
| `mad` | MAD-based (robust). Flags values outside `median +/- k * MAD / 0.6745`. Recommended for most use cases. |
| `iqr` | IQR-based (box-plot rule). Flags values outside `Q1/Q3 +/- k * IQR`. |
| `zscore` | Z-score. Flags values with `|z| > threshold`. Assumes approximate normality. |
| `mad_bounds` | Global MAD bounds on the raw series without deseasonalization. Best for coarse removal of physically impossible values (sensor overflow, resets). |

### 5.3 outlier.replacement

Controls how detected outlier positions are filled after removal.

| Key | Type | Default | Description |
|---|---|---|---|
| `strategy` | string | `linear` | Fill method. See options below. |
| `max_fill_length` | int | `0` | Maximum consecutive outlier positions to fill. `0` = no limit. Set > 0 to leave long gaps as NaN (e.g. instrument outages). |

### strategy options

| Value | Description |
|---|---|
| `linear` | Linear interpolation between the nearest valid neighbours. Best for smooth signals. |
| `forward_fill` | Copy the last valid value forward. Best for step-like or slowly varying signals. |
| `mean` | Replace with the series mean. Use only when temporal order is not meaningful. |

### Example

```json
"outlier": {
    "deseasonalization": {
        "strategy": "median_year",
        "ma_window_size": 365,
        "median_year_min_years": 5
    },
    "detection": {
        "method": "mad",
        "iqr_multiplier": 1.5,
        "mad_multiplier": 3.0,
        "zscore_threshold": 3.0
    },
    "replacement": {
        "strategy": "linear",
        "max_fill_length": 0
    }
}
```

---

## 6. loki_homogeneity

Default config: `config/homogenization.json`

The homogenization pipeline runs the following steps in order:

```
1. GapFiller                    -- fill missing values
2. OutlierCleaner (pre)         -- coarse outlier removal on raw series
3. Deseasonalizer               -- remove seasonal signal
4. OutlierCleaner (post)        -- fine outlier removal on residuals
5. MultiChangePointDetector     -- detect structural breaks
6. SeriesAdjuster               -- correct series for detected shifts
```

### 6.1 homogeneity.gap_filling

| Key | Type | Default | Description |
|---|---|---|---|
| `apply_gap_filling` | bool | `true` | Enable gap filling step |
| `strategy` | string | `linear` | Fill strategy: `linear`, `forward_fill`, `mean`, `none` |
| `max_fill_length` | int | `0` | Maximum consecutive gap samples to fill. `0` = no limit. |
| `gap_threshold_factor` | float | `1.5` | Multiplier on expected step to detect a gap. |
| `min_series_years` | int | `10` | Minimum span for `median_year` gap filling. |

### 6.2 homogeneity.pre_outlier

Coarse outlier removal on the raw (gap-filled) series before deseasonalization.
Intended to remove physically impossible values only. Use a large multiplier.

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `false` | Enable pre-outlier removal |
| `method` | string | `mad_bounds` | Detection method (same options as outlier.detection.method) |
| `mad_multiplier` | float | `5.0` | Use high value (5.0+) for coarse filtering only |
| `iqr_multiplier` | float | `1.5` | IQR fence width |
| `zscore_threshold` | float | `3.0` | Z-score threshold |
| `replacement_strategy` | string | `linear` | Fill strategy for detected positions |
| `max_fill_length` | int | `0` | Maximum consecutive positions to fill |

### 6.3 homogeneity.deseasonalization

Same options as [outlier.deseasonalization](#51-outlierdeseasonalization).

Default `strategy` here is `median_year` (climatological data assumed).

### 6.4 homogeneity.post_outlier

Fine outlier removal on deseasonalized residuals, before change point detection.
Use a tighter multiplier than pre_outlier.

Same keys as [pre_outlier](#62-homogeneitypre_outlier).
Recommended defaults: `method: mad`, `mad_multiplier: 3.0`.

### 6.5 homogeneity.detection

Controls the change point detector (SNHT-based).

| Key | Type | Default | Description |
|---|---|---|---|
| `min_segment_points` | int | `60` | Minimum number of observations per segment |
| `min_segment_duration` | string | `""` | Human-readable minimum segment duration. Overrides `min_segment_seconds` if set. Format: `"1y"`, `"180d"`, `"6h"`, `"30m"`, `"60s"`. |
| `min_segment_seconds` | float | `0.0` | Minimum segment duration in seconds (alternative to `min_segment_duration`) |
| `significance_level` | float | `0.05` | p-value threshold for accepting a change point |
| `acf_dependence_limit` | float | `0.2` | ACF lag-1 limit above which dependence correction is applied |
| `correct_for_dependence` | bool | `true` | Apply ACF-based effective sample size correction |

### 6.6 homogeneity.apply_adjustment

| Key | Type | Default | Description |
|---|---|---|---|
| `apply_adjustment` | bool | `true` | Apply shift corrections to produce the homogenized series. Set `false` to detect only without modifying the series. |

### Example

```json
"homogeneity": {
    "apply_gap_filling": true,
    "gap_filling": {
        "strategy": "linear",
        "max_fill_length": 30
    },
    "pre_outlier": {
        "enabled": true,
        "method": "mad_bounds",
        "mad_multiplier": 5.0,
        "replacement_strategy": "linear",
        "max_fill_length": 0
    },
    "deseasonalization": {
        "strategy": "median_year",
        "median_year_min_years": 5
    },
    "post_outlier": {
        "enabled": true,
        "method": "mad",
        "mad_multiplier": 3.0,
        "replacement_strategy": "linear",
        "max_fill_length": 0
    },
    "detection": {
        "min_segment_duration": "1y",
        "significance_level": 0.05,
        "acf_dependence_limit": 0.2,
        "correct_for_dependence": true
    },
    "apply_adjustment": true
}
```

---

## 7. loki_filter

Default config: `config/filter.json`

The filter pipeline runs the following steps in order:

```
1. GapFiller                    -- fill NaN / missing values
2. (optional) FilterWindowAdvisor -- auto-estimate window or bandwidth
3. Filter::apply()              -- smooth the series
4. CSV export + plots
```

### 7.1 filter.gap_filling

Controls NaN and gap handling before the filter is applied.
Filters require clean (NaN-free) input -- all gaps must be resolved here.

| Key | Type | Default | Description |
|---|---|---|---|
| `strategy` | string | `linear` | Fill strategy. See options below. |
| `max_fill_length` | int | `0` | Maximum consecutive gap samples to fill. `0` = no limit. |

### strategy options

| Value | Description |
|---|---|
| `linear` | Linear interpolation between nearest valid neighbours. |
| `forward_fill` | Copy last valid value forward. |
| `mean` | Replace with series mean. |
| `none` | No filling -- gaps remain as NaN (filter will throw). |

---

### 7.2 filter.method

Selects the smoothing algorithm.

| Key | Type | Default | Description |
|---|---|---|---|
| `method` | string | `kernel` | Filter algorithm. See options below. |
| `auto_window_method` | string | `silverman_mad` | Advisor method used when window/bandwidth is `0`. See options below. |

### method options

| Value | Class | Best for |
|---|---|---|
| `moving_average` | `MovingAverageFilter` | Any data, fast, symmetric smoothing |
| `ema` | `EmaFilter` | Streaming or ms-resolution data, causal |
| `weighted_ma` | `WeightedMovingAverageFilter` | Custom kernel weights, any data |
| `kernel` | `KernelSmoother` | Climatological and GNSS data |
| `loess` | `LoessFilter` | Climatological data, locally adaptive |
| `savitzky_golay` | `SavitzkyGolayFilter` | ms / high-frequency data, peak preservation |

### auto_window_method options

Used only when the relevant window or bandwidth parameter is `0`.

| Value | Description |
|---|---|
| `silverman_mad` | Robust bandwidth: `sigma = MAD/0.6745`, `bw = sigma * (4/(3n))^0.2`. Recommended default. |
| `silverman` | Classic Silverman rule: `bw = 0.9 * min(std, IQR/1.34) * n^(-0.2)`. |
| `acf_peak` | Detects first local maximum in ACF as estimated period, derives window from it. |

---

### 7.3 filter.moving_average

| Key | Type | Default | Description |
|---|---|---|---|
| `window` | int | `365` | Symmetric window length in samples (must be odd). `0` = auto via advisor. |

---

### 7.4 filter.ema

| Key | Type | Default | Description |
|---|---|---|---|
| `alpha` | float | `0.1` | Smoothing factor in (0, 1]. Smaller = smoother. No auto-window applies. |

> **Note**: EMA is causal -- it only looks backward. No edge NaN is produced.
> `auto_window_method` is ignored for EMA.

---

### 7.5 filter.weighted_ma

| Key | Type | Default | Description |
|---|---|---|---|
| `weights` | float[] | `[1,2,3,2,1]` | Symmetric weight vector. Length determines window size. Must be non-empty. |

> **Note**: Window size = `weights.length`. No auto-window applies -- weights
> are always specified explicitly.

---

### 7.6 filter.kernel

| Key | Type | Default | Description |
|---|---|---|---|
| `bandwidth` | float | `0.1` | Fraction of series length used as kernel half-width: `k = ceil(bw * n)`. Range: `(0, 1]`. `0` = auto via advisor. |
| `kernel_type` | string | `epanechnikov` | Kernel shape. See options below. |
| `gaussian_cutoff` | float | `3.0` | Truncation limit for Gaussian kernel in units of `h` (standard deviations). Ignored for other kernels. |

### kernel_type options

| Value | Description |
|---|---|
| `epanechnikov` | Parabolic kernel. Optimal MSE, recommended default. |
| `gaussian` | Gaussian (normal) kernel, truncated at `gaussian_cutoff * h`. |
| `uniform` | Rectangular kernel (simple moving average with fractional bandwidth). |
| `triangular` | Triangular kernel. Intermediate between uniform and Epanechnikov. |

---

### 7.7 filter.loess

| Key | Type | Default | Description |
|---|---|---|---|
| `bandwidth` | float | `0.25` | Fraction of series length used as k-nearest-neighbour span. Range: `(0, 1]`. `0` = auto via advisor. |
| `degree` | int | `1` | Polynomial degree for local regression. `1` = linear, `2` = quadratic. |
| `kernel_type` | string | `tricube` | Weighting kernel for local regression. See options below. |
| `robust` | bool | `false` | Enable IRLS robust fitting (downweights outliers). Recommended when residuals contain outliers. |
| `robust_iterations` | int | `3` | Number of IRLS re-weighting iterations. Only used when `robust: true`. |

### kernel_type options (LOESS)

| Value | Description |
|---|---|
| `tricube` | Classic LOESS kernel. Recommended default. |
| `epanechnikov` | Parabolic kernel. Slightly faster than tricube. |
| `gaussian` | Gaussian weighting. Softer tails than tricube. |

> **Note**: LOESS uses k-nearest-neighbours, so no edge NaN is produced --
> the neighbourhood shifts at series boundaries. Complexity is O(n * k * p^2);
> not recommended for ms-resolution data.

---

### 7.8 filter.savitzky_golay

| Key | Type | Default | Description |
|---|---|---|---|
| `window` | int | `11` | Convolution window length in samples (must be odd and >= `degree + 2`). `0` = auto via advisor. |
| `degree` | int | `2` | Polynomial degree for local fitting. Must satisfy `degree < window`. |

> **Note**: Edge coefficients are precomputed -- no edge NaN is produced.
> Fast O(n * w) convolution; recommended for ms / high-frequency data.

---

### 7.9 Plots (filter pipeline)

In addition to the generic plots (see [Section 3](#3-common-plots)), loki_filter
produces the following pipeline-specific plots:

| Flag | Default | Description |
|---|---|---|
| `filter_overlay` | `true` | Original + filtered series on one plot, two coloured lines. |
| `filter_overlay_residuals` | `true` | Two-panel plot: upper = original + filtered overlay, lower = residuals (original - filtered). Main diagnostic plot. |
| `filter_residuals` | `false` | Standalone residuals line plot. |
| `filter_residuals_acf` | `false` | ACF of residuals with 95% confidence band. Useful for verifying that filter removed the target frequency component. |
| `residuals_histogram` | `false` | Histogram of residuals. |
| `residuals_qq` | `false` | Q-Q plot of residuals against normal distribution. |

---

### Example

```json
"filter": {
    "gap_filling": {
        "strategy": "linear",
        "max_fill_length": 0
    },
    "method": "kernel",
    "auto_window_method": "silverman_mad",
    "moving_average": {
        "window": 365
    },
    "ema": {
        "alpha": 0.1
    },
    "weighted_ma": {
        "weights": [1.0, 2.0, 3.0, 2.0, 1.0]
    },
    "kernel": {
        "bandwidth": 0.1,
        "kernel_type": "epanechnikov",
        "gaussian_cutoff": 3.0
    },
    "loess": {
        "bandwidth": 0.25,
        "degree": 1,
        "kernel_type": "tricube",
        "robust": false,
        "robust_iterations": 3
    },
    "savitzky_golay": {
        "window": 11,
        "degree": 2
    }
}
```

> **Auto-window rule**: If the relevant `window` or `bandwidth` key is `0`
> (or omitted), `FilterWindowAdvisor` is called automatically using the
> method specified in `auto_window_method`. The advisor result is logged at
> `INFO` level so the effective value is always visible in the log.
> Set `window`/`bandwidth` to a positive value to disable auto-estimation.
---

## 8. loki_regression

Default config: `config/regression.json`

The regression pipeline runs the following steps in order:

```
1. GapFiller                  -- fill NaN / missing values
2. Regressor::fit()           -- fit the selected model
3. RegressionDiagnostics      -- ANOVA, influence, VIF, Breusch-Pagan
4. CSV export                 -- mjd; original; fitted; residual (+ trend/seasonal for trend method)
5. Protocol                   -- OUTPUT/PROTOCOLS/regression_[dataset]_[param]_protocol.txt
6. Plots                      -- OUTPUT/IMG/
```

---

### 8.1 regression.gap_filling

Controls NaN and gap handling before regression is applied.

| Key | Type | Default | Description |
|---|---|---|---|
| `strategy` | string | `linear` | Fill strategy. See options below. |
| `max_fill_length` | int | `0` | Maximum consecutive gap samples to fill. `0` = no limit. |

### strategy options

| Value | Description |
|---|---|
| `linear` | Linear interpolation between nearest valid neighbours. |
| `forward_fill` | Copy last valid value forward. |
| `mean` | Replace with series mean. |
| `none` | No filling -- gaps remain as NaN (valid observations are used, NaN skipped). |

---

### 8.2 regression.method

Selects the regression model.

| Key | Type | Default | Description |
|---|---|---|---|
| `method` | string | `linear` | Regression algorithm. See options below. |

### method options

| Value | Class | Description |
|---|---|---|
| `linear` | `LinearRegressor` | OLS fit of `y = a0 + a1 * t`. Coefficients: `[a0, a1]`. |
| `polynomial` | `PolynomialRegressor` | OLS polynomial of degree `d`. Coefficients: `[a0, a1, ..., ad]`. Supports analytical LOO-CV and k-fold CV. |
| `harmonic` | `HarmonicRegressor` | OLS fit of `K` sin/cos pairs at sub-harmonics of `period`. Coefficients: `[a0, s1, c1, ..., sK, cK]`. |
| `trend` | `TrendEstimator` | Joint OLS fit of linear trend + `K` harmonics. Coefficients: `[a0, a1, s1, c1, ..., sK, cK]`. CSV includes separate trend and seasonal columns. |
| `robust` | `RobustRegressor` | IRLS polynomial regression. Downweights outliers via Huber or Bisquare weight function. |
| `calibration` | `CalibrationRegressor` | Total Least Squares (TLS) via SVD. Minimises orthogonal distances -- use when both `x` and `y` have measurement error. Residuals are orthogonal; `cofactorX` is not available. |

> **X-axis convention**: All regressors use `x = mjd - tRef` (days relative to first
> valid observation) for numerical stability. `tRef` is stored in `RegressionResult`
> and reported in the protocol.

---

### 8.3 regression.polynomial_degree

| Key | Type | Default | Valid range | Description |
|---|---|---|---|---|
| `polynomial_degree` | int | `1` | >= 1 | Polynomial degree for `polynomial` and `robust` methods. Degree 1 is equivalent to linear regression. |

> Used by: `polynomial`, `robust`. Ignored by all other methods.

---

### 8.4 regression.harmonic_terms and period

| Key | Type | Default | Valid range | Description |
|---|---|---|---|---|
| `harmonic_terms` | int | `2` | >= 1 | Number of sin/cos pairs `K`. Each pair adds 2 parameters. |
| `period` | float | `365.25` | > 0 | Fundamental period in days. Sub-harmonics are `T/1, T/2, ..., T/K`. |

> Used by: `harmonic`, `trend`. Ignored by all other methods.
>
> **Tip**: For annual climatological signal use `period: 365.25` with `harmonic_terms: 2`
> (annual + semi-annual). For GNSS draconitic signal use `period: 351.4`.

---

### 8.5 regression.robust settings

| Key | Type | Default | Description |
|---|---|---|---|
| `robust` | bool | `false` | Enable IRLS robust estimation. For `robust` method this is always forced `true`. |
| `robust_iterations` | int | `10` | Maximum number of IRLS re-weighting iterations. |
| `robust_weight_fn` | string | `bisquare` | Weight function. See options below. |

### robust_weight_fn options

| Value | Tuning constant | Description |
|---|---|---|
| `bisquare` | `c = 4.685` | Tukey bisquare. Hard redescending -- completely zeros out extreme outliers. Recommended default (95% Gaussian efficiency). |
| `huber` | `k = 1.345` | Huber. Soft -- linear tails for large residuals. Less aggressive than bisquare. |

> Used by: `robust`. When `robust: true` is set for `linear`, `polynomial`, `harmonic`,
> or `trend`, IRLS is applied through `LsqSolver`. For `polynomial` with `robust: true`,
> k-fold CV is used instead of analytical LOO-CV.

---

### 8.6 regression.cross_validation

| Key | Type | Default | Valid range | Description |
|---|---|---|---|---|
| `cv_folds` | int | `10` | 2 -- 100 | Number of folds for k-fold CV. Clamped to `min(100, n/2)` with a warning. For OLS `polynomial` regression, analytical LOO-CV via hat matrix is used regardless of this setting. For `robust` fits, k-fold CV is always used. |

> LOO-CV (Leave-One-Out) is computed in O(n) via the hat matrix diagonal `h_ii`:
> `cv_error_i = e_i / (1 - h_ii)`. This is exact and does not require re-fitting.

---

### 8.7 regression.prediction

| Key | Type | Default | Description |
|---|---|---|---|
| `compute_prediction` | bool | `false` | If `true`, evaluate the fitted model beyond the data range. |
| `prediction_horizon` | float | `0.0` | Days beyond the last observation to predict. |
| `confidence_level` | float | `0.95` | Confidence level for confidence and prediction intervals (e.g. `0.95` = 95%). |

> When `compute_prediction: true`, the regressor's `predict()` method is called
> and results are appended to the CSV output. Prediction intervals are wider than
> confidence intervals -- they account for both parameter uncertainty and residual variance.
>
> **Note for TLS** (`calibration` method): prediction intervals are approximate
> (based on `sigma0` treated as vertical error) because `cofactorX` is not available.

---

### 8.8 regression.significance_level

| Key | Type | Default | Description |
|---|---|---|---|
| `significance_level` | float | `0.05` | Alpha level for hypothesis tests in diagnostics and protocol. Used for F-test (ANOVA) and Breusch-Pagan rejection decisions. |

---

### 8.9 Diagnostics

`RegressionDiagnostics` runs automatically after each fit. Results are logged and written to the protocol. No configuration is needed -- all diagnostics are always computed.

| Diagnostic | Method | Output |
|---|---|---|
| ANOVA table | F-test: `F = MSR / MSE` | SSR, SSE, SST, F-statistic, p-value, df |
| Cook's distance | `D_i = (e_i^2 * h_ii) / (p * sigma0^2 * (1 - h_ii)^2)` | Flagged if `D_i > 4/n` |
| Leverage | Hat matrix diagonal `h_ii` via thin QR | Flagged if `h_ii > 2p/n` |
| Standardized residuals | `r_i = e_i / (sigma0 * sqrt(1 - h_ii))` | Written with influence measures |
| VIF | `VIF_j = 1 / (1 - R^2_j)`, auxiliary OLS per predictor | Flagged if `VIF > 10` |
| Breusch-Pagan | LM = `n * R^2` of auxiliary regression `e_i^2 ~ y-hat_i` | Rejected if `p < significance_level` |

> **Note**: VIF requires at least 2 predictor columns (intercept + 1). For `linear`
> regression with a single predictor, VIF is not computed (only 2 columns in design matrix).
>
> **Note**: Cook's distance and leverage require `designMatrix` to be populated in
> `RegressionResult`. All regressors store this automatically after `fit()`.

---

### 8.10 Plots (regression pipeline)

In addition to the generic plots (see [Section 3](#3-common-plots)), loki_regression
produces the following pipeline-specific plots:

| Flag | Default | Description |
|---|---|---|
| `regression_overlay` | `true` | Original series + fitted curve on one plot. |
| `regression_residuals` | `true` | Residuals vs time with zero baseline. |
| `regression_qq_bands` | `true` | QQ plot of residuals with confidence bands. |
| `regression_cdf_plot` | `false` | ECDF of residuals vs theoretical normal CDF. |
| `regression_residual_acf` | `false` | ACF of residuals with 95% confidence band (`+/- 1.96/sqrt(n)`). Useful for detecting autocorrelation in residuals. |
| `regression_residual_hist` | `false` | Histogram of residuals with fitted normal curve. |
| `regression_influence` | `false` | Cook's distance bar chart with `4/n` threshold line. |
| `regression_leverage` | `false` | Leverage `h_ii` vs standardized residuals scatter. Reference lines at `2p/n` (leverage) and `+/-2` (residuals). |

---

### 8.11 CSV output

The CSV file is written to `<workspace>/OUTPUT/CSV/`.
Filename: `[stationId]_[componentName]_regression.csv`

For all methods except `trend`:

```
mjd ; original ; fitted ; residual
```

For `trend` method (TrendEstimator):

```
mjd ; original ; fitted ; trend ; seasonal ; residual
```

> **Note**: `trend` and `seasonal` columns are currently written as `NaN` placeholders
> pending full `DecompositionResult` integration in the CSV writer (planned).

---

### 8.12 Protocol output

Written to `<workspace>/OUTPUT/PROTOCOLS/`.
Filename: `regression_[dataset]_[componentName]_protocol.txt`

The protocol contains:
- Model name, dataset, series name, observation count, parameter count, DOF
- Coefficient table with estimates and standard errors (N/A for TLS)
- Model fit metrics: sigma0, R^2, adjusted R^2, AIC, BIC
- ANOVA table: SSR, SSE, SST, F-statistic, p-value
- Residual diagnostics: mean, std dev, Breusch-Pagan result, Cook's D count, leverage count
- VIF table (if applicable)

---

### Example

```json
"regression": {
    "gap_filling": {
        "strategy": "linear",
        "max_fill_length": 0
    },
    "method": "linear",
    "polynomial_degree": 1,
    "harmonic_terms": 2,
    "period": 365.25,
    "robust": false,
    "robust_iterations": 10,
    "robust_weight_fn": "bisquare",
    "compute_prediction": false,
    "prediction_horizon": 365.25,
    "confidence_level": 0.95,
    "significance_level": 0.05,
    "cv_folds": 10
}
```
