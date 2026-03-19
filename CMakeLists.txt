cmake_minimum_required(VERSION 3.25)

project(
    loki
    VERSION 0.1.0
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

# ── Applications ──────────────────────────────────────────────────────────────
add_subdirectory(apps/loki)
add_subdirectory(apps/loki_homogeneity)

# ── Tests ─────────────────────────────────────────────────────────────────────
if(LOKI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
