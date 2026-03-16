# CLAUDE.md — Project Instructions for LOKI

This file contains instructions for Claude when working on the LOKI project.
It should be placed in the root of the repository and kept up to date.

---

## Project Overview

LOKI is a professional C++ toolkit for statistical analysis of time series data.
The long-term goal is a modular ecosystem of analysis libraries with a GUI/IDE frontend.
Current focus: change point detection and homogeneity testing (`loki_homogeneity`).

---

## Workflow Rules

### One task per conversation thread
- Each new feature, refactoring task, or module gets its **own conversation thread**.
- Use this thread (the root project thread) only for **architecture, organization, and planning**.
- Start each task thread with a reference to this file: *"Working on LOKI — see CLAUDE.md"*.

### Before writing any code
- Discuss the approach first. Do not jump into implementation without agreeing on design.
- If the task is ambiguous, ask clarifying questions before starting.
- Propose the function/class signatures and overall structure first, then wait for approval.

---

## Code Style and Standards

### Language and Standard
- Language: **C++20**
- All new code must compile cleanly with `-Wall -Wextra -Wpedantic` and no warnings.
- No compiler extensions (`CMAKE_CXX_EXTENSIONS OFF`).

### Naming Conventions
- Classes: `PascalCase` (e.g., `ChangePointDetector`)
- Functions and methods: `camelCase` (e.g., `detectChangePoints()`)
- Member variables: `m_camelCase` (e.g., `m_data`)
- Constants and enums: `UPPER_SNAKE_CASE`
- Files: `camelCase.cpp` / `camelCase.hpp`

### Headers
- Use `.hpp` extension for all C++ headers.
- Use `#pragma once` at the top of every header.
- Include order: own headers -> project headers -> third-party -> standard library.

### Comments
- **All comments must be written in English.**
- **All comments must use ASCII characters only** — no Unicode box-drawing characters
  such as `─`, `│`, `└`, etc. Use plain `-` or `=` for decorative separators.
  Unicode in source files causes GCC on Windows to misparse tokens.
- Every class must have a Doxygen-style comment block explaining its purpose.
- Every public method must have a brief Doxygen comment.
- Inline comments should explain *why*, not *what*.

Example:
```cpp
/**
 * @brief Detects change points in a time series using the SNHT method.
 *
 * Implements the Standard Normal Homogeneity Test (Alexandersson, 1986).
 * The series must be detrended and deseasoned before calling this method.
 */
class SnhtDetector {
public:
    /**
     * @brief Runs the SNHT test and returns the index of the most significant change point.
     * @param series Input time series values.
     * @return Index of detected change point, or -1 if none found.
     * @throws LOKIException if the series has fewer than MIN_SERIES_LENGTH points.
     */
    int detect(const std::vector<double>& series) const;
};
```

---

## Error Handling

### Use exceptions -- no silent failures
- Define and use a project exception hierarchy rooted at `LOKIException`.
- Never return error codes from functions where an exception is more appropriate.
- Never silently ignore errors or return magic values like `-1` or `NaN` without documentation.

### Exception hierarchy (defined in `libs/loki_core/include/loki/core/exceptions.hpp`)
```
std::exception
+-- LOKIException                  <- base for all LOKI exceptions
    +-- DataException               <- invalid or missing data
    |   +-- SeriesTooShortException
    |   +-- MissingValueException
    +-- IoException                 <- file I/O errors
    |   +-- FileNotFoundException
    |   +-- ParseException
    +-- ConfigException             <- configuration errors
    +-- AlgorithmException          <- numerical or algorithmic failures
        +-- ConvergenceException
        +-- SingularMatrixException
```

### Namespace
- All exception classes live in `namespace loki` (defined in `exceptions.hpp`).
- In `.cpp` files, add `using namespace loki;` after the last `#include` so exception
  names can be used without the `loki::` prefix inside library code.
- In `apps/` (main), use the fully qualified `loki::LOKIException` in catch blocks.
- `exceptions.hpp` **must be included early** in the include chain. It is included by
  `logger.hpp`, which is included by `config.hpp` -- so any file that includes
  `config.hpp` or `logger.hpp` will have exceptions available automatically.

### Where to catch
- Catch exceptions as **high up** in the call stack as makes sense (ideally in `main` or app layer).
- In library code (`libs/`), throw -- do not catch and swallow.
- In `apps/` (main), catch and report cleanly to the user.

### Example pattern
```cpp
// In library code -- throw, do not catch
double computeMean(const std::vector<double>& data) {
    if (data.empty()) {
        throw DataException("Cannot compute mean of an empty series.");
    }
    // ...
}

// In app layer -- catch and handle
try {
    auto result = detector.detect(series);
} catch (const loki::LOKIException& ex) {
    std::cerr << "[LOKI ERROR] " << ex.what() << "\n";
    return EXIT_FAILURE;
} catch (const std::exception& ex) {
    std::cerr << "[UNEXPECTED ERROR] " << ex.what() << "\n";
    return EXIT_FAILURE;
}
```

