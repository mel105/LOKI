cmake_minimum_required(VERSION 3.25)

project(
    loki
    VERSION 0.3.0
    DESCRIPTION "LOKI — Statistical Analysis Toolkit for Time Series Data"
    LANGUAGES CXX
)

# ── Language standard ──────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── CMake module path ──────────────────────────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# ── Project-wide helpers ───────────────────────────────────────────────────────
include(CompilerOptions)
include(Dependencies)
include(Version)

# ── Options ───────────────────────────────────────────────────────────────────
option(LOKI_BUILD_TESTS "Build unit and integration tests" ON)

# ── Libraries ─────────────────────────────────────────────────────────────────
add_subdirectory(libs/loki_core)
add_subdirectory(libs/loki_homogeneity)
add_subdirectory(libs/loki_outlier)
add_subdirectory(libs/loki_filter)
add_subdirectory(libs/loki_regression)
add_subdirectory(libs/loki_stationarity)
add_subdirectory(libs/loki_arima)
add_subdirectory(libs/loki_ssa)
add_subdirectory(libs/loki_decomposition)
add_subdirectory(libs/loki_spectral)
add_subdirectory(libs/loki_kalman)
add_subdirectory(libs/loki_qc)
add_subdirectory(libs/loki_clustering)
add_subdirectory(libs/loki_simulate)
add_subdirectory(libs/loki_evt)
add_subdirectory(libs/loki_kriging)

# ── Applications ──────────────────────────────────────────────────────────────
add_subdirectory(apps/loki)
add_subdirectory(apps/loki_homogeneity)
add_subdirectory(apps/loki_outlier)
add_subdirectory(apps/loki_filter)
add_subdirectory(apps/loki_regression)
add_subdirectory(apps/loki_stationarity)
add_subdirectory(apps/loki_arima)
add_subdirectory(apps/loki_ssa)
add_subdirectory(apps/loki_decomposition)
add_subdirectory(apps/loki_spectral)
add_subdirectory(apps/loki_kalman)
add_subdirectory(apps/loki_qc)
add_subdirectory(apps/loki_clustering)
add_subdirectory(apps/loki_simulate)
add_subdirectory(apps/loki_evt)
add_subdirectory(apps/loki_kriging)

# ── Tests ─────────────────────────────────────────────────────────────────────
if(LOKI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ── Demos ─────────────────────────────────────────────────────────────────────
option(LOKI_BUILD_DEMOS "Build demo executables" ON)
if(LOKI_BUILD_DEMOS)
    add_subdirectory(tests/demo)
endif()