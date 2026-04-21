# Configuration Reference

All LOKI programs are configured via a JSON file passed as the first command-line argument.

Common sections (`input`, `output`, `plots`, `stats`) are shared across all programs.
Module-specific sections (`outlier`, `homogeneity`, `filter`, ...) are ignored by
programs that do not use them, so a single config file can drive multiple programs.

---

## Structure overview

```json
{
    "input":       { ... },
    "output":      { ... },
    "plots":       { ... },
    "stats":       { ... },
    "<module>":    { ... }
}
```

---

## Common: input

Controls how input data files are located and parsed.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `file` | string | — | Input filename. Resolved relative to `<workspace>/INPUT/`. |
| `time_format` | string | `gpst_seconds` | Time encoding. See options below. |
| `time_columns` | int[] | `[0]` | 0-based field indices that form the time token. |
| `delimiter` | string | `";"` | Field separator. |
| `comment_char` | string | `"%"` | Lines starting with this character are skipped. |
| `columns` | int[] | all | 1-based indices of value columns to load. |
| `merge_strategy` | string | `separate` | How to combine multiple files. |

**`time_format` options**

| Value | Description |
|-------|-------------|
| `gpst_seconds` | Seconds since GPS epoch 1980-01-06 |
| `gpst_week_sow` | Two columns: GPS week + seconds of week |
| `utc` | ISO string: `YYYY-MM-DD HH:MM:SS[.sss]` |
| `mjd` | Modified Julian Date (floating point) |
| `unix` | Seconds since Unix epoch 1970-01-01 |
| `index` | Sequential integer index (no absolute time) |

---

## Common: output

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `log_level` | string | `info` | Minimum severity: `debug`, `info`, `warning`, `error` |

---

## Common: plots

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `output_format` | string | `png` | Image format: `png`, `eps`, `svg` |
| `time_format` | string | `""` | X-axis label format. Empty = inherit from input. |
| `enabled` | object | — | Map of plot name to bool. |

---

## Common: stats

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `enabled` | bool | `true` | Compute and log descriptive statistics |
| `nan_policy` | string | `skip` | `skip`, `throw`, or `propagate` |
| `hurst` | bool | `true` | Compute Hurst exponent |

---

## Full reference

The complete configuration reference — covering all 16 analysis modules with every
parameter, default value, and usage note — is maintained in the repository:

[**CONFIG_REFERENCE.md on GitHub**](https://github.com/mel105/LOKI/blob/master/CONFIG_REFERENCE.md)

The document is structured as one chapter per module and includes:

- All JSON keys with types and defaults
- Enum option tables
- Minimal and full example JSON blocks
- Best-practice guidance per data domain (climatological, GNSS, sensor)
- Diagnostics and troubleshooting tables
