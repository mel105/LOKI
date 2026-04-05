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
9. [loki_stationarity](#9-loki_stationarity)
10. [loki_arima](#10-loki_arima)
11. [loki_ssa](#11-loki_ssa)
1. [loki_decomposition](#12-loki_decomposition)

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
| `leverage_plot` | `true` | Leverage values h_ii vs time with threshold line and flagged positions. Generated only when `detection.method` is `hat_matrix`. |


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
| `hat_matrix` | DEH-based (Hau & Tong, 1989). Detects geometrically remote state vectors via hat matrix leverage values h_ii. Does not depend on estimated AR coefficients. Use when residual-based methods fail to detect innovation outliers or masked outliers. |

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

### 5.4 outlier.hat_matrix

Controls the DEH-based hat matrix detector (Hau & Tong, 1989).
Active only when `detection.method` is `hat_matrix`.

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `true` | Enable or disable the hat matrix detector. |
| `ar_order` | int | `5` | AR lag order p. Each row of the design matrix contains p lagged residual values. Higher p captures more temporal context. |
| `significance_level` | float | `0.05` | Alpha for the chi-squared threshold. Threshold = chi2_quantile(1 - alpha, p) / n. |

### ar_order selection guide

The detection threshold is derived from the chi-squared distribution with p
degrees of freedom. The choice of p depends on the sampling rate and the
expected temporal extent of anomalies.

| Sampling rate | Recommended ar_order | Context captured |
|---|---|---|
| Daily | 5 -- 14 | 1 -- 2 weeks |
| 6-hourly | 28 -- 56 | 1 -- 2 weeks |
| Hourly | 24 -- 168 | 1 day -- 1 week |
| Sub-hourly / ms | 10 -- 30 | application-specific |

To select p empirically: compute the ACF of the deseasonalized residuals and
choose p as the lag where ACF first crosses the 95% confidence band
(+/- 1.96 / sqrt(n)). This lag represents the effective decorrelation length
of the residuals.

> **Note on batch detection**: a single outlier at position t causes lag vectors
> z_{t+1}, ..., z_{t+p} to be remote as well, because they all contain the
> anomalous value. This produces clusters of up to p consecutive flagged positions.
> This is expected behaviour documented in Hau & Tong (1989), Section 6, and
> reflects true contamination of the AR neighbourhood -- not a false positive rate issue.

> **Note on replacement**: detected positions are replaced using the same
> `outlier.replacement` settings as the standard pipeline (linear interpolation
> by default). The seasonal component (if any) is subtracted before replacement
> and added back after reconstruction, identical to the O1-O3 pipeline.

### Example
```json
"outlier": {
    "deseasonalization": {
        "strategy": "median_year",
        "median_year_min_years": 5
    },
    "detection": {
        "method": "hat_matrix"
    },
    "replacement": {
        "strategy": "linear",
        "max_fill_length": 0
    },
    "hat_matrix": {
        "enabled": true,
        "ar_order": 30,
        "significance_level": 0.001
    }
}
```
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

---

### 8.13 regression.nonlinear

Applies only when `method: "nonlinear"`. Controls the Levenberg-Marquardt
nonlinear least-squares solver and the choice of parametric model.

All other regression keys (`polynomial_degree`, `harmonic_terms`, `period`,
`robust`, `robust_iterations`, `robust_weight_fn`, `cv_folds`) are ignored
when `method: "nonlinear"`.

---

#### Model selection

| Key | Type | Default | Description |
|---|---|---|---|
| `model` | string | `exponential` | Parametric model to fit. See model table below. |
| `initial_params` | float[] | see defaults | Starting parameter vector for LM. Length must match the model. **Strongly recommended** -- LM convergence is sensitive to starting values. |

#### Available models

| Value | Formula | Parameters | Count |
|---|---|---|---|
| `exponential` | `y = a * exp(b * x)` | `[a, b]` | 2 |
| `logistic` | `y = L / (1 + exp(-k * (x - x0)))` | `[L, k, x0]` | 3 |
| `gaussian` | `y = A * exp(-((x - mu)^2) / (2 * sigma^2))` | `[A, mu, sigma]` | 3 |

> **X-axis convention**: `x = mjd - tRef` (days relative to first valid observation),
> identical to all other regressors. Initial parameter values must be consistent
> with this scale.

---

#### Model guide: when to use each model

**`exponential`** -- `y = a * exp(b * x)`

Use when the signal grows or decays at a rate proportional to its current value.
- `b > 0`: exponential growth (e.g. cumulative drift, sensor degradation)
- `b < 0`: exponential decay (e.g. relaxation after a step, radioactive decay analogy)
- Starting values: set `a` near the first observed value, `b` near `0`.
  For slow drift over years: `b` in the range `[-0.01, 0.01]`.
  For fast decay over days: `b` around `-0.1` to `-1.0`.
- Caution: diverges rapidly for large `|b * x|`. If `x` spans thousands of days,
  keep `|b|` small (order `1e-3` or less).

**`logistic`** -- `y = L / (1 + exp(-k * (x - x0)))`

Use when the signal follows an S-shaped curve: starts near zero, rises
(or falls) and saturates at a plateau. Classic applications:
- Velocity profiles: acceleration phase -> cruising speed (train, vehicle data)
- Adoption curves, population growth with carrying capacity
- Any signal with a clear lower bound, upper bound, and sigmoidal transition
Parameters:
- `L`: asymptotic maximum (saturation level). Set near the observed plateau.
- `k`: steepness of the transition. Larger `k` = sharper transition.
  For a transition over ~100 days: `k ~ 0.05`. Over ~10 days: `k ~ 0.5`.
- `x0`: inflection point in days (x = mjd - tRef). Set near the middle of
  the transition region.
- Starting values: inspect the data visually -- estimate where the curve
  reaches half its maximum (that is `x0`), what the plateau is (`L`),
  and how sharp the rise is (`k`).

**`gaussian`** -- `y = A * exp(-((x - mu)^2) / (2 * sigma^2))`

Use when the signal forms a single isolated bell-shaped peak above a baseline.
Applications:
- Isolated anomalies or events in a residual series
- Spectral peaks in time domain
- Any signal dominated by a single symmetric hump
Parameters:
- `A`: peak amplitude. Set near the observed maximum.
- `mu`: peak centre in days (x = mjd - tRef). Set near the observed peak position.
- `sigma`: peak width (standard deviation in days). Larger = broader peak.
  For a peak spanning ~30 days: `sigma ~ 10`. Spanning ~1 year: `sigma ~ 100`.
- Caution: if the baseline is not near zero, subtract it first or add a
  constant term. The Gaussian model has no intercept parameter.
- Starting values are critical: if `mu` is far from the actual peak,
  LM may not find the solution.

---

#### Solver settings

| Key | Type | Default | Description |
|---|---|---|---|
| `max_iterations` | int | `100` | Maximum LM iterations. Increase to `200-500` for difficult problems. |
| `grad_tol` | float | `1e-8` | Gradient convergence criterion: stop when `max(J^T r) < grad_tol`. Tighter values require more iterations but give a more precise solution. |
| `step_tol` | float | `1e-8` | Step convergence criterion: stop when relative step size `< step_tol`. |
| `lambda_init` | float | `1e-3` | Initial Marquardt damping parameter. Larger values make the first steps more like gradient descent (safer but slower). If LM diverges immediately, try `1e-1` or `1.0`. |
| `lambda_factor` | float | `10.0` | Damping increase/decrease factor on step rejection/acceptance. Standard value, rarely needs changing. |
| `confidence_level` | float | `0.95` | Confidence level for prediction and confidence intervals. Intervals are linearisation-based (delta method) -- approximate for strongly nonlinear models. |

---

#### Convergence troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| `[WARNING] LM did not converge` | Bad initial params or too few iterations | Improve `initial_params`, increase `max_iterations` |
| R^2 very low after convergence | Wrong model for the data | Try a different `model` |
| Very large parameter values | Scale mismatch | Check that `initial_params` are consistent with `x = mjd - tRef` scale |
| Intervals are very wide | Small dataset or nearly flat Jacobian | Normal for nonlinear models with few observations |
| Divergence with `b` large in exponential | `b * x_max` overflow | Use smaller `|b|` as starting value, check data span |

---

#### Diagnostics for nonlinear fits

The standard diagnostics (ANOVA, Cook's distance, leverage, VIF, Breusch-Pagan)
are computed identically to linear methods. However, note:

- `designMatrix` in `RegressionResult` contains the **Jacobian at the solution**,
  not a true design matrix. For near-linear models this is a good approximation.
- Cook's distance and leverage are therefore **approximate** for nonlinear models.
- VIF on the Jacobian columns is meaningful only if the columns are interpretable
  as predictors (usually not the case for nonlinear models -- treat VIF with caution).
- ANOVA F-test is valid as a global test of fit (SSR/SSE decomposition holds).
- Prediction intervals use the **delta method** (linearisation via gradient):
  they are asymptotically correct but may undercover for strongly nonlinear models
  or small datasets.

---

#### Example: exponential decay

```json
"regression": {
    "method": "nonlinear",
    "compute_prediction": true,
    "prediction_horizon": 180.0,
    "confidence_level": 0.95,
    "significance_level": 0.05,
    "nonlinear": {
        "model": "exponential",
        "initial_params": [5.0, -0.005],
        "max_iterations": 200,
        "grad_tol": 1e-8,
        "step_tol": 1e-8,
        "lambda_init": 1e-3,
        "lambda_factor": 10.0,
        "confidence_level": 0.95
    }
}
```

#### Example: logistic (train velocity profile)

```json
"regression": {
    "method": "nonlinear",
    "compute_prediction": false,
    "nonlinear": {
        "model": "logistic",
        "initial_params": [80.0, 0.1, 50.0],
        "max_iterations": 300,
        "lambda_init": 1e-2
    }
}
```

#### Example: Gaussian peak

```json
"regression": {
    "method": "nonlinear",
    "compute_prediction": false,
    "nonlinear": {
        "model": "gaussian",
        "initial_params": [10.0, 500.0, 30.0],
        "max_iterations": 200
    }
}
```

---

## 9. loki_stationarity

Default config: `config/stationarity.json`

The stationarity pipeline tests whether a time series is stationary (has a
constant mean and variance over time, with no unit root). It is used both as
a **diagnostic tool** before homogenization (to detect structural breaks and
non-stationarity caused by change points) and as a **prerequisite check**
before ARIMA modelling (to determine the differencing order d).

The pipeline runs the following steps in order:

```
1. GapFiller                  -- fill NaN / missing values (linear, always on)
2. Deseasonalizer             -- subtract seasonal component
3. (optional) Differencing    -- apply first-order or seasonal differencing
4. StationarityAnalyzer       -- run all enabled tests
5. CSV export                 -- mjd; original; residual; diff1; diff2
6. Protocol                   -- OUTPUT/PROTOCOLS/stationarity_[dataset]_[param]_protocol.txt
7. Plots                      -- time series, ACF, PACF, histogram, QQ
```

### Recommended workflow

```
loki_stationarity (before homogenization)
  -> identifies non-stationarity caused by change points, trends, outliers

loki_homogeneity
  -> corrects change points and shifts

loki_stationarity (after homogenization)
  -> verifies that residuals are now stationary

loki_arima (if stationary confirmed)
  -> fits AR/MA model; uses recommended d from stationarity protocol
```

---

### 9.1 stationarity.deseasonalization

Subtracts a seasonal component before testing so that tests operate on
residuals rather than the raw signal. Same options as
[outlier.deseasonalization](#51-outlierdeseasonalization).

| Key | Type | Default | Description |
|---|---|---|---|
| `strategy` | string | `median_year` | Deseasonalization method. See options below. |
| `ma_window_size` | int | `1461` | Window in samples for `moving_average`. For 6-hourly data: 1461 = 1 year. |
| `median_year_min_years` | int | `5` | Minimum years per slot for `median_year`. |

### strategy options

| Value | Best for |
|---|---|
| `median_year` | Climatological and GNSS data with a clear annual cycle |
| `moving_average` | Sensor data without a strict annual period |
| `none` | Pre-computed residuals or series without periodic component |

> **Note on window size**: `ma_window_size` is in samples, not time units.
> For 6-hourly data one year = 1461 samples. For daily data one year = 365 samples.

---

### 9.2 stationarity.differencing

Optional differencing applied to the deseasonalized residuals before testing.
Useful when you already suspect the series needs differencing and want to
verify that the differenced series is stationary.

| Key | Type | Default | Description |
|---|---|---|---|
| `apply` | bool | `false` | Apply differencing before running tests. |
| `order` | int | `1` | Differencing order. `1` = first differences `y[t] - y[t-1]`. `2` = second differences. Maximum: `2`. |

> **Note**: Differencing is applied to the **residuals** (after deseasonalization),
> not to the raw series. The CSV output always contains `diff1` and `diff2`
> columns regardless of this setting -- they are computed for reference.

---

### 9.3 stationarity.tests

Controls which tests are enabled and their individual configurations.

#### ADF test (`tests.adf`)

The **Augmented Dickey-Fuller** test is a unit root test.

- **H0**: the series has a unit root (is non-stationary, I(1))
- **H1**: the series is stationary (I(0))
- **Reject H0** when `tau < critical value` (left-tailed test)
- Critical values from MacKinnon (1994) response surface
- Lag order selected automatically by AIC or BIC (Schwert 1989 upper bound)

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `true` | Run the ADF test. |
| `trend_type` | string | `constant` | Deterministic component included in the regression. See options below. |
| `lag_selection` | string | `aic` | Method for automatic lag order selection. See options below. |
| `max_lags` | int | `-1` | Upper bound on lag search. `-1` = auto via Schwert (1989): `floor(12 * (n/100)^0.25)`. |
| `significance_level` | float | `0.05` | Alpha for the `rejected` flag in the protocol. |

### trend_type options (ADF and PP)

| Value | Regression model | Use when |
|---|---|---|
| `none` | `dy_t = gamma * y_{t-1} + ...` | Series has zero mean and no trend (rare in practice) |
| `constant` | `dy_t = alpha + gamma * y_{t-1} + ...` | Series fluctuates around a non-zero constant mean. **Most common choice.** |
| `trend` | `dy_t = alpha + delta*t + gamma * y_{t-1} + ...` | Series has a visible deterministic linear trend in addition to possible unit root |

> **Guidance**: for climatological residuals after deseasonalization, `constant`
> is almost always the correct choice. Use `trend` only if you observe a clear
> linear drift in the residuals (e.g. long-term climate change signal).

### lag_selection options

| Value | Description |
|---|---|
| `aic` | Minimise AIC over lags 0..max_lags. **Recommended.** Tends to select more lags, capturing more serial correlation. |
| `bic` | Minimise BIC over lags 0..max_lags. More parsimonious than AIC. |
| `fixed` | Use exactly `max_lags` lags. Requires `max_lags >= 0`. |

#### KPSS test (`tests.kpss`)

The **Kwiatkowski-Phillips-Schmidt-Shin** test is a stationarity test --
the **complement** of ADF/PP. It has the opposite null hypothesis.

- **H0**: the series **is** stationary (around a constant or trend)
- **H1**: the series has a unit root (is non-stationary)
- **Reject H0** when `eta > critical value` (right-tailed test)
- Critical values from Kwiatkowski et al. (1992), Table 1 (asymptotic)
- Long-run variance estimated via Newey-West with Bartlett kernel

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `true` | Run the KPSS test. |
| `trend_type` | string | `level` | Model specification. See options below. |
| `lags` | int | `-1` | Newey-West bandwidth. `-1` = auto: `floor(4 * (n/100)^(2/9))`. |
| `significance_level` | float | `0.05` | Alpha for the `rejected` flag. Supported: 0.01, 0.025, 0.05, 0.10. |

### trend_type options (KPSS)

| Value | Tests stationarity around | Use when |
|---|---|---|
| `level` | A constant mean (eta_mu statistic) | Series has no trend. **Most common choice.** |
| `trend` | A linear trend (eta_tau statistic) | Series has a deterministic linear trend and you want to test trend-stationarity |

> **Important**: KPSS is very sensitive to remaining trends and change points.
> A single undetected shift in the series can cause `eta` to be extremely large
> (10x or more above the critical value), even if the series is otherwise stationary.
> This is expected behaviour -- it is precisely what KPSS is designed to detect.

#### PP test (`tests.pp`)

The **Phillips-Perron** test is a non-parametric alternative to ADF.
Instead of augmenting the regression with lagged differences (ADF), PP
applies a semi-parametric Newey-West correction to the standard
Dickey-Fuller t-statistic.

- **H0**: the series has a unit root (same as ADF)
- **H1**: the series is stationary
- **Reject H0** when `Z(t) < critical value` (left-tailed, same as ADF)
- Critical values from MacKinnon (1994) -- identical distribution to ADF
- More robust to heteroskedasticity than ADF; less sensitive to lag choice

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `true` | Run the PP test. |
| `trend_type` | string | `constant` | Same options as ADF: `none`, `constant`, `trend`. |
| `lags` | int | `-1` | Newey-West bandwidth for the correction term. `-1` = auto. |
| `significance_level` | float | `0.05` | Alpha for the `rejected` flag. |

#### Runs test (`tests.runs_test`)

A supplementary non-parametric test for **randomness** (serial independence).
Does not test for unit roots or stationarity per se -- it tests whether the
sequence of signs above/below the median is random. Included as a quick
diagnostic for serial dependence in residuals.

- **H0**: the sequence is random (no serial dependence)
- **H1**: positive or negative serial correlation is present
- The `rejected` flag does **not** influence the joint conclusion or `recommendedDiff`

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `true` | Run the runs test. |

---

### 9.4 stationarity.significance_level

Global significance level used for the **joint conclusion** logic.
Individual test significance levels (in `tests.adf`, `tests.kpss`, `tests.pp`)
control those tests' own `rejected` flags independently.

| Key | Type | Default | Description |
|---|---|---|---|
| `significance_level` | float | `0.05` | Alpha for the joint `isStationary` and `recommendedDiff` conclusion. |

---

### 9.5 Joint conclusion logic

The `StationarityAnalyzer` combines individual test results into a single
verdict using the following logic:

| ADF/PP verdict | KPSS verdict | Joint conclusion | Recommended d |
|---|---|---|---|
| Unit root REJECTED (stationary) | Stationarity NOT rejected (stationary) | **STATIONARY** | 0 |
| Unit root NOT rejected (non-stationary) | Stationarity REJECTED (non-stationary) | **NON-STATIONARY** | 1 |
| Unit root REJECTED | Stationarity REJECTED | **CONFLICTING** -> conservative: NON-STATIONARY | 1 |
| Unit root NOT rejected | Stationarity NOT rejected | **CONFLICTING** -> conservative: NON-STATIONARY | 1 |

> **On conflicting results**: conflicting ADF/KPSS outcomes are common in practice
> and indicate ambiguity rather than error. They arise when:
> - The series has structural breaks or change points (KPSS rejects, ADF may not)
> - The series has long memory / strong persistence (KPSS sensitive, ADF less so)
> - The sample is too short for reliable asymptotic approximations
>
> In these cases, visual inspection of the time series and ACF plots is essential.
> Run `loki_homogeneity` to remove change points and re-test.

> **On `recommendedDiff`**: the value `d` is a recommendation for ARIMA modelling,
> not a prescription. For climatological residuals that are stationary but have
> strong serial dependence, `d=0` with a high AR order is usually preferable to
> differencing. Differencing can destroy meaningful low-frequency signal.

---

### 9.6 Plots (stationarity pipeline)

In addition to the generic plots (see [Section 3](#3-common-plots)), loki_stationarity
produces the following pipeline-specific plot:

| Flag | Default | Description |
|---|---|---|
| `pacf_plot` | `true` | Partial autocorrelation function (PACF) of the residuals with 95% confidence band at +/- 1.96/sqrt(n). Used to identify the AR order p for ARIMA modelling: PACF cuts off sharply at lag p for a pure AR(p) process. |

> **ACF vs PACF for model identification**:
> - **ACF** (autocorrelation function): decays geometrically for AR processes,
>   cuts off sharply at lag q for MA(q). Use ACF to identify MA order.
> - **PACF** (partial autocorrelation function): cuts off sharply at lag p for AR(p),
>   decays geometrically for MA processes. Use PACF to identify AR order.
> - For ARIMA: examine ACF and PACF of the differenced series (if d > 0).

---

### 9.7 CSV output

Written to `<workspace>/OUTPUT/CSV/`.
Filename: `[stationId]_[componentName]_stationarity.csv`

```
mjd ; original ; residual ; diff1 ; diff2
```

| Column | Description |
|---|---|
| `mjd` | Modified Julian Date of the observation |
| `original` | Raw loaded value |
| `residual` | Value after deseasonalization (series used for testing) |
| `diff1` | First differences of residuals: `residual[t] - residual[t-1]` |
| `diff2` | Second differences: `diff1[t] - diff1[t-1]` |

> `diff1` and `diff2` are always written regardless of the `differencing.apply`
> setting. They are shorter than `original` and `residual` by 1 and 2 rows
> respectively -- NaN is used to pad them to the original length in the file.

---

### 9.8 Protocol output

Written to `<workspace>/OUTPUT/PROTOCOLS/`.
Filename: `stationarity_[dataset]_[componentName]_protocol.txt`

The protocol contains:
- Series name, observation count, deseasonalization strategy, differencing settings
- Per-test section: test statistic, critical values at 1%/5%/10%, verdict
- Joint conclusion: `isStationary`, `recommendedDiff`, human-readable summary

---

### Example

```json
"stationarity": {
    "deseasonalization": {
        "strategy": "median_year",
        "ma_window_size": 1461,
        "median_year_min_years": 5
    },
    "differencing": {
        "apply": false,
        "order": 1
    },
    "tests": {
        "adf": {
            "enabled": true,
            "trend_type": "constant",
            "lag_selection": "aic",
            "max_lags": -1,
            "significance_level": 0.05
        },
        "kpss": {
            "enabled": true,
            "trend_type": "level",
            "significance_level": 0.05
        },
        "pp": {
            "enabled": true,
            "trend_type": "constant",
            "significance_level": 0.05
        },
        "runs_test": {
            "enabled": true
        }
    },
    "significance_level": 0.05
}
```

## 10. loki_arima

Default config: `config/arima.json`

The ARIMA pipeline fits an ARIMA(p,d,q) or SARIMA(p,d,q)(P,D,Q)[s] model
to a time series after gap filling and deseasonalization. It is the natural
next step after `loki_stationarity` has confirmed stationarity and recommended
a differencing order d.

The pipeline runs the following steps in order:

```
1. GapFiller              -- fill NaN / missing values (linear)
2. Deseasonalizer         -- subtract seasonal component
3. Differencing           -- d ordinary + D seasonal (auto or configured)
4. ArimaOrderSelector     -- grid search over [0..maxP] x [0..maxQ] (if autoOrder=true)
5. ArimaFitter            -- CSS-HR two-step OLS (Hannan-Rissanen)
6. Ljung-Box diagnostic   -- test residuals for white noise (logged only)
7. ArimaForecaster        -- h-step-ahead forecast with 95% prediction intervals
8. CSV export             -- mjd; original; residual; fitted; forecast; lower95; upper95
9. Protocol               -- ORDER / COEFFICIENTS / DIAGNOSTICS / FORECAST
10. Plots                 -- time series, ACF, PACF, histogram of model residuals
```

### Fitting method: CSS-HR

The Conditional Sum of Squares (CSS) method via Hannan-Rissanen (HR)
two-step approximation is used:

**Step 1 -- Innovation proxy**: fit a high-order AR to the differenced
series to obtain residual proxies for the unobserved innovations epsilon_t.
AR order: `min(max(p + q + P + Q + 4, 10), n/4)`.

**Step 2 -- Joint OLS**: assemble a regressor matrix from lagged y values
(AR lags) and lagged epsilon proxies (MA lags), then solve via
ColPivHouseholderQR. Intercept is always included.

For SARIMA, lag indices follow the **multiplicative Box-Jenkins convention**:
AR lags = `{i + j*s : i in 0..p, j in 0..P} \ {0}` (sorted unique set).
MA lags analogous with q, Q. Cross-terms (e.g. lag s+1 for
ARIMA(1,0,0)(1,0,0)[s]) are automatically included.

### Information criteria

Computed from the CSS log-likelihood approximation:

```
logLik = -n/2 * (log(2*pi*sigma2) + 1)
AIC    = -2 * logLik + 2 * k
BIC    = -2 * logLik + k * log(n)
```

where `k = |arLags| + |maLags| + 1` (number of AR coefficients + MA
coefficients + intercept).

Negative AIC/BIC values are normal for continuous data with small variance.
Use AIC/BIC for **comparing models on the same series** -- the absolute value
has no meaning; only differences matter.

---

### 10.1 arima.gap_fill_strategy / gap_fill_max_length

| Key | Type | Default | Description |
|---|---|---|---|
| `gap_fill_strategy` | string | `linear` | Gap filling strategy. Only `linear` is currently supported. |
| `gap_fill_max_length` | int | `0` | Maximum gap length to fill (in samples). `0` = fill all gaps. |

---

### 10.2 arima.deseasonalization

Subtracts a seasonal component before fitting. Same options as
[outlier.deseasonalization](#51-outlierdeseasonalization).

| Key | Type | Default | Description |
|---|---|---|---|
| `strategy` | string | `median_year` | Deseasonalization method: `median_year`, `moving_average`, `none`. |
| `ma_window_size` | int | `1461` | Window in samples for `moving_average`. For 6-hourly data: 1461 = 1 year. |
| `median_year_min_years` | int | `5` | Minimum years per slot for `median_year`. |

---

### 10.3 arima.auto_order / p / d / q / max_p / max_q / criterion

Controls how the model order (p, d, q) is determined.

| Key | Type | Default | Description |
|---|---|---|---|
| `auto_order` | bool | `true` | If true, select (p, q) by AIC/BIC grid search over [0..max_p] x [0..max_q]. |
| `p` | int | `1` | AR order. Used only if `auto_order=false`. |
| `d` | int | `-1` | Differencing order. `-1` = auto via StationarityAnalyzer. `0`, `1`, `2` = fixed. |
| `q` | int | `0` | MA order. Used only if `auto_order=false`. |
| `max_p` | int | `5` | Upper bound for p in grid search. |
| `max_q` | int | `5` | Upper bound for q in grid search. |
| `criterion` | string | `aic` | Order selection criterion: `aic` or `bic`. |

> **On `d=-1` (auto)**: runs `StationarityAnalyzer` internally with default
> settings (ADF + KPSS + PP). Uses `recommendedDiff` from the joint conclusion.
> For more control over the stationarity tests, run `loki_stationarity` first
> and set `d` manually from the protocol.

> **On grid search**: the selector fits all `(max_p+1) * (max_q+1)` models via
> CSS and selects the minimum AIC or BIC. Models that fail to fit are silently
> skipped with a warning. ARIMA(0,d,0) is always included as the baseline.
> Seasonal order (P, D, Q, s) is fixed during the grid search -- only (p, q)
> varies.

> **Common issue -- overfitting**: if the selected order is high (e.g. p=5,
> q=5), check whether AR and MA coefficients partially cancel each other
> (e.g. AR(4) large positive, MA(4) large negative). This indicates
> near-cancellation of AR/MA polynomials, a sign of over-parameterisation.
> Try setting `auto_order: false` with a smaller model.

---

### 10.4 arima.seasonal

Controls the SARIMA seasonal component. Set `s=0` to disable SARIMA entirely
(pure ARIMA). When `s > 0`, the multiplicative Box-Jenkins expansion is used.

| Key | Type | Default | Description |
|---|---|---|---|
| `P` | int | `0` | Seasonal AR order. |
| `D` | int | `0` | Seasonal differencing order. Applied before ordinary differencing. |
| `Q` | int | `0` | Seasonal MA order. |
| `s` | int | `0` | Seasonal period in samples. `0` = no seasonal component. |

> **Lag index expansion** for SARIMA(p,d,q)(P,D,Q)[s]:
> AR lags = `{i + j*s : i in {0..p}, j in {0..P}} \ {0}` (sorted unique).
> For example, ARIMA(1,0,0)(1,0,0)[12] gives AR lags = {1, 12, 13}.
> MA lags follow the same rule with q and Q.

> **Common seasonal periods**:
> - 6-hourly climatological data: `s=1461` (1 year = 1461 samples)
> - Hourly data: `s=8760`
> - Monthly data: `s=12`
> - Daily data: `s=365`

> **SARIMA vs deseasonalization**: if `strategy != none` in
> `arima.deseasonalization`, the seasonal component is already removed before
> fitting. In that case, SARIMA seasonal terms may be unnecessary. Use SARIMA
> with `strategy: none` if you want the model to capture seasonality directly.

---

### 10.5 arima.fitter

| Key | Type | Default | Description |
|---|---|---|---|
| `method` | string | `css` | Fitting method. Only `css` is currently implemented. `mle` is reserved for future use. |
| `max_iterations` | int | `200` | Reserved for future MLE use. |
| `tol` | float | `1e-8` | Reserved for future MLE use. |

---

### 10.6 arima.compute_forecast / forecast_horizon / confidence_level

| Key | Type | Default | Description |
|---|---|---|---|
| `compute_forecast` | bool | `false` | Compute and export an h-step-ahead forecast. |
| `forecast_horizon` | float | `0.0` | Forecast horizon in **days**. Converted to samples using the observed time step. |
| `confidence_level` | float | `0.95` | Confidence level for prediction intervals. Currently fixed at 95% (1.96 sigma). |
| `significance_level` | float | `0.05` | Alpha used for the Ljung-Box diagnostic and for StationarityAnalyzer when `d=-1`. |

> **Forecast interpretation**: the point forecast is on the **differenced and
> deseasonalized** series, not on the original observations. For climatological
> residuals after deseasonalization and differencing, the long-run forecast
> converges to zero (the mean of the stationary series). Prediction intervals
> widen for the first ~p+q steps, then stabilise once the MA(inf) psi-weights
> decay to zero.

> **Prediction interval formula**:
> `forecast_h +/- 1.96 * sqrt(sigma2 * sum_{j=0}^{h-1} psi_j^2)`
> where psi_j are the MA(inf) coefficients computed from AR and MA polynomials.

---

### 10.7 CSV output

Written to `<workspace>/OUTPUT/CSV/`.
Filename: `[stationId]_[componentName]_arima.csv`

```
mjd ; original ; residual ; fitted ; forecast ; lower95 ; upper95
```

| Column | Description |
|---|---|
| `mjd` | Modified Julian Date |
| `original` | Raw loaded value |
| `residual` | Deseasonalized value (input to ARIMA) |
| `fitted` | Model fitted value on the differenced series |
| `forecast` | `nan` for observed period; point forecast for future steps |
| `lower95` | Lower 95% prediction interval bound |
| `upper95` | Upper 95% prediction interval bound |

> Observed rows have `nan` in forecast/lower95/upper95.
> Forecast rows have `nan` in mjd/original/residual/fitted
> (future timestamps are not available from the model).

---

### 10.8 Protocol output

Written to `<workspace>/OUTPUT/PROTOCOLS/`.
Filename: `[stationId]_[componentName]_arima.txt`

Sections:
- **MODEL ORDER**: ARIMA(p,d,q) or SARIMA(p,d,q)(P,D,Q)[s], method, n
- **COEFFICIENTS**: intercept, AR coefficients with lag indices, MA coefficients
- **DIAGNOSTICS**: sigma2, logLik, AIC, BIC
- **FORECAST**: h-step point forecasts with 95% prediction intervals (if enabled)

---

### 10.9 Plots (arima pipeline)

| Flag | Default | Description |
|---|---|---|
| `time_series` | `true` | Time series of deseasonalized residuals. |
| `histogram` | `true` | Histogram of model residuals (after fitting). |
| `acf` | `true` | ACF of model residuals -- should show no significant autocorrelation for a well-specified model. |
| `pacf_plot` | `true` | PACF of model residuals. |

> **Residual diagnostics**: for a well-specified ARIMA model, the model
> residuals should be approximately white noise -- no significant spikes in
> ACF/PACF beyond lag 0, histogram approximately normal, Ljung-Box p-value
> above the significance level. Significant ACF/PACF spikes after fitting
> indicate that the model order is too low.

---

### 10.10 Example config

```json
"arima": {
    "gap_fill_strategy": "linear",
    "gap_fill_max_length": 0,

    "deseasonalization": {
        "strategy": "median_year",
        "ma_window_size": 1461,
        "median_year_min_years": 5
    },

    "auto_order": true,
    "p": 1,
    "d": -1,
    "q": 0,
    "max_p": 5,
    "max_q": 5,
    "criterion": "aic",

    "seasonal": {
        "P": 0,
        "D": 0,
        "Q": 0,
        "s": 0
    },

    "fitter": {
        "method": "css",
        "max_iterations": 200,
        "tol": 1e-8
    },

    "compute_forecast": false,
    "forecast_horizon": 0.0,
    "confidence_level": 0.95,
    "significance_level": 0.05
}
```

---

## 11. loki_ssa

Default config: `config/ssa.json`

Singular Spectrum Analysis (SSA) decomposes a time series into interpretable
components (trend, oscillations, noise) without assuming a parametric model.
It is particularly suited for climatological and GNSS data with quasi-periodic
structure and unknown spectral content.

The pipeline runs the following steps in order:

```
1. GapFiller              -- fill missing values
2. Deseasonalizer         -- subtract seasonal component (usually "none" for SSA)
3. Trajectory matrix      -- embed series into K x L Hankel matrix
4. SVD                    -- randomized or full eigendecomposition
5. Diagonal averaging     -- reconstruct L elementary components
6. W-correlation matrix   -- (optional) measure separability between components
7. Grouper                -- assign eigentriples to named groups
8. Reconstruction         -- sum components per group
```

> **SSA vs ARIMA**: SSA extracts structure non-parametrically; ARIMA models
> residual autocorrelation parametrically. A typical workflow is to run SSA
> first to extract trend and oscillations, then fit ARIMA to the SSA noise
> component.

> **Deseasonalization**: for SSA the default is `strategy: none` because SSA
> captures the seasonal cycle as oscillatory eigentriples. If the seasonal
> amplitude is very large relative to the trend, pre-subtracting the
> median-year profile can improve trend extraction.

---

### 11.1 ssa.gap_fill_strategy / gap_fill_max_length

| Key | Type | Default | Description |
|---|---|---|---|
| `gap_fill_strategy` | string | `linear` | Gap filling strategy. Only `linear` is currently supported. |
| `gap_fill_max_length` | int | `0` | Maximum gap length to fill (in samples). `0` = fill all gaps. |

---

### 11.2 ssa.deseasonalization

Subtracts a seasonal component before SSA. Same options as
[outlier.deseasonalization](#51-outlierdeseasonalization).

| Key | Type | Default | Description |
|---|---|---|---|
| `strategy` | string | `none` | Deseasonalization method: `none`, `median_year`, `moving_average`. |
| `ma_window_size` | int | `1461` | Window in samples for `moving_average`. For 6-hourly data: 1461 = 1 year. |
| `median_year_min_years` | int | `5` | Minimum years per slot for `median_year`. |

> **Recommendation**: use `strategy: none` for SSA. SSA handles seasonality
> through its oscillatory eigentriples. Pre-deseasonalization is only useful
> when the seasonal amplitude completely dominates the signal and masks the
> trend in the scree plot.

---

### 11.3 ssa.window

Controls the window length L -- the number of columns of the trajectory
(Hankel) matrix. L is the most important SSA parameter.

| Key | Type | Default | Description |
|---|---|---|---|
| `window_length` | int | `0` | Explicit window length in samples. `0` = auto-select. |
| `period` | int | `0` | Dominant period in samples. Used in auto-selection when `window_length=0`. |
| `period_multiplier` | int | `2` | Multiplier for auto L: `L = period * period_multiplier`. |
| `max_window_length` | int | `20000` | Safety cap on auto-selected L. Critical for high-rate data (ms resolution). |

#### Auto-selection logic

```
if window_length > 0 (explicit):
    validate: window_length >= 2 and window_length <= n/2
              and window_length <= max_window_length
    if any constraint violated: warning + fallback to auto

if window_length == 0 (auto):
    if period > 0:
        L = period * period_multiplier
    else:
        L = n / 2
    L = min(L, max_window_length, n/2)
    L = max(L, 2)
```

> **Choice of L**: L should be a multiple of the dominant period for clean
> separation of trend and oscillatory components. For 6-hourly climatological
> data: `period=1461` (1 year), `period_multiplier=1` gives L=1461. Using
> `period_multiplier=2` gives L=2922 which captures longer inter-annual cycles
> but increases computation time significantly.

> **Performance warning**: computation time scales as O(K * L * r) for
> randomized SVD and O(n * L * r) for diagonal averaging, where K = n - L + 1
> and r = `svd_rank`. For large L (> 1000) and long series (n > 10000),
> always set `svd_rank` explicitly (see section 11.5).

> **Common window lengths by data type**:
>
> | Data type | Resolution | Recommended L |
> |---|---|---|
> | Climatological / IWV | 6h | 365 (quarter-year) to 1461 (1 year) |
> | Climatological / IWV | 1h | 2190 (quarter-year) to 8760 (1 year) |
> | GNSS coordinates | 1s | 30362 (draconitic period 351.4 days) |
> | Train / vehicle sensors | ms | set `max_window_length` explicitly |

---

### 11.4 ssa.grouping

Controls how eigentriples are assigned to named groups after decomposition.

| Key | Type | Default | Description |
|---|---|---|---|
| `method` | string | `wcorr` | Grouping strategy. See options below. |
| `wcorr_threshold` | float | `0.8` | W-correlation threshold for hierarchical cut (`wcorr` method). |
| `kmeans_k` | int | `0` | Number of clusters for `kmeans`. `0` = auto via silhouette score. |
| `variance_threshold` | float | `0.95` | Cumulative variance fraction for `variance` method. |
| `manual_groups` | object | `{}` | Name -> index list map for `manual` method. |

#### Grouping methods

| Value | Description | When to use |
|---|---|---|
| `wcorr` | Hierarchical clustering on w-correlation distance matrix (average linkage). Groups with w-corr above `wcorr_threshold` are merged. | Default. Mathematically correct SSA grouping. Requires `compute_wcorr: true`. |
| `manual` | User specifies eigentriple indices explicitly in `manual_groups`. An empty index list acts as catch-all for unassigned eigentriples. | When the spectral structure is known (e.g. from a prior scree plot). |
| `kmeans` | K-means on [variance fraction, zero-crossing rate] feature vector per eigentriple. | Exploratory analysis. |
| `variance` | Keep the first k eigentriples explaining `variance_threshold` of captured variance; remainder becomes "noise". | Fast baseline. |

> **On `wcorr` grouping**: pairs of eigentriples belonging to the same
> oscillation have nearly equal eigenvalues and high mutual w-correlation
> (close to 1). Well-separated pairs (low w-correlation) should be kept in
> separate groups. A threshold of 0.8 is a good starting point; lower it to
> 0.6-0.7 for noisier data.

> **On `manual` grouping**: use the scree plot and w-correlation heatmap from
> a first run to identify oscillatory pairs, then assign them manually.
> Example: if the scree plot shows a dominant F0 (trend) and pairs at
> [1,2], [3,4], assign:
> ```json
> "manual_groups": {
>     "trend":  [0],
>     "annual": [1, 2],
>     "semi":   [3, 4],
>     "noise":  []
> }
> ```
> The empty `"noise"` list captures all remaining eigentriples.

> **Oscillatory pairs**: in SSA, a periodic component of frequency f appears
> as a pair of eigentriples with nearly equal eigenvalues. The pair shares the
> same frequency but is 90 degrees out of phase. They must be grouped together
> to reconstruct the oscillation correctly.

---

### 11.5 ssa.reconstruction

| Key | Type | Default | Description |
|---|---|---|---|
| `method` | string | `diagonal_averaging` | Reconstruction method. See options below. |

| Value | Description |
|---|---|
| `diagonal_averaging` | Standard SSA reconstruction. Anti-diagonal averaging of each elementary matrix `E_i = sv_i * u_i * v_i^T`. Mathematically correct. Use for all final results. |
| `simple` | Fast approximation. Takes the first column of each trajectory window. Less accurate but faster for exploration. |

> Use `diagonal_averaging` for all production runs. `simple` is only useful
> for quick exploration of very large datasets.

---

### 11.6 ssa.svd_rank / svd_oversampling / svd_power_iter

Controls the randomized SVD algorithm (Halko et al. 2011).

| Key | Type | Default | Description |
|---|---|---|---|
| `svd_rank` | int | `40` | Number of eigentriples to compute. `0` = full eigendecomposition of C = X^T*X (only feasible for small L). |
| `svd_oversampling` | int | `10` | Extra columns in the random projection for accuracy. Working rank = `svd_rank + svd_oversampling`. |
| `svd_power_iter` | int | `2` | Subspace power iterations. More iterations improve accuracy for slowly decaying spectra at the cost of extra matrix multiplications. |

> **On `svd_rank`**: for climatological data you rarely need more than 40-60
> eigentriples. The trend is almost always in F0 (or F0+F1), and the dominant
> annual cycle is in the first 2-4 pairs. Set `svd_rank` to cover the
> components of interest plus a safety margin for the grouper.

> **On `svd_rank=0` (full decomposition)**: uses `SelfAdjointEigenSolver` on
> the L x L covariance matrix C = X^T * X. Feasible only for small L (< 500).
> For L=1461 this takes tens of minutes. Use randomized SVD (`svd_rank > 0`)
> for all practical cases.

> **On accuracy**: the randomized SVD error for singular value i is bounded by
> O(sigma_{k+1}) where sigma_{k+1} is the next singular value after the
> computed rank. For climatological data with fast spectral decay (large gap
> between F0 and F1...) the approximation is excellent. Increase
> `svd_power_iter` to 3-4 if the scree plot shows unexpectedly noisy variance
> fractions.

> **Performance guide** (approximate, 6h climatological data, n=36524):
>
> | L | svd_rank | SVD time | Diag avg time | Total |
> |---|---|---|---|---|
> | 365 | 40 | ~70s | ~70s | ~2.5 min |
> | 365 | 20 | ~35s | ~35s | ~1 min |
> | 1461 | 40 | ~5 min | ~5 min | ~10 min |

---

### 11.7 ssa.compute_wcorr / wcorr_max_components

| Key | Type | Default | Description |
|---|---|---|---|
| `compute_wcorr` | bool | `true` | Compute the w-correlation matrix. Required for `grouping.method=wcorr`. |
| `wcorr_max_components` | int | `30` | Maximum number of components used in the w-correlation matrix. `0` = all `svd_rank` components. |

> **W-correlation matrix**: an r x r matrix W where W[i][j] in [0,1] measures
> how separable components i and j are. Values close to 1 mean the components
> are well-separated (should be in different groups). Values close to 0 mean
> strong mixing (should be grouped together). The heatmap plot visualises this.

> **Performance**: w-correlation is O(r^2 * n). For r=30, n=36524: ~33M
> weighted inner products -- fast (< 1 second). For r=100 it becomes
> O(100^2 * 36524) = 365M ops which takes several seconds. Keep
> `wcorr_max_components` at 30-50 in practice; the high-index eigentriples
> are noise and their w-correlations are not informative for grouping.

> **Disable for speed**: set `compute_wcorr: false` to skip this step
> entirely. In that case `grouping.method=wcorr` will fail -- use `variance`
> or `manual` instead.

---

### 11.8 ssa.significance_level / forecast_tail

| Key | Type | Default | Description |
|---|---|---|---|
| `significance_level` | float | `0.05` | Reserved for future hypothesis tests on SSA components. |
| `forecast_tail` | int | `1461` | Number of observed samples shown before the forecast region in the reconstruction plot. In samples. |

---

### 11.9 CSV output

Written to `<workspace>/OUTPUT/CSV/`.
Filename: `[stationId]_[componentName]_ssa.csv`

```
mjd ; original ; trend ; noise ; [group_1 ; group_2 ; ...]
```

| Column | Description |
|---|---|
| `mjd` | Modified Julian Date |
| `original` | Gap-filled (and optionally deseasonalized) series value |
| `trend` | Reconstruction of the group named "trend". `nan` if no such group. |
| `noise` | Reconstruction of the group named "noise". `nan` if no such group. |
| `[group name]` | One column per additional group (e.g. `annual`, `signal`, `group_1`). |

> Column order: `trend` and `noise` always appear in positions 3 and 4.
> Additional groups follow in the order they appear in `result.groups`
> (sorted by descending variance fraction).

---

### 11.10 Protocol output

Written to `<workspace>/OUTPUT/PROTOCOLS/`.
Filename: `[stationId]_[componentName]_ssa.txt`

Sections:
- **Header**: series name, n, L, K, grouping method
- **EIGENVALUES**: first 20 eigenvalues with variance fraction and cumulative
- **GROUPS**: group name, variance fraction, eigentriple index list

---

### 11.11 Plots (ssa pipeline)

| Flag | Default | Description |
|---|---|---|
| `ssa_scree` | `true` | Eigenvalue spectrum (log scale, bar chart) with cumulative variance curve. Dashed line at 95% cumulative variance. |
| `ssa_wcorr` | `true` | W-correlation matrix heatmap (r x r). White = well-separated, dark blue = strongly mixed. |
| `ssa_components` | `true` | First N elementary reconstructed components vs MJD (stacked panels). N controlled by `nComponents` in `plotAll()`. |
| `ssa_reconstruction` | `true` | Original series (grey) + per-group reconstructions overlaid. Noise group shown in separate bottom panel. |

> **Reading the scree plot**: a large gap between F0 and F1 indicates a
> dominant trend. Pairs of bars with similar height indicate oscillatory
> components. A flat tail indicates noise. The 95% dashed line helps decide
> how many components to retain.

> **Reading the w-correlation heatmap**: blocks of high correlation along the
> diagonal indicate groups of related eigentriples. Off-diagonal high
> correlations indicate pairs (oscillations). Ideally, the matrix shows
> isolated blocks for the trend, each oscillatory pair, and noise.

---

### 11.12 Example config

```json
"ssa": {
    "gap_fill_strategy": "linear",
    "gap_fill_max_length": 0,

    "deseasonalization": {
        "strategy": "none",
        "ma_window_size": 1461,
        "median_year_min_years": 5
    },

    "window": {
        "window_length": 0,
        "period": 1461,
        "period_multiplier": 1,
        "max_window_length": 20000
    },

    "grouping": {
        "method": "variance",
        "wcorr_threshold": 0.8,
        "kmeans_k": 0,
        "variance_threshold": 0.95,
        "manual_groups": {
            "trend":  [0],
            "annual": [1, 2, 3, 4],
            "noise":  []
        }
    },

    "reconstruction": {
        "method": "diagonal_averaging"
    },

    "svd_rank": 40,
    "svd_oversampling": 10,
    "svd_power_iter": 2,

    "compute_wcorr": true,
    "wcorr_max_components": 30,

    "significance_level": 0.05,
    "forecast_tail": 1461
}
```

#### Recommended starting configurations

**Fast exploration** (under 1 minute for 25 years of 6h data):
```json
"window":   { "window_length": 365 },
"grouping": { "method": "variance", "variance_threshold": 0.95 },
"svd_rank": 20,
"compute_wcorr": false
```

**Standard analysis** (2-3 minutes):
```json
"window":   { "window_length": 365, "period": 1461, "period_multiplier": 1 },
"grouping": { "method": "wcorr", "wcorr_threshold": 0.8 },
"svd_rank": 40,
"compute_wcorr": true,
"wcorr_max_components": 30
```

**Manual grouping after scree inspection**:
```json
"window":   { "window_length": 1461 },
"grouping": {
    "method": "manual",
    "manual_groups": {
        "trend":  [0],
        "annual": [1, 2],
        "semi":   [3, 4],
        "noise":  []
    }
},
"svd_rank": 20,
"compute_wcorr": false
```

12. [loki_decomposition](#12-loki_decomposition)

---

## 12. loki_decomposition

**Program:** `loki_decomposition.exe`
**Default config:** `config/decomposition.json`

Decomposes a time series into three additive components:

```
Y[t] = T[t] + S[t] + R[t]
```

where T = trend, S = seasonal, R = residual. Two methods are available:
Classical (fast, parametric) and STL (iterative LOESS, robust to outliers).

The section key in the JSON file is `"decomposition"`.

---

### 12.1 decomposition.method / period

| Key | Type | Default | Description |
|---|---|---|---|
| `method` | string | `classical` | Decomposition algorithm. See options below. |
| `period` | int | `1461` | Period of the seasonal component **in samples**. Must be >= 2. |
| `gap_fill_strategy` | string | `linear` | Gap filling applied before decomposition. `linear`, `forward_fill`, or `mean`. |
| `gap_fill_max_length` | int | `0` | Maximum gap length to fill (samples). `0` = unlimited. |
| `significance_level` | float | `0.05` | Reserved for future hypothesis tests on residuals. |

| Value | Description |
|---|---|
| `classical` | Centered moving-average trend + per-slot seasonal median/mean. Fast, no iteration, no parameters to tune. |
| `stl` | Seasonal-Trend decomposition using LOESS (Cleveland et al. 1990). Iterative, robust to outliers, more accurate seasonal extraction. Slower than Classical. |

> **Choosing `period`**: the period must be expressed in **samples**, not in
> time units. Common values:
>
> | Sampling rate | 1 year | 1 day |
> |---|---|---|
> | 6-hourly | 1461 | 4 |
> | Daily | 365 | 1 |
> | Hourly | 8760 | 24 |
> | Monthly | 12 | — |
>
> For GNSS data with draconitic periodicity use 351.4 days converted to
> samples. For irregular sampling use the median number of samples per cycle.

> **Choosing `method`**: use `classical` for a quick first look or when the
> seasonal amplitude is stable over time. Use `stl` when the seasonal pattern
> changes slowly over the record, when there are outliers that should not
> contaminate the seasonal estimate, or when a smoother trend is needed.

---

### 12.2 decomposition.classical

Settings for the Classical decomposition path. Ignored when `method = stl`.

| Key | Type | Default | Description |
|---|---|---|---|
| `trend_filter` | string | `moving_average` | Trend estimator. Only `moving_average` is currently implemented. |
| `seasonal_type` | string | `median` | Slot aggregation method. `median` or `mean`. |

| Value | Description |
|---|---|
| `median` | Each seasonal slot is estimated as the median of all detrended values at that phase. Robust to occasional large anomalies. Recommended for climatological data. |
| `mean` | Each seasonal slot is estimated as the mean. Slightly smoother, but sensitive to outliers. Use when the series has already been cleaned. |

> **How the Classical trend works**: a centered moving average of width =
> `period` is applied to the series. The NaN values at the first and last
> `period/2` samples (edge effect) are filled by the nearest valid value
> (bfill/ffill). The seasonal component is then computed from the detrended
> residuals Y - T by aggregating all values that fall in the same phase slot
> (index mod period) and normalising so that the sum over one full period
> equals zero.

> **When `seasonal_type = median` vs `mean`**: for 6h climatological data
> spanning 25 years, each slot contains ~25 values. The median is almost
> as efficient as the mean and protects against years with anomalous conditions
> (e.g. a volcanic eruption year that shifts all summer values). Use `mean`
> only when the input has already been outlier-cleaned.

---

### 12.3 decomposition.stl

Settings for the STL decomposition path. Ignored when `method = classical`.

| Key | Type | Default | Description |
|---|---|---|---|
| `n_inner` | int | `2` | Inner loop iterations (seasonal + trend pass per outer step). Must be >= 1. |
| `n_outer` | int | `1` | Outer robustness iterations. `0` = no robustness weighting (equivalent to non-robust STL). |
| `s_degree` | int | `1` | Local polynomial degree for the seasonal LOESS smoother. `1` or `2`. |
| `t_degree` | int | `1` | Local polynomial degree for the trend LOESS smoother. `1` or `2`. |
| `s_bandwidth` | float | `0.0` | Seasonal LOESS bandwidth as a fraction of subseries length. `0.0` = auto (1.5 / period). |
| `t_bandwidth` | float | `0.0` | Trend LOESS bandwidth as a fraction of series length. `0.0` = auto (1.5 * period / n). |

> **Inner loop (`n_inner`)**: each inner iteration refines both the seasonal
> and trend estimates. Two iterations are almost always sufficient; the
> difference between 2 and 5 is negligible for typical climatological data.
> Values of 1 are acceptable for fast exploration.

> **Outer loop (`n_outer`)**: each outer iteration recomputes bisquare
> robustness weights from the current residuals. Points with large residuals
> (outliers, extreme events) receive low weight and are downweighted in the
> LOESS fits. `n_outer = 0` gives non-robust STL (same seasonal amplitude
> as Classical). `n_outer = 1` is sufficient for data with occasional
> anomalies. Use `n_outer = 3` for data with frequent outliers or step changes.

> **Bandwidths**: the auto-selected bandwidths are:
> - `s_bandwidth = 1.5 / period` -- each subseries uses 1.5 cycle-lengths of
>   neighbours. This gives smooth inter-annual evolution of the seasonal shape.
> - `t_bandwidth = 1.5 * period / n` -- the trend smoother spans ~1.5 periods.
>   For 25 years of 6h data this is approximately 1.5 years.
>
> To get a stiffer (less smooth) trend, increase `t_bandwidth`. To allow the
> seasonal shape to evolve more rapidly over years, decrease `s_bandwidth`.
> Both must be in (0, 1] if set manually.

> **Degree**: `s_degree = 1` (linear local fit) is appropriate for all
> practical cases. `s_degree = 2` (quadratic) can improve accuracy when the
> seasonal shape has sharp curvature (e.g. temperature at high latitudes)
> at the cost of higher computation time.

---

### 12.4 Interpreting the decomposition output

**Protocol variance explained** (example from a 25-year 6h climatological series):

```
Trend    : 25.51 %
Seasonal : 27.40 %
Residual : 47.58 %
```

The variance fractions are computed as `(std of component)^2 / (std of original)^2`.
They do not sum to 100 % in general because the components are not orthogonal.

> **Residual dominates (> 40 %)**: this is normal and expected for
> climatological data at sub-daily resolution. Synoptical variability (weather
> systems, frontal passages) operates on timescales of days to weeks and
> cannot be captured by the trend or the annual seasonal component. A large
> residual is a physical property of the data, not a sign of a poor model.

> **Trend and Seasonal have similar variance**: this indicates a well-developed
> annual cycle that is comparable in amplitude to the long-term variability.
> Typical for temperature, pressure, and humidity at mid-latitudes.

> **Residual panel has visible structure**: if the residual panel shows a
> periodic pattern or a slow drift, the model is incomplete. Consider switching
> to STL (which can capture a slowly evolving seasonal shape) or running
> `loki_ssa` on the residual to identify remaining oscillatory components.

> **Residual panel looks like white noise**: the decomposition is complete.
> The residual is suitable for input to `loki_arima` or stationarity testing
> with `loki_stationarity`.

---

### 12.5 Relationship to other LOKI modules

| Module | Role in relation to decomposition |
|---|---|
| `loki_filter` | Can estimate the trend alone (moving average, LOESS). Use when only the trend is needed without a seasonal component. |
| `loki_ssa` | Non-parametric data-driven decomposition. Does not assume a fixed period. Better for multi-scale oscillations. Slower. Use after Classical/STL to analyse the residual. |
| `loki_arima` | Suitable input: the residual R[t] from decomposition, if it is approximately stationary. |
| `loki_stationarity` | Use on the residual R[t] to verify that trend and seasonal have been fully removed before ARIMA modelling. |
| `loki_homogeneity` | Run on the residual R[t] for change point detection. The decomposed residual is more stationary and gives fewer false detections than the raw series. |

---

### 12.6 CSV output

Written to `<workspace>/OUTPUT/CSV/`.
Filename: `decomposition_[dataset]_[component].csv`

```
time_mjd ; original ; trend ; seasonal ; residual
```

| Column | Description |
|---|---|
| `time_mjd` | Modified Julian Date of each observation |
| `original` | Gap-filled input series value |
| `trend` | Estimated trend component T[t] |
| `seasonal` | Estimated seasonal component S[t] |
| `residual` | Remainder R[t] = original - trend - seasonal |

---

### 12.7 Protocol output

Written to `<workspace>/OUTPUT/PROTOCOLS/`.
Filename: `decomposition_[dataset]_[component].txt`

Sections:
- **Header**: dataset, component, method, period, N
- **Algorithm parameters**: trend filter and seasonal type (Classical) or n_inner, n_outer, degrees (STL)
- **Component statistics**: mean and std for original, trend, seasonal, residual
- **Variance explained**: variance fraction of each component relative to original

---

### 12.8 Plots (decomposition pipeline)

| Flag | Default | Description |
|---|---|---|
| `decomp_overlay` | `true` | Original series (grey) with trend overlaid (red). Shows how well the trend follows the long-term evolution. |
| `decomp_panels` | `true` | 3-panel stacked figure: trend (top), seasonal (middle), residual (bottom). Standard decomposition visualisation. |
| `decomp_diagnostics` | `false` | 4-panel residual diagnostics: residuals vs index, Normal Q-Q plot with 95% confidence bands, histogram with normal fit, and ACF of residuals. |

> **Reading the panels plot**: the trend panel should show a smooth
> long-term signal without rapid oscillations. The seasonal panel should show
> a stable repeating pattern. If the seasonal amplitude changes visibly over
> time, STL with `n_outer >= 1` will give a better fit. The residual panel
> should be centred on zero with no visible trend or periodic structure.

> **Enable `decomp_diagnostics`** when you intend to use the residual as input
> to ARIMA or stationarity tests. The ACF panel reveals whether any periodic
> structure remains, and the Q-Q plot indicates whether the residuals are
> approximately Gaussian.

---

### 12.9 Example config

```json
"decomposition": {
    "method": "classical",
    "period": 1461,
    "gap_fill_strategy": "linear",
    "gap_fill_max_length": 0,

    "classical": {
        "trend_filter": "moving_average",
        "seasonal_type": "median"
    },

    "stl": {
        "n_inner": 2,
        "n_outer": 1,
        "s_degree": 1,
        "t_degree": 1,
        "s_bandwidth": 0.0,
        "t_bandwidth": 0.0
    },

    "significance_level": 0.05
}
```

#### Recommended starting configurations

**Classical -- quick first look** (seconds for any dataset size):
```json
"method": "classical",
"period": 1461,
"classical": { "seasonal_type": "median" }
```

**STL -- robust, slowly evolving seasonality** (minutes for 25 years of 6h data):
```json
"method": "stl",
"period": 1461,
"stl": { "n_inner": 2, "n_outer": 3 }
```

**STL -- faster, non-robust** (for pre-cleaned data):
```json
"method": "stl",
"period": 1461,
"stl": { "n_inner": 1, "n_outer": 0 }
```

**Daily data (1 year = 365 samples)**:
```json
"method": "classical",
"period": 365,
"classical": { "seasonal_type": "median" }
```