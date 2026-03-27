# PROTOCOLS.md -- Protocol Format Reference for LOKI

This file documents the planned protocol (TXT) output format for all pipeline
modules. Protocols are written to `OUTPUT/PROTOCOLS/` alongside LOG, CSV, IMG.

Filename convention: `[program]_[dataset]_[componentName]_protocol.txt`
e.g. `filter_CLIM_DATA_EX1_col_3_protocol.txt`

All float values: 4 decimal places default.
Tags: `[PASS]` / `[FAIL]` / `[WARN]` for diagnostic results.
Dates: human-readable UTC where available, otherwise MJD.
Protocols are always generated -- not gated by a config flag.

---

## loki_regression -- DONE (R7)

```
============================================================
 REGRESSION PROTOCOL -- LinearRegressor
============================================================
 Dataset:      CLIM_DATA_EX1    Series: col_3
 Observations: 1276    Parameters: 2    DOF: 1274
 Method:       linear    Robust: no

 COEFFICIENTS
 -------------------------------------------------------
 Parameter    Estimate     Std.Err     t-stat    p-value
 a0           5.7421       0.0312      184.1     <0.001
 a1           0.0023       0.0004        5.8     <0.001

 MODEL FIT
 -------------------------------------------------------
 sigma0:           0.0318
 R^2:              0.9823    Adjusted R^2: 0.9822
 AIC:              -4821.3   BIC: -4810.1
 F-statistic:      33.7      p-value: <0.001

 CROSS-VALIDATION
 -------------------------------------------------------
 Method:           LOO-CV (analytical)
 CV RMSE:          0.0341
 CV MAE:           0.0251
 CV Bias:          0.0001

 RESIDUAL DIAGNOSTICS
 -------------------------------------------------------
 Mean:             0.0000    Std dev: 0.0318
 Normality (J-B):  p = 0.312   [PASS]
 Autocorr. (D-W):  1.94        [PASS]
============================================================
```

---

## loki_filter -- PLANNED (F7)

Requires C2 (Jarque-Bera, Durbin-Watson from `loki_core/stats/hypothesis`).

```
============================================================
 FILTER PROTOCOL -- KernelSmoother(Epanechnikov)
============================================================
 Dataset:      CLIM_DATA_EX1    Series: col_3
 Observations: 36524
 Filter:       KernelSmoother(Epanechnikov)
 Window:       3652 samples     Bandwidth: 0.100000
 Auto-window:  yes (SILVERMAN_MAD)

 RESIDUAL STATISTICS
 -------------------------------------------------------
 Mean:              0.0001    Std dev: 0.0318
 MAD:               0.0214    RMSE:    0.0318
 Min:              -0.1123    Max:     0.1456

 RESIDUAL DIAGNOSTICS
 -------------------------------------------------------
 Normality (J-B):   p = 0.041   [FAIL -- non-normal residuals]
 Autocorr. (D-W):   0.43        [FAIL -- strong autocorrelation]
 ACF lag-1:         0.821

 TUNING HINTS
 -------------------------------------------------------
 High ACF in residuals suggests bandwidth is too large.
 Current: bandwidth=0.100 -> window=3652 samples (~913 days at 6h resolution).
 Suggested: try bandwidth 0.01-0.03 for better frequency resolution.
============================================================
```

Tuning hints logic:
- ACF lag-1 > 0.5  -> bandwidth too large, suggest reducing
- ACF lag-1 < 0.1 and D-W near 2.0 -> good fit
- D-W < 1.5 -> positive autocorrelation remaining

---

## loki_outlier -- PLANNED

Requires C2 (Jarque-Bera from `loki_core/stats/hypothesis`).

```
============================================================
 OUTLIER PROTOCOL -- MAD detector
============================================================
 Dataset:      CLIM_DATA_EX1    Series: col_3
 Observations: 36524
 Deseasonalization: MEDIAN_YEAR

 DETECTION RESULTS
 -------------------------------------------------------
 Method:            MAD    Multiplier: 3.0
 Outliers detected: 47     (0.13% of series)
 Replacement:       linear interpolation   Max fill: unlimited

 SERIES STATISTICS
 -------------------------------------------------------
             Before cleaning    After cleaning
 Mean:            0.0923             0.0921
 Std dev:         0.0341             0.0318
 MAD:             0.0241             0.0198
 Min:            -0.1456            -0.0987
 Max:             0.2341             0.1823

 DIAGNOSTICS
 -------------------------------------------------------
 Normality (J-B):   p = 0.124   [PASS]
============================================================
```

---

## loki_homogeneity -- PLANNED

Requires C2 for ACF dependence correction statistics.

```
============================================================
 HOMOGENEITY PROTOCOL -- SNHT (Alexandersson)
============================================================
 Dataset:      CLIM_DATA_EX1    Series: col_3
 Observations: 36524
 Significance level: 0.05    Min segment: 180d

 PRE-PROCESSING
 -------------------------------------------------------
 Gap filling:           linear    Filled: 23 gaps
 Pre-outliers removed:  23        (method: mad_bounds, k=5.0)
 Deseasonalization:     MEDIAN_YEAR
 Post-outliers removed: 12        (method: mad, k=3.0)
 ACF dependence correction: yes   (lag-1 ACF = 0.34)

 CHANGE POINTS DETECTED: 3
 -------------------------------------------------------
 #   Index    MJD          Date         Shift       p-value
 1   8640     51840.000    2001-01-15   -0.0312      0.001
 2   14400    53400.000    2005-03-22    0.0187      0.023
 3   21600    55200.000    2010-02-08   -0.0095      0.041

 SEGMENT STATISTICS
 -------------------------------------------------------
 Seg  Date range                n       Mean     Std dev
 1    1987-01-01 -- 2001-01-15  8640    0.0923   0.0318
 2    2001-01-15 -- 2005-03-22  5760    0.0611   0.0301
 3    2005-03-22 -- 2010-02-08  7200    0.0798   0.0325
 4    2010-02-08 -- 2016-12-31  7924    0.0703   0.0312

 ADJUSTMENTS APPLIED
 -------------------------------------------------------
 Reference segment: 1 (oldest / leftmost)
 Total cumulative correction: +0.0220
 Segment 2 corrected by: +0.0312
 Segment 3 corrected by: +0.0125
 Segment 4 corrected by: +0.0220
============================================================
```

---

## Implementation Notes (for when these threads start)

### Shared infrastructure
- Protocol dir: `OUTPUT/PROTOCOLS/` -- `protocolsDir` already in `AppConfig`.
- All `main.cpp` files must create it with `std::filesystem::create_directories(cfg.protocolsDir)`.
- A `ProtocolWriter` helper class per module handles formatting.
  Consider a shared `loki_core/io/protocolWriter.hpp` if patterns converge
  (header + section + table + footer pattern is identical across all modules).
- Float precision: 4 decimal places default, `std::fixed`.
- `[PASS]` / `[FAIL]` / `[WARN]` tags for all diagnostic results.
- Dates via `TimeStamp::toUtcString()` where available, otherwise MJD.

### Dependencies per module
| Module | Requires from C2 |
|---|---|
| `loki_filter` | `jarqueBera`, `durbinWatson`, `computeMetrics` |
| `loki_outlier` | `jarqueBera`, `computeMetrics` |
| `loki_homogeneity` | `durbinWatson` (already used for ACF correction) |
| `loki_regression` | `tCdf`, `fCdf`, `jarqueBera`, `durbinWatson`, `computeMetrics` |
