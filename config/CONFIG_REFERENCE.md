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