---

## Code Delivery

### Always use artifacts
- All code (`.cpp`, `.hpp`, `CMakeLists.txt`) must be delivered in **artifacts**, never in plain chat text.
- Each artifact contains exactly **one file**.
- The artifact title must match the file name (e.g., `changePoint.hpp`).

### License header
- Do NOT add any license/copyright header block at the top of `.cpp` or `.hpp` files.
- License information is managed separately (LICENSE file in repository root).

### Robustness checklist before delivering code
Before presenting any code artifact, verify:
- [ ] No raw owning pointers -- use `std::unique_ptr` / `std::shared_ptr` / value types.
- [ ] No `using namespace std;` in headers.
- [ ] All inputs validated, exceptions thrown where appropriate.
- [ ] No magic numbers -- use named constants.
- [ ] No compiler warnings with `-Wall -Wextra -Wpedantic`.
- [ ] Doxygen comments on all public interfaces.
- [ ] Comments in English, ASCII characters only (no Unicode box-drawing).
- [ ] `using namespace loki;` present in `.cpp` files after the last `#include`.

---

## Project Structure Reference

```
loki/
+-- CLAUDE.md                        <- this file
+-- CMakeLists.txt
+-- CMakePresets.json
+-- cmake/
|   +-- CompilerOptions.cmake
|   +-- Dependencies.cmake
|   +-- Version.cmake
+-- apps/
|   +-- loki/
|       +-- CMakeLists.txt
|       +-- main.cpp
+-- libs/
|   +-- loki_core/
|   |   +-- CMakeLists.txt
|   |   +-- include/
|   |   |   +-- loki/
|   |   |       +-- core/
|   |   |       |   +-- exceptions.hpp
|   |   |       |   +-- version.hpp
|   |   |       |   +-- config.hpp
|   |   |       |   +-- configLoader.hpp
|   |   |       |   +-- directoryScanner.hpp
|   |   |       |   +-- logger.hpp
|   |   |       +-- timeseries/
|   |   |       |   +-- timeSeries.hpp
|   |   |       |   +-- timeStamp.hpp
|   |   |       +-- stats/
|   |   |       |   +-- descriptive.hpp
|   |   |       +-- io/
|   |   |           +-- loader.hpp
|   |   |           +-- dataManager.hpp
|   |   |           +-- gnuplot.hpp
|   |   |           +-- plot.hpp
|   |   +-- src/
|   |       +-- core/
|   |       |   +-- configLoader.cpp
|   |       |   +-- directoryScanner.cpp
|   |       |   +-- logger.cpp
|   |       +-- timeseries/
|   |       |   +-- timeSeries.cpp
|   |       |   +-- timeStamp.cpp
|   |       +-- stats/
|   |       +-- io/
|   |           +-- loader.cpp
|   |           +-- dataManager.cpp
|   |
|   +-- loki_homogeneity/
|       +-- CMakeLists.txt
|       +-- include/
|       |   +-- loki/
|       |       +-- homogeneity/
|       |           +-- changePointDetector.hpp
|       |           +-- snhtDetector.hpp
|       |           +-- seriesAdjuster.hpp
|       |           +-- homogenizer.hpp
|       +-- src/
|           +-- changePointDetector.cpp
|           +-- snhtDetector.cpp
|           +-- seriesAdjuster.cpp
|           +-- homogenizer.cpp
|
+-- tests/
|   +-- CMakeLists.txt
|   +-- unit/
|   |   +-- core/
|   |   +-- homogeneity/
|   +-- integration/
|
+-- config/
|   +-- loki_homogeneity.json
|
+-- scripts/
|   +-- build.sh
|   +-- clean.sh
|
+-- docs/
```

---

## Dependencies

All dependencies managed via CMake `FetchContent` -- no vendored source code in repository.

| Library | Version | Purpose |
|---|---|---|
| `Eigen3` | 3.4.0 | Linear algebra, LSQ, SVD, Kalman |
| `nlohmann_json` | 3.11.3 | Configuration files |
| `Catch2` | 3.5.2 | Unit and integration tests |

**Removed:** `newmat` (obsolete), `Dependencies/json-3.5.0` (vendored, replaced by FetchContent).

---

## Planned Modules

