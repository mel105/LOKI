# Getting Started

## Prerequisites

### Windows (MSYS2 UCRT64) — tested and supported

1. Install [MSYS2](https://www.msys2.org/). During setup choose the **UCRT64** environment.

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

!!! note
    Use the **UCRT64** shell for all build and run commands.
    The MINGW64 shell also works but UCRT64 is the recommended environment.

### Linux — planned, not yet tested

LOKI is developed on Windows/MSYS2. Linux support is planned. The codebase uses
standard C++20 with no platform-specific APIs, so a GCC 13+ toolchain with CMake 3.25+
and gnuplot should work without modification. Verified Linux instructions will be added
once testing is complete.

---

## Clone

```bash
git clone https://github.com/mel105/LOKI.git
cd LOKI
```

---

## Build

All dependencies are fetched automatically via CMake `FetchContent` on first configure.
An internet connection is required for the first build.

| Library | Version | Purpose |
|---------|---------|---------|
| Eigen3 | 3.4.0 | Linear algebra, LSQ, SVD |
| nlohmann_json | 3.11.3 | Configuration files |
| Catch2 | 3.5.2 | Unit and integration tests |

```bash
# Debug build -- with tests
cmake --preset debug
cmake --build --preset debug

# Release build -- optimised, no tests
cmake --preset release
cmake --build --preset release
```

Build output lands in `build/debug/` or `build/release/`.

!!! tip "Running outside the MSYS2 shell (Windows)"
    Copy the required UCRT64 runtime DLLs alongside the executables:

    - `libgcc_s_seh-1.dll`
    - `libstdc++-6.dll`
    - `libwinpthread-1.dll`

    These are found in `C:/msys64/ucrt64/bin/`.

---

## Run tests

```bash
ctest --preset debug
```

Or run individual test executables directly from `build/debug/tests/`.

---

## First run

Each module reads a JSON configuration file. Here is a minimal end-to-end example
using `loki_outlier`.

### 1. Prepare a workspace

```
my_workspace/
  INPUT/
    my_data.txt      # semicolon-delimited: MJD ; value
  config.json
```

### 2. Write a minimal config.json

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

### 3. Run from the workspace directory

```bash
loki_outlier.exe config.json
```

### 4. Inspect results

```
my_workspace/
  OUTPUT/
    IMG/           # PNG plots
    CSV/           # numerical results with semicolon delimiter
    PROTOCOLS/     # plain-text analysis report
    LOG/           # execution log
```

---

## Next steps

- Read the [Configuration Reference](config.md) for a full description of all options.
- Browse the [Module list](modules.md) to find the right tool for your data.
