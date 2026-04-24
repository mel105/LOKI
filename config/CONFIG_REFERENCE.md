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
12. [loki_decomposition](#12-loki_decomposition)
13. [loki_spectral](#13-loki_spectral)
14. [loki_kalman](#14-loki_kalman)
15. [loki_qc](#15-loki_qc)
16. [loki_clustering](#16-loki_clustering)
17. [loki_simulate](#17-loki_simulate)
18. [loki_evt](#18-loki_evt)
19. [loki_kriging](#19-loki_kriging)
20. [loki_spline](#20-loki_spline)
21. [loki_spatial](#21-loki_spatial)

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
| `strategy` | string | `linear` | Fill strategy: `linear`, `forward_fill`, `mean`, `spline`, `none` |
| `max_fill_length` | int | `0` | Maximum consecutive gap samples to fill. `0` = no limit. |
| `gap_threshold_factor` | float | `1.5` | Multiplier on expected step to detect a gap. |
| `min_series_years` | int | `10` | Minimum span for `median_year` gap filling. Ignored for `spline`. |

### 6.2 homogeneity.pre_outlier

Coarse outlier removal on the raw (gap-filled) series before deseasonalization.
Intended to remove physically impossible values only. Use a large multiplier (>= 5.0).
Detection runs on original-scale values -- no seasonal subtraction occurs here.

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `false` | Enable pre-outlier removal |
| `method` | string | `mad_bounds` | Detection method: `mad_bounds`, `mad`, `iqr`, `zscore` |
| `mad_multiplier` | float | `5.0` | Use high value (5.0+) for coarse filtering only |
| `iqr_multiplier` | float | `1.5` | IQR fence width |
| `zscore_threshold` | float | `3.0` | Z-score threshold |
| `replacement_strategy` | string | `linear` | Fill strategy for detected positions: `linear`, `forward_fill`, `mean` |
| `max_fill_length` | int | `0` | Maximum consecutive positions to fill |

### 6.3 homogeneity.deseasonalization

| Key | Type | Default | Description |
|---|---|---|---|
| `strategy` | string | `median_year` | Deseasonalization method: `median_year`, `moving_average`, `none` |
| `ma_window_size` | int | `365` | Window size in samples for `moving_average` strategy |
| `median_year_min_years` | int | `5` | Minimum years of data required to build the median year profile |

Default `strategy` is `median_year` (recommended for climatological data).
For 6-hour resolution data, use `ma_window_size: 1461` (1 year = 4 x 365.25 samples).

### 6.4 homogeneity.post_outlier

Fine outlier removal on deseasonalized residuals, before change point detection.
Detection operates directly on residuals -- seasonal component is already removed in step 3.
Use a tighter multiplier than pre_outlier.

Same keys as [pre_outlier](#62-homogeneitypre_outlier).
Recommended defaults: `method: mad`, `mad_multiplier: 3.0`.

### 6.5 homogeneity.detection

Controls the change point detection method and its parameters.

#### 6.5.1 Method selection

| Key | Type | Default | Description |
|---|---|---|---|
| `method` | string | `yao_davis` | Detection algorithm: `yao_davis`, `snht`, `pelt`, `bocpd` |
| `min_segment_points` | int | `60` | Minimum number of observations per segment. Applied by `MultiChangePointDetector` before calling the detector. Ignored by `pelt` and `bocpd` (use their own `min_segment_length` instead). |
| `min_segment_duration` | string | `""` | Human-readable minimum segment duration. Format: `"1y"`, `"180d"`, `"6h"`. Overrides `min_segment_seconds`. |
| `min_segment_seconds` | float | `0.0` | Minimum segment duration in seconds. |
| `significance_level` | float | `0.05` | p-value threshold for `yao_davis` and `snht`. Not used by `pelt` or `bocpd`. |
| `acf_dependence_limit` | float | `0.2` | ACF lag-1 limit above which dependence correction is applied (`yao_davis` only). |
| `correct_for_dependence` | bool | `true` | Apply ACF-based sigmaStar correction (`yao_davis` only). |

#### 6.5.2 Method comparison

| Method | Algorithm | CP search | Hypothesis test | Best for |
|---|---|---|---|---|
| `yao_davis` | Yao & Davis (1986) T-statistic with ACF correction | Recursive binary splitting | Asymptotic Gumbel + sigmaStar | General purpose, well-validated |
| `snht` | Alexandersson (1986) SNHT T-statistic | Recursive binary splitting | Monte Carlo permutation | Climate station data, reference series |
| `pelt` | Killick et al. (2012) penalised cost | Single global pass O(n) | Penalised likelihood (no p-value) | Fast, many potential CPs, deterministic |
| `bocpd` | Adams & MacKay (2007) Bayesian online | Left-to-right online | Posterior probability threshold | Streaming data, needs careful prior calibration |

**Key behavioural differences:**

- `yao_davis` and `snht` use **recursive binary splitting** -- each call finds one CP, then recurses on both halves. `min_segment_points` controls the recursion depth.
- `pelt` and `bocpd` find **all CPs in a single pass** -- `min_segment_points` is ignored; use their own `min_segment_length` parameter instead.
- Only `yao_davis` and `snht` produce meaningful `p-value` fields in the output. PELT sets `p=0.0` (no hypothesis test). BOCPD sets `p` to the posterior CP probability at the detected position.

#### 6.5.3 yao_davis settings

No additional JSON block required. Parameters are taken from the top-level `detection` keys:
`significance_level`, `acf_dependence_limit`, `correct_for_dependence`.

**Recommended settings for 6h climatological GPS IWV data:**
```json
"detection": {
    "method": "yao_davis",
    "min_segment_points": 1461,
    "significance_level": 0.005,
    "acf_dependence_limit": 0.2,
    "correct_for_dependence": true
}
```
Typical result: 7-10 change points on a 25-year series.

#### 6.5.4 snht settings

| Key | Type | Default | Description |
|---|---|---|---|
| `snht.n_permutations` | int | `999` | Number of Monte Carlo permutations for p-value estimation. Use `4999` for publication-quality results. |
| `snht.seed` | int | `0` | RNG seed. `0` = non-deterministic. Set a fixed value (e.g. `42`) for reproducibility. |

The SNHT p-value is estimated via permutation -- minimum achievable p-value is `1/(n_permutations+1)`.
With `n_permutations=999`, minimum p is `0.001`. With `4999`, minimum p is `0.0002`.

**Recommended settings for 6h climatological GPS IWV data:**
```json
"detection": {
    "method": "snht",
    "min_segment_points": 5804,
    "significance_level": 0.005,
    "snht": {
        "n_permutations": 999,
        "seed": 42
    }
}
```
Typical result: 10-15 change points on a 25-year series (more sensitive than `yao_davis`).

**Tuning guidance for SNHT:**
SNHT is inherently more sensitive than Yao&Davis because its T-statistic responds
more strongly to small mean shifts. When used in recursive binary splitting mode,
sensitivity is controlled primarily by `min_segment_points` and secondarily by
`significance_level`.

| Too many CPs | Too few CPs |
|---|---|
| Increase `min_segment_points` | Decrease `min_segment_points` |
| Decrease `significance_level` (e.g. `0.001`) | Increase `significance_level` (e.g. `0.01`) |
| Increase `n_permutations` (improves p-value resolution) | -- |

At 6h resolution (4 obs/day): `1461` = 1 year, `2922` = 2 years, `4383` = 3 years, `5844` = 4 years.
`n_permutations` has minimal effect on the number of detected CPs -- it only affects p-value
precision. The dominant control is `min_segment_points`.

#### 6.5.5 pelt settings

| Key | Type | Default | Description |
|---|---|---|---|
| `pelt.penalty_type` | string | `bic` | Penalty function: `bic`, `aic`, `mbic`, `fixed` |
| `pelt.fixed_penalty` | float | `0.0` | Penalty value when `penalty_type == "fixed"` |
| `pelt.min_segment_length` | int | `2` | Minimum observations in any segment |

**Penalty formulas** (sigma^2 estimated via MAD of first differences):

| Type | Formula | Character |
|---|---|---|
| `aic` | `2 * sigma^2` | Liberal -- more CPs |
| `bic` | `log(n) * sigma^2` | Moderate -- good default |
| `mbic` | `3 * log(n) * sigma^2` | Conservative -- fewer CPs |
| `fixed` | user-specified | Full manual control |

**Recommended settings for 6h climatological GPS IWV data:**
```json
"detection": {
    "method": "pelt",
    "pelt": {
        "penalty_type": "mbic",
        "min_segment_length": 3652
    }
}
```
Typical result: 7-9 change points on a 25-year series with `mbic`.

**Tuning guidance for PELT:**

| Too many CPs | Too few CPs |
|---|---|
| Switch `aic` -> `bic` -> `mbic` | Switch `mbic` -> `bic` -> `aic` |
| Increase `min_segment_length` | Decrease `min_segment_length` |
| Use `fixed` with manually increased penalty | Use `fixed` with manually decreased penalty |

PELT does not produce p-values. The `p=0.0` in the output is expected -- the decision
is based entirely on the penalty, not a hypothesis test.

#### 6.5.6 bocpd settings

| Key | Type | Default | Description |
|---|---|---|---|
| `bocpd.hazard_lambda` | float | `250.0` | Expected run length (average samples between CPs). Set to expected segment length in samples. |
| `bocpd.prior_mean` | float | `0.0` | Prior mean of the segment mean (NIX model). Set to the expected mean of deseasonalized residuals. |
| `bocpd.prior_var` | float | `1.0` | Prior precision weight kappa_0. Smaller = vaguer prior on mean. |
| `bocpd.prior_alpha` | float | `1.0` | InverseGamma shape prior. Use >= 2.0 for well-defined prior variance mean. |
| `bocpd.prior_beta` | float | `1.0` | InverseGamma scale prior. Set close to actual series variance. |
| `bocpd.threshold` | float | `0.5` | Posterior probability threshold for declaring a CP. Range (0, 1). |
| `bocpd.min_segment_length` | int | `30` | Minimum samples between consecutive detected CPs. |

**Critical calibration note:** BOCPD performance is highly sensitive to `prior_beta`.
This parameter must be set close to the actual within-segment variance of the series.
For deseasonalized GPS IWV residuals: `std ~ 0.034`, so `sigma^2 ~ 0.001`,
use `prior_beta ~ 0.001-0.003`. Using the default `prior_beta=1.0` on such data
will result in zero detections because the model's predictive distribution is far
too wide to concentrate posterior mass on any run length.

**Recommended settings for 6h climatological GPS IWV data:**
```json
"detection": {
    "method": "bocpd",
    "bocpd": {
        "hazard_lambda": 4383.0,
        "prior_mean": 0.0,
        "prior_var": 0.01,
        "prior_alpha": 2.0,
        "prior_beta": 0.002,
        "threshold": 0.3,
        "min_segment_length": 1461
    }
}
```

**Tuning guidance for BOCPD:**

| Problem | Remedy |
|---|---|
| Zero detections | Lower `threshold` (try 0.15-0.2); set `prior_beta` close to actual `sigma^2` |
| Too many detections | Increase `threshold` (try 0.5-0.7); increase `hazard_lambda`; increase `min_segment_length` |
| Very slow | BOCPD is O(n^2) in memory per step before pruning. Long series (n>10000) may be slow. |

BOCPD is best suited for **shorter series or streaming scenarios** where segments
are well-separated and the hazard rate is known approximately. For long climatological
archives (25+ years, n>30000) with small mean shifts, `yao_davis` or `pelt` are more
reliable choices with less calibration effort.

### 6.6 homogeneity.apply_adjustment

| Key | Type | Default | Description |
|---|---|---|---|
| `apply_adjustment` | bool | `true` | Apply shift corrections to produce the homogenized series. Set `false` to detect only without modifying the series. |

The first detected segment is always the **reference segment** -- it is left unchanged.
All subsequent segments are shifted toward the mean level of the first segment.

### 6.7 Complete example

```json
"homogeneity": {
    "apply_gap_filling": true,
    "gap_filling": {
        "strategy": "linear",
        "max_fill_length": 0,
        "gap_threshold_factor": 1.5,
        "min_series_years": 10
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
        "ma_window_size": 1461,
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
        "method": "pelt",
        "min_segment_points": 1461,
        "min_segment_duration": "365d",
        "significance_level": 0.005,
        "acf_dependence_limit": 0.2,
        "correct_for_dependence": true,
        "snht": {
            "n_permutations": 999,
            "seed": 42
        },
        "pelt": {
            "penalty_type": "mbic",
            "fixed_penalty": 0.0,
            "min_segment_length": 3652
        },
        "bocpd": {
            "hazard_lambda": 4383.0,
            "prior_mean": 0.0,
            "prior_var": 0.01,
            "prior_alpha": 2.0,
            "prior_beta": 0.002,
            "threshold": 0.3,
            "min_segment_length": 1461
        }
    },
    "apply_adjustment": true
}
```

### 6.8 Method selection guide

```
Is the series long (n > 10000) with small expected shifts?
  YES --> yao_davis or pelt
          yao_davis: well-validated, produces p-values, slower (recursive)
          pelt:      fast O(n), no p-values, deterministic, easy to tune via penalty

Is computation time critical?
  YES --> pelt (single pass, O(n) amortised)
  NO  --> yao_davis or snht

Is the series a climate station record compared to a reference series?
  YES --> snht (designed for this use case)
          Tune min_segment_points to 3-4 years in samples

Is the data arriving as a stream or near-real-time?
  YES --> bocpd (online algorithm, processes left-to-right)
          Requires careful prior_beta calibration to actual series variance

Typical result on 25-year 6h GPS IWV data (n~36500):
  yao_davis : ~7-8 CP   (min_segment=1461, significance=0.005)
  snht      : ~10-15 CP (min_segment=5800, significance=0.005)
  pelt mbic : ~7-9 CP   (min_segment=3652)
  bocpd     : variable, needs calibration
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

## 13. loki_spectral

**Program:** `loki_spectral.exe`
**Default config:** `config/spectral.json`

Performs spectral analysis of a time series to identify dominant periodicities,
estimate power spectral density, and characterise amplitude and phase structure.
Two methods are available: FFT (for uniform data) and Lomb-Scargle (for gappy
or unevenly sampled data). An optional STFT spectrogram shows how spectral
content evolves over time.

The pipeline runs the following steps in order:

```
1. GapFiller              -- fill missing values (required for FFT)
2. Method selection       -- "auto", "fft", or "lomb_scargle"
3. Periodogram            -- FFT (Cooley-Tukey) or Lomb-Scargle
4. Peak detection         -- local maxima above noise floor, ranked by power
5. CSV / Protocol         -- top N peaks with period, frequency, power, FAP
6. Plots                  -- PSD, amplitude, phase, spectrogram
```

The section key in the JSON file is `"spectral"`.

---

### 13.1 spectral.method / gap_fill_strategy / gap_fill_max_length

| Key | Type | Default | Description |
|---|---|---|---|
| `method` | string | `auto` | Analysis method. See options below. |
| `gap_fill_strategy` | string | `linear` | Gap filling applied before analysis. `linear`, `forward_fill`, `mean`, `none`. |
| `gap_fill_max_length` | int | `0` | Maximum consecutive gap samples to fill. `0` = no limit. |
| `significance_level` | float | `0.05` | Reserved for future hypothesis tests on spectral peaks. |

### method options

| Value | Description | When to use |
|---|---|---|
| `auto` | Inspects the series: if fewer than 5% of consecutive time steps exceed 1.1x the median step, uses FFT; otherwise uses Lomb-Scargle. | **Recommended default.** Correct choice without user intervention. |
| `fft` | Always uses FFT. Requires a gap-free, uniformly sampled series. GapFiller runs first; a warning is logged if gaps remain. | When you know the data is uniform and want the fastest computation. |
| `lomb_scargle` | Always uses Lomb-Scargle. Works directly on gappy or unevenly sampled data. Slower than FFT. Computes False Alarm Probability (FAP) for each peak. | GNSS data, sensor data with frequent dropouts, or when FAP is required. |

> **Auto-selection note**: for 6h climatological data with no gaps, `auto`
> always selects FFT. For GNSS or train sensor data with gaps, `auto` selects
> Lomb-Scargle. Check the log for the message
> `SpectralAnalyzer: auto-selected method 'fft'` to confirm.

> **FFT vs Lomb-Scargle on uniform data**: both give equivalent PSD estimates
> on gap-free uniform data. FFT is orders of magnitude faster. Use FFT for
> routine analysis and Lomb-Scargle only when FAP values are needed or gaps
> are present.

---

### 13.2 spectral.fft

Settings for the FFT path. Ignored when `method = lomb_scargle`.

| Key | Type | Default | Description |
|---|---|---|---|
| `window_function` | string | `hann` | Tapering window applied before FFT. See options below. |
| `welch` | bool | `false` | Enable Welch PSD averaging. Reduces variance at the cost of frequency resolution. |
| `welch_segments` | int | `8` | Number of overlapping segments for Welch. Must be >= 2. |
| `welch_overlap` | float | `0.5` | Segment overlap fraction in [0, 1). |

### window_function options

| Value | Side-lobe level | Best for |
|---|---|---|
| `hann` | -31 dB | **Recommended default.** Good balance of leakage suppression and amplitude accuracy. |
| `hamming` | -43 dB | Slightly better side-lobe suppression than Hann. Common in signal processing. |
| `blackman` | -58 dB | Strong leakage suppression. Use when weak peaks near strong peaks must be resolved. |
| `flattop` | -93 dB | Best amplitude accuracy for calibration. Wide main lobe -- poor frequency resolution. |
| `rectangular` | -13 dB | No tapering. Best frequency resolution but worst leakage. Use only for pure tones. |

> **Which window to choose**: for climatological data with a dominant annual
> cycle and weak sub-harmonics, `hann` is always appropriate. Use `blackman`
> if you need to detect a weak signal (e.g. 0.1% power) adjacent to a strong
> peak (e.g. the annual cycle).

> **Welch averaging**: divides the signal into `welch_segments` overlapping
> blocks and averages their periodograms. This reduces statistical variance
> (smoother PSD) but also reduces frequency resolution. For 25 years of 6h
> data with 8 segments, each segment is ~3 years long -- the frequency
> resolution is degraded 8x. Use Welch only when a smooth PSD estimate is
> more important than precise peak location. For peak detection, disable Welch.

> **Welch segment count guide**:
>
> | Segments | PSD smoothness | Frequency resolution | Use case |
> |---|---|---|---|
> | 1 (Welch off) | Low (noisy) | Highest | Peak detection, precise period identification |
> | 4 | Moderate | Good | Balanced analysis |
> | 8 | High | Reduced 8x | PSD shape characterisation |
> | 16 | Very high | Reduced 16x | Broadband noise floor estimation |

---

### 13.3 spectral.lomb_scargle

Settings for the Lomb-Scargle path. Ignored when `method = fft`.

| Key | Type | Default | Description |
|---|---|---|---|
| `oversampling` | float | `4.0` | Frequency grid oversampling factor. Must be >= 1. |
| `fast_nfft` | bool | `false` | Reserved: NFFT approximation for n >= 100k (not yet implemented). |
| `fap_threshold` | float | `0.01` | False Alarm Probability threshold for peak reporting. Peaks with FAP > threshold are excluded. |

> **Oversampling**: the frequency grid runs from `f_min = 1/T_span` to
> `f_max = 1/(2 * median_step)` (Nyquist). The number of frequency points is
> `oversampling * T_span * f_max`. Higher oversampling gives finer frequency
> resolution but increases computation time linearly. For 25 years of 6h data:
> oversampling=4 gives ~146,000 frequency points, taking ~2-5 minutes.
>
> | Oversampling | Frequency resolution | Time (25y, 6h) |
> |---|---|---|
> | 1 | Coarse | ~30 s |
> | 4 | Standard | ~2-5 min |
> | 10 | Fine | ~10-15 min |

> **FAP (False Alarm Probability)**: the probability that a peak of the
> observed height would arise by chance from white noise. Computed using the
> Baluev (2008) analytic approximation. FAP < 0.01 means the peak is
> significant at the 1% level. In the protocol, peaks with FAP < 0.001 are
> reported as `< 0.001`. For FFT results, FAP is shown as `N/A`.

> **fap_threshold**: only peaks with FAP <= fap_threshold are included in the
> output table. Set to 1.0 to include all peaks regardless of significance.
> Set to 0.001 for strict significance filtering.

---

### 13.4 spectral.spectrogram

Controls the Short-Time Fourier Transform (STFT) spectrogram. Disabled by default.
Must also set `plots.spectral_spectrogram: true` to generate the plot.

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `false` | Enable STFT computation. |
| `window_length` | int | `1461` | STFT window length in samples. Must be >= 4. |
| `overlap` | float | `0.5` | Window overlap fraction in [0, 1). Higher = more frames = smoother time axis. |
| `focus_period_min` | float | `0.0` | Lower period bound in days for the frequency zoom. `0.0` = no zoom. |
| `focus_period_max` | float | `0.0` | Upper period bound in days for the frequency zoom. `0.0` = no zoom. |

> **Window length**: determines the frequency resolution of each STFT frame.
> Longer windows give better frequency resolution but worse time resolution.
> For 6h data:
>
> | Window length | Duration | Frequency resolution | Time resolution |
> |---|---|---|---|
> | 365 | 91 days | ~4 cpd bins | High |
> | 1461 | 365 days | ~1 cpd bins | Medium |
> | 2922 | 730 days | ~0.5 cpd bins | Low |
>
> Rule of thumb: window should be at least 2x the longest period of interest.
> To resolve the annual cycle (365 days), use `window_length >= 730`.

> **Overlap**: 0.5 means successive frames start `window_length/2` samples
> apart. Higher overlap produces more frames and a smoother time axis, but
> increases computation time proportionally. For 25 years of 6h data:
>
> | Overlap | Frames (L=1461) | Computation |
> |---|---|---|
> | 0.5 | ~48 | Fast |
> | 0.75 | ~97 | Moderate |
> | 0.9 | ~243 | Slow |

> **Focus window** (`focus_period_min` / `focus_period_max`): zooms the Y
> axis of the spectrogram heatmap to a specific period range. This does not
> affect the computation -- only the display range.
> Example: `focus_period_min: 300, focus_period_max: 450` zooms to the region
> around the annual cycle (365 days). Leave both at 0.0 to show the full
> frequency range.

> **Typical spectrogram settings for climatological data**:
> ```json
> "spectrogram": {
>     "enabled": true,
>     "window_length": 1461,
>     "overlap": 0.75,
>     "focus_period_min": 30.0,
>     "focus_period_max": 500.0
> }
> ```
> This gives ~97 frames covering the annual cycle and its harmonics.

---

### 13.5 spectral.peaks

Controls peak detection and ranking in the periodogram.

| Key | Type | Default | Description |
|---|---|---|---|
| `top_n` | int | `10` | Maximum number of peaks to report. |
| `min_period_days` | float | `0.0` | Ignore peaks with period shorter than this. `0.0` = no lower limit. |
| `max_period_days` | float | `0.0` | Ignore peaks with period longer than this. `0.0` = no upper limit. |

> **Peak detection algorithm**: local maxima in the periodogram above the
> median noise floor are identified, sorted by power, and the top N are
> reported. Lomb-Scargle peaks are additionally filtered by FAP threshold.

> **max_period_days**: strongly recommended to set this for FFT results.
> Without it, the rank-1 peak is often a spurious low-frequency artefact
> caused by long-term trend or drift in the data. Setting
> `max_period_days: 3650` (10 years) eliminates artefacts longer than the
> meaningful geophysical band. For climatological data a typical setting is:
> ```json
> "peaks": {
>     "top_n": 10,
>     "min_period_days": 1.0,
>     "max_period_days": 400.0
> }
> ```
> This keeps the annual cycle (365 d), semi-annual (182 d), and shorter
> sub-seasonal peaks while discarding long-period trend artefacts.

> **min_period_days**: useful for GNSS or sensor data where sub-daily noise
> is not of interest. Set to `1.0` to exclude all sub-daily periods.

---

### 13.6 Plots (spectral pipeline)

Spectral plot flags are set directly under `"plots"` (not inside an `"enabled"` sub-object).

| Flag | Default | Description |
|---|---|---|
| `spectral_psd` | `true` | PSD / periodogram on log-log axes (X = period in days, Y = power). Dominant peaks annotated with rank numbers (red impulse lines). |
| `spectral_peaks` | `true` | Annotated peak markers on the PSD plot. Set `false` to show a clean PSD without annotations. Has no effect if `spectral_psd` is `false`. |
| `spectral_amplitude` | `false` | Amplitude spectrum `|X[k]|` on log-linear axes (X = period, Y = amplitude in signal units). Useful for interpreting peak strength in physical units. FFT only -- for Lomb-Scargle, amplitude = sqrt(normalised power). |
| `spectral_phase` | `false` | Phase spectrum `atan2(Im, Re)` in radians. Only significant-amplitude frequencies are plotted (threshold: 1% of max amplitude). FFT only -- empty for Lomb-Scargle. |
| `spectral_spectrogram` | `false` | 2-D time-frequency heatmap (STFT). Only generated when `spectral.spectrogram.enabled` is also `true`. |

> **Reading the PSD plot**: X-axis is period in days on a log scale (0.1 to
> 100,000 days). Y-axis is PSD on a log scale. Dominant physical signals appear
> as sharp vertical spikes. The red rank numbers identify the top-N peaks found
> by the peak detector. Broad bumps indicate quasi-periodic or broadband energy.

> **Reading the amplitude plot**: unlike PSD (which scales as power per
> frequency bin), the amplitude plot shows `|X[k]|` which is proportional to
> the sinusoidal amplitude at each frequency. A peak at 365 days with amplitude
> 0.023 means that the annual sinusoidal component has an amplitude of ~0.023
> in the units of the input signal. This is more physically interpretable than
> PSD units.

> **Reading the phase plot**: only frequencies where the amplitude exceeds 1%
> of the maximum amplitude are shown (others are numerical noise). A stable
> phase at a dominant period (e.g. the annual cycle) has physical meaning --
> it indicates the phase offset of that oscillation relative to the start of
> the time series. Scattered points at other frequencies indicate noise.

> **Reading the spectrogram**: X-axis is MJD (time), Y-axis is period in days.
> Colour indicates spectral power (log scale). A horizontal band at constant
> period means a stationary periodic signal present throughout the record. A
> band that appears/disappears or shifts in period over time indicates a
> non-stationary oscillation. The annual cycle (365 d) should appear as a
> persistent red/orange band if the focus window covers it.

---

### 13.7 Protocol output

Written to `<workspace>/OUTPUT/PROTOCOLS/`.
Filename: `spectral_[dataset]_[component]_protocol.txt`

The protocol contains:
- Dataset name, component, method, observation count, time span
- Median sampling step in days
- Ranked table of top N peaks: rank, period (days), frequency (cpd), power, FAP

Example:
```
==========================================================
 LOKI Spectral Analysis Protocol
==========================================================
 Dataset    : CLIM_DATA_EX1
 Component  : col_3
 Method     : fft
 N          : 36524 observations
 Span       : 25.00 years (9131.00 days)
 Median step: 0.25 days
----------------------------------------------------------
 Top 10 dominant period(s):

 Rank   Period (days)   Freq (cpd)    Power     FAP
 ----------------------------------------------------------
    1         364.09      0.002747    1.0000  N/A
    2           1.00      1.000000    0.0235  N/A
    3         184.09      0.005432    0.0217  N/A
    ...
==========================================================
```

---

### 13.8 Recommended configurations

**Quick FFT analysis -- peak identification only** (seconds):
```json
"spectral": {
    "method": "fft",
    "fft": { "window_function": "hann", "welch": false },
    "peaks": { "top_n": 10, "min_period_days": 1.0, "max_period_days": 400.0 }
}
```

**Smooth PSD shape (Welch, no peak annotation)**:
```json
"spectral": {
    "method": "fft",
    "fft": { "window_function": "hann", "welch": true, "welch_segments": 8, "welch_overlap": 0.5 }
}
```
```json
"plots": { "spectral_psd": true, "spectral_peaks": false }
```

**Lomb-Scargle with FAP (GNSS / gappy data)**:
```json
"spectral": {
    "method": "lomb_scargle",
    "lomb_scargle": { "oversampling": 4.0, "fap_threshold": 0.01 },
    "peaks": { "top_n": 10, "min_period_days": 1.0, "max_period_days": 400.0 }
}
```

**Full analysis with spectrogram (climatological data)**:
```json
"spectral": {
    "method": "auto",
    "fft": { "window_function": "hann", "welch": false },
    "spectrogram": {
        "enabled": true,
        "window_length": 1461,
        "overlap": 0.75,
        "focus_period_min": 30.0,
        "focus_period_max": 500.0
    },
    "peaks": { "top_n": 10, "min_period_days": 1.0, "max_period_days": 400.0 }
}
```
```json
"plots": {
    "spectral_psd": true,
    "spectral_peaks": true,
    "spectral_amplitude": true,
    "spectral_phase": false,
    "spectral_spectrogram": true
}
```

**Amplitude + phase for signal characterisation**:
```json
"plots": {
    "spectral_psd": true,
    "spectral_peaks": true,
    "spectral_amplitude": true,
    "spectral_phase": true,
    "spectral_spectrogram": false
}
```

---

### 13.9 Common issues and remedies

| Symptom | Cause | Fix |
|---|---|---|
| Rank 1 peak at very long period (> 3000 days) | Long-term trend / drift dominates the FFT | Set `max_period_days: 3650` in `peaks` |
| Annual cycle shows at 341 days instead of 365 | Welch reduces frequency resolution | Set `welch: false` for precise peak location |
| Lomb-Scargle very slow | Large n + high oversampling | Reduce `oversampling` to 1 or 2 for exploration |
| Spectrogram is empty / white | Focus window excludes all frequency bins | Set `focus_period_min: 0, focus_period_max: 0` to disable zoom |
| Phase plot is all scattered noise | All amplitudes below 1% threshold | Signal too weak or series too noisy for phase estimation |
| FAP column shows N/A | FFT method selected (FAP only from Lomb-Scargle) | Switch to `method: lomb_scargle` if FAP is required |
| Amplitude plot shows large values at long periods | Trend/drift contributes low-frequency energy | Apply detrending before spectral analysis, or restrict `max_period_days` |

## 14. loki_kalman

**Program:** `loki_kalman.exe`
**Default config:** `config/kalman.json`

Applies a linear Kalman filter with optional RTS backward smoother and
EM-based noise covariance estimation to a time series. Produces filtered
and smoothed state estimates, one-step-ahead innovations, Kalman gain,
filter uncertainty, and a multi-step forecast with prediction intervals.

The pipeline runs the following steps in order:

```
1. GapFiller              -- fill missing values (optional pre-processing)
2. dt estimation          -- median of consecutive time differences (seconds)
3. Noise estimation       -- manual / heuristic / EM
4. KalmanModelBuilder     -- construct F, H, Q, R, x0, P0
5. KalmanFilter           -- forward pass (predict + update)
6. RtsSmoother            -- backward pass (optional)
7. Forecast               -- repeated predict steps beyond last observation
8. CSV / Protocol         -- state estimates, innovations, uncertainty
9. Plots                  -- overlay, innovations, gain, uncertainty, forecast, diagnostics
```

The section key in the JSON file is `"kalman"`.

---

### 14.1 kalman.model

Selects the state space model. The model determines the state vector,
transition matrix F, and observation matrix H.

| Key | Type | Default | Description |
|---|---|---|---|
| `model` | string | `local_level` | State space model. See options below. |

### model options

| Value | State vector | F matrix | H matrix | Best for |
|---|---|---|---|---|
| `local_level` | `[level]` | `[1]` | `[1]` | Climatological signals (IWV, troposfera), slowly varying sensors |
| `local_trend` | `[level, trend]` | `[[1,dt],[0,1]]` | `[1,0]` | GNSS coordinates with long-term drift |
| `constant_velocity` | `[velocity, accel]` | `[[1,dt],[0,1]]` | `[1,0]` | Train velocity measured directly from encoder or Doppler radar |

> **On `dt`**: the sampling interval in seconds is estimated automatically
> from the median of consecutive time differences in the loaded series.
> It does not need to be specified in the config. For 6h data: dt = 21600 s.
> For 1 kHz sensor data: dt = 0.001 s.

> **`local_trend` vs `constant_velocity`**: both have identical F and H
> structure. The difference is physical interpretation and recommended Q/R
> values. `local_trend` models a slowly evolving level with a drift component;
> `constant_velocity` models a directly measured velocity with an acceleration
> component.

> **Choosing the model**:
> - **Climatological / IWV / troposfera**: always `local_level`. The signal
>   varies on synoptic timescales without a systematic drift.
> - **GNSS coordinates (dN, dE, dU)**: `local_trend` when a long-term drift
>   (post-glacial rebound, tectonic motion) is expected.
> - **Train velocity from encoder or Doppler**: `constant_velocity`. The
>   measured quantity is velocity directly, not position.

---

### 14.2 kalman.gap_fill_strategy / gap_fill_max_length

Gap filling is applied as a **pre-processing step** before the filter runs.
Any remaining NaN values after gap filling are handled inside the Kalman
filter as **predict-only epochs** (the update step is skipped, so the filter
covariance grows until the next valid observation).

| Key | Type | Default | Description |
|---|---|---|---|
| `gap_fill_strategy` | string | `auto` | Gap filling strategy. See options below. |
| `gap_fill_max_length` | int | `0` | Maximum consecutive gap samples to fill. `0` = no limit. Longer gaps remain as NaN and are handled by the filter. |

### gap_fill_strategy options

| Value | Description | When to use |
|---|---|---|
| `auto` | For 6h climatological data (dt < 0.3 days) with series >= 10 years: uses `median_year`. Otherwise: `linear`. | **Recommended default.** |
| `linear` | Linear interpolation between nearest valid neighbours. | Short gaps, any sampling rate. |
| `median_year` | Median annual profile lookup for each missing epoch. | 6h or finer climatological data with >= 10 years span. |
| `none` | No gap filling -- all NaN remain; filter predicts without update at those epochs. | When gaps carry physical meaning (instrument outage). |

> **Gap filling vs filter handling**: gap filling before the filter is
> optional. The Kalman filter naturally handles missing observations by
> running a predict-only step (no update). Gap filling is recommended only
> when the gaps are short and the interpolated values are physically
> meaningful. For long gaps (> 1 week for 6h data) set `gap_fill_max_length`
> to a reasonable value and let the filter propagate uncertainty across the
> remaining NaN epochs.

---

### 14.3 kalman.noise

Controls how the process noise variance Q and measurement noise variance R
are determined. These are the most important parameters for filter behaviour.

#### The Q/R ratio and Kalman gain

The steady-state Kalman gain for `local_level` is approximately:

```
K = Q / (Q + R)
```

Small Q/R (Q << R) -> small K -> filter trusts the model, heavy smoothing.
Large Q/R (Q >> R) -> large K -> filter trusts measurements, tracks closely.

| Key | Type | Default | Description |
|---|---|---|---|
| `estimation` | string | `manual` | Noise estimation method. See options below. |
| `Q` | float | `1.0` | Process noise variance (manual mode only). Must be > 0. |
| `R` | float | `1.0` | Measurement noise variance (manual mode only). Must be > 0. |
| `smoothing_factor` | float | `10.0` | Q = R / smoothing_factor (heuristic mode). Must be > 0. |
| `Q_init` | float | `1.0` | Initial Q for EM algorithm. Must be > 0. |
| `R_init` | float | `1.0` | Initial R for EM algorithm. Must be > 0. |
| `em_max_iter` | int | `100` | Maximum EM iterations. Must be >= 1. |
| `em_tol` | float | `1e-6` | EM convergence criterion: relative change in log-likelihood. Must be > 0. |

### estimation options

| Value | Description | When to use |
|---|---|---|
| `manual` | Uses Q and R directly as specified. No estimation. | When physical knowledge of noise levels is available. Fastest. |
| `heuristic` | R = var(measurements), Q = R / smoothing_factor. | Quick starting point when physical units are unknown. |
| `em` | Expectation-Maximisation: iterates forward filter + RTS smoother to find Q and R that maximise log-likelihood. | When optimal noise parameters are unknown and series is long (n > 1000). Slow for large n. |

> **Manual Q and R selection guide** (physical units squared):
>
> | Data type | Recommended Q | Recommended R | Resulting K |
> |---|---|---|---|
> | GPS IWV (m), tracking | 1e-4 | 4e-6 | ~0.96 |
> | GPS IWV (m), smoothing | 1e-4 | 4e-4 | ~0.20 |
> | GPS IWV (m), trend extraction | 1e-6 | 1e-3 | ~0.001 |
> | Train velocity (m/s) | application-specific | application-specific | -- |
>
> R corresponds to the squared measurement noise. For GPS IWV: instrument
> precision ~2 mm -> R = (0.002)^2 = 4e-6. For a smoother result increase R
> (model representativeness uncertainty) rather than decrease Q.

> **EM convergence**: for 6h climatological data (n=36524), EM with
> `em_max_iter=100` and `em_tol=1e-6` typically does not converge fully --
> the log-likelihood change at iteration 100 is still ~1e-5. This is not
> a problem in practice: after ~20-30 iterations Q and R are already stable
> to 3 significant figures. Tighten `em_tol` to `1e-4` or `1e-3` for faster
> convergence at the cost of a slightly suboptimal solution. Each EM iteration
> runs a full forward filter + RTS smoother pass: for n=36524 each iteration
> takes ~1.5 seconds.

> **EM result interpretation**: EM maximises the likelihood of the observed
> data under the specified model. For `local_level` without seasonal
> pre-processing, EM tends to find large Q (the filter tracks synoptic
> variability) and very small R (GPS IWV precision is genuinely high). This
> is physically correct but produces a near-interpolating filter (K close to 1).
> If smoothing is desired, use `manual` with a larger R that encodes
> representativeness uncertainty, not just instrument noise.

> **heuristic smoothing_factor guide**:
>
> | smoothing_factor | Q/R ratio | Effect |
> |---|---|---|
> | 1 | 1.0 | K ~ 0.5, balanced |
> | 10 | 0.1 | K ~ 0.09, moderate smoothing |
> | 100 | 0.01 | K ~ 0.01, heavy smoothing |
> | 500 | 0.002 | K ~ 0.002, trend extraction |

---

### 14.4 kalman.smoother

| Key | Type | Default | Description |
|---|---|---|---|
| `smoother` | string | `rts` | Backward smoother. See options below. |

### smoother options

| Value | Description | When to use |
|---|---|---|
| `rts` | Rauch-Tung-Striebel backward smoother. Uses all future observations to refine past state estimates. Runs a full backward pass after the forward filter. | **Recommended for all offline analysis.** Gives lower uncertainty than the forward filter. |
| `none` | No smoother. Only forward-filtered estimates are produced. | When only causal (real-time) estimates are needed, or when computation time is critical. |

> **Filter vs smoother**: the forward filter at epoch t uses observations
> up to time t only. The RTS smoother at epoch t uses all n observations.
> The smoother uncertainty is always <= filter uncertainty. For offline
> reanalysis (climatological studies, post-processing of sensor logs)
> always use `smoother: rts`. For real-time applications set `smoother: none`.

> **Computational cost**: the RTS smoother adds one backward pass of O(n)
> operations. For n=36524 this takes < 1 second. It is negligible compared
> to the EM estimation cost.

---

### 14.5 kalman.forecast

Controls multi-step prediction beyond the last observation.

| Key | Type | Default | Description |
|---|---|---|---|
| `steps` | int | `0` | Number of prediction steps beyond the last observation. `0` = no forecast. Must be >= 0. |
| `confidence_level` | float | `0.95` | Confidence level for the prediction interval. The interval is `state +/- z * std`, where `z = 1.96` for 95%. |

> **Forecast interpretation**: the forecast is generated by repeated application
> of the state transition F starting from the last filtered state. For
> `local_level`, the point forecast is constant (equal to the last filtered
> level). For `local_trend` and `constant_velocity`, the point forecast
> follows the estimated drift. The prediction uncertainty grows monotonically
> because process noise accumulates with each step:
>
> ```
> P[t+k|t] = F^k * P[t|t] * (F^k)' + sum_{j=0}^{k-1} F^j * Q * (F^j)'
> ```
>
> For `local_level`: `std[t+k] = sqrt(P[t|t] + k * Q)`.

> **Forecast horizon guide** (6h data, 1 step = 6h):
>
> | steps | Horizon | Physical meaning |
> |---|---|---|
> | 4 | 1 day | Short-term nowcast |
> | 24 | 6 days | Medium-range |
> | 96 | 24 days | Monthly outlook (uncertainty very large) |
>
> For `local_level` with GPS IWV: forecast uncertainty at step k is
> sqrt(P_ss + k * Q) where P_ss ~ 1.5e-5 m^2. At k=24 (6 days) and Q=1e-4:
> std ~ sqrt(1.5e-5 + 24 * 1e-4) ~ 0.050 m, i.e. +/-0.1 m at 95%.
> This is comparable to the total IWV variability, so forecasts beyond
> 24-48 steps are not informative for this dataset.

> **`confidence_level`**: currently uses z = 1.96 for 95% regardless of
> the configured value. The z-value will be derived from the normal quantile
> function in a future update.

---

### 14.6 kalman.significance_level

| Key | Type | Default | Description |
|---|---|---|---|
| `significance_level` | float | `0.05` | Reserved for future hypothesis tests on innovations. Not currently used in the pipeline. |

---

### 14.7 Plots (kalman pipeline)

Kalman plot flags are set directly under `"plots"` (not inside an `"enabled"` sub-object),
following the same convention as the spectral module.

| Flag | Default | Description |
|---|---|---|
| `kalman_overlay` | `true` | Original (grey dots) + filtered (blue line) + smoothed RTS (red line, if enabled) + 2-sigma confidence band (shaded blue). Full series length. |
| `kalman_innovations` | `true` | One-step-ahead innovations (y[t] - H * x_hat[t\|t-1]) vs time. Steady-state +/-2sigma band shown as dashed horizontal lines. |
| `kalman_gain` | `false` | Kalman gain K[t][0] vs time. Shows convergence from initial value to steady-state. |
| `kalman_uncertainty` | `false` | Filter std sqrt(P[t\|t]) and smoother std sqrt(P[t\|T]) vs time. |
| `kalman_forecast` | `true` | Last 1461 observed samples + forecast with 2-sigma prediction interval. Only generated when `forecast.steps > 0`. |
| `kalman_diagnostics` | `false` | 4-panel residual diagnostics of innovations: residuals vs fitted, Normal Q-Q with 95% bands, histogram with normal fit, ACF. Delegates to `loki::Plot::residualDiagnostics()`. |

> **Reading the overlay plot**: the filtered and smoothed lines should
> follow the signal trend. A large gap between filtered and smoothed
> indicates the smoother is using future information to correct the
> filter -- normal behaviour at abrupt changes. The confidence band
> width reflects filter uncertainty (proportional to sqrt(P[t|t]));
> for stationary models it is constant after a short initial transient.

> **Reading the innovations plot**: innovations should be approximately
> white noise centred on zero. Systematic structure indicates model
> misspecification:
> - **Positive drift**: signal has a trend not captured by the model.
>   Switch to `local_trend`.
> - **Oscillatory pattern**: seasonal or periodic component present.
>   Consider deseasonalising before filtering.
> - **Negative ACF at lag 1**: filter is slightly over-smoothing (K too small).
>   Increase Q or decrease R.
> - **Positive ACF at lag 1**: filter is under-smoothing (K too large, tracks
>   noise). Decrease Q or increase R.

> **Reading the gain plot**: K converges from the initial value (near 1 for
> diffuse P0) to the steady-state value within a few steps. If K stays near 1
> after convergence, Q >> R (filter trusts measurements). If K is near 0,
> Q << R (filter trusts the model / heavy smoothing).

> **Reading the uncertainty plot**: filter std is constant in steady state
> for stationary models. Smoother std is always <= filter std. A gap between
> the two indicates how much information the smoother gains from future
> observations -- wider gap = more benefit from smoothing.

---

### 14.8 Protocol output

Written to `<workspace>/OUTPUT/PROTOCOLS/`.
Filename: `kalman_[dataset]_[component]_protocol.txt`

The protocol contains:
- Dataset name, component, model, noise estimation method, smoother
- N total observations, N observed (non-NaN)
- Estimated Q and R
- Log-likelihood of the measurement sequence
- EM iterations and convergence status (if estimation = em)
- Forecast summary: steps, start/end MJD, final point estimate and std (if enabled)

---

### 14.9 Example configs

**GPS IWV tracking (EM noise estimation, RTS smoother)**:
```json
"kalman": {
    "gap_fill_strategy": "auto",
    "gap_fill_max_length": 0,
    "model": "local_level",
    "noise": {
        "estimation": "em",
        "Q_init": 1.0,
        "R_init": 1.0,
        "em_max_iter": 100,
        "em_tol": 1e-4
    },
    "smoother": "rts",
    "forecast": { "steps": 24, "confidence_level": 0.95 },
    "significance_level": 0.05
}
```

**GPS IWV tracking (manual, physically motivated)**:
```json
"kalman": {
    "gap_fill_strategy": "auto",
    "model": "local_level",
    "noise": {
        "estimation": "manual",
        "Q": 0.0001,
        "R": 0.000004
    },
    "smoother": "rts",
    "forecast": { "steps": 24 }
}
```

**GPS IWV smoothing (extract slow signal)**:
```json
"kalman": {
    "gap_fill_strategy": "auto",
    "model": "local_level",
    "noise": {
        "estimation": "manual",
        "Q": 0.000001,
        "R": 0.001
    },
    "smoother": "rts",
    "forecast": { "steps": 0 }
}
```

**Train velocity (constant_velocity model, heuristic noise)**:
```json
"kalman": {
    "gap_fill_strategy": "linear",
    "gap_fill_max_length": 10,
    "model": "constant_velocity",
    "noise": {
        "estimation": "heuristic",
        "smoothing_factor": 50.0
    },
    "smoother": "rts",
    "forecast": { "steps": 100 }
}
```

**GNSS coordinate drift (local_trend, manual)**:
```json
"kalman": {
    "gap_fill_strategy": "linear",
    "model": "local_trend",
    "noise": {
        "estimation": "manual",
        "Q": 1e-8,
        "R": 1e-6
    },
    "smoother": "rts",
    "forecast": { "steps": 365 }
}
```

---

### 14.10 Common issues and remedies

| Symptom | Cause | Fix |
|---|---|---|
| Filtered series tracks every measurement (no smoothing) | Q >> R, K close to 1 | Increase R or decrease Q; use `heuristic` with high `smoothing_factor` |
| Filtered series barely moves (over-smoothing) | Q << R, K close to 0 | Decrease R or increase Q |
| EM does not converge in 100 iterations | `em_tol` too tight for large n | Relax `em_tol` to `1e-4` or `1e-3` |
| EM result gives near-interpolating filter (K ~ 1) | Model has no seasonal component; EM assigns all variance to Q | Expected for IWV without deseasonalisation; use `manual` with larger R for smoothing |
| Innovations show lag-1 negative ACF | Filter over-smoothing relative to data autocorrelation | Increase Q slightly |
| Innovations show lag-1 positive ACF | Data has short-term persistence not captured by model | Expected for atmospheric data; consider AR-augmented model (future feature) |
| Forecast std grows too quickly | Q is large (filter is sensitive to model noise) | Decrease Q for long-horizon forecasting |
| Overlay shows only partial series | gnuplot xrange issue (fixed in current version) | Rebuild with latest `plotKalman.cpp` |
| Protocol shows `EM converged: no` | Normal for large n with tight tolerance | Check that Q and R have stabilised in the log; result is still usable |

## 15. loki_qc

**Program:** `loki_qc.exe`
**Default config:** `config/qc.json`

Diagnostic entry gate for the LOKI pipeline. Runs before any other analysis
module. Does **not** modify data. Produces a quality control protocol, a
per-epoch flagging CSV, and optional plots.

The pipeline runs the following steps in order:

```
1. Sampling rate analysis  -- median step, uniformity fraction
2. Temporal coverage       -- gaps, completeness, span
3. Descriptive statistics  -- mean, std, IQR, skewness, kurtosis, Hurst, JB test
4. Outlier detection       -- IQR, MAD, Z-score on MA-detrended residuals
5. Seasonal consistency    -- MedianYearSeries feasibility (auto-disabled sub-hourly)
6. Protocol                -- OUTPUT/PROTOCOLS/qc_[dataset]_[component]_protocol.txt
7. Flags CSV               -- OUTPUT/CSV/qc_[dataset]_[component]_flags.csv
8. Plots                   -- OUTPUT/IMG/
```

The section key in the JSON file is `"qc"`.

---

### 15.1 Top-level qc settings

| Key | Type | Default | Description |
|---|---|---|---|
| `temporal_enabled` | bool | `true` | Run Section 1: temporal coverage analysis. |
| `stats_enabled` | bool | `true` | Run Section 2: descriptive statistics. |
| `outlier_enabled` | bool | `true` | Run Section 3: outlier detection. |
| `sampling_enabled` | bool | `true` | Run Section 4: sampling rate analysis. Always runs first (required by other sections). |
| `seasonal_enabled` | bool | `true` | Run Section 5: seasonal consistency. Auto-disabled when median step > 3600 s. |
| `hurst_enabled` | bool | `true` | Include Hurst exponent in descriptive stats (R/S analysis). Can be slow for large n. |
| `ma_window_size` | int | `365` | Moving average window in samples for trend removal before outlier detection. Should cover one dominant period: daily data -> 365, 6-hourly -> 1461. |
| `significance_level` | float | `0.05` | Alpha for the Jarque-Bera normality test. |
| `uniformity_threshold` | float | `0.05` | Fraction of non-uniform time steps above which Lomb-Scargle is recommended over FFT. |
| `min_span_years` | float | `10.0` | Minimum series span in years for MedianYearSeries gap filling to be recommended. |

---

### 15.2 qc.outlier

Per-method outlier detection settings. Detectors operate on MA-detrended
residuals (original values minus a centered moving average of width
`ma_window_size`). Edge epochs within the MA half-window are excluded.

| Key | Type | Default | Description |
|---|---|---|---|
| `iqr_enabled` | bool | `true` | Enable IQR-based outlier detection. |
| `iqr_multiplier` | float | `1.5` | IQR fence multiplier. Standard: 1.5 (mild outliers). |
| `mad_enabled` | bool | `true` | Enable MAD-based outlier detection. |
| `mad_multiplier` | float | `3.0` | Normalised MAD multiplier (~sigma units under normality). |
| `zscore_enabled` | bool | `true` | Enable Z-score outlier detection. |
| `zscore_threshold` | float | `3.0` | Z-score threshold. Flags values with `|z| > threshold`. |

> **On deseasonalization for QC**: unlike `loki_outlier` which supports
> `median_year` deseasonalization, QC always uses a simple moving average
> (`ma_window_size` samples) to estimate and remove the trend before running
> detectors. This keeps QC self-contained with no dependency on series length
> or calendar structure. Set `ma_window_size` to cover one dominant period.

> **Interpreting multi-method results**: the three detectors have different
> robustness characteristics. IQR is most sensitive to moderate deviations;
> MAD is robust to clustered outliers; Z-score assumes approximate normality.
> The protocol reports each method separately and the union count. The flags
> CSV uses separate bits per method (see Section 15.5).

---

### 15.3 qc.seasonal

Controls the seasonal consistency check (Section 5). Evaluates whether
the series is suitable for `MEDIAN_YEAR` gap filling in downstream modules.

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `true` | Enable the seasonal consistency check. Automatically overridden to `false` when median step > 3600 s (sub-hourly data). |
| `min_years_per_slot` | int | `5` | Minimum valid years per MedianYearSeries slot for the profile to be considered reliable. |
| `min_month_coverage` | float | `0.5` | Minimum fraction of expected epochs per calendar month below which a month is considered poorly covered. Years with more than 6 poorly-covered months are flagged. |

> **Auto-disable for sub-hourly data**: if the median sampling step is
> shorter than 3600 s (e.g. millisecond sensor data), the seasonal section
> is automatically disabled regardless of the `enabled` flag. This is logged
> at INFO level.

> **MedianYearSeries feasibility**: the protocol and recommendations block
> report whether `MEDIAN_YEAR` gap filling is feasible for this series based
> on span >= `min_span_years` and per-month coverage >= `min_month_coverage`.
> This recommendation is informational -- no gap filling is performed by QC.

---

### 15.4 Recommendations block

The protocol ends with an explicit recommendation block covering four topics:

| Topic | Logic |
|---|---|
| **Gap filling** | `MEDIAN_YEAR` if series is long and coverage is good; `LINEAR` otherwise; `FORWARD_FILL` for very short series. |
| **Spectral method** | `LOMB_SCARGLE` if `uniformityFraction > uniformity_threshold`; `FFT` otherwise. |
| **Outlier cleaning** | `RECOMMENDED` if any outliers detected, with count and percentage; `NOT REQUIRED` if none. |
| **Homogeneity testing** | `CONSIDER` if span >= 3 years and completeness >= 70%; `SKIP` otherwise. |
| **MedianYearSeries feasibility** | `FEASIBLE` or `NOT FEASIBLE` with reason. |

---

### 15.5 Flagging CSV

Written to `<workspace>/OUTPUT/CSV/`.
Filename: `qc_[dataset]_[component]_flags.csv`

```
mjd ; utc ; value ; flag
```

| Column | Description |
|---|---|
| `mjd` | Modified Julian Date of the epoch |
| `utc` | UTC string ("YYYY-MM-DD hh:mm:ss.sss") |
| `value` | Original series value; `NaN` for missing epochs |
| `flag` | Bitfield integer. See flag table below. |

### Flag bit values

| Bit | Value | Meaning |
|---|---|---|
| — | `0` | Valid -- no issues detected |
| 0 | `1` | Gap -- epoch has a NaN value |
| 1 | `2` | Outlier detected by IQR method |
| 2 | `4` | Outlier detected by MAD method |
| 3 | `8` | Outlier detected by Z-score method |

Flags are OR-combined. For example, an epoch flagged by both IQR and MAD
receives flag = `2 | 4 = 6`. An epoch that is simultaneously a gap and an
outlier receives flag = `1 | 2 = 3` (gap takes priority in coverage plots).

---

### 15.6 Plots (qc pipeline)

QC plot flags are set directly under `"plots"` (same convention as kalman
and spectral modules, not inside an `"enabled"` sub-object).

| Flag | Default | Description |
|---|---|---|
| `qc_coverage` | `true` | Time-axis bar chart coloured by worst flag per day (or per epoch for short series). Green = valid, orange = outlier, red = gap. |
| `qc_histogram` | `true` | Value histogram with fitted normal curve overlay. |
| `qc_acf` | `false` | ACF of valid observations up to `plot_options.acf_max_lag` lags. |

> **Coverage plot aggregation**: for series with >= 1000 epochs, one bar per
> calendar day is shown (colour = worst flag in that day). For series with
> < 1000 epochs, one bar per epoch is shown. Colour priority:
> **red (gap) > orange (outlier) > green (valid)**.

> **Histogram and ACF**: both delegate to `loki::Plot` (core module).
> The histogram uses `plot_options.histogram_bins` bins (default 30).
> The ACF uses `plot_options.acf_max_lag` lags (default 40).

---

### 15.7 Protocol output

Written to `<workspace>/OUTPUT/PROTOCOLS/`.
Filename: `qc_[dataset]_[component]_protocol.txt`

Sections:
- **Section 1: Temporal coverage** -- start/end in MJD, UTC, GPS total seconds; span; expected vs actual epochs; completeness %; gap count, longest gap, median gap.
- **Section 4: Sampling rate** -- median, min, max step in seconds; non-uniform step count.
- **Section 2: Descriptive statistics** -- N valid, N NaN, mean, median, std, IQR, skewness, kurtosis, min, max, P05/P25/P75/P95, Hurst exponent, JB test result.
- **Section 3: Outlier detection** -- per-method count and %, union count.
- **Section 5: Seasonal consistency** -- MedianYearSeries feasibility, years with poor coverage.
- **Recommendations** -- gap filling, spectral method, outlier cleaning, homogeneity, MedianYearSeries.

---

### 15.8 Example config

```json
"qc": {
    "temporal_enabled":     true,
    "stats_enabled":        true,
    "outlier_enabled":      true,
    "sampling_enabled":     true,
    "seasonal_enabled":     true,
    "hurst_enabled":        true,
    "ma_window_size":       1461,
    "significance_level":   0.05,
    "uniformity_threshold": 0.05,
    "min_span_years":       10.0,

    "outlier": {
        "iqr_enabled":      true,
        "iqr_multiplier":   1.5,
        "mad_enabled":      true,
        "mad_multiplier":   3.0,
        "zscore_enabled":   true,
        "zscore_threshold": 3.0
    },

    "seasonal": {
        "enabled":            true,
        "min_years_per_slot": 5,
        "min_month_coverage": 0.5
    }
}
```

```json
"plots": {
    "output_format": "png",
    "qc_coverage":   true,
    "qc_histogram":  true,
    "qc_acf":        false
}
```

#### Recommended starting configurations

**Full diagnostic for long climatological series (6h, >= 10 years)**:
```json
"qc": {
    "ma_window_size": 1461,
    "hurst_enabled":  true,
    "min_span_years": 10.0,
    "outlier": { "iqr_multiplier": 1.5, "mad_multiplier": 3.0 },
    "seasonal": { "enabled": true, "min_years_per_slot": 5 }
}
```

**Fast diagnostic for short or high-rate sensor data**:
```json
"qc": {
    "ma_window_size":  51,
    "hurst_enabled":   false,
    "seasonal_enabled": false,
    "outlier": { "iqr_enabled": false, "mad_multiplier": 4.0 }
}
```

**GNSS coordinate series (daily, 5-10 years)**:
```json
"qc": {
    "ma_window_size":       365,
    "min_span_years":       5.0,
    "uniformity_threshold": 0.02,
    "outlier": { "mad_multiplier": 3.5, "zscore_threshold": 3.5 },
    "seasonal": { "min_years_per_slot": 3, "min_month_coverage": 0.3 }
}
```

## 16. loki_clustering

**Program:** `loki_clustering.exe`
**Default config:** `config/clustering.json`

Groups time series observations into clusters based on configurable feature
vectors. Two algorithms are available: k-means (partition-based) and DBSCAN
(density-based). The module is general-purpose -- applicable to climatological,
GNSS, and sensor data. Primary use cases include phase segmentation of vehicle
velocity profiles and density-based outlier detection.

The pipeline runs the following steps in order:

```
1. GapFiller              -- fill NaN / missing values (optional)
2. Feature extraction     -- build feature matrix from configured features
3. Z-score normalisation  -- each feature column normalised independently
4. k-means or DBSCAN      -- clustering in normalised feature space
5. Edge point assignment  -- NaN derivative rows assigned to nearest centroid
6. Label naming           -- user-defined or auto-generated cluster names
7. Protocol               -- OUTPUT/PROTOCOLS/clustering_[dataset]_[component]_protocol.txt
8. Labels CSV             -- OUTPUT/CSV/clustering_[dataset]_[component]_labels.csv
9. Plots                  -- labels time axis, feature scatter, silhouette
```

The section key in the JSON file is `"clustering"`.

---

### 16.1 Top-level clustering settings

| Key | Type | Default | Description |
|---|---|---|---|
| `method` | string | `kmeans` | Clustering algorithm. `"kmeans"` or `"dbscan"`. |
| `gap_fill_strategy` | string | `linear` | Gap fill strategy before feature extraction. `"linear"`, `"forward_fill"`, `"mean"`, `"none"`. |
| `gap_fill_max_length` | int | `0` | Maximum consecutive gap samples to fill. `0` = no limit. |
| `significance_level` | float | `0.05` | Reserved for future hypothesis tests. |

---

### 16.2 clustering.features

Controls which features are extracted from the series to form the observation
vector for each epoch. All enabled features are z-score normalised before
clustering so that their scales do not bias the distance metric.

| Key | Type | Default | Description |
|---|---|---|---|
| `value` | bool | `true` | Use the raw series value `v[t]` as a feature. |
| `derivative` | bool | `false` | Use the signed first difference `v[t] - v[t-1]`. Positive = increasing, negative = decreasing. |
| `abs_derivative` | bool | `false` | Use `|v[t] - v[t-1]|`. Magnitude of change only, direction discarded. |
| `second_derivative` | bool | `false` | Use `v[t] - 2*v[t-1] + v[t-2]`. Rate of change of the rate of change. |
| `slope` | bool | `false` | Use the OLS slope over a sliding window of `slope_window` samples. Positive = sustained increase, negative = sustained decrease. |
| `slope_window` | int | `15` | Window length in samples for the OLS slope estimator. Must be >= 2. |

#### Feature selection guide

At least one feature must be enabled. If all are disabled, `value` is
automatically re-enabled with a warning.

The first two samples (or more, depending on which features are active) cannot
have derivative or slope features computed. These **edge points** are assigned
to the nearest centroid using only the available (non-NaN) features. This is
handled automatically.

| Goal | Recommended features | Notes |
|---|---|---|
| Separate by value range only | `value: true` | Simplest. Good for climatological data where distinct regimes differ in mean value. |
| Separate value + rate of change (unsigned) | `value: true`, `abs_derivative: true` | Good for short sensor records. Does not distinguish acceleration from braking. |
| Separate value + direction of change | `value: true`, `derivative: true` | Distinguishes zrýchlenie from brzdenie. Noise-sensitive for small windows. |
| Phase segmentation of velocity profiles | `value: true`, `slope: true` | **Recommended for vehicle/train data.** `slope` captures sustained trends over a window, not just instantaneous change. Robust to measurement noise. |
| Curvature-sensitive segmentation | `value: true`, `slope: true`, `second_derivative: true` | Use when transition sharpness matters (e.g. detecting braking onset). |

> **On `derivative` vs `slope`**: `derivative = v[t] - v[t-1]` is an
> instantaneous quantity -- highly sensitive to measurement noise at 1 Hz
> or faster. `slope` fits a linear trend over `slope_window` samples using
> OLS, which effectively averages out noise. For vehicle sensor data,
> `slope` with `slope_window = 10..20` is strongly preferred over `derivative`.

> **On `abs_derivative` vs `derivative`**: `abs_derivative` discards the
> direction of change. k-means cannot distinguish zrýchľovanie from brzdenie
> when using `abs_derivative` alone. Use signed `derivative` or `slope` when
> directional separation is required.

> **Note on `slope_window`**: the window is in samples, not time units. At
> 1 Hz, `slope_window = 15` covers 15 seconds. At 10 Hz, the same window
> covers 1.5 seconds. Adjust to match the expected duration of the
> acceleration/braking phases in your data.

#### Feature combination examples

**Climatological data -- separate dry/wet regimes:**
```json
"features": { "value": true }
```

**Train velocity -- 3 phases (stationary / moving / changing speed):**
```json
"features": {
    "value":        true,
    "abs_derivative": true
}
```

**Train velocity -- 4 phases with direction (stationary / accelerating / cruising / decelerating):**
```json
"features": {
    "value":        true,
    "slope":        true,
    "slope_window": 15
}
```

**GNSS velocity -- direction-sensitive with curvature:**
```json
"features": {
    "value":             true,
    "slope":             true,
    "second_derivative": true,
    "slope_window":      20
}
```

---

### 16.3 clustering.kmeans

Controls the k-means clustering algorithm. Features are assumed to be
z-score normalised. The implementation uses k-means++ initialisation
(default) with multiple random restarts to avoid local minima.

| Key | Type | Default | Valid range | Description |
|---|---|---|---|---|
| `k` | int | `0` | >= 0 | Number of clusters. `0` = auto-select via silhouette score in `[k_min, k_max]`. |
| `k_min` | int | `2` | >= 2 | Lower bound for auto k selection. |
| `k_max` | int | `10` | >= k_min | Upper bound for auto k selection. |
| `max_iter` | int | `300` | >= 1 | Maximum iterations per run. |
| `n_init` | int | `10` | >= 1 | Number of independent random restarts. Best result (lowest inertia) is kept. |
| `tol` | float | `1e-4` | > 0 | Convergence criterion: maximum centroid shift per iteration. |
| `init` | string | `"kmeans++"` | — | Initialisation method. `"kmeans++"` or `"random"`. |
| `labels` | string[] | `[]` | — | User-defined cluster names. See label assignment below. |

#### Auto k selection (`k = 0`)

When `k = 0`, the algorithm evaluates all integer values of k in
`[k_min, k_max]` and selects the k with the highest global silhouette
coefficient. The selected k is reported in the protocol and log.

Silhouette coefficient ranges from -1 to 1:
- Near 1: points are well-matched to their cluster and far from neighbours.
- Near 0: points are near cluster boundaries.
- Negative: points may be misassigned.

> **Auto k is slower**: each candidate k requires `n_init` full k-means runs.
> For `k_max = 10` and `n_init = 10`, up to 90 independent runs are executed.
> For n=1276 this takes a few seconds. For large n (> 50000) consider setting
> k manually or reducing `n_init` to 3.

> **Auto k may not match physical expectation**: silhouette maximisation finds
> the statistically best-separated clustering, which may not correspond to the
> physically meaningful number of phases. If you know the expected number of
> phases (e.g. 4 for stationary/accelerating/cruising/decelerating), set k
> explicitly.

#### Label assignment

If `labels` is non-empty and its length equals k, labels are assigned to
clusters in order of **ascending first-feature centroid value**. The cluster
with the lowest mean value receives the first label, the highest receives the
last.

Example: with `k = 4` and `labels: ["stationary", "decelerating", "cruising", "accelerating"]`
and `value` as the first feature:
- Lowest centroid value → `"stationary"`
- Second lowest → `"decelerating"`
- Second highest → `"cruising"`
- Highest → `"accelerating"`

> **Important**: the label order is based on the first feature (value), not
> on the slope or derivative centroid. If the physical label assignment does
> not match the value ordering in your data, adjust the label order in the
> config or inspect the protocol centroid table to determine the correct mapping.

> **Mismatch handling**: if `labels.length != k`, auto labels
> (`cluster_0`, `cluster_1`, ...) are used and a warning is logged.

#### k-means configuration guide

| Scenario | Recommended settings |
|---|---|
| Known number of phases | `k = 4` (or appropriate number), `n_init = 10` |
| Unknown number of phases | `k = 0`, `k_min = 2`, `k_max = 8`, `n_init = 5` |
| Large dataset (n > 10000) | `k` manual, `n_init = 3`, `max_iter = 100` |
| Noisy sensor data | Increase `n_init` to 20 for more stable results |

---

### 16.4 clustering.dbscan

Controls the DBSCAN density-based clustering algorithm. DBSCAN does not
require the number of clusters to be specified. Points in low-density
regions are classified as noise (label = -1).

| Key | Type | Default | Valid range | Description |
|---|---|---|---|---|
| `eps` | float | `0.0` | >= 0 | Neighbourhood radius in z-score normalised feature space. `0.0` = auto-estimate via k-NN elbow method. |
| `min_pts` | int | `5` | >= 2 | Minimum number of points within `eps` for a point to be a core point. |
| `metric` | string | `"euclidean"` | — | Distance metric. `"euclidean"` or `"manhattan"`. |

#### Auto eps estimation (`eps = 0.0`)

When `eps = 0.0`, the algorithm estimates `eps` automatically using the
k-NN distance elbow method:

1. For each point, compute the distance to its `min_pts`-th nearest neighbour.
2. Sort these distances in ascending order.
3. Find the point of maximum curvature (elbow) via the second discrete derivative.
4. Use the distance at the elbow as `eps`.

The estimated `eps` is logged at INFO level.

#### DBSCAN parameter guide

| Parameter | Effect | Guidance |
|---|---|---|
| `eps` too small | Almost all points classified as noise | Increase `eps` or use auto estimation |
| `eps` too large | All points merged into one cluster | Decrease `eps` |
| `min_pts` too small | Noise-sensitive, many small clusters | Increase to 5--10 for 1 Hz sensor data |
| `min_pts` too large | Core points rare, many noise points | Decrease for sparse data |

> **DBSCAN vs k-means for outlier detection**: DBSCAN naturally identifies
> isolated points as noise without requiring a predefined number of clusters.
> This makes it well-suited for detecting sensor dropouts, measurement spikes,
> and physically impossible values in vehicle sensor data. Enable
> `outlier.enabled: true` to propagate noise points to the flags CSV.

> **DBSCAN for phase segmentation**: DBSCAN works well when phases have
> clearly different densities in feature space. For vehicle velocity data
> where phases blend smoothly (gradual acceleration), k-means with `slope`
> typically gives more interpretable results.

> **Note on `min_pts` and sampling rate**: at 1 Hz, `min_pts = 5` means
> a phase must last at least 5 seconds to be recognised as a cluster. At
> 10 Hz, the same setting requires only 0.5 seconds. Adjust to match the
> minimum expected phase duration.

---

### 16.5 clustering.outlier

Controls whether DBSCAN noise points are propagated as outlier flags in the
output CSV. Has no effect when `method = "kmeans"`.

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `false` | If `true`, DBSCAN noise points (label = -1) are flagged as outliers in the labels CSV (`outlier_flag = 1`). |

> **When to enable**: use `outlier.enabled: true` when running DBSCAN
> specifically for outlier detection rather than phase segmentation. The
> flags CSV can then be used by downstream modules to mask or remove
> the flagged epochs.

---

### 16.6 Labels CSV output

Written to `<workspace>/OUTPUT/CSV/`.
Filename: `clustering_[dataset]_[component]_labels.csv`

```
mjd ; utc ; value ; label ; label_name ; outlier_flag
```

| Column | Description |
|---|---|
| `mjd` | Modified Julian Date of the epoch |
| `utc` | UTC string ("YYYY-MM-DD hh:mm:ss.sss") |
| `value` | Series value at this epoch; `NaN` for missing epochs |
| `label` | Integer cluster label. `-1` = DBSCAN noise or NaN epoch |
| `label_name` | Human-readable cluster name (e.g. `"stationary"`, `"cruising"`) |
| `outlier_flag` | `1` if DBSCAN noise and `outlier.enabled = true`; `0` otherwise |

NaN epochs (not clustered) receive `label = -1`, `label_name = "invalid"`,
`outlier_flag = 0`.

---

### 16.7 Protocol output

Written to `<workspace>/OUTPUT/PROTOCOLS/`.
Filename: `clustering_[dataset]_[component]_protocol.txt`

The protocol contains:
- Dataset name, component, method, active features.
- k-means: selected k (auto or manual), total inertia, global silhouette.
- DBSCAN: cluster count, noise point count, effective eps.
- Per-cluster summary: name, epoch count, inertia, centroid in original units.
- Outlier summary (if `outlier.enabled = true`).

---

### 16.8 Plots (clustering pipeline)

Clustering plot flags are set directly under `"plots"` (same convention as
kalman and spectral modules).

| Flag | Default | Description |
|---|---|---|
| `clustering_labels` | `true` | Time axis with coloured dots per cluster label. X-axis uses relative time in seconds for series shorter than 1 day; MJD otherwise. DBSCAN noise points shown as red circles. |
| `clustering_scatter` | `true` | 2-D scatter of feature[0] vs feature[1], coloured by label. Only generated when >= 2 features are active. DBSCAN noise as red crosses. |
| `clustering_silhouette` | `true` | Per-cluster silhouette bar chart with global mean dashed line. Only generated for k-means. |

> **Reading the labels plot**: each dot is one epoch coloured by its cluster
> assignment. Transitions between colours indicate phase boundaries. For
> vehicle data, `stationary` (lowest value) should appear as a flat region
> near zero; `accelerating` / `decelerating` as transitional segments.

> **Reading the scatter plot**: each axis corresponds to one feature
> (z-score normalised). Well-separated clusters appear as non-overlapping
> point clouds. Overlapping clusters indicate that the chosen features are
> insufficient to distinguish those phases -- consider adding `slope` or
> `derivative`. DBSCAN noise points (red crosses) should appear as isolated
> points far from the main clouds.

> **Reading the silhouette plot**: each bar shows the mean silhouette
> coefficient for one cluster. Bars extending rightward (positive) indicate
> well-separated clusters. Bars near zero or negative indicate ambiguous
> assignment. The dashed vertical line is the global mean. A global silhouette
> above 0.5 is generally considered good separation.

---

### 16.9 Example configs

**Auto k-means -- climatological data (value only):**
```json
"clustering": {
    "method": "kmeans",
    "features": { "value": true },
    "kmeans": { "k": 0, "k_min": 2, "k_max": 6 }
}
```

**k-means with 4 phases -- train velocity (value + slope):**
```json
"clustering": {
    "method": "kmeans",
    "features": {
        "value":        true,
        "slope":        true,
        "slope_window": 15
    },
    "kmeans": {
        "k":      4,
        "n_init": 10,
        "labels": ["stationary", "decelerating", "cruising", "accelerating"]
    }
}
```

**DBSCAN outlier detection -- sensor data:**
```json
"clustering": {
    "method": "dbscan",
    "features": { "value": true, "abs_derivative": true },
    "dbscan": {
        "eps":     0.0,
        "min_pts": 5,
        "metric":  "euclidean"
    },
    "outlier": { "enabled": true }
}
```

**3-phase segmentation -- simpler vehicle data:**
```json
"clustering": {
    "method": "kmeans",
    "features": {
        "value":          true,
        "abs_derivative": true
    },
    "kmeans": {
        "k":      3,
        "labels": ["stationary", "changing_speed", "cruising"]
    }
}
```

---

### 16.10 Common issues and remedies

| Symptom | Cause | Fix |
|---|---|---|
| All points in one cluster | `eps` too large (DBSCAN) or k=1 equivalent | Decrease `eps` or use auto estimation |
| Most points classified as noise | `eps` too small or `min_pts` too high | Increase `eps` or decrease `min_pts` |
| Scatter plot not generated | Fewer than 2 features active | Enable a second feature (e.g. `slope`) |
| Silhouette plot not generated | `method = "dbscan"` | Silhouette only available for k-means |
| Labels assigned in wrong order | k-means orders by first-feature centroid | Reorder `labels` array to match ascending value order |
| `accelerating` and `decelerating` mixed | `abs_derivative` discards direction | Switch to `derivative` or `slope` (signed) |
| Slow cruising misclassified as accelerating | `slope_window` too small, noisy slope estimate | Increase `slope_window` to 20--30 |
| Empty cluster warnings during fit | Data has fewer distinct modes than k | Reduce k or use `k = 0` auto-select |
| Segfault / crash with derivative features | Edge points had NaN in feature matrix | Fixed in current version -- update `kMeans.cpp` |

---

### 16.11 Best practices

**Feature selection:**
- Start with `value` only to understand the data distribution.
- Add `slope` (not `derivative`) when directional phase separation is needed.
- Avoid combining `derivative` and `abs_derivative` -- they are linearly
  dependent and add no new information.
- For noisy sensor data, always prefer `slope` over `derivative`.

**Number of clusters:**
- Use `k = 0` (auto) for exploratory analysis; inspect the silhouette plot
  and protocol to understand the natural cluster structure.
- Use a fixed k when you have prior physical knowledge of the phase count.
- Silhouette > 0.5: good separation. Silhouette < 0.3: consider reducing k
  or changing features.

**k-means stability:**
- Increase `n_init` to 20 when results vary between runs (local minima).
- Use `init: "kmeans++"` (default) -- it is significantly more stable than
  `"random"` for most datasets.
- For very large series (n > 100000), reduce `n_init` to 3 and set k manually.

**DBSCAN parameter selection:**
- Always try `eps = 0.0` first to let the elbow method estimate it.
- If the auto estimate gives too many noise points, manually set `eps` to
  a value slightly above the auto estimate.
- Set `min_pts` to roughly the expected minimum phase duration in samples
  divided by 2.

**slope_window selection:**
- Rule of thumb: set `slope_window` to cover half the typical duration of
  the shortest phase of interest.
- At 1 Hz: phases lasting >= 10 s → `slope_window = 5..10`.
- At 1 Hz: phases lasting >= 30 s → `slope_window = 15..20`.
- Larger windows give smoother slope estimates but delay phase transition
  detection.

  ## 17. loki_simulate

**Program:** `loki_simulate.exe`
**Default config:** `config/simulate.json`

Generates synthetic time series realizations and performs parametric bootstrap
analysis. Two modes are available: synthetic (no input data required) and
bootstrap (fits a model on real data and generates replicas).

The pipeline runs the following steps in order:

```
Synthetic mode:
1. ArimaSimulator / KalmanSimulator  -- generate nSim independent realizations
2. (optional) Anomaly injection      -- outliers, gaps, mean shifts
3. Envelope computation              -- 5/25/50/75/95 percentiles per time step
4. CSV export                        -- envelope + simulation matrix (if nSim <= 50)
5. Protocol                          -- summary statistics
6. Plots                             -- overlay, envelope

Bootstrap mode:
1. GapFiller                         -- fill missing values
2. ArimaAnalyzer / KalmanSimulator   -- fit model on real data
3. Simulator                         -- generate nSim replicas from fitted model
4. (optional) Anomaly injection      -- outliers, gaps, mean shifts per replica
5. Bootstrap CI                      -- block/percentile/BCa CI for key parameters
6. Envelope computation
7. CSV export
8. Protocol
9. Plots                             -- overlay, envelope, bootstrap dist, ACF comparison
```

The section key in the JSON file is `"simulate"`.

---

### 17.1 simulate.mode / model

| Key | Type | Default | Description |
|---|---|---|---|
| `mode` | string | `synthetic` | Pipeline mode. `"synthetic"` or `"bootstrap"`. |
| `model` | string | `arima` | Generative model. `"arima"`, `"ar"` (alias for arima with q=0), or `"kalman"`. |

| Mode | Input required | What it does |
|---|---|---|
| `synthetic` | No | Generates nSim realizations from ARIMA or Kalman parameters. No real data needed. |
| `bootstrap` | Yes | Fits the model on the loaded series, generates B replicas, computes bootstrap CIs. |

> **`"ar"` vs `"arima"`**: `"ar"` is an alias for `"arima"` with `q=0`. The
> two are interchangeable; `"ar"` is provided for clarity when the MA component
> is intentionally absent.

---

### 17.2 simulate.n / seed / n_simulations

| Key | Type | Default | Description |
|---|---|---|---|
| `n` | int | `1000` | Length of each generated series in samples. Must be >= 1. Used only in synthetic mode; in bootstrap mode the length is taken from the input series. |
| `seed` | uint64 | `42` | RNG seed for reproducibility. `0` = non-deterministic (random_device). Each simulation uses `seed + simIndex` internally. |
| `n_simulations` | int | `100` | Number of independent realizations to generate (B). Must be >= 1. |

> **Reproducibility**: setting the same `seed` and `n_simulations` always
> produces identical output. Changing only `n_simulations` does not affect
> the first B simulations -- new simulations are appended deterministically.

> **n_simulations trade-off**: more simulations give a smoother envelope and
> tighter bootstrap CIs, but increase computation time linearly.
> Recommended values:
>
> | Purpose | n_simulations |
> |---|---|
> | Quick exploration | 20--50 |
> | Standard analysis | 100--200 |
> | Publication-quality bootstrap CIs | 500--1000 |

---

### 17.3 simulate.arima

Controls ARIMA(p,d,q) parametric generation. Used when `model = "arima"` or `"ar"`.

In **synthetic mode**: AR and MA coefficients are set automatically to
`phi_i = 0.5 / p` (AR) and `theta_j = 0.3 / q` (MA) -- stable, generic
autocorrelated process.

In **bootstrap mode**: coefficients are taken from the `ArimaAnalyzer` fit
result on the input series. The `arima` sub-section in the JSON is used only
as an initial hint for `ArimaConfig` (p, q, autoOrder). The actual simulated
process uses the fitted coefficients.

| Key | Type | Default | Description |
|---|---|---|---|
| `p` | int | `1` | AR order. |
| `d` | int | `0` | Differencing order. `0` = stationary process. `1` = integrated (random walk character). |
| `q` | int | `0` | MA order. |
| `sigma` | float | `1.0` | Innovation standard deviation. Must be > 0. |

> **ARIMA(p,d,q) quick reference**:
>
> | Model | d | Character | Envelope shape |
> |---|---|---|---|
> | AR(1) | 0 | Short memory, stationary | Constant-width band around 0 |
> | AR(2) | 0 | Short memory, can oscillate | Constant-width band, slight oscillatory texture |
> | ARIMA(1,1,0) | 1 | Random walk with AR(1) | Diverging lievik, grows as sqrt(t) |
> | ARIMA(2,0,1) | 0 | Mixed AR+MA, stationary | Similar to AR(2), slightly smoother |
>
> For **stationary** simulation use `d=0`. For **non-stationary** (integrated
> process) simulation use `d=1`. Note that `d=1` causes the envelope to
> diverge over time -- this is correct behaviour, not a bug.

> **sigma and model variance**: the total variance of a stationary AR(p)
> process is `sigma^2 / (1 - sum(phi_i^2))`. With the default equal
> coefficients, a synthetic AR(2) with `sigma=1.0` produces series with
> std approximately 1.05--1.30 depending on p. This is expected and
> visible in the protocol as `simStdMean > sigma`.

---

### 17.4 simulate.kalman

Controls Kalman state-space generation. Used when `model = "kalman"`.

In **bootstrap mode**: the simulator uses the Q and R values from this
sub-section directly (manual noise mode). EM-estimated Q/R from the actual
`loki_kalman` module is not performed inside `loki_simulate` -- use
`loki_kalman` first to determine appropriate Q and R, then configure them
here.

| Key | Type | Default | Description |
|---|---|---|---|
| `model` | string | `local_level` | State space model. `"local_level"`, `"local_trend"`, or `"constant_velocity"`. |
| `Q` | float | `0.001` | Process noise variance. Must be > 0. |
| `R` | float | `0.01` | Observation noise variance. Must be > 0. |

> **Critical: Q/R ratio determines simulation behaviour.** An incorrect Q/R
> ratio is the most common cause of diverging simulations in bootstrap mode.
>
> | Q/R ratio | Kalman gain K | Simulation character |
> |---|---|---|
> | >> 1 (Q >> R) | K near 1 | Filter tracks measurements; state is nearly a random walk. Simulations diverge. |
> | ~1 | K ~ 0.5 | Balanced tracking and smoothing. |
> | << 1 (Q << R) | K near 0 | Filter trusts the model; state evolves slowly. Simulations are smooth and bounded. |
>
> **For sensor data (e.g. WIG_SPEED)**: the raw velocity series is highly
> autocorrelated and does NOT behave like a local_level random walk. A naive
> Kalman bootstrap with large Q will produce diverging simulations (lievik
> shape). Either:
> (a) Use `model = "arima"` in bootstrap mode, which captures the true
>     autocorrelation structure; or
> (b) Use `model = "kalman"` with `Q` set to a very small value relative to
>     the actual series variance (e.g. Q = 1e-6, R = 0.01 for unit-variance
>     data).

> **Diagnostic: if the envelope diverges in bootstrap mode**, the ACF
> comparison plot will confirm: simulated ACF will show near-unit persistence
> while the original ACF decays quickly. This indicates the model has too much
> process noise relative to the autocorrelation structure of the data. Reduce
> Q by 2--3 orders of magnitude.

---

### 17.5 simulate.gap_fill_strategy / gap_fill_max_length

Used only in **bootstrap mode** to fill gaps in the input series before
model fitting. The simulator itself always generates gap-free series.

| Key | Type | Default | Description |
|---|---|---|---|
| `gap_fill_strategy` | string | `linear` | Gap fill strategy. `"linear"`, `"forward_fill"`, `"median_year"`, `"spline"`, `"none"`. |
| `gap_fill_max_length` | int | `0` | Maximum consecutive gap samples to fill. `0` = no limit. |

---

### 17.6 simulate.bootstrap_method / confidence_level

Used only in **bootstrap mode**.

| Key | Type | Default | Description |
|---|---|---|---|
| `bootstrap_method` | string | `block` | Bootstrap CI method for parameter estimation. See options below. |
| `confidence_level` | float | `0.95` | Confidence level for all bootstrap CIs. Must be in (0, 1). |
| `significance_level` | float | `0.05` | Reserved for future diagnostics. |

### bootstrap_method options

| Value | Description | When to use |
|---|---|---|
| `block` | Moving block bootstrap. Resamples contiguous blocks to preserve autocorrelation structure. Block length auto-selected from ACF. | **Recommended default** for all autocorrelated series (climatological, GNSS, most sensor data). |
| `percentile` | IID resampling with replacement. Ignores autocorrelation structure. | Only for series confirmed to be near-iid (ACF lag-1 < 0.1). Slightly faster. |
| `bca` | Bias-corrected accelerated bootstrap. Corrects for bias and acceleration. Falls back to percentile if jackknife is degenerate (nearly-constant statistic). | Small samples (n < 200) or when bias correction is important. |

> **block vs percentile for autocorrelated data**: the standard iid bootstrap
> (percentile) is statistically invalid for autocorrelated time series -- it
> breaks the dependence structure and produces CIs that are too narrow.
> Always use `block` for climatological, GNSS, or sensor data with significant
> autocorrelation (ACF lag-1 > 0.2). The block length is selected automatically
> from the ACF and logged at INFO level.

> **bca degenerate fallback**: for statistics with very small variance across
> bootstrap samples (e.g. the mean of a long, stable series), the BCa
> jackknife distribution is nearly constant. In this case the implementation
> falls back to percentile CI with a LOKI_WARNING. This is correct behaviour.

---

### 17.7 simulate.inject_outliers / inject_gaps / inject_shifts

Optional anomaly injection applied **per replica** after generation.
Each anomaly type is independent and uses a deterministic sub-seed
derived from the base `seed` for reproducibility.

#### inject_outliers

Randomly selects a fraction of samples and adds a signed spike of specified
magnitude.

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `false` | Enable outlier injection. |
| `fraction` | float | `0.01` | Fraction of samples to perturb. Range [0, 1]. `0.01` = 1% of samples. |
| `magnitude` | float | `5.0` | Spike magnitude in units of the series. Applied with a random sign per outlier. |

#### inject_gaps

Randomly places zero-filled segments of random length within each replica.
Intended to simulate instrument outages.

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `false` | Enable gap injection. |
| `n_gaps` | int | `5` | Number of gap regions per replica. |
| `max_length` | int | `10` | Maximum gap length in samples. Actual length is uniform in [1, max_length]. |

#### inject_shifts

Randomly selects positions and adds a permanent step change from that
position to the end of the series (cumulative shift).

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `false` | Enable mean shift injection. |
| `n_shifts` | int | `2` | Number of step changes per replica. |
| `magnitude` | float | `1.0` | Shift magnitude. Applied with random sign. |

> **Use case: homogeneity validation**. Generate synthetic series with
> controlled shifts using `inject_shifts`, then export individual simulations
> from the CSV and run them through `loki_homogeneity` to measure detection
> power as a function of shift magnitude and segment length.
>
> Recommended setup:
> ```json
> "n_simulations": 50,
> "n": 3000,
> "inject_shifts": { "enabled": true, "n_shifts": 2, "magnitude": 2.0 }
> ```
> The `nSim <= 50` threshold causes the full simulation matrix CSV to be
> written, giving you 50 individual series to process.

> **Injection order**: outliers are injected first, then gaps (zero-fill),
> then shifts. Gaps do not overwrite the shift baseline -- they set affected
> samples to zero regardless of the shift.

---

### 17.8 Plots (simulate pipeline)

Simulate plot flags are set directly under `"plots"`.

| Flag | Default | Description |
|---|---|---|
| `simulate_overlay` | `true` | First 20 replicas overlaid as thin light-blue lines + median (dark blue) + original series if in bootstrap mode (red). |
| `simulate_envelope` | `true` | Filled percentile bands: 5--95% (light blue) and 25--75% (medium blue) + median line + original if available. |
| `simulate_bootstrap_dist` | `true` | Histogram of per-simulation means with CI bounds (blue = lower/upper, red = estimate). Bootstrap mode only. |
| `simulate_acf_comparison` | `true` | ACF impulse plot of the original series (red) vs mean ACF of simulations (blue dots). Bootstrap mode only. |

> **`simulate_bootstrap_dist` and `simulate_acf_comparison`** are silently
> skipped in synthetic mode even if set to `true`.

> **Reading the ACF comparison plot**: the most important diagnostic in
> bootstrap mode. If the simulated mean ACF (blue) closely tracks the original
> ACF (red impulses), the model captures the autocorrelation structure of the
> data well. Deviations indicate model misspecification:
>
> | Observation | Interpretation |
> |---|---|
> | Sim ACF decays much slower than original | Model has too much memory. Reduce p (ARIMA) or reduce Q (Kalman). |
> | Sim ACF decays much faster than original | Model has too little memory. Increase p or increase Q. |
> | Sim ACF near zero at all lags | Model generating near-white-noise. Check that autoOrder is working. |
> | Both ACFs high at all lags shown | Both series have long memory; may need `d=1` or a different model. |

---

### 17.9 CSV output

**Envelope CSV** (always written):
Filename: `simulate_[dataset]_[component]_envelope.csv`

```
t ; p05 ; p25 ; p50 ; p75 ; p95 [; original]
```

The `original` column is appended in bootstrap mode only.

**Simulation matrix CSV** (written only when `n_simulations <= 50`):
Filename: `simulate_[dataset]_[component]_simulations.csv`

```
t ; sim_0 ; sim_1 ; ... ; sim_N
```

One column per simulation. Use this for feeding individual realizations
into other LOKI modules (e.g. `loki_homogeneity` for validation).

> **Large nSim**: for `n_simulations > 50` the matrix CSV is suppressed to
> avoid files of hundreds of megabytes. Only the envelope summary is written.
> If you need the full matrix, set `n_simulations <= 50` and increase the
> threshold manually in `simulateAnalyzer.cpp` if needed.

---

### 17.10 Protocol output

Written to `<workspace>/OUTPUT/PROTOCOLS/`.
Filename: `simulate_[dataset]_[component]_protocol.txt`

Sections:
- **Header**: dataset, component, mode, model, n, nSim.
- **Simulation summary**: mean/std of per-simulation means and std devs.
- **Bootstrap CIs** (bootstrap mode only): parameter table with estimate, lower, upper, bias, SE.
- **Envelope sample**: first 5 and last 5 time steps with all five percentiles.

---

### 17.11 Method selection guide

```
What is your goal?

Test whether ARIMA or Kalman can reproduce the autocorrelation structure of my data?
  --> mode: bootstrap, model: arima (start here; ARIMA is more flexible for short memory)
  --> Check: ACF comparison plot. If sim ACF tracks original, model is valid.

Generate realizations with known properties for module validation?
  --> mode: synthetic
  --> arima with d=0 for stationary, d=1 for random walk
  --> inject_shifts for change point detection validation
  --> inject_outliers for outlier detector sensitivity testing

Get bootstrap CIs for the mean and variance of my series?
  --> mode: bootstrap, bootstrap_method: block (for autocorrelated data)
  --> Check: bootstrap_dist plot, CI table in protocol

Understand how much a Kalman model varies across realizations?
  --> mode: synthetic, model: kalman
  --> Use Q/R from a prior loki_kalman run on your real data
  --> Check: envelope plot (should not diverge for local_level with small Q)
```

---

### 17.12 Common issues and remedies

| Symptom | Cause | Fix |
|---|---|---|
| Envelope diverges (lievik shape) with Kalman bootstrap | Q/R ratio too large; model behaves like random walk | Reduce Q by 2--3 orders of magnitude, or switch to `model: arima` |
| ACF of simulations much higher than original | Model has too much memory | Reduce AR order p or reduce Q (Kalman) |
| Envelope constant width but original has visible trend | ARIMA with `d=0` on a non-stationary series | Set `d=1` or detrend the series before bootstrap |
| `bootstrap_dist` plot CI lines outside histogram | BCa fallback to percentile; CI estimated from original not replicas | Expected for nearly constant statistics; check LOKI_WARNING in log |
| Simulation matrix CSV not written | `n_simulations > 50` | Reduce to <= 50 to enable full matrix output |
| Protocol shows `simStdStd` very large | High between-simulation variance; model is non-stationary | Check d, Q/R; inspect overlay plot for divergence |
| `simulate_acf_comparison` plot not generated | Running in synthetic mode | Expected; plot only available in bootstrap mode |
| Injected shifts not visible in envelope | Shifts are random in position and sign; they average out | Inspect overlay plot or individual simulations from CSV |

---

### 17.13 Example configs

**Synthetic ARIMA(2,0,1) -- stationary exploration:**
```json
"simulate": {
    "mode": "synthetic",
    "model": "arima",
    "n": 2000,
    "seed": 42,
    "n_simulations": 100,
    "arima": { "p": 2, "d": 0, "q": 1, "sigma": 1.0 }
}
```

**Synthetic ARIMA(1,1,0) -- random walk / integrated process:**
```json
"simulate": {
    "mode": "synthetic",
    "model": "arima",
    "n": 1000,
    "seed": 7,
    "n_simulations": 50,
    "arima": { "p": 1, "d": 1, "q": 0, "sigma": 0.5 }
}
```

**Synthetic Kalman local_level -- smooth signal:**
```json
"simulate": {
    "mode": "synthetic",
    "model": "kalman",
    "n": 3000,
    "seed": 42,
    "n_simulations": 100,
    "kalman": { "model": "local_level", "Q": 0.0001, "R": 0.01 }
}
```

**Synthetic with shift injection -- homogeneity validation:**
```json
"simulate": {
    "mode": "synthetic",
    "model": "arima",
    "n": 3000,
    "n_simulations": 50,
    "arima": { "p": 1, "d": 0, "q": 0, "sigma": 1.0 },
    "inject_shifts": { "enabled": true, "n_shifts": 2, "magnitude": 2.0 }
}
```

**Bootstrap ARIMA on real data -- block CI:**
```json
"simulate": {
    "mode": "bootstrap",
    "model": "arima",
    "n_simulations": 200,
    "seed": 42,
    "gap_fill_strategy": "linear",
    "bootstrap_method": "block",
    "confidence_level": 0.95,
    "arima": { "p": 2, "d": 0, "q": 1, "sigma": 1.0 }
}
```

**Bootstrap Kalman -- smooth physical signal:**
```json
"simulate": {
    "mode": "bootstrap",
    "model": "kalman",
    "n_simulations": 100,
    "seed": 99,
    "bootstrap_method": "percentile",
    "kalman": { "model": "local_level", "Q": 0.000001, "R": 0.01 }
}
```

```json
"plots": {
    "output_format": "png",
    "simulate_overlay": true,
    "simulate_envelope": true,
    "simulate_bootstrap_dist": true,
    "simulate_acf_comparison": true
}
```

## 18. loki_evt

**Program:** `loki_evt.exe`
**Default config:** `config/evt.json`

Performs Extreme Value Theory (EVT) analysis on a time series to estimate
return levels and exceedance probabilities for rare events. Two methods are
available: Peaks-Over-Threshold / GPD (primary) and Block Maxima / GEV
(secondary). Designed for SIL safety analysis, climatological extremes,
and sensor anomaly characterisation.

The pipeline runs the following steps in order:

```
1. GapFiller                  -- fill NaN / missing values
2. Deseasonalizer             -- subtract seasonal component (optional)
3. ThresholdSelector          -- auto or manual threshold selection (POT only)
4. Gpd::fit / Gev::fit        -- MLE via Nelder-Mead (PWM fallback for n < 50)
5. Return level computation   -- point estimates with CI (profile likelihood /
                                 bootstrap / delta method)
6. Goodness-of-fit tests      -- Anderson-Darling, Kolmogorov-Smirnov
7. CSV export                 -- return_period ; estimate ; lower_ci ; upper_ci
8. Protocol                   -- OUTPUT/PROTOCOLS/evt_[dataset]_[component]_protocol.txt
9. Plots                      -- mean excess, stability, return levels, exceedances, GPD Q-Q
```

The section key in the JSON file is `"evt"`.

---

### 18.1 evt.method / time_unit / return_periods

| Key | Type | Default | Description |
|---|---|---|---|
| `method` | string | `pot` | Analysis method. `"pot"`, `"block_maxima"`, or `"both"`. |
| `time_unit` | string | `hours` | Time unit for lambda (exceedance rate) and return periods. See options below. |
| `return_periods` | float[] | `[10, 100, 1000, 1e6, 1e8]` | Return periods to estimate, expressed in `time_unit` units. Must be non-empty. |
| `confidence_level` | float | `0.95` | Confidence level for CI on return levels. Must be in (0, 1). |
| `significance_level` | float | `0.05` | Alpha for GoF test rejection decisions. |
| `gap_fill_strategy` | string | `linear` | Gap fill strategy before analysis. `"linear"`, `"median_year"`, `"spline"`, `"none"`. |
| `gap_fill_max_length` | int | `0` | Maximum consecutive gap samples to fill. `0` = no limit. |

### method options

| Value | Description | When to use |
|---|---|---|
| `pot` | Peaks-Over-Threshold: fit GPD to exceedances above a threshold. Uses all exceedances -- more data-efficient than block maxima. | **Recommended for all data types.** Especially for sensor data where annual blocks are physically meaningless. |
| `block_maxima` | Block Maxima: divide series into blocks, extract per-block maxima, fit GEV. | Climatological data only, where annual blocks (e.g. 1461 samples at 6h) correspond to natural extreme periods. |
| `both` | Run both methods. Produces two independent result sets and two protocol entries. | Comparing methods on climatological data. |

> **Method selection guidance**:
> - For **train/vehicle sensor data**: always use `"pot"`. Block maxima waste
>   data (only one maximum per block) and blocks have no physical meaning for
>   short measurement campaigns.
> - For **climatological IWV / troposfera**: `"pot"` is preferred. `"block_maxima"`
>   may also be used for annual extreme analysis when the series spans at least
>   5-10 years (at least 5 annual maxima are needed for reliable GEV fitting).
> - For **GNSS coordinates**: use `"pot"` on residuals after deseasonalization
>   and detrending.

### time_unit options

| Value | Description |
|---|---|
| `seconds` | Exceedance rate and return periods in seconds. |
| `minutes` | Exceedance rate and return periods in minutes. |
| `hours` | Exceedance rate and return periods in hours. **Recommended for SIL 4.** |
| `days` | Exceedance rate and return periods in days. |
| `years` | Exceedance rate and return periods in years. Convenient for climatological return levels. |

> **`time_unit` and input sampling rate**: the input series can be at any
> sampling rate regardless of `time_unit`. The median time step `dt` is
> estimated automatically from the series in hours, then converted to the
> configured `time_unit`. Lambda (exceedances per time_unit) is then computed
> as `n_exceedances / (n_obs * dt_in_time_unit)`.
>
> For **SIL 4** applications: use `time_unit = "hours"` with
> `return_periods = [1e8]`. A return period of 1e8 hours corresponds to a
> failure probability of 1e-8 per hour, the SIL 4 threshold.
> Input data at 1 Hz (dt = 1/3600 h) is handled correctly.

> **Return period vs exceedance probability**: T years (or T hours etc.) return
> period corresponds to an annual (or hourly) exceedance probability of 1/T.
> The return level z_T satisfies P(X > z_T) = 1/T per time_unit.

---

### 18.2 evt.deseasonalization

Subtracts a seasonal component before EVT analysis. When enabled, the return
levels are expressed in units of **residuals** (deviations from the seasonal
mean), not in original series units. This is noted explicitly in the protocol.

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `false` | Enable deseasonalization before EVT. |
| `strategy` | string | `median_year` | Deseasonalization method. `"none"`, `"moving_average"`, `"median_year"`. |
| `ma_window_size` | int | `1461` | MA window in samples for `moving_average`. For 6-hourly data: 1461 = 1 year. |
| `median_year_min_years` | int | `5` | Minimum years per slot for `median_year`. Slots with fewer years return NaN. |

### strategy options

| Value | Best for |
|---|---|
| `median_year` | Climatological and GNSS data with a clear annual cycle. Requires >= 5 years and step >= 1 hour. |
| `moving_average` | Sensor or signal data without a strict annual period. No minimum span required. |
| `none` | Pre-computed residuals, or series without a periodic component. |

> **When to enable deseasonalization**:
> - **Climatological IWV**: enable with `median_year`. Without it, exceedances
>   include seasonally elevated values (summer IWV peaks) which are not true
>   extreme events -- just the seasonal maximum.
> - **Train velocity**: consider `moving_average` if a systematic speed profile
>   (acceleration / cruising / deceleration) is present. However, if the goal
>   is to characterise absolute velocity extremes, leave disabled.
> - **Already-detrended residuals**: set `strategy: "none"` or `enabled: false`.

> **Return level interpretation with deseasonalization enabled**:
> A return level of 0.5 m in the IWV residuals means an anomaly 0.5 m
> above the seasonal median occurs once per T time_units. To obtain the
> absolute IWV level, add the seasonal median for the relevant time of year.
> The protocol contains a note reminding the user of this interpretation.

---

### 18.3 evt.threshold

Controls threshold selection for the POT method. Ignored when `method = "block_maxima"`.

| Key | Type | Default | Description |
|---|---|---|---|
| `auto` | bool | `true` | If `true`, use mean excess elbow detection to select the threshold automatically. |
| `method` | string | `mean_excess` | Auto-selection method. Only `"mean_excess"` is currently implemented. |
| `value` | float | `0.0` | Manual threshold value. Used only when `auto = false`. |
| `min_exceedances` | int | `30` | Minimum required exceedances above the threshold. Must be >= 5. Enforced in both auto and manual modes. |
| `n_candidates` | int | `50` | Number of candidate thresholds evaluated in auto mode. Range spans from the median to the quantile leaving `min_exceedances` above it. |

> **Threshold selection guidance**: the threshold u should be chosen in the
> region where the GPD approximation is valid -- above u, the conditional
> distribution of excesses should be approximately GPD. The diagnostics are:
>
> - **Mean excess plot**: E[X - u | X > u] should be approximately linear
>   above the chosen threshold (linear mean excess = GPD with constant xi).
>   The auto elbow detector finds the start of the linear region.
> - **Stability plot**: xi and sigma should be approximately constant
>   (stable) above the chosen threshold.
>
> **Common mistake**: choosing a threshold that is too high, leaving fewer
> than 30 exceedances. With n < 50, the implementation falls back from MLE
> to Probability-Weighted Moments (PWM), which is less accurate. This is
> reported in the protocol as `Converged: no (PWM fallback)`.

> **Manual threshold workflow**: run once with `auto = true` to generate
> the mean excess and stability plots. Visually identify the region where
> the mean excess plot is linear and the stability plot is flat. Set
> `auto = false` and `value` to a threshold in that region, then re-run.

> **Auto threshold limitation**: the elbow detector uses the maximum second
> difference of the mean excess curve. For short series (n < 500) or
> series with irregular structure, the auto-selected threshold may not be
> physically optimal. Always visually inspect the mean excess plot.

---

### 18.4 evt.ci

Controls confidence interval computation for return level estimates.

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `true` | Compute confidence intervals for return levels. |
| `method` | string | `profile_likelihood` | CI method. See options below. |
| `n_bootstrap` | int | `1000` | Number of bootstrap replicas. Used only when `method = "bootstrap"`. |
| `max_exceedances_bootstrap` | int | `10000` | Maximum exceedances used in bootstrap resampling. For large n, a random subsample of this size is drawn before resampling. |

### CI method options

| Value | Description | When to use |
|---|---|---|
| `profile_likelihood` | Profile log-likelihood CI. For each return level, the profile logLik is maximised numerically. CI bounds found via Brent root finding where logLik drops by chi2(0.95)/2 = 1.92. | **Mandatory for large T (SIL 4).** Asymmetric, correct for heavy tails. |
| `bootstrap` | Parametric bootstrap: resample exceedances, refit GPD, percentile CI. Subsampled to `max_exceedances_bootstrap` if n is large. | Large n with fast computation required, or moderate T (< 1e6). |
| `delta` | Delta method: linearisation via numerical gradient. Symmetric CI. | Quick exploration only. Unreliable for T > 1e4 and xi != 0. Do not use for SIL 4. |

> **Why `profile_likelihood` is required for SIL 4**: the delta method
> assumes a symmetric, approximately normal distribution of the return level
> estimator. For T = 1e8 and xi > 0 (heavy-tailed distribution), the
> estimator is highly skewed -- the upper CI bound can be orders of magnitude
> larger than the lower bound. Profile likelihood correctly captures this
> asymmetry. The delta method may underestimate the upper bound by 10x or more
> for SIL 4 return periods.

> **Bootstrap CI with large n**: for series with tens of thousands of
> exceedances (e.g. low threshold on long climatological data), the bootstrap
> resamples from a random subsample of `max_exceedances_bootstrap` observations.
> This preserves CI quality while keeping computation time bounded. The
> subsampling is logged at INFO level.

> **Profile likelihood convergence**: for each return period T, the profile
> CI requires a 1D optimisation plus Brent root finding. If the search
> interval is too narrow (return level far from the point estimate), the CI
> bounds may fall back to the point estimate with a LOKI_WARNING. In this
> case, the CI bounds in the protocol will equal the estimate. This is most
> likely for extreme T values (1e8) with xi near 0 (exponential tail) where
> the curve is very flat.

---

### 18.5 evt.block_maxima

Controls block extraction for the Block Maxima / GEV method.
Ignored when `method = "pot"`.

| Key | Type | Default | Description |
|---|---|---|---|
| `block_size` | int | `1461` | Number of samples per block. Must be >= 2. |

> **Block size selection**: the block should correspond to a natural extreme
> period. For 6-hourly climatological data: `block_size = 1461` (1 year =
> 4 * 365.25 samples). For daily data: `block_size = 365`. For monthly data:
> `block_size = 12`.
>
> A minimum of 3 blocks is required (DataException if fewer). For reliable
> GEV fitting, at least 10-20 annual maxima (10-20 years of data) are
> recommended.

> **GEV fit fallback**: unlike GPD (which has a PWM fallback for n < 50),
> GEV always uses MLE via Nelder-Mead. If fewer than 3 block maxima are
> available, a DataException is thrown. With 3-10 maxima the fit may be
> unreliable -- check the GoF p-values.

> **Block size and `time_unit`**: for block maxima, the return period T is
> expressed in blocks of length `block_size`. If `time_unit = "years"` and
> `block_size = 1461` (6h data), then T = 100 means 100 blocks = 100 annual
> maxima = 100 years. Ensure `time_unit` and `block_size` are consistent.

---

### 18.6 Goodness of fit

Two GoF tests are run automatically for every fit. Results are reported in
the protocol.

| Test | Statistic | H0 | Interpretation |
|---|---|---|---|
| Anderson-Darling | A^2 | Data follows fitted GPD/GEV | A^2 large / p < alpha: fit rejected |
| Kolmogorov-Smirnov | D | Data follows fitted GPD/GEV | D large / p < alpha: fit rejected |

> **Expected behaviour**: for GPD with xi near 0 and n >= 100, AD p > 0.05
> indicates an acceptable fit. For heavy-tailed fits (xi > 0.5) or very
> small n (< 50, PWM path), GoF p-values are often low even for correctly
> generated data -- do not interpret this as evidence of a bad model.

> **GoF with deseasonalized residuals**: after subtraction of the median-year
> profile, residuals have approximately zero mean. The GPD is fit to
> exceedances above the threshold -- positive residuals exceeding u. If the
> deseasonalized residuals have a nearly symmetric distribution, approximately
> half the data contributes to exceedances and the fit is well-conditioned.

---

### 18.7 GPD shape parameter interpretation

| xi value | Distribution family | Physical interpretation |
|---|---|---|
| xi > 0 | Frechet (heavy tail) | Unbounded exceedances; return levels grow as T^xi. Typical for atmospheric extremes. |
| xi = 0 | Gumbel limit (exponential tail) | Exceedances decay exponentially. Typical for deseasonalized residuals near zero. |
| xi < 0 | Weibull (bounded upper tail) | Physical maximum exists: max value = threshold + sigma / (-xi). Typical for velocities with a hard upper bound. |
| xi < -0.5 | Outside MLE constraint | Not physically meaningful for GPD. The constraint xi > -0.5 is enforced during fitting (barrier penalty). |

> **xi > 0.5 warning**: large positive xi produces very heavy tails and
> extremely wide CI for large T. This is often a symptom of a threshold that
> is too high (too few exceedances, PWM fallback) or a genuinely heavy-tailed
> process. Check the stability plot -- xi should be stable across a range of
> thresholds if the GPD approximation is valid.

---

### 18.8 Plots (EVT pipeline)

EVT plot flags are set directly under `"plots"` (same convention as kalman,
spectral, and clustering modules).

| Flag | Default | Description |
|---|---|---|
| `evt_mean_excess` | `true` | Mean excess E[X-u \| X>u] vs threshold u. Red dashed line = selected threshold. Linear region indicates valid GPD approximation. POT only. |
| `evt_stability` | `true` | Two-panel plot: xi (shape) and sigma (scale) stability vs threshold u. Parameters should be approximately constant in the valid GPD region. POT only. |
| `evt_return_levels` | `true` | Return level estimate (solid blue) + CI bounds (dashed) on log T axis. CI bounds reflect the chosen CI method. |
| `evt_exceedances` | `true` | Empirical CDF of exceedances (blue dots) vs fitted GPD CDF (red curve). Good fit = dots close to curve. POT only. |
| `evt_gpd_fit` | `true` | GPD Q-Q plot: theoretical GPD quantiles vs empirical quantiles. Points on the dashed reference line indicate a good fit. POT only. |

> **Reading the mean excess plot**: the x-axis is the threshold u; the y-axis
> is the mean excess (average of values above u, minus u). If the GPD
> approximation is valid, the mean excess should be linear in u above the
> true threshold. The selected threshold (red dashed line) should be near the
> start of the linear region, not in the noisy far-right tail.

> **Reading the stability plot**: above the valid threshold, xi should be
> approximately constant (flat) as u increases. A jump in xi (as observed
> when too few points remain above u) indicates that the auto threshold was
> placed too high. A continuously changing xi indicates the GPD approximation
> may not hold -- consider a higher threshold or a different model.

> **Reading the return level plot**: for xi > 0, the curve grows as a power
> law T^xi; for xi = 0, logarithmically; for xi < 0, it approaches a
> finite asymptote. The CI width grows with T -- for SIL 4 (T = 1e8) the
> CI may span several orders of magnitude, which is correct and reflects the
> uncertainty inherent in extreme extrapolation.

> **Reading the GPD Q-Q plot**: the x-axis is the theoretical quantile
> from the fitted GPD; the y-axis is the empirical quantile (sorted
> exceedances). Points close to the dashed y=x line indicate a good fit.
> Systematic curvature above the line indicates the empirical tail is heavier
> than GPD (xi may be underestimated). Points clustering near (0,0) indicate
> many small exceedances -- the threshold may be too low.

---

### 18.9 CSV output

Written to `<workspace>/OUTPUT/CSV/`.
Filename: `evt_[dataset]_[component]_return_levels.csv`

```
return_period ; estimate ; lower_ci ; upper_ci
```

| Column | Description |
|---|---|
| `return_period` | Return period in `time_unit` units |
| `estimate` | Point estimate of the return level |
| `lower_ci` | Lower CI bound at `confidence_level` |
| `upper_ci` | Upper CI bound at `confidence_level` |

> When `ci.enabled = false`, `lower_ci` and `upper_ci` equal `estimate`.

---

### 18.10 Protocol output

Written to `<workspace>/OUTPUT/PROTOCOLS/`.
Filename: `evt_[dataset]_[component]_protocol.txt`

Sections:
- **Header**: dataset, component, method.
- **Deseasonalization note** (if enabled): strategy and reminder that return
  levels are in residual units.
- **GPD Fit** (POT) or **GEV Fit** (block maxima): threshold, n exceedances
  or n blocks, lambda, xi, sigma (+ mu for GEV), log-likelihood, convergence.
- **Goodness of Fit**: AD and KS statistics and p-values.
- **Return Levels**: table of return period, estimate, lower CI, upper CI.

---

### 18.11 Recommended configurations

**Climatological IWV -- standard analysis (6h data, median-year deseasonalization):**
```json
"evt": {
    "method": "pot",
    "time_unit": "hours",
    "return_periods": [10, 100, 1000, 10000, 100000],
    "gap_fill_strategy": "linear",
    "deseasonalization": {
        "enabled": true,
        "strategy": "median_year",
        "ma_window_size": 1461,
        "median_year_min_years": 5
    },
    "threshold": { "auto": true, "min_exceedances": 50, "n_candidates": 60 },
    "ci": { "enabled": true, "method": "profile_likelihood" }
}
```

**Train velocity -- SIL 4 analysis (1 Hz, absolute values, POT):**
```json
"evt": {
    "method": "pot",
    "time_unit": "hours",
    "return_periods": [1000, 1000000, 100000000],
    "gap_fill_strategy": "linear",
    "deseasonalization": { "enabled": false },
    "threshold": {
        "auto": false,
        "value": 25.0,
        "min_exceedances": 30
    },
    "ci": {
        "enabled": true,
        "method": "profile_likelihood"
    }
}
```

**Climatological -- POT vs Block Maxima comparison:**
```json
"evt": {
    "method": "both",
    "time_unit": "years",
    "return_periods": [10, 100, 1000],
    "deseasonalization": {
        "enabled": true,
        "strategy": "median_year",
        "ma_window_size": 1461
    },
    "threshold": { "auto": true, "min_exceedances": 40 },
    "ci": { "method": "bootstrap", "n_bootstrap": 500 },
    "block_maxima": { "block_size": 1461 }
}
```

**Exploratory -- fast bootstrap CI:**
```json
"evt": {
    "method": "pot",
    "time_unit": "hours",
    "return_periods": [100, 1000, 10000],
    "threshold": { "auto": true, "min_exceedances": 30 },
    "ci": {
        "method": "bootstrap",
        "n_bootstrap": 200,
        "max_exceedances_bootstrap": 5000
    }
}
```

---

### 18.12 Common issues and remedies

| Symptom | Cause | Fix |
|---|---|---|
| `Converged: no (PWM fallback)` | Fewer than 50 exceedances above threshold | Lower threshold or reduce `min_exceedances` |
| xi > 2, return levels astronomically large | Threshold too high, too few exceedances, unstable MLE | Lower threshold; inspect stability plot |
| AD p = 0 (GoF failure) | GPD fit poor; threshold in unstable region | Move threshold to flatter region of stability plot |
| Profile likelihood CI equals estimate | Brent search failed; logLik curve too flat | Expected for xi near 0 and large T; use bootstrap CI instead |
| `all analysis methods failed` | DataException from ThresholdSelector | Set threshold manually or increase series length |
| Return level plot upper CI diverges | xi > 0, large T extrapolation | Correct behaviour; reduce return periods or use bootstrap CI |
| Stability plot: xi jumps above u = X | Too few exceedances at high thresholds | Normal; ignore the unstable region above the elbow |
| CLIM data: exceedances are seasonal peaks | Deseasonalization not enabled | Enable `deseasonalization` with `strategy: median_year` |

---

### 18.13 Method selection guide

```
Data type?
  Climatological (IWV, temperature, pressure) -- long series (>= 5 years, 6h):
    --> method: pot, time_unit: hours or years
    --> deseasonalization: enabled, strategy: median_year
    --> return_periods: physical extremes (years to centuries)
    --> ci: profile_likelihood

  Train / vehicle sensor (velocity, acceleration) -- short campaign (minutes to hours):
    --> method: pot, time_unit: hours
    --> deseasonalization: disabled (or moving_average if systematic profile present)
    --> return_periods: SIL-relevant (1e6 to 1e8 hours)
    --> ci: profile_likelihood (required for SIL 4)
    --> NOTE: n_obs typically too small for reliable SIL 4 extrapolation;
        interpret CI bounds as very conservative

  GNSS coordinates / velocities -- medium series (1-5 years):
    --> method: pot, time_unit: hours or days
    --> deseasonalization: enabled if annual signal present
    --> return_periods: mission-relevant extremes

Do I know a good threshold?
  YES --> threshold: { "auto": false, "value": X }
  NO  --> threshold: { "auto": true }  (then inspect mean excess plot)

Which CI method?
  SIL 4 (T >= 1e8) or xi != 0  --> profile_likelihood (mandatory)
  Fast exploration               --> bootstrap with n_bootstrap = 200
  Quick symmetric estimate       --> delta (unreliable for T > 1e4)
```

# 19. loki_kriging

Kriging is a geostatistical interpolation and prediction method that provides
optimal linear unbiased estimates together with a quantified uncertainty
(Kriging variance). In LOKI, Kriging operates in temporal mode: observations
are a single time series and the lag is the absolute time difference between
two observations in MJD days.

Spatial and space-time modes are reserved for future releases (the configuration
keys are parsed and stored, but any attempt to use them raises an exception).

---

## 19.1 Minimal working configuration

```json
{
    "workspace": "C:/data/project",

    "input": {
        "mode": "single_file",
        "file": "GNSS_IWV.txt",
        "time_format": "mjd",
        "time_columns": [0],
        "delimiter": ";",
        "comment_char": "%",
        "columns": [1]
    },

    "kriging": {
        "mode": "temporal",
        "method": "ordinary",
        "variogram": { "model": "spherical" }
    },

    "plots": {
        "output_format": "png"
    }
}
```

All unspecified keys fall back to documented defaults.

---

## 19.2 Top-level kriging keys

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `mode` | string | `"temporal"` | Kriging mode. Only `"temporal"` is implemented. `"spatial"` and `"space_time"` are placeholders. |
| `method` | string | `"ordinary"` | Kriging estimator. `"simple"`, `"ordinary"`, `"universal"`. |
| `gap_fill_strategy` | string | `"linear"` | Pre-processing gap fill before variogram fitting. `"linear"`, `"spline"`, `"none"`. |
| `gap_fill_max_length` | int | `0` | Maximum gap length to fill in samples. `0` = unlimited. |
| `known_mean` | double | `0.0` | Known global mean mu. Used only when `method = "simple"`. |
| `trend_degree` | int | `1` | Polynomial degree of the drift function. Used only when `method = "universal"`. Valid range: 1–5. |
| `cross_validate` | bool | `true` | Run leave-one-out cross-validation after fitting. |
| `confidence_level` | double | `0.95` | Confidence level for prediction intervals. Must be in (0, 1). |
| `significance_level` | double | `0.05` | Significance level for diagnostics. |

---

## 19.3 Kriging methods

### simple
Known constant mean. The estimator is:

    Z*(t) = mu + lambda^T * (z - mu)

where lambda solves K * lambda = k(t).

Use when the global mean is reliably known — for example, deseasonalized
residuals centred at zero. Numerically most stable.

```json
"method": "simple",
"known_mean": 0.0
```

### ordinary (default)
Unknown local mean with the constraint that weights sum to one. The extended
system adds a Lagrange multiplier row and column. No assumption about the mean
is required. This is the standard default for most applications.

```json
"method": "ordinary"
```

### universal
Mean is modelled as a polynomial in time:

    mu(t) = beta_0 + beta_1 * t + ... + beta_p * t^p

where t = MJD - t_ref for numerical stability. Appropriate when a deterministic
trend is present — for example, a linear velocity trend in GNSS coordinates or
a systematic drift in a sensor calibration.

```json
"method": "universal",
"trend_degree": 1
```

Use `trend_degree = 1` for linear trend, `2` for quadratic. Values above 2 are
rarely justified and increase the risk of overfitting.

---

## 19.4 variogram sub-section

The variogram describes how spatial (temporal) correlation decays with lag.
It is the most sensitive step in the Kriging pipeline — a poorly fitted
variogram leads to unreliable predictions and misleading CI bands.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `model` | string | `"spherical"` | Theoretical model. See Section 19.4.1. |
| `n_lag_bins` | int | `20` | Number of equal-width lag bins for the empirical variogram. |
| `max_lag` | double | `0.0` | Maximum lag in MJD days. `0` = auto (half the total time span). |
| `nugget` | double | `0.0` | Initial nugget for WLS fitting. `0` = estimate from data. |
| `sill` | double | `0.0` | Initial sill for WLS fitting. `0` = estimate from data. |
| `range` | double | `0.0` | Initial range for WLS fitting. `0` = estimate from data. |

### 19.4.1 Variogram models

**spherical**

    gamma(h) = nugget + (sill - nugget) * [1.5*(h/range) - 0.5*(h/range)^3]  for h <= range
             = nugget + (sill - nugget)                                         for h >  range

Reaches the sill exactly at h = range. Most commonly used in practice.
Recommended default for GNSS and climatological data.

**exponential**

    gamma(h) = nugget + (sill - nugget) * [1 - exp(-h / range)]

Approaches the sill asymptotically. Practical range is approximately 3 * range
parameter. Appropriate when correlation decays smoothly without a hard cutoff.
Recommended for IWV and tropospheric delay residuals.

**gaussian**

    gamma(h) = nugget + (sill - nugget) * [1 - exp(-(h/range)^2)]

Very smooth near the origin — models processes with continuous derivatives.
Practical range is approximately sqrt(3) * range parameter. Recommended for
smooth sensor data such as vehicle velocity. Can cause numerical instability
for large datasets (n > 500) with closely spaced observations.

**power**

    gamma(h) = nugget + sill * h^range

Unbounded — no finite sill. Models intrinsic processes (fractal-like behaviour).
The `sill` parameter stores the scaling coefficient and `range` stores the power
exponent (0, 2). Rarely needed in practice.

**nugget**

    gamma(h) = nugget  for h > 0

Pure nugget — no temporal correlation. Represents white noise. The fitted
variogram will pass through a single parameter (the nugget). Useful as a
diagnostic: if the best fit is a pure nugget model, the data has no detectable
temporal structure at the sampled resolution.

### 19.4.2 Nugget interpretation

The nugget represents two combined effects:
- **Measurement noise:** random error in the sensor or instrument.
- **Micro-variability:** real physical variation at lags smaller than the
  sampling interval.

A nugget-to-sill ratio above 0.5 means that more than half of the total variance
is unstructured noise. In this case, Kriging still provides the optimal linear
estimator, but predictions will be significantly smoothed relative to the data.

### 19.4.3 Choosing n_lag_bins and max_lag

- `n_lag_bins`: 15–30 is typical. Too few bins lose detail; too many produce
  noisy empirical variogram bins with few pairs.
- `max_lag`: set to approximately half the total time span, or to the distance
  beyond which you expect no correlation. For sensor data at 1 Hz spanning
  21 minutes, the default auto value (~0.007 days) is appropriate.
- Rule of thumb: each bin should contain at least 30 pairs. Check the protocol
  output for bin counts.

---

## 19.5 prediction sub-section

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `enabled` | bool | `false` | Enable prediction beyond the observation grid. |
| `horizon_days` | double | `0.0` | Forecast horizon in MJD days beyond the last observation. |
| `n_steps` | int | `10` | Number of uniformly spaced steps within the horizon. |
| `target_mjd` | array of double | `[]` | Explicit MJD values at which to predict (optional). |

When `enabled = false`, Kriging predicts only at the observation times (useful
as a gap-filling alternative without forecast).

**Time unit conversion for sensor data:**

| Duration | horizon_days |
|----------|-------------|
| 10 seconds | 0.0001157 |
| 30 seconds | 0.000347 |
| 1 minute | 0.000694 |
| 5 minutes | 0.003472 |
| 1 hour | 0.041667 |

**Critical rule:** the forecast horizon should not exceed the variogram range.
Beyond the range, covariances between the forecast point and all observations
approach zero, and the prediction converges to the global mean (Ordinary
Kriging) or to the drift value (Universal Kriging). This is statistically
correct but physically uninformative. The CI band will widen rapidly.

---

## 19.6 stations sub-section (spatial placeholder)

Used only when `mode = "spatial"`. Not yet implemented — parsed and stored but
ignored. Included here for forward compatibility.

```json
"stations": [
    { "file": "STA001.txt", "x": 17.1074, "y": 48.1486 },
    { "file": "STA002.txt", "x": 17.2201, "y": 48.2103 }
]
```

`x` and `y` are spatial coordinates (e.g. longitude and latitude in decimal
degrees, or easting and northing in metres).

---

## 19.7 Plot flags

Add these keys directly inside the top-level `"plots"` object.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `kriging_variogram` | bool | `true` | Empirical variogram bins and fitted theoretical curve with nugget/sill/range annotations. |
| `kriging_predictions` | bool | `true` | Original series (red circles), Kriging estimate (blue line), and 95% CI band (pink fill). |
| `kriging_crossval` | bool | `true` | Two-panel LOO cross-validation: errors vs sample index (top) and standardised error histogram vs N(0,1) (bottom). |

```json
"plots": {
    "output_format": "png",
    "kriging_variogram":    true,
    "kriging_predictions":  true,
    "kriging_crossval":     true
}
```

---

## 19.8 Output files

All output files are written to `OUTPUT/` sub-directories of the workspace.

| File | Location | Description |
|------|----------|-------------|
| `kriging_[dataset]_[component]_variogram.png` | `IMG/` | Variogram plot. |
| `kriging_[dataset]_[component]_predictions.png` | `IMG/` | Predictions plot with CI band. |
| `kriging_[dataset]_[component]_crossval.png` | `IMG/` | LOO cross-validation diagnostics. |
| `[dataset]_[component]_kriging_protocol.txt` | `PROTOCOLS/` | Plain-text protocol with variogram parameters and CV statistics. |
| `[dataset]_[component]_kriging_predictions.csv` | `CSV/` | Prediction grid: MJD, UTC, value, variance, ci_lower, ci_upper, is_observed. CSV delimiter is semicolon. |

---

## 19.9 Cross-validation diagnostics

The LOO shortcut identity (Dubrule 1983) is used:

    e_i = alpha_i / [K^{-1}]_{ii}

where alpha = K^{-1} * z. This avoids re-solving the system for each left-out
point, reducing complexity from O(n^4) to O(n^2).

| Metric | Ideal value | Interpretation |
|--------|-------------|----------------|
| RMSE | — | Root mean squared LOO error. Compare to sample StdDev. |
| MAE | — | Mean absolute LOO error. More robust than RMSE to outliers. |
| meanSE | 0 | Mean standardised error. Non-zero indicates systematic bias. |
| meanSSE | 1 | Mean squared standardised error. > 1 means variance is underestimated; < 1 means overestimated. |

A good variogram fit produces meanSSE close to 1. Values above 2 or below 0.5
indicate that the theoretical model does not describe the data well.

---

## 19.10 Best practices by data type

### Climatological data (6h resolution, ~25 years, MJD timestamps)

For 30 000+ observations the global Kriging system is infeasible (n x n matrix
inversion). Recommended workflow:

1. Deseasonalize first using `loki_decomposition` or `loki_homogeneity`.
2. Apply Kriging to residuals only — these are shorter in effective correlation
   length, reducing the required max_lag.
3. Use `method = "simple"` with `known_mean = 0.0` on deseasonalized residuals.
4. If the residual series is still long, work on yearly or seasonal segments.

```json
"kriging": {
    "mode": "temporal",
    "method": "simple",
    "known_mean": 0.0,
    "variogram": {
        "model": "exponential",
        "n_lag_bins": 20,
        "max_lag": 10.0
    },
    "prediction": { "enabled": false },
    "cross_validate": true
}
```

### GNSS station coordinates and velocities (~1000–5000 observations)

Linear velocity trend is present — use Universal Kriging with `trend_degree = 1`.
The Kriging residuals represent non-linear transient deformations (earthquakes,
seasonal loading). Spherical model is appropriate for the residuals.

```json
"kriging": {
    "mode": "temporal",
    "method": "universal",
    "trend_degree": 1,
    "variogram": {
        "model": "spherical",
        "n_lag_bins": 20,
        "max_lag": 0.0
    },
    "prediction": {
        "enabled": true,
        "horizon_days": 30.0,
        "n_steps": 30
    },
    "cross_validate": true
}
```

### GNSS IWV and tropospheric delay (~1000–5000 observations)

No significant long-term trend. Exponential model captures the rapid decay of
atmospheric correlation. The nugget is typically moderate (10–30% of sill).

```json
"kriging": {
    "mode": "temporal",
    "method": "ordinary",
    "variogram": {
        "model": "exponential",
        "n_lag_bins": 20,
        "max_lag": 0.0
    },
    "prediction": {
        "enabled": true,
        "horizon_days": 1.0,
        "n_steps": 24
    },
    "cross_validate": true
}
```

### Vehicle or train sensor data (~1000–1500 observations, 1 Hz or ms resolution)

Smooth signal with high nugget (sensor noise). Gaussian model is most
appropriate. The variogram range is typically short (seconds to minutes).
Forecast horizon must not exceed the range — beyond that, the prediction
converges to the series mean.

**Variogram model selection:**
- Gaussian: recommended for smooth physical signals (velocity, acceleration).
- Exponential: if the empirical variogram rises steeply near h = 0.
- Check the variogram plot: the fitted curve should pass through the centre
  of the empirical bins, not just the first few.

```json
"kriging": {
    "mode": "temporal",
    "method": "ordinary",
    "variogram": {
        "model": "gaussian",
        "n_lag_bins": 20,
        "max_lag": 0.0
    },
    "prediction": {
        "enabled": true,
        "horizon_days": 0.000347,
        "n_steps": 30
    },
    "cross_validate": true,
    "confidence_level": 0.95
}
```

**Horizon guidance for 1 Hz data:**
Read the `Range` value from the protocol output. Set `horizon_days` to at most
half the range. For example, if `Range = 0.00253 days` (~218 seconds), a
reasonable horizon is `0.00116 days` (~100 seconds). Beyond the range the
prediction is uninformative.

---

## 19.11 Anomaly detection via cross-validation

The LOO standardised errors stored in the CSV output can be used as an
anomaly score. A point with |standardised error| > 3 is a candidate outlier
in the geostatistical sense — its value is inconsistent with the temporal
neighbourhood defined by the variogram.

This is complementary to the IQR/MAD detectors in `loki_outlier`: Kriging
anomalies have temporal context (they depend on the local correlation
structure), while distributional detectors are global.

Workflow:
1. Run `loki_kriging` with `cross_validate = true`.
2. Open `OUTPUT/CSV/[dataset]_[component]_kriging_predictions.csv`.
3. Rows where `is_observed = 1` have a corresponding LOO error recorded in
   the protocol. Compare `value` against the Kriging estimate from LOO.
4. Points with |e_i / sigma_i| > 3 are flagged.

---

## 19.12 Known limitations

- **Scalability:** Global Kriging inverts an n x n matrix. For n > 3000 the
  matrix inversion is slow and memory intensive (~72 MB for n = 3000 in
  double precision). Future releases will add a moving-window neighbourhood
  option (`max_neighbours` parameter) to handle large series.

- **Stationarity assumption:** The theoretical variogram assumes second-order
  stationarity (constant mean and variance). For non-stationary data (e.g.
  series with trends or changing variance), use Universal Kriging or
  deseasonalize first.

- **Forecast divergence:** Beyond the variogram range, Ordinary Kriging
  predictions converge to the local mean and the CI widens rapidly. This is
  correct behaviour, not a bug. Universal Kriging will instead follow the
  fitted polynomial drift.

- **Gaussian variogram instability:** For closely spaced observations and
  large n, the Gaussian covariance matrix can become poorly conditioned.
  A small jitter (1e-10) is added to the diagonal, but if instability
  persists, switch to exponential or spherical.

- **Spatial and space-time modes:** Not yet implemented. These modes will
  require a multi-series input format and spatial coordinates. Design is
  documented in CLAUDE.md.

# 20. loki_spline

B-spline approximation fits a smooth piecewise polynomial curve through a
time series using least squares. Unlike interpolating splines (which pass
exactly through every data point), the B-spline approximation controls the
number of control points independently of the number of observations. Fewer
control points produce a smoother result; more control points track the data
more closely. The number of control points is the primary smoothing parameter.

LOKI implements clamped B-splines of configurable degree using the
Cox-de Boor basis recursion. The LSQ system is solved via
`ColPivHouseholderQR`. When `n_control_points = 0` (default), the optimal
number of control points is selected automatically by k-fold
cross-validation with the one-standard-error elbow rule.

---

## 20.1 Minimal working configuration

```json
{
    "workspace": "C:/data/project",

    "input": {
        "mode": "single_file",
        "file": "SENSOR_DATA.txt",
        "time_format": "gpst_seconds",
        "time_columns": [0],
        "delimiter": ";",
        "comment_char": "%",
        "columns": [2]
    },

    "spline": {
        "method": "bspline",
        "bspline": {
            "degree": 3,
            "fit_mode": "approximation",
            "n_control_points": 0
        }
    },

    "plots": {
        "output_format": "png"
    }
}
```

All unspecified keys fall back to documented defaults. The pipeline will
auto-detect non-uniform sampling, run 5-fold CV to select `nCtrl`, fit the
B-spline, compute a residual-based CI band, and write plots and CSV output.

---

## 20.2 Top-level spline keys

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `method` | string | `"bspline"` | Fitting method. Only `"bspline"` is implemented. `"nurbs"` is a placeholder that raises an exception if requested. |
| `gap_fill_strategy` | string | `"linear"` | Pre-processing gap fill applied before fitting. `"linear"`, `"spline"`, `"none"`. |
| `gap_fill_max_length` | int | `0` | Maximum gap length to fill in samples. `0` = unlimited. |
| `confidence_level` | double | `0.95` | Confidence level for the CI band in the overlay plot. Must be in (0, 1). |
| `significance_level` | double | `0.05` | Significance level for residual diagnostics. |

---

## 20.3 bspline sub-section

### 20.3.1 All keys

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `degree` | int | `3` | Polynomial degree of each spline piece. Valid range: 1–5. |
| `fit_mode` | string | `"approximation"` | Fitting mode. See Section 20.3.2. |
| `n_control_points` | int | `0` | Number of control points. `0` = automatic via CV. |
| `n_control_min` | int | `5` | Lower bound for the CV search range. |
| `n_control_max` | int | `0` | Upper bound for the CV search range. `0` = auto: `min(n/5, 200)`. |
| `knot_placement` | string | `"uniform"` | Knot placement method. See Section 20.3.3. |
| `cv_folds` | int | `5` | Number of folds for k-fold cross-validation. Valid range: 2–20. |
| `exact_interpolation_max_n` | int | `2000` | Maximum number of observations allowed in `exact_interpolation` mode. |

### 20.3.2 Fit modes

**`approximation`** (default)

Solves the overdetermined LSQ system `N * c = z` where `N` is the
`nObs x nCtrl` basis matrix and `nCtrl < nObs`. This is the primary use
case. The number of control points directly controls the smoothing level:

- Too few control points: underfitting — the curve misses systematic
  variation and residuals show clear patterns.
- Too many control points: overfitting — the curve tracks noise and
  CV RMSE increases.
- Optimal: selected automatically by cross-validation (see Section 20.4).

**`exact_interpolation`**

Sets `nCtrl = nObs`. The fitted curve passes exactly through every
observation. This is equivalent to a classical spline interpolation.

Restrictions:
- Only useful for small, clean series (no noise amplification concern).
- Raises `ConfigException` when `nObs > exact_interpolation_max_n` (default
  2000). The LSQ system is square but poorly conditioned for large n.
- CV is skipped in this mode.
- For gap filling on large series, use `loki_filter` SplineFilter instead.

### 20.3.3 Knot placement

Knot placement determines where the interior knot breakpoints are located
along the normalised parameter axis [0, 1].

**`uniform`**

Interior knots are equally spaced between 0 and 1 regardless of where
the data points are. This is appropriate for uniformly sampled data
(constant timestep). Default for most sensor data.

**`chord_length`**

Uses the Hartley-Judd averaging method to place knots denser in regions
where the data changes rapidly. Each interior knot is the mean of `p`
consecutive Greville abscissae derived from the parameter vector.

Recommended when:
- The series has irregular gaps (variable sample rate).
- The signal has abrupt transitions in some intervals and slow change in
  others (e.g. train acceleration events mixed with constant-velocity phases).

**Auto-detection of non-uniform sampling:**

If `knot_placement = "uniform"` is configured but the pipeline detects
non-uniform sampling (coefficient of variation of timestep differences > 0.1),
it automatically switches to `"chord_length"` and logs a warning. The
`autoKnot = true` flag is recorded in the result and shown in the protocol.

### 20.3.4 Degree selection

| Degree | Name | Continuity | Recommended for |
|--------|------|------------|-----------------|
| 1 | Linear | C^0 | Piecewise linear trend; rarely used for smooth data. |
| 2 | Quadratic | C^1 | Smooth but may show kinks at knots. |
| 3 | Cubic | C^2 | Default. Best balance of smoothness and flexibility. |
| 4 | Quartic | C^3 | Slightly smoother than cubic; rarely needed. |
| 5 | Quintic | C^4 | Very smooth; risk of Runge-like oscillations for large nCtrl. |

Use `degree = 3` (cubic) for nearly all time series applications. Higher
degrees increase the condition number of the basis matrix.

---

## 20.4 Cross-validation and automatic nCtrl selection

When `n_control_points = 0`, the pipeline sweeps `nCtrl` from `n_control_min`
to `n_control_max` and computes k-fold CV RMSE for each candidate.

**Search range:**

- `n_control_min`: lower bound. Must be `>= degree + 1`. Values below 5
  rarely make physical sense. Default: 5.
- `n_control_max`: upper bound. Default `0` = auto: `min(nObs / 5, 200)`.
  The cap of 200 keeps CV tractable for large series (n ~ 36 000). For
  sensor data with n ~ 1500, the auto cap is 200 (well above any practical
  optimum). Increase manually if the CV curve has not yet levelled off at
  the right boundary.

**Selection rule — one-standard-error elbow:**

1. Find the minimum CV RMSE across all candidates (best fit).
2. Compute the standard deviation of all CV RMSE values on the curve.
3. Select the *smallest* `nCtrl` whose CV RMSE does not exceed
   `minRmse + stdRmse`.

This rule prefers a simpler (more parsimonious) model when the improvement
from adding more control points falls within the noise of the CV estimate.
It is a conservative rule: the selected `nCtrl` is typically somewhat smaller
than the pure minimum-RMSE optimum.

**When to use manual nCtrl:**

- You have domain knowledge about the expected number of degrees of freedom.
- CV is slow due to a very large series (increase `n_control_max` cap or
  set `n_control_points` directly).
- You want reproducible results without CV variance.

---

## 20.5 Confidence interval

The CI band is residual-based:

    lower(t) = fitted(t) - z * residualStd
    upper(t) = fitted(t) + z * residualStd

where `z` is the standard normal quantile for `confidence_level` and
`residualStd` is the standard deviation of the training residuals.

This is a homoscedastic (constant-width) CI. It correctly represents
the average noise level but does not widen in regions where the model
has fewer nearby knots. For heteroscedastic noise (variance changes over
time), increase `n_control_points` to allow the spline to adapt locally,
or pre-process with a variance-stabilising transform.

---

## 20.6 Plot flags

Add these keys directly inside the top-level `"plots"` object.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `spline_overlay` | bool | `true` | Original series + fitted B-spline + CI band. |
| `spline_residuals` | bool | `true` | Residuals vs sample index with reference lines at 0, ±RMSE, ±2·RMSE. |
| `spline_basis` | bool | `false` | All basis functions N_{i,p}(t) plotted over [0, 1] with knot positions. Disabled by default — cluttered for nCtrl > 20. |
| `spline_knots` | bool | `true` | Original series with vertical dashed lines at interior knot positions. |
| `spline_cv` | bool | `true` | CV RMSE vs nCtrl curve with optimal nCtrl marked. Only generated when CV was run (auto mode). |
| `spline_diagnostics` | bool | `false` | 4-panel residual diagnostics: ACF, histogram, QQ plot, fitted vs residuals. Delegates to `Plot::residualDiagnostics()`. |

```json
"plots": {
    "output_format": "png",
    "spline_overlay":     true,
    "spline_residuals":   true,
    "spline_basis":       false,
    "spline_knots":       true,
    "spline_cv":          true,
    "spline_diagnostics": false
}
```

---

## 20.7 Output files

| File | Location | Description |
|------|----------|-------------|
| `spline_[dataset]_[component]_overlay.png` | `IMG/` | Fit overlay with CI band. |
| `spline_[dataset]_[component]_residuals.png` | `IMG/` | Residuals with reference lines. |
| `spline_[dataset]_[component]_basis.png` | `IMG/` | Basis functions (if enabled). |
| `spline_[dataset]_[component]_knots.png` | `IMG/` | Knot positions overlaid on series. |
| `spline_[dataset]_[component]_cv.png` | `IMG/` | CV curve (auto mode only). |
| `[dataset]_[component]_spline_protocol.txt` | `PROTOCOLS/` | Plain-text protocol: degree, nCtrl, knot placement, RMSE, R², CV summary. |
| `[dataset]_[component]_spline_fit.csv` | `CSV/` | Per-observation output: index, MJD, UTC, observed, fitted, residual, ci_lower, ci_upper. Delimiter: semicolon. |

---

## 20.8 Best practices by data type

### Vehicle or train sensor data (~1000–1500 observations, 1 Hz)

Smooth physical signal (velocity, acceleration) with moderate sensor noise.
The B-spline approximation serves as a noise filter and feature extractor.
Let CV select the optimal nCtrl automatically.

```json
"spline": {
    "method": "bspline",
    "gap_fill_strategy": "linear",
    "confidence_level": 0.95,
    "bspline": {
        "degree": 3,
        "fit_mode": "approximation",
        "n_control_points": 0,
        "n_control_min": 5,
        "n_control_max": 0,
        "knot_placement": "uniform",
        "cv_folds": 5
    }
}
```

After fitting, inspect the `spline_cv` plot. The CV curve should show a clear
elbow. If the curve is still decreasing at the right edge, increase
`n_control_max` or set `n_control_points` manually.

For phase segmentation tasks (cruising vs acceleration), compare the B-spline
fit with `loki_clustering`. The residuals from the B-spline can serve as a
de-trended feature for clustering.

### Climatological data (6-hour resolution, 25+ years, MJD timestamps)

Direct B-spline fit on a 36 000-point series is expensive for CV. Use one of
the following strategies:

**Strategy A — Seasonal trend extraction:**
Set a moderate fixed nCtrl (e.g. 100–300) to capture the long-term trend
only. Skip CV.

```json
"bspline": {
    "degree": 3,
    "fit_mode": "approximation",
    "n_control_points": 150,
    "knot_placement": "uniform"
}
```

**Strategy B — Residual smoothing after decomposition:**
Run `loki_decomposition` first. Apply `loki_spline` to the trend component
or to residuals (much shorter effective correlation length, CV is fast).

**`n_control_max` guidance for large series:**
The auto cap is `min(n/5, 200)`. For n = 36 524 this gives 200 — appropriate
for capturing inter-annual and decadal variability. Do not increase beyond
500 without checking the condition number of the basis matrix.

### GNSS station coordinates and velocities

Linear velocity trend superimposed on noise. For trend extraction,
`degree = 3` with a small nCtrl (5–20) is sufficient. For residual analysis
after trend removal, use a larger nCtrl or switch to `loki_kriging` which
provides a principled uncertainty model.

```json
"bspline": {
    "degree": 3,
    "fit_mode": "approximation",
    "n_control_points": 10,
    "knot_placement": "uniform"
}
```

### Small clean series (< 100 points), exact interpolation

Use when the goal is smooth interpolation between known points — for example,
a reference calibration curve or a manually digitised profile.

```json
"bspline": {
    "degree": 3,
    "fit_mode": "exact_interpolation",
    "exact_interpolation_max_n": 200
}
```

Note: `exact_interpolation` is ill-suited for noisy data — it amplifies noise
at the control points. For noisy data always use `approximation`.

---

## 20.9 Diagnosing a poor fit

| Symptom | Likely cause | Remedy |
|---------|-------------|--------|
| CV curve still decreasing at right boundary | `n_control_max` too small | Increase `n_control_max` or set `n_control_points` manually |
| Residuals show systematic wave pattern | nCtrl too small (underfitting) | Increase `n_control_min` or use a larger manual `n_control_points` |
| Residuals oscillate rapidly | nCtrl too large (overfitting) | Decrease `n_control_points` or tighten `n_control_max` |
| Fit poorly tracks dense regions, overshoots sparse ones | Uniform knots on non-uniform data | Set `knot_placement = "chord_length"` explicitly |
| AlgorithmException: rank-deficient basis matrix | nCtrl too close to nObs, or degenerate knot vector | Reduce nCtrl; try `"uniform"` knots if using `"chord_length"` |
| Large CI band width | High residual noise | Expected for noisy data; consider increasing `n_control_points` to reduce RMSE, or pre-filter |
| Protocol shows `autoKnot = true` (unexpected) | Timestep CV > 0.1 in gap-filled series | Verify gap-filling result; set `knot_placement = "chord_length"` explicitly to suppress the warning |

---

## 20.10 Relationship to other LOKI modules

| Module | Relationship |
|--------|-------------|
| `loki_filter` SplineFilter | Uses `CubicSpline` from `loki_core/math/spline.hpp` for gap filling and smoothing. Fixed subsample knots; not LSQ. `loki_spline` is a superset with CV and diagnostics. |
| `loki_decomposition` | STL uses LOESS for trend/seasonal. `loki_spline` can be applied to the extracted trend component for a parametric description. |
| `loki_kriging` | Both smooth time series. Kriging provides a probabilistic model with a physically interpretable variogram; B-spline provides a deterministic curve with CV-selected smoothness. For forecasting beyond the data, use Kriging. For smooth interpolation and trend description, use B-spline. |
| `loki_regression` PolynomialRegressor | Polynomial regression is a degenerate B-spline with one segment. Use `loki_spline` when the signal is too complex for a global polynomial. |
| `loki_clustering` | B-spline residuals and fitted values can be used as features for clustering (e.g. slope of fit within a window). |

# 21. loki_spatial

`loki_spatial` performs 2-D spatial interpolation from irregularly distributed
scatter observations (x, y, value) to a regular output grid. Input data is
loaded from a flat text file (including GSLIB format); output is a grid CSV,
a protocol, and a set of plots.

Six interpolation methods are available:

| Method | Uncertainty | Parameters | Best for |
|--------|-------------|------------|----------|
| `kriging` | Yes (variance) | variogram model | Geostatistical data with spatial correlation |
| `idw` | No | power | Quick baseline, no assumptions |
| `rbf` | No | kernel, epsilon | Smooth scattered data, arbitrary geometry |
| `natural_neighbor` | No | none | Dense irregular grids, no extrapolation needed |
| `bspline_surface` | No | degree, nCtrl | Quasi-regular grids, smooth surfaces |
| `nurbs` | — | — | PLACEHOLDER — throws `AlgorithmException` |

---

## 21.1 Minimal working configuration

```json
{
    "workspace": "C:/data/project",

    "spatial": {
        "method": "kriging",
        "cross_validate": true,
        "confidence_level": 0.95,

        "input": {
            "file": "STATIONS.dat",
            "delimiter": " ",
            "comment_prefix": "#",
            "x_column": 0,
            "y_column": 1,
            "value_columns": [2],
            "no_data_value": -999.9999,
            "no_data_tolerance": 0.01,
            "coordinate_unit": "m"
        },

        "kriging": {
            "method": "ordinary",
            "variogram": {
                "model": "spherical"
            }
        }
    },

    "plots": {
        "output_format": "png"
    }
}
```

All unspecified keys fall back to documented defaults. The pipeline will
auto-detect the grid resolution from the data, fit a spherical variogram,
run Ordinary Kriging, perform LOO cross-validation, and write plots and CSV.

---

## 21.2 Top-level spatial keys

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `method` | string | `"kriging"` | Interpolation method. See Section 21.3. |
| `cross_validate` | bool | `true` | Run leave-one-out cross-validation. Writes a CV CSV and CV plot. |
| `confidence_level` | double | `0.95` | Confidence level for Kriging CI bands. Must be in (0, 1). Ignored for non-Kriging methods. |

---

## 21.3 Input sub-section

The `spatial.input` block configures the scatter data loader. It is independent
of the time series `input` block used by other LOKI modules.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `file` | string | — | Input file name, relative to `workspace/INPUT/`. Required. |
| `delimiter` | string | `" "` | Column delimiter character. Use `" "` to split on any whitespace. Use `";"`, `","`, or `"\t"` for fixed delimiters. |
| `comment_prefix` | string | `"#"` | Prefix that marks a line as a comment. Accepted values: `"#"`, `"%"`, `"!"`, `"//"`, `";"`, `"*"`. Any other value raises `ConfigException`. |
| `x_column` | int | `0` | 0-based column index of the X coordinate. |
| `y_column` | int | `1` | 0-based column index of the Y coordinate. |
| `value_columns` | int[] | `[]` | 0-based indices of value columns to interpolate. Empty array = all columns except `x_column` and `y_column`. |
| `no_data_value` | double | `-999.0` | Sentinel value for missing observations. |
| `no_data_tolerance` | double | `0.01` | Values within `|v - no_data_value| <= tolerance` are treated as missing. |
| `coordinate_unit` | string | `""` | Informational label for the coordinate unit: `"m"`, `"km"`, `"deg"`, `"rad"`, or `""`. No transformation is applied. |

### 21.3.1 GSLIB format auto-detection

If the second non-empty comment line in the file is a bare integer N, the file
is treated as GSLIB format:

```
# Dataset title           ← ignored
# 8                       ← N = number of columns
# X m  ...                ← column 0 name
# Y m  ...                ← column 1 name
# Thk m ...               ← column 2 name
...
12100 8300 37.15 ...      ← data begins here
```

Column names are extracted from the N name lines (first word of each line).
For non-GSLIB files the loader extracts names generically from comment lines
matching `# NAME ...` where NAME is a valid identifier `[A-Za-z][A-Za-z0-9_]*`.

### 21.3.2 Coordinate system

Coordinates are treated as Cartesian. For geographic data (latitude/longitude)
in a small region (< 5° extent), the Mercator approximation holds and no
transformation is needed — interpolation proceeds as if coordinates were metres.

For large regions or accuracy-critical geodetic work, project to a local
Cartesian system (e.g. UTM) before loading. Coordinate transformation is the
domain of the planned `loki_geodesy` module.

---

## 21.4 Grid sub-section

The output grid geometry can be fully automatic or manually configured.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `resolution_x` | double | `0.0` | Grid spacing in the X direction. `0` = auto. |
| `resolution_y` | double | `0.0` | Grid spacing in the Y direction. `0` = auto (same as X). |
| `x_min` | double | `0.0` | Grid left boundary. `0` with `x_max = 0` = auto from data. |
| `x_max` | double | `0.0` | Grid right boundary. |
| `y_min` | double | `0.0` | Grid bottom boundary. |
| `y_max` | double | `0.0` | Grid top boundary. |

**Auto resolution heuristic:** median of all nearest-neighbour distances
divided by 2. This adapts to the local data density without being skewed
by outlier point pairs.

**Auto bounds:** data range extended by one grid cell padding on each side,
so edge observations are always interior to the grid.

Grid dimensions are derived as:
```
nCols = round((xMax - xMin) / resX) + 1
nRows = round((yMax - yMin) / resY) + 1
```

---

## 21.5 Kriging sub-section

Used when `method = "kriging"`.

### 21.5.1 Kriging method

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `method` | string | `"ordinary"` | Kriging variant. See table below. |
| `known_mean` | double | `0.0` | Global mean for Simple Kriging. Ignored for other variants. |
| `trend_degree` | int | `1` | Polynomial drift degree for Universal Kriging. Valid: `1` or `2`. |

| Variant | When to use |
|---------|-------------|
| `"ordinary"` | Default. Unknown local mean. Robust for most geostatistical applications. |
| `"simple"` | Use when the global mean is known from domain knowledge (e.g. a long-term average). Slightly more precise than Ordinary when the mean is correct. |
| `"universal"` | Use when the data has a systematic spatial trend (gradient across the field). Includes a polynomial drift term of degree `trend_degree`. |

**Diagnosing the need for Universal Kriging:** if the omnidirectional variogram
does not reach a sill but keeps growing linearly with lag distance, a spatial
trend is present. Switch to `method = "universal"` with `trend_degree = 1`.
The variogram is then fitted to the residuals after drift removal, and the
resulting sill and range are physically meaningful.

### 21.5.2 Variogram sub-section

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `model` | string | `"spherical"` | Theoretical variogram model. See table below. |
| `n_lag_bins` | int | `20` | Number of lag bins for the empirical variogram. Must be >= 3. |
| `max_lag` | double | `0.0` | Maximum lag distance for binning. `0` = auto (half of max pairwise distance). |
| `nugget` | double | `0.0` | Initial nugget for WLS fitting. `0` = estimated from data. |
| `sill` | double | `0.0` | Initial sill for WLS fitting. `0` = estimated from data. |
| `range` | double | `0.0` | Initial range for WLS fitting. `0` = estimated from data. |

| Model | Behaviour | When to use |
|-------|-----------|-------------|
| `"spherical"` | Reaches sill at finite range. Most common in practice. | Default for most datasets. |
| `"exponential"` | Approaches sill asymptotically. Lighter tail than Gaussian. | Porosity, permeability data. Slightly more robust than spherical for noisy empirical variograms. |
| `"gaussian"` | Parabolic near origin (very smooth process). | Smoothly varying fields (temperature, elevation). |
| `"power"` | No sill — intrinsic process. | Fractal or non-stationary fields. Incompatible with Simple Kriging. |
| `"nugget"` | Pure nugget effect (no spatial correlation). | Nugget-only baseline; rarely useful alone. |

**Choosing a model:** plot the empirical variogram (`spatial_variogram` plot)
and compare the shape near the origin with the theoretical models:

- Steep linear rise near origin → spherical or exponential.
- Parabolic (smooth) rise → gaussian.
- No levelling off → power model or spatial trend (consider Universal Kriging).
- Near-zero semi-variance at short lags with a jump → nugget > 0.

---

## 21.6 IDW sub-section

Used when `method = "idw"`.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `power` | double | `2.0` | Distance decay exponent. Higher values give more weight to nearby points. Must be > 0. |

Typical values: `1.0` (linear), `2.0` (standard), `3.0` (sharp local influence).
IDW does not provide uncertainty estimates; variance and CI grids are zero-filled.

Bilineárna interpolácia (interpBilinear) je implementovaná v loki_core/spatial/spatialInterp.hpp ako pomocná funkcia, ale nie je vystavená ako samostatná metóda v pipeline. Dôvod: bilineárna interpolácia operuje na existujúcom pravidelnom gridi (prevzorkuje ho na iné rozlíšenie), nie na nepravidelných scatter bodoch. Pre vstup vo forme scatter dát preto nemá priame uplatnenie.
Typické interné použitie:

```
//Reprojekcia výstupného gridu z hrubého na jemné rozlíšenie
Eigen::MatrixXd fineGrid = loki::spatial::interpBilinear(
    coarseGrid, coarseExtent, fineExtent);
```

Ak potrebuješ jednoduchú a rýchlu interpoláciu scatter dát bez štatistických predpokladov, použij "idw" s power = 1.0 — výsledok je podobný bilineárnej interpolácii, ale funguje priamo na nepravidelných vstupných bodoch.

Vložiť to môžeš buď za úvodnú tabuľku metód (ako poznámku pod čiarou), alebo na koniec sekcie 21.6 IDW ako porovnávací komentár. Ja by som ho dal za tabuľku v úvode — tam je kontext najčistejší.

---

## 21.7 RBF sub-section

Used when `method = "rbf"`.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `kernel` | string | `"thin_plate_spline"` | RBF kernel type. See table below. |
| `epsilon` | double | `0.0` | Shape parameter. `0` = auto: `1 / (sqrt(n) * medianNNdist)`. Ignored for `thin_plate_spline` and `cubic`. |

| Kernel | phi(r) | Shape param | Notes |
|--------|--------|-------------|-------|
| `"thin_plate_spline"` | r² log(r) | — | Natural 2-D generalisation of cubic spline. No shape parameter. Default. |
| `"cubic"` | r³ | — | Simpler conditionally PD kernel. Smoother than TPS for small r. |
| `"multiquadric"` | sqrt(r² + ε²) | ε | Larger ε = flatter, smoother interpolant. |
| `"inverse_multiquadric"` | 1/sqrt(r² + ε²) | ε | Strictly PD. Decays at large r; most robust for irregular configurations. |
| `"gaussian"` | exp(−ε² r²) | ε | Strictly PD. Very smooth. May be ill-conditioned for small ε or dense grids. |

**Shape parameter guidance:** for `multiquadric` and `inverse_multiquadric`,
a common heuristic is `epsilon = 1 / (sqrt(n) * d_avg)` where `d_avg` is the
mean nearest-neighbour distance. The auto-estimation (`epsilon = 0`) uses this
formula. For `gaussian`, `epsilon ~ 1 / (2 * d_avg)` is a reasonable starting
point.

**Numerical note:** RBF coordinates are normalised to [0,1] internally before
assembling the system matrix. This is necessary for `thin_plate_spline` and
`cubic` kernels where phi values scale as r²log(r) in metre-scale coordinates,
causing severe ill-conditioning of the saddle-point system. Normalisation
reduces the condition number from ~1e20 to ~300 in typical configurations.

---

## 21.8 B-spline surface sub-section

Used when `method = "bspline_surface"`.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `degree_u` | int | `3` | B-spline degree in the X (U) direction. Valid: 1–5. |
| `degree_v` | int | `3` | B-spline degree in the Y (V) direction. Valid: 1–5. |
| `n_ctrl_u` | int | `6` | Control points in U. Must be >= `degree_u + 1`. |
| `n_ctrl_v` | int | `6` | Control points in V. Must be >= `degree_v + 1`. |
| `knot_placement` | string | `"uniform"` | `"uniform"` or `"chord_length"`. |

The surface is a tensor product B-spline `S(u,v) = sum_ij c_ij * N_i(u) * N_j(v)`.
Scatter (x, y) coordinates are normalised to [0,1] separately before fitting.
The overdetermined LSQ system is solved via `ColPivHouseholderQR`.

**Note:** tensor product B-splines assume a quasi-regular distribution of input
points. For strongly irregular scatter data, `rbf` with `thin_plate_spline` or
`kriging` is generally preferable.

Minimum required observations: `n_ctrl_u * n_ctrl_v`. The pipeline raises
`DataException` if fewer points are available.

---

## 21.9 Plot flags

Add these keys inside the top-level `"plots"` object.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `spatial_heatmap` | bool | `true` | Interpolated grid as a pm3d colour map. |
| `spatial_scatter` | bool | `true` | Input scatter points coloured by value. Requires `cross_validate: true` (point coordinates come from the CV result). |
| `spatial_variogram` | bool | `true` | Empirical variogram bins + fitted model curve. Only generated for `method = "kriging"`. |
| `spatial_crossval` | bool | `true` | LOO error scatter map — each input point coloured by its LOO prediction error. |
| `spatial_variance` | bool | `true` | Kriging prediction variance grid as a colour map. Only generated for `method = "kriging"`. |

```json
"plots": {
    "output_format": "png",
    "spatial_heatmap":   true,
    "spatial_scatter":   true,
    "spatial_variogram": true,
    "spatial_crossval":  true,
    "spatial_variance":  true
}
```

---

## 21.10 Output files

| File | Location | Description |
|------|----------|-------------|
| `spatial_[dataset]_[variable]_heatmap.png` | `IMG/` | Interpolated regular grid as colour map. |
| `spatial_[dataset]_[variable]_scatter.png` | `IMG/` | Input scatter points coloured by observed value. |
| `spatial_[dataset]_[variable]_variogram.png` | `IMG/` | Variogram plot (Kriging only). |
| `spatial_[dataset]_[variable]_crossval.png` | `IMG/` | LOO error scatter map. |
| `spatial_[dataset]_[variable]_variance.png` | `IMG/` | Kriging variance grid (Kriging only). |
| `spatial_[dataset]_[variable]_protocol.txt` | `PROTOCOLS/` | Plain-text protocol: method, n, mean, variance, grid geometry, variogram parameters, CV diagnostics. |
| `spatial_[dataset]_[variable]_grid.csv` | `CSV/` | Grid values: `x;y;value[;variance;ci_lower;ci_upper]`. CI columns present for Kriging only. |
| `spatial_[dataset]_[variable]_cv.csv` | `CSV/` | LOO cross-validation: `x;y;error[;std_error]`. `std_error` present for Kriging only. |

---

## 21.11 Best practices by use case

### Geostatistical data (geology, hydrogeology, environmental monitoring)

Ordinary Kriging with a fitted variogram is the standard approach. Let the
auto-resolution heuristic set the grid, inspect the variogram plot, and
adjust the model if needed.

```json
"spatial": {
    "method": "kriging",
    "kriging": {
        "method": "ordinary",
        "variogram": {
            "model": "spherical",
            "n_lag_bins": 20,
            "max_lag": 0.0
        }
    }
}
```

If the variogram does not reach a sill (linear growth), switch to Universal
Kriging with `trend_degree = 1`. This removes the linear spatial trend before
variogram fitting, making the sill and range physically meaningful.

### GNSS station network (tropospheric delay, ZTD interpolation)

Stations have irregular spatial coverage; LLA coordinates can be treated as
Cartesian for regional networks (< 500 km extent). Ordinary Kriging with
a Gaussian variogram typically fits well for smoothly varying atmospheric
fields.

```json
"spatial": {
    "method": "kriging",
    "input": { "coordinate_unit": "deg" },
    "kriging": {
        "method": "ordinary",
        "variogram": { "model": "gaussian", "n_lag_bins": 15 }
    },
    "grid": { "resolution_x": 0.5, "resolution_y": 0.5 }
}
```

### Quick exploratory baseline (IDW)

IDW requires no variogram fitting and has a single intuitive parameter.
Use it as a fast sanity check before committing to Kriging.

```json
"spatial": {
    "method": "idw",
    "idw": { "power": 2.0 }
}
```

Compare the IDW heatmap with the Kriging heatmap. If they agree qualitatively,
the spatial structure is robust. If they differ significantly, the variogram
model choice matters — inspect the variogram plot carefully.

### Smooth physical surface (RBF thin plate spline)

RBF with thin plate spline is the natural 2-D extension of cubic spline
interpolation. It minimises the bending energy of the surface and requires
no shape parameter. Use when data is dense enough that exact interpolation
through all points is appropriate.

```json
"spatial": {
    "method": "rbf",
    "rbf": { "kernel": "thin_plate_spline" }
}
```

For sparser data where smoothing is preferred over exact interpolation,
use `bspline_surface` with `n_ctrl_u` and `n_ctrl_v` smaller than the
number of points.

### Dense irregular scatter (Natural Neighbor)

Natural Neighbor interpolation is parameter-free and reproduces linear
functions exactly. It is well-suited for datasets where point density is
sufficient to define Voronoi cells reliably (typically n > 20). It does not
extrapolate beyond the convex hull of input points — queries outside the
hull fall back to the nearest observed value with a logged warning.

```json
"spatial": {
    "method": "natural_neighbor",
    "cross_validate": true
}
```

---

## 21.12 Diagnosing a poor result

| Symptom | Likely cause | Remedy |
|---------|-------------|--------|
| Variogram sill and range are unrealistically large | Spatial trend in data | Switch to `kriging.method = "universal"` with `trend_degree = 1` |
| Heatmap shows bull's-eye artefacts around data points | IDW with high power, or RBF overfitting | Reduce IDW `power`; or switch to Kriging / Natural Neighbor |
| Heatmap is too smooth, misses local features | Kriging range too large, or IDW power too low | Reduce initial `range`; increase IDW `power`; inspect variogram |
| RBF raises rank-deficient exception | Near-coincident or collinear input points | Check for duplicate coordinates; switch to `"inverse_multiquadric"` or `"gaussian"` kernel which are strictly positive definite |
| Natural Neighbor: many "outside convex hull" warnings | Queries cover area beyond data extent | Tighten grid bounds manually via `x_min / x_max / y_min / y_max`; or switch to IDW/Kriging which extrapolate |
| CV RMSE >> sample variance | Method poorly suited to data structure | Try an alternative method; inspect scatter and heatmap plots |
| B-spline raises DataException (too few points) | `n_ctrl_u * n_ctrl_v > n` | Reduce `n_ctrl_u` and `n_ctrl_v` |
| Variance map is uniformly high everywhere | Nugget dominates; very short spatial correlation | Reduce nugget initial value; increase `n_lag_bins`; try exponential model |
| Variance is zero at observation locations but high elsewhere | Normal Kriging behaviour (exact interpolation property) | Expected; not a bug |

---

## 21.13 Relationship to other LOKI modules

| Module | Relationship |
|--------|-------------|
| `loki_kriging` | Temporal 1-D Kriging in time. `loki_spatial` reuses the same variogram infrastructure (`loki_core/math/krigingVariogram.hpp`) but operates on 2-D Euclidean lag. The two modules are independent pipelines with separate configs. |
| `loki_geodesy` (planned) | Geodetic coordinate transformations with covariance propagation. When available, project LLA → local Cartesian before passing to `loki_spatial`. |
| `loki_multivariate` (planned) | Co-kriging (cross-variogram between variables) will be implemented there, reusing the Kriging math primitives from `loki_core/math/`. |
| `loki_qc` | Run `loki_qc` first on each station time series. Pass only the QC-approved mean or trend value per station to `loki_spatial` as the scatter observation. |
| `loki_decomposition` / `loki_ssa` | Extract the signal component of interest (trend, seasonal anomaly) per station before spatial interpolation, rather than interpolating raw observations. |