| Module | Description |
|---|---|
| `loki_homogeneity` | Change point detection, SNHT, Pettitt, Buishand -- **current focus** |
| `loki_outlier` | IQR, Z-score, Mahalanobis, hat matrix, clustering-based |
| `loki_decomposition` | Trend, seasonality, residuals (STL, classical) |
| `loki_svd` | SVD/PCA decomposition of time series |
| `loki_filter` | Moving average, Butterworth, Savitzky-Golay |
| `loki_kalman` | Kalman filter, smoother, Extended Kalman Filter |
| `loki_arima` | AR, ARMA, ARIMA, SARIMA modelling |
| `loki_spectral` | FFT, power spectrum, Lomb-Scargle (for unevenly sampled data) |
| `loki_stationarity` | ADF, KPSS, unit root tests |
| `loki_clustering` | k-means, DBSCAN, Hungarian algorithm |
| `loki_qc` | Quality control, gap filling, flagging |
| `loki_regression` | Linear, polynomial, robust regression |

---

## Build Instructions

### Platform: Windows MINGW64 shell, UCRT64 toolchain (GCC 13.2)

```bash
# From the LOKI repository root:

mkdir -p build/debug
cd build/debug

cmake ../.. \
  -G "MinGW Makefiles" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=/c/msys64/ucrt64/bin/g++.exe \
  -DCMAKE_MAKE_PROGRAM=/c/msys64/ucrt64/bin/mingw32-make.exe \
  -DLOKI_BUILD_TESTS=OFF

/c/msys64/ucrt64/bin/mingw32-make -j4
```

### First run -- copy runtime DLLs
After first build, copy UCRT64 runtime DLLs next to the executable:

```bash
cp /c/msys64/ucrt64/bin/libgcc_s_seh-1.dll   build/debug/apps/loki/
cp /c/msys64/ucrt64/bin/libstdc++-6.dll       build/debug/apps/loki/
cp /c/msys64/ucrt64/bin/libwinpthread-1.dll   build/debug/apps/loki/
```

These DLLs are required because the shell PATH points to MINGW64 but the compiler
is from UCRT64. Without them the executable exits with code 127 (DLL not found).

### Running
```bash
./build/debug/apps/loki/loki.exe config/loki_homogeneity.json
```

### CMakePresets (future use with Ninja)
Presets in `CMakePresets.json` support both `MinGW Makefiles` and `Ninja` generators.
Ninja is not currently installed. To install it, download `ninja-win.zip` from
https://github.com/ninja-build/ninja/releases and place `ninja.exe` in
`C:/msys64/ucrt64/bin/`. Then use:

```bash
cmake --preset debug
cmake --build --preset debug
```

---

## Workspace Layout

The workspace root is an **absolute path** set in `config/loki_homogeneity.json`
under the `"workspace"` key. All other paths are relative to it:

```
<workspace>/
+-- INPUT/          <- input time series files
+-- OUTPUT/
    +-- LOG/        <- log files written by Logger
    +-- CSV/        <- exported results (future)
    +-- IMG/        <- gnuplot images (future)
```

Example config (`config/loki_homogeneity.json`):
```json
{
    "workspace": "C:/Users/eliasmichal/Documents/Osobne",
    "input": {
        "file": "SENSOR_DATA.txt",
        "time_format": "gpst_seconds",
        "delimiter": ";",
        "comment_char": "%",
        "columns": [],
        "merge_strategy": "separate"
    },
    "output": {
        "log_level": "info"
    },
    "homogeneity": {
        "method": "snht",
        "significance_level": 0.05
    }
}
```

The `"file"` path is resolved relative to `<workspace>/INPUT/` automatically.
Do **not** include `INPUT/` in the workspace path itself -- the loader appends it.

---

## Known Issues and Workarounds

### Unicode in source files (Windows GCC)
GCC on Windows can misparse tokens when source files contain UTF-8 box-drawing
characters (e.g. `─`, `│`) in comments. Symptoms: `expected unqualified-id before '&'`,
even on syntactically correct catch blocks.
**Rule: all comments must use ASCII characters only.**

### exceptions.hpp must be included early
`LOKIException` and its subclasses must be visible before any catch block.
`logger.hpp` includes `exceptions.hpp`, so including `logger.hpp` first is sufficient.
If a translation unit does not include `logger.hpp`, add:
```cpp
#include "loki/core/exceptions.hpp"
```

### namespace loki in .cpp files
All LOKI types live in `namespace loki`. Library `.cpp` files add:
```cpp
using namespace loki;
```
after the last `#include` so exception names and other types can be used unqualified.

### DLL mismatch on Windows
The MINGW64 shell PATH includes `/mingw64/bin/` but the compiler and runtime are
in `/c/msys64/ucrt64/bin/`. Always copy the three UCRT64 DLLs next to the executable
after first build (see Build Instructions above).

---

## Notes and Reminders

- Data files (`.csv`, `.dat`, etc.) and plot outputs (`.eps`, `.png`) are **not** committed.
- Third-party dependencies are managed via CMake `FetchContent` -- no vendored source code.
- The build directory is always `build/` and is gitignored.
- `newmat` is replaced by `Eigen3` throughout the codebase.
- This file should be updated whenever architecture decisions change.