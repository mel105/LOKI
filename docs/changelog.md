# Changelog

## Current — development snapshot

LOKI has undergone a complete architectural rewrite since the original alpha release.
The framework is now a modular C++20 system with a shared core library and sixteen
independent analysis modules.

### What changed from the alpha

| Area | Alpha (0.0.11) | Current |
|------|---------------|---------|
| Language standard | C++14 | C++20 |
| Linear algebra | newmat | Eigen3 3.4.0 |
| Configuration | custom | nlohmann_json |
| Build system | legacy CMake | CMake 3.25+ with presets |
| Platform | Linux only | Windows MSYS2/UCRT64 (Linux planned) |
| Scope | Change point detection only | 16 analysis modules |
| Architecture | monolithic | modular, loki_core + thin orchestrators |

### Modules added since alpha

`loki_outlier`, `loki_filter`, `loki_regression`, `loki_stationarity`,
`loki_arima`, `loki_ssa`, `loki_decomposition`, `loki_spectral`,
`loki_kalman`, `loki_qc`, `loki_clustering`, `loki_simulate`,
`loki_evt`, `loki_kriging`, `loki_spline`

### Core extensions added

- B-spline basis and LSQ fitting (`loki_core/math/bspline`, `bsplineFit`)
- Kriging math primitives (`loki_core/math/kriging*`)
- Randomized SVD (`loki_core/math/randomizedSvd`)
- Nelder-Mead optimiser (`loki_core/math/nelderMead`)
- GapFiller and Deseasonalizer (`loki_core/timeseries/`)
- DataManager pattern for structured workspace I/O

---

## Upcoming

- `loki_spatial` — 2D spatial Kriging
- Linux build verification and CI
- Doxygen API reference published via GitHub Pages
- Worked examples with real datasets
