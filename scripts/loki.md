# LOKI -- Script Reference

Single unified script for all build, clean, run and test operations.

```
./scripts/loki.sh <command> [app] [options]
```

---

## Commands

| Command | What it does |
|---|---|
| `build` | Full cmake configure + make. Use when changing CMakeLists, adding libraries, or first build. |
| `clean` | Remove build directories. |
| `run` | Incremental build (changed files only) + run application. No cmake reconfigure. |
| `test` | Incremental build + run Catch2 test suite via ctest. |

---

## Apps

| App | Executable | Default config |
|---|---|---|
| `loki` (default) | `build/debug/apps/loki/loki.exe` | `config/loki_homogeneity.json` |
| `homogenization` | `build/debug/apps/loki_homogeneity/homogenization.exe` | `config/homogenization.json` |
| `all` | all apps | -- (build/clean only) |

---

## Options

| Option | Commands | Description |
|---|---|---|
| `debug` / `release` | build, clean | Build preset. Default: `debug`. |
| `--tests` | build, clean, test | Enable Catch2 test suite. |
| `--copy-dlls` | build, run | Copy UCRT64 runtime DLLs next to executables. |
| `--rebuild` | clean, test | Clean everything before building. |
| `--filter <pattern>` | test | Run only tests whose name matches pattern. |
| `--verbose` | test | Print every assertion, not just failures. |
| `--list` | test | List all available tests without running. |
| `<config.json>` | run | Override default config file. |

---

## build

Full cmake configure + make. Always reconfigures from scratch -- slower but reliable.
Use when: first build, adding a new library, changing CMakeLists.txt.

```bash
# Standard debug build
./scripts/loki.sh build

# Debug build with tests
./scripts/loki.sh build --tests

# First build -- copy DLLs so executables can run
./scripts/loki.sh build --copy-dlls

# First build with tests
./scripts/loki.sh build --tests --copy-dlls

# Build specific app only (still builds all libs, just runs the right exe target)
./scripts/loki.sh build homogenization --copy-dlls

# Build all apps + copy DLLs to all destinations
./scripts/loki.sh build all --copy-dlls

# Release build (no tests)
./scripts/loki.sh build release
```

---

## clean

Removes build directories. Does not touch source files or config.

```bash
# Remove both debug and release builds
./scripts/loki.sh clean

# Remove only debug build
./scripts/loki.sh clean debug

# Remove only release build
./scripts/loki.sh clean release

# Clean everything and immediately rebuild (debug + DLLs)
./scripts/loki.sh clean --rebuild

# Clean and rebuild including tests
./scripts/loki.sh clean --rebuild --tests
```

---

## run

Incremental build (only recompiles changed files) then launches the executable.
Does not call cmake -- fast. Automatically checks for missing DLLs and copies them.

```bash
# Run loki (default app, default config)
./scripts/loki.sh run

# Run loki with a different config
./scripts/loki.sh run config/my_config.json

# Run homogenization (default config: config/homogenization.json)
./scripts/loki.sh run homogenization

# Run homogenization with a different config
./scripts/loki.sh run homogenization config/my_station.json
```

---

## test

Incremental build + ctest. If the tests directory does not exist yet,
automatically runs a full build with --tests --copy-dlls.
Tests are only supported in debug builds.

```bash
# Run all tests
./scripts/loki.sh test

# Run all tests, clean rebuild first
./scripts/loki.sh test --rebuild

# Run only tests matching a pattern
./scripts/loki.sh test --filter exceptions
./scripts/loki.sh test --filter timeStamp
./scripts/loki.sh test --filter descriptive

# Run matching tests with full assertion output
./scripts/loki.sh test --filter stats --verbose

# List all available test names (no execution)
./scripts/loki.sh test --list

# Full verbose run of all tests
./scripts/loki.sh test --verbose
```

---

## Typical workflows

### First setup
```bash
./scripts/loki.sh build --tests --copy-dlls
```

### Daily work -- edit code, run app
```bash
./scripts/loki.sh run
# or
./scripts/loki.sh run homogenization
```

### Daily work -- edit code, run tests
```bash
./scripts/loki.sh test
./scripts/loki.sh test --filter gapFiller
```

### Something is broken -- clean start
```bash
./scripts/loki.sh clean --rebuild
```

### Clean start with tests
```bash
./scripts/loki.sh clean --rebuild --tests
```

### Release build
```bash
./scripts/loki.sh build release
```

### Adding a new app (e.g. kalman)
1. Add to `APP_EXE` and `APP_CONFIG` maps in `scripts/loki.sh`:
```bash
APP_EXE[kalman]="apps/kalman/kalman.exe"
APP_CONFIG[kalman]="config/kalman.json"
```
2. Done -- all commands work automatically for the new app.

---

## Notes

| Topic | Note |
|---|---|
| Where to run | Always from the repository root (where `CMakeLists.txt` is). |
| DLLs | Required after first build and after toolchain change. `--copy-dlls` handles it. Copied to all known app dirs and tests dir. |
| cmake vs make | `build` always runs cmake (slow). `run` and `test` only call make (fast). |
| Tests in release | Not supported. Tests are always debug-only. |
| Adding tests | Add `.cpp` to `tests/unit/<module>/` and register in `tests/CMakeLists.txt` via `loki_add_test_exe()`. |
| Config override | Only valid for `run`. For `build`, `clean`, `test` it is ignored. |